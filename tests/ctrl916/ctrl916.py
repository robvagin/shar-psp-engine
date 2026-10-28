#!/usr/bin/env python3
"""№947б (бывш. №916): управление пешком — стик относительно ТЕКУЩЕГО вида камеры. По CTRLYAW (flag camlog) сцен CT_hold и CT_fresh (tests/ctrl916/ctrl916.txt):
  CT_hold:  стик ушёл с ↑ на ↗, через 2 с к нему добавлен ↓ (угол стика 135°): ход = вид камеры НА МОМЕНТ смены − 135° с точностью LIM° (по умолчанию 25); камера к этому моменту ушла за курсом на ≈45°,
            поэтому правило «кадр первого нажатия держится» (№916/№920) даёт расхождение больше LIM;
  CT_fresh: стик отпущен и тронут заново: ход совпадает с НЫНЕШНИМ видом камеры в пределах 25°.
Красный контроль: прогон с flag ctrl947old 1 (правило №916/№920) обязан дать FAIL в CT_hold:  python3 ctrl916.py smoke.txt --expect fail
"""
import re, sys
a = sys.argv[1:]; LIM = float(a[a.index('--lim') + 1]) if '--lim' in a else 25.
expect_fail = '--expect' in a and a[a.index('--expect') + 1] == 'fail'
w = lambda x: (x + 180) % 360 - 180
sc = None; R = {}; S0 = {}
for l in open(a[0], errors='replace'):
    m = re.match(r'SCENE (\S+) (\S+)', l)
    if m: sc = m.group(1); S0[sc] = float(m.group(2)); continue
    m = re.search(r'CTRLYAW t=(\S+) held=\S+ view=(\S+) camYaw=(\S+) busy=\d bt=\S+ spd=(\S+) on=\d frame=(\S+) walk=(\S+) pos=', l)
    if m and sc: R.setdefault(sc, []).append([float(x) for x in m.groups()])   # t, view, camYaw, spd, frame, walk
def last_walk(rows, sec=0.8):
    ws = [r for r in rows if r[3] > 3.5]
    if not ws: return None
    t1 = ws[-1][0]; return [r for r in ws if r[0] > t1 - sec]
ok = True
h = R.get('CT_hold', []); f = R.get('CT_fresh', []); end = last_walk(h)
if not h or not end: print('CT_hold: нет ходьбы'); ok = False
else:
    tchg = S0['CT_hold'] + 6.5   # сценарий: wait 3, ↑, +1,5 с → ↗, +2 с → к ним добавлен ↓
    # №1059 (PS2, ps2-walk-camera-stick): стик от НЫНЕШНЕГО вида непрерывно (камера следует за ходом ≤ 36°/с), ход = нынешний вид − 135° (PS2: 125° при стике 135°)
    vt = min(h, key=lambda r: abs(r[0] - tchg)); d = max(abs(w(r[5] - (r[1] - 135.))) for r in end); exp = end[-1][1] - 135.
    print(f'CT_hold: стик вправо-назад (135°), ход {end[-1][5]:.0f}°, от нынешнего вида (камера {end[-1][1]:.0f}°) ожидается {exp:.0f}°: расхождение {d:.0f}° (предел {LIM:.0f}°)')
    ok = ok and d <= LIM
end2 = last_walk(f)
if not end2: print('CT_fresh: нет ходьбы'); ok = False
else:
    d2 = max(abs(w(r[5] - r[2])) for r in end2)
    print(f'CT_fresh: новый ход отличается от нынешнего вида камеры на {d2:.0f}° (предел 25°)'); ok = ok and d2 <= 25.
print('PASS' if ok else 'FAIL')
sys.exit(0 if ok != expect_fail else 1)
