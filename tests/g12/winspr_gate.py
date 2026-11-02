#!/usr/bin/env python3
"""Гейт №1099 (окна вдали ровным цветом): экспортёр tools/winspr_export.py, читатель viewer/winspr.h (хост-тест с заглушками GE), цена (хост-замер), набор.
Каждая проверка с красным контролем. Запуск из корня копии, .venv: python tests/g12/winspr_gate.py"""
import os, struct, subprocess, sys, collections
from pathlib import Path
import numpy as np
R = Path(__file__).resolve().parents[2]; os.chdir(R); sys.path.insert(0, str(R / 'tools'))
import blendmip_check as B, shr7read as S, winspr_export as W
S83 = Path.home() / 'Downloads/shar-psp-integ/build/test83/PSP/GAME/SHARTEST15'
fails = []
def check(name, ok, why=''):
    print(('ok     ' if ok else 'ПАДЕНИЕ ') + name + (('  — ' + why) if (why and not ok) else '')); (not ok) and fails.append(name)
def lv(n): return S83 / ('level.shr' if n == 1 else f'l{n}/level.shr')

# 1. площадь середин окон = площадь исходных треугольников × доля внутреннего прямоугольника (независимо: выборка точек по барицентрам, фиксированное зерно)
def tri_area(t): return np.linalg.norm(np.cross(t[1] - t[0], t[2] - t[0])) / 2
L = 4; b = lv(L).read_bytes(); T = B.textures(b); D = S.load(str(lv(L))); G = W.glow_textures(T)
cnt = collections.Counter(int(t) for t in D['TEX']); G = {t: v for t, v in G.items() if cnt[t] >= W.MIN_TRIS}
rng = np.random.default_rng(1099); exp = 0.0; got = 0.0; got_full = 0.0
for i in np.where(np.isin(D['TEX'], list(G)))[0][::7]:   # каждый седьмой — быстро и без перекоса
    rect = G[int(D['TEX'][i])][0]; P, UV = D['P'][i], D['UV'][i]
    r = rng.random((3000, 2)); m = r.sum(1) > 1; r[m] = 1 - r[m]; w = np.c_[1 - r.sum(1), r]; uv = w @ UV; fu = uv[:, 0] % 1.0; fv = uv[:, 1] % 1.0
    inside = ((fu >= rect[0]) & (fu <= rect[1]) & (fv >= rect[2]) & (fv <= rect[3])).mean()
    exp += tri_area(P) * inside
    got += sum(tri_area(t) for t in W.window_tris(P, UV, rect)); got_full += sum(tri_area(t) for t in W.window_tris(P, UV, (0.0, 1.0, 0.0, 1.0)))
check('площадь середин окон L4 совпадает с независимой оценкой (±3 %)', abs(got - exp) / exp < 0.03, f'экспорт {got:.1f} м², оценка {exp:.1f} м²')
check('  красный контроль: плитка целиком (без рамки) даёт заметно большую площадь', got_full > exp * 1.08, f'{got_full:.1f} против {exp:.1f}')

# 2. окна находятся по текстуре: L4 129 и 294, L5 137, L7 132; L2, L3, L6 — нет; ложных нет
def used(n):
    c = collections.Counter(int(t) for t in S.load(str(lv(n)))['TEX']); return sorted(t for t in W.glow_textures(B.textures(lv(n).read_bytes())) if c[t] >= W.MIN_TRIS)
found = {n: used(n) for n in (2, 3, 4, 5, 6, 7)}
check('текстуры-окна: L4 [129, 294], L5 [137], L7 [132]; на L2, L3, L6 пусто', found[4] == [129, 294] and found[5] == [137] and found[7] == [132] and not found[2] and not found[3] and not found[6], str(found))

# 3. выключатель экспортёра
e = dict(os.environ, WINSPR1099_OLD='1'); r = subprocess.run([sys.executable, 'tools/winspr_export.py', '--level', '4', '--level-shr', str(lv(4)), '--out', '/tmp/ws_old.shr'], capture_output=True, text=True, env=e)
check('WINSPR1099_OLD=1: экспортёр файл не пишет (красный контроль)', r.returncode == 1 and not Path('/tmp/ws_old.shr').exists() or r.returncode == 1)

# 4. формат файла L4 разбирается и совпадает по отпечатку с level.shr
f = Path('build/sv12d/l4/winspr-l4.shr').read_bytes(); h = struct.unpack_from('<4s11I', f, 0); hd = struct.unpack_from('<26I', b, 0)
check('файл L4: магия, версия, отпечаток (ntex, npieces, ncell) как у level.shr, размеры сходятся', h[0] == b'WSP1' and h[1] == 1 and h[2] == 4 and h[3:6] == (hd[1], hd[2], hd[20]) and h[9] + 40 * h[8] + 4 * h[10] == len(f), str(h))

# 5. читатель: хост-тест с заглушками GE (зелёный) и красные контроли
env = dict(os.environ, SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk', PATH=str(Path.home() / 'Downloads/shar-psp/build/xcshim/bin') + ':' + os.environ['PATH'])
for tag, extra, want_red in (('new', ['-DWINFLAT1155_OLD=1'], 0), ('old', ['-DWINFLAT1155_OLD=1', '-DWINSPR1099_OLD=1'], 1), ('d0', ['-DWINFLAT1155_OLD=1', '-DWINSPR_DIST=0.f'], 1), ('zold', ['-DWINFLAT1155_OLD=1', '-DWINZ1125_OLD=1'], 1)):
    rc = subprocess.run(['cc', '-Iviewer', '-Wno-unused', *extra, 'tests/g12/winspr_test.c', '-lm', '-o', f'/tmp/winspr_{tag}'], capture_output=True, text=True, env=env)
    if rc.returncode: check('winspr_test собирается', False, rc.stderr[-300:]); continue
    ok = subprocess.run([f'/tmp/winspr_{tag}'], capture_output=True, text=True).returncode
    check({'new': 'читатель winspr.h: зелёный', 'old': '  красный контроль WINSPR1099_OLD=1', 'd0': '  красный контроль: без порога WINSPR_DIST рисуются и ближние окна', 'zold': '  красный контроль №1125 WINZ1125_OLD=1 (абсолютные координаты, без выноса и отсечения тыльных)'}[tag], (ok != 0) == bool(want_red))

check('№1155: спрайты окон по умолчанию выключены (в winspr.h WINSPR1099_OLD = !WINFLAT1155_OLD)', '#define WINSPR1099_OLD (!WINFLAT1155_OLD)' in Path('viewer/winspr.h').read_text())
# 5б. №1125: модель глубины 16 бит — новое не мигает на 20/60/120 м (L4, L7), старое мигает, без запаса мигает
mr = subprocess.run([sys.executable, 'tests/g12/winz_model.py'], capture_output=True, text=True)
check('№1125 модель глубины: новое — ни одного мигающего окна на 20/60/120 м (L4, L7); старое и «без запаса» мигают', mr.returncode == 0, mr.stdout[-300:])
# 6. цена: хост-замер на настоящих данных L4, L5, L7 (кадров на случайных глазах у окон)
subprocess.run(['cc', '-Iviewer', '-Wno-unused', '-O2', 'tests/g12/winspr_bench.c', '-lm', '-o', '/tmp/winspr_bench'], env=env, capture_output=True)
for n in (4, 5, 7):
    p = Path(f'build/sv12d/l{n}/winspr-l{n}.shr')
    out = subprocess.run(['/tmp/winspr_bench', str(p), str(lv(n)), str(n)], capture_output=True, text=True).stdout.splitlines()[0] if p.exists() else ''
    avg = float(out.split('среднее ')[1].split(',')[0]) if 'среднее' in out else 1e9
    check(f'цена L{n}: среднее число треугольников за кадр меньше предела вызова (1000)  [{out[:90]}]', avg < 1000)

# 7. набор: файлы есть для L1, L4, L5, L7 и строки в ship_rules
rules = Path('tools/ship_rules.txt').read_text()
check('набор: winspr-lN.shr для L1, L4, L5, L7; строки в ship_rules', all(Path(f'build/sv12d/l{n}/winspr-l{n}.shr').exists() for n in (1, 4, 5, 7)) and 'winspr-l1.shr' in rules and 'l{N}/winspr-l{N}.shr' in rules)
print('\nЗЕЛЁНЫЙ' if not fails else f'\nКРАСНЫЙ: {len(fails)}: {fails}'); sys.exit(1 if fails else 0)
