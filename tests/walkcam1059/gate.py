#!/usr/bin/env python3
"""№1059 accept60: управление пешком при смене камеры против чисел PS2 (knowledge/facts/ps2-walk-camera-stick.yaml).
  python3 gate.py exit_dirs.smoke open_left.smoke booth_left.smoke mission_left.smoke [--expect fail]
Допуски = разброс PS2 между прогонами + 3°: ход − вид при ← 73…90° (PS2 76, 86, 84–87, 83), при ↓ 180 ±5° (PS2 180,0 ± 0,3), при ↑ 0…14° (PS2 13° → 0 на облёте), скорость камеры 36 ±4°/с (PS2 35–37), «камера стоит» < 5° за 3 с (PS2 0°).
Знак ход − вид в порте зеркален PS2 (ось z), сравнивается модуль. PS2: стик ← = 76°/круг, камера 35°/с; ↓ = 180° и камера стоит; ↑ после облёта = 0°."""
import re, sys
a = [x for x in sys.argv[1:] if not x.startswith('--')]; expect_fail = '--expect' in sys.argv and sys.argv[sys.argv.index('--expect') + 1] == 'fail'
w = lambda x: (x + 180) % 360 - 180
def load(path):
    R = {}; sc = None
    for l in open(path, errors='replace'):
        m = re.match(r'SCENE (\S+)', l)
        if m: sc = m.group(1); continue
        m = re.search(r'CTRLYAW t=(\S+) held=\S+ view=(\S+) camYaw=\S+ busy=(\d) bt=\S+ spd=(\S+) on=\d frame=\S+ walk=(\S+) pos=', l)
        if m and sc: R.setdefault(sc, []).append(tuple(float(x) for x in m.groups()))   # t, view, busy, spd, walk
    return R
def steady(rows, tail=2.0):   # ходьба после облёта: spd > 3, busy = 0, последние tail с таких отсчётов
    ws = [r for r in rows if r[3] > 3.0 and r[2] == 0]
    if len(ws) < 10: return None
    t1 = ws[-1][0]; return [r for r in ws if r[0] > t1 - tail]
ok = True
def chk(name, cond, msg):
    global ok; print(('  ok  ' if cond else '  FAIL ') + name + ': ' + msg); ok = ok and cond
E = load(a[0]); O = load(a[1]); B = load(a[2]); M = load(a[3])
for sc, want, lim in (('WD_up', 7., 7.), ('WD_left', 81.5, 8.5), ('WD_down', 180., 5.)):
    s = [r for r in E.get(sc, []) if r[3] > 3.0] if sc == 'WD_up' else steady(E.get(sc, []))   # ↑: Гомер упирается в стену через 0,5 с, смотрим всю ходьбу (PS2 −2…13° на облёте)
    s = s if s and len(s) >= 3 else None
    if not s: chk(sc, False, 'нет ходьбы после облёта'); continue
    rel = [abs(w(r[4] - r[1])) for r in s]; m = sum(rel) / len(rel)
    chk(sc, abs(m - want) <= lim, f'|ход − вид| {m:.0f}° (PS2 {want:.1f}° ±{lim:.1f})')
s = E.get('WD_down', [])
ws = [r for r in s if r[3] > 3.0 and r[2] == 0]
if len(ws) > 10:
    t1 = ws[-1][0]; tail = [r for r in ws if r[0] > t1 - 3.0]; dv = abs(w(tail[-1][1] - tail[0][1]))
    chk('WD_down камера', dv < 5., f'поворот камеры за {tail[-1][0] - tail[0][0]:.1f} с: {dv:.0f}° (PS2 стоит, < 5°)')
s = steady(O.get('WC_left', []), 4.0)
if not s: chk('WC_left', False, 'нет ходьбы')
else:
    rel = sum(abs(w(r[4] - r[1])) for r in s) / len(s); om = abs(w(s[-1][1] - s[0][1])) / (s[-1][0] - s[0][0])
    chk('WC_left ход − вид', 73. <= rel <= 90., f'{rel:.0f}° (PS2 76…87°, допуск 73…90°)')
    chk('WC_left камера', abs(om - 36.) <= 4., f'{om:.0f}°/с (PS2 35–37, допуск 36 ±4)')
s = steady(B.get('BO_left', []), 2.0)
if not s: chk('BO_left', False, 'нет ходьбы после будки')
else:
    rel = sum(abs(w(r[4] - r[1])) for r in s) / len(s); chk('BO_left после будки', 73. <= rel <= 90., f'|ход − вид| {rel:.0f}° (PS2 76…87°, допуск 73…90°: после облёта будки стик от нынешнего вида)')
s = steady(M.get('MS_left', []), 2.0)
if not s: chk('MS_left', False, 'нет ходьбы после старта миссии')
else:
    rel = sum(abs(w(r[4] - r[1])) for r in s) / len(s); chk('MS_left после старта миссии', 73. <= rel <= 90., f'|ход − вид| {rel:.0f}° (PS2 84–87° при стике ←, wc1059_msn_left; ↑ даёт 0° сразу, wc1059_msn_up)')
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok != expect_fail else 1)
