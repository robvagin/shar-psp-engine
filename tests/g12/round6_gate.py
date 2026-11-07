#!/usr/bin/env python3
"""Гейт круга 6 окна g12-data (перепроверка по оригиналу и тестам). Без эмулятора. Каждая проверка идёт парой: новое поведение зелёное,
то же с выключателем _OLD=1 (или с испорченным входом) краснеет. Запуск: python tests/g12/round6_gate.py  (из корня копии, .venv)."""
import os, re, struct, subprocess, sys, glob, collections
from pathlib import Path
R = Path(__file__).resolve().parents[2]; os.chdir(R)
SP = [str(R / 'tools'), str(R / 'hit-and-run-web/tools'), str(R / 'tools/missions')]
sys.path[:0] = SP
INTEG = Path.home() / 'Downloads/shar-psp-integ/build'
S83, S81 = INTEG / 'test83/PSP/GAME/SHARTEST15', INTEG / 'test81/PSP/GAME/SHARTEST15'
fails = []
def check(name, ok, why=''):
    print(('ok     ' if ok else 'ПАДЕНИЕ ') + name + (('  — ' + why) if (why and not ok) else '')); (not ok) and fails.append(name)
def py(code, env=None):
    e = dict(os.environ); e.update(env or {})
    return subprocess.run([sys.executable, '-c', 'import sys; sys.path[:0]=%r\n%s' % (SP, code)], capture_output=True, text=True, env=e, cwd=R).stdout.strip()
def lvl(root, n, name): return root / name if n == 1 else root / f'l{n}' / name

# 1. монеты: вызов 11 — всегда пять монет вперёд (оригинал диспетчеризует по номеру, имя только подпись)
c = "import prp2_export as P; print(P.call_op('SPAWN_25_COINS_Z'), P.call_op('SPAWN_5_COINS_Z'), P.call_op('SPAWN_1_COIN'))"
n, o = py(c), py(c, {'PRP2_COINS25_OLD': '1'})
check('P11-12/P16-19 SPAWN_25_COINS_Z = (COINS_Z, 5)', n.startswith('(2, 5) (2, 5)'), n)
check('  красный контроль PRP2_COINS25_OLD=1 даёт 25', o.startswith('(2, 25)'), o)
# 1б. номер вызова ↔ имя на диске: везде согласованы, кроме номера 11 (два имени, одно действие)
import stateprop_dump as SD
pairs = collections.defaultdict(set)
def walk(x):
    if isinstance(x, dict):
        for st in x.get('states', []) if 'states' in x else []:
            for nm, cid, _f in st.get('callbacks', []): pairs[cid].add(nm)
        for v in x.values(): walk(v)
    elif isinstance(x, (list, tuple)):
        for v in x: walk(v)
for N in range(1, 8): walk(SD.scan_level(N))
multi = {k: v for k, v in pairs.items() if len(v) > 1}
check('вызовы стейтпропов: имя ↔ номер однозначны, кроме 11', list(multi) == [11], str(multi))

# 2. частицы удара: девять видов оригинала; искры и кокосы — «нет частиц»
c = "import prp2_export as P; d=P.PARTICLE_CODE; print(d.get('spark'), d.get('needles'), d.get('meter'), d.get('popsicle'), d.get('shrub'), d.get('trash'), d.get('leaves'), d.get('mail'))"
n, o = py(c), py(c, {'PRP2_PARTICLES_OLD': '1'})
check('P11-13 частицы: искр нет, 7/24/26 как номера оригинала, 3/4/5/6', n == 'None 7 24 26 3 4 5 6', n)
check('  красный контроль PRP2_PARTICLES_OLD=1', o.split()[0] == '1' and o.split()[1] == 'None', o)

# 3. камеры зон машины: нейтральные рельсы (ни бита 1, ни бита 8) работают и из машины
import zonecam_export as Z
def cars(level): return len(Z.car_cams(level))
newc = [cars(N) for N in range(1, 8)]
oldc = [int(py("import zonecam_export as Z; print(len(Z.car_cams(%d)))" % N, {'ZONECAM_RAILNEUTRAL_OLD': '1'})) for N in range(1, 8)]
check('P18-02 нейтральные рельсы в пакете машины: +1,+2,0,+1,+2,0,0', [a - b for a, b in zip(newc, oldc)] == [1, 2, 0, 1, 2, 0, 0], f'{newc} / {oldc}')
shipped = []
for N in range(1, 8):
    f = lvl(S83, N, f'carzone-l{N}.shr'); shipped.append(struct.unpack_from('<I', f.read_bytes(), 124)[0] if f.exists() else -1)
check('  в наборе TEST83 carzone-lN.shr = старый экспорт (число камер совпадает, пакет круга 6 в набор ещё не положен)', shipped == oldc, f'{shipped} / {oldc}')

# 3б. поле Data статичной камеры: бит 0 — только машина (с порогом скорости), бит 1 — только пешком, 0 — обоим (оригинал: staticcamlocator, 10-е слово)
c = "import zonecam_export as Z; print(Z.for_car(0,0), Z.for_car(0,1), Z.for_car(0,2), Z.for_foot(0,0), Z.for_foot(0,1), Z.for_foot(0,2))"
n, o = py(c), py(c, {'ZONECAM_DATA_OLD': '1'})
check('P18-01 Data: машина = 0 и 1, пешком = 0 и 2', n == 'True True False True False True', n)
check('  красный контроль ZONECAM_DATA_OLD=1 толкует иначе', o != n, o)

# 4. пешеходные пути: замкнут при совпадении по каждой оси в 0,001 м
import numpy as np, traffic_export as T
a = np.array([[0, 0, 0], [5, 0, 0], [0.004, 0, 0]])
check('P17-28 разница 4 мм по одной оси — путь открыт', T.ped_closed(a[[0, 2]]) is False)
check('P17-28 разница 0,5 мм по трём осям — замкнут', T.ped_closed(np.array([[0, 0, 0], [5e-4, 5e-4, 5e-4]])) is True)
os.environ['PED_CLOSE_OLD'] = '1'; old_closed = T.ped_closed(a[[0, 2]]); del os.environ['PED_CLOSE_OLD']
check('  красный контроль PED_CLOSE_OLD=1 замыкает по расстоянию', old_closed is True)

# 5. полосы дорог: центр полосы k из N — (k + 0,5) / N от угла 3; при выключателе полосы идут зеркально
roads_new = T.road_network(1)[1]; os.environ['TRAFFIC_LANE_C3_OLD'] = '1'
try:
    import importlib; importlib.reload(T); roads_old = T.road_network(1)[1]
finally:
    del os.environ['TRAFFIC_LANE_C3_OLD']; importlib.reload(T)
ok = bool(roads_new) and len(roads_new) == len(roads_old)
mirrored = 0
if ok:
    for rn, ro in zip(roads_new, roads_old):
        L = rn['lanes']
        if L == 2 and np.allclose(rn['pts'][0], ro['pts'][1]) and np.allclose(rn['pts'][1], ro['pts'][0]) and not np.allclose(rn['pts'][0], rn['pts'][1]): mirrored += 1
check('P16-03 полоса k нового = полоса N-1-k прежнего (двухполосные дороги L1 зеркальны)', ok and mirrored > 0, f'зеркальных двухполосных {mirrored}')

# 6. сложность среза в TRF1: байт 1 слова дороги, срезы 15…23 (диск)
rd = [r for r in roads_new if r.get('diff')]
check('P18-10 у дорог-срезов L1 сложность 15…23 (16 штук)', len(rd) == 16 and all(15 <= r['diff'] <= 23 for r in rd), f'{len(rd)}')

# 7. оверлей уровней: уровень в наборе TEST83 = TEST81 + байты PRP2/CAR1; COL1, заголовок, размер не тронуты (хеши mcpr/lmap/сигнатуры верны)
import sv_overlay as O
def overlay_ok(a, b):
    if len(a) != len(b) or a[:104] != b[:104]: return False
    sec = O.sections(a); lo, hi = sec['COL1']
    if a[lo:hi] != b[lo:hi]: return False
    allowed = [sec['PRP2'], sec['CAR1']]
    d = [i for i in range(len(a)) if a[i] != b[i]] if a != b else []
    return all(any(s <= i < e for s, e in allowed) for i in d)
for N in range(1, 8):
    p83, p81 = lvl(S83, N, 'level.shr'), lvl(S81, N, 'level.shr')
    if not (p83.exists() and p81.exists()): check(f'оверлей L{N}: файлы есть', False, 'нет level.shr'); continue
    A, B = p81.read_bytes(), p83.read_bytes()
    check(f'оверлей L{N}: набор = TEST81 + правки только PRP2/CAR1', overlay_ok(A, B))
    if N == 2:
        bad = bytearray(B); lo = O.sections(B)['COL1'][0] + 100; bad[lo] ^= 1
        check('  красный контроль: байт COL1 испорчен — гейт краснеет', not overlay_ok(A, bytes(bad)))

# 8. объёмы «смена света»: хост-тест читателя (зелёный) и тот же с SVLIGHT_OLD=1 (красный)
env = dict(os.environ, SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk', PATH=str(Path.home() / 'Downloads/shar-psp/build/xcshim/bin') + ':' + os.environ['PATH'])
sv1 = str(lvl(S83, 1, 'svdata-l1.shr'))
for tag, extra, want in (('new', [], 0), ('old', ['-DSVLIGHT_OLD=1'], 1)):
    r = subprocess.run(['cc', '-Iviewer', *extra, 'tests/g12/svlight_test.c', 'viewer/svdata.c', '-lm', '-o', f'/tmp/svlight_{tag}'], capture_output=True, text=True, env=env)
    if r.returncode: check('svlight_test собирается', False, r.stderr[-200:]); continue
    rc = subprocess.run([f'/tmp/svlight_{tag}', sv1], capture_output=True, text=True).returncode
    check('P12-08 объёмы смены света: ' + ('читатель зелёный' if not want else 'красный контроль SVLIGHT_OLD=1 краснеет'), (rc != 0) == bool(want))

# 9. опись набора: файлы моего окна, которые читает движок, есть в наборе и в ship_rules (урок jumpdir)
rules = [l.split('|')[0].strip() for l in Path('tools/ship_rules.txt').read_text().splitlines() if l.strip() and not l.startswith('#') and '|' in l]
rx = [re.compile('^' + re.escape(x).replace(r'\{N\}', r'\d') + '$') for x in rules]
MINE = ['surf-l%d.shr', 'svdata-l%d.shr', 'jumpdir-l%d.shr', 'zonecam-l%d.shr', 'carzone-l%d.shr', 'msncars-l%d.shr']
def inventory(rulelist, setdir):
    miss = []
    for n in MINE:
        for N in range(1, 8):
            f = n % N
            if not lvl(setdir, N, f).exists(): miss.append(('набор', N, f))
            rel = f if N == 1 else f'l{N}/{f}'
            if not any(r.match(rel) for r in rulelist): miss.append(('правило', N, f))
    return miss
m = inventory(rx, S83)
check('опись: мои файлы данных есть в наборе TEST83 и в ship_rules', not m, str(m[:6]))
check('  красный контроль: без строк surf/svdata в правилах опись краснеет', bool(inventory([r for r in rx if 'surf' not in r.pattern and 'svdata' not in r.pattern], S83)))

print('\nЗЕЛЁНЫЙ' if not fails else f'\nКРАСНЫЙ: {len(fails)}: {fails}'); sys.exit(1 if fails else 0)
