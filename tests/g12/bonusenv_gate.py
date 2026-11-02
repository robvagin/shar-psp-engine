#!/usr/bin/env python3
"""Гейт №1109 (окружение бонус-трасс): фон неба уровня в b0N.shr, свет трассы против света уровня, привязка кольца-панорамы (хост-тест). Красные контроли на каждой проверке.
python tests/g12/bonusenv_gate.py   (из корня копии, .venv; build/sv12e/bonus/b0N.shr — вывод tools/bonus/export.py --track N)"""
import os, struct, subprocess, sys, collections
from pathlib import Path
import numpy as np
R = Path(__file__).resolve().parents[2]; os.chdir(R)
SP = [str(R / 'tools'), str(R / 'hit-and-run-web/tools'), str(R / 'tools/missions'), str(R / 'tools/bonus')]; sys.path[:0] = SP
S83 = Path.home() / 'Downloads/shar-psp-integ/build/test83/PSP/GAME/SHARTEST15'
fails = []
def check(name, ok, why=''):
    print(('ok     ' if ok else 'ПАДЕНИЕ ') + name + (('  — ' + why) if (why and not ok) else '')); (not ok) and fails.append(name)
def sky_tris(path):
    b = Path(path).read_bytes(); h = struct.unpack_from('<26I', b, 0); pt = h[19]; t = 0
    for i in range(h[2]):
        r = struct.unpack_from('<6f5i4f3I2I', b, pt + 80 * i)
        if r[10] == 2: t += r[9] // 3
    return t
lvl = lambda L: S83 / ('level.shr' if L == 1 else f'l{L}/level.shr')

# 1. небо уровня в трассе: b01, b04..b07 — небо своего уровня (то же число треугольников); b02, b03 — своё небо, фон не добавлен
own = {2: 3600, 3: 4497}
for n in range(1, 8):
    p = R / f'build/sv12e/bonus/b0{n}.shr'
    got = sky_tris(p) if p.exists() else -1
    want = own.get(n, sky_tris(lvl(n)))
    check(f'b0{n}: небо {"своё" if n in own else "уровня %d" % n} ({want} треуг.)', got == want, f'{got} против {want}')
# красный контроль: прежнее поведение (фон не пишется) — экспорт b04 с BONUSENV1109_OLD=1
env = dict(os.environ, BONUSENV1109_OLD='1', PATH=str(Path.home() / 'Downloads/shar-psp/build/xcshim/bin') + ':' + os.environ['PATH'], SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk')
tmp = R / 'build/sv12e/bonus_old'; tmp.mkdir(parents=True, exist_ok=True)
r = subprocess.run([sys.executable, 'tools/bonus/export.py', '--track', '4', '--out', str(tmp)], capture_output=True, text=True, env=env)
check('  красный контроль BONUSENV1109_OLD=1: у b04 неба уровня нет (как было)', r.returncode == 0 and sky_tris(tmp / 'b04.shr') == 0, r.stderr[-200:])

# 2. свет трассы = свет уровня у b02…b07 (данные оригинала совпали); b01 — свой (4 направленных, синеватый фон) — остаётся данными трассы
import level_light as LL
ART = R / 'hit-and-run-web/source/game/art'
irr = lambda G: np.array(G['amb'], float) + .25 * np.sum([c for _, c in G['dirs']], 0).astype(float)
same = [n for n in range(1, 8) if np.allclose(irr(LL.group_light(ART / f'b0{n}.p3d')), irr(LL.level_light(n)))]
check('свет: у b02…b07 группа света трассы совпала с группой уровня N, у b01 нет', same == [2, 3, 4, 5, 6, 7], str(same))
check('  красный контроль: сравнение с чужим уровнем (b04 с L1) не совпадает', not np.allclose(irr(LL.group_light(ART / 'b04.p3d')), irr(LL.level_light(1))))

# 3. кольцо-панорама: привязка трассы к центру ближайшей панорамы (хост-тест, красный — без привязки)
e = dict(os.environ, SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk', PATH=str(Path.home() / 'Downloads/shar-psp/build/xcshim/bin') + ':' + os.environ['PATH'])
for tag, extra, red in (('new', [], 0), ('old', ['-DBNSENV_NOANCHOR=1'], 1)):
    c = subprocess.run(['cc', '-Iviewer', '-Wno-unused', *extra, 'tests/g12/bonusenv_test.c', '-lm', '-o', f'/tmp/bonusenv_{tag}'], capture_output=True, text=True, env=e)
    if c.returncode: check('bonusenv_test собирается', False, c.stderr[-200:]); continue
    ok = subprocess.run([f'/tmp/bonusenv_{tag}', str(S83)], capture_output=True, text=True).returncode
    check('кольцо-панорама: ' + ('с привязкой все 7 трасс получают слой' if not red else 'красный контроль: без привязки слоя нет'), (ok != 0) == bool(red))
# 4. файлы и строки набора
rules = Path('tools/ship_rules.txt').read_text()
check('набор: build/sv12e/bonus/b0N.shr для N=1…7 и правила bonus/b0N.shr в ship_rules', all((R / f'build/sv12e/bonus/b0{n}.shr').exists() for n in range(1, 8)) and 'bonus/b01.shr' in rules and 'bonus/b07.shr' in rules)
print('\nЗЕЛЁНЫЙ' if not fails else f'\nКРАСНЫЙ: {len(fails)}: {fails}'); sys.exit(1 if fails else 0)
