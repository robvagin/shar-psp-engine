"""Дальность Д1, снимки «было/стало»: точки таблицы дали, откуда в стороне, где таблица режет, видно больше всего клеток-ориентиров дальше урезанной дальности.
  python3 -I tools/far1177/dalimp_spots.py LEVEL.shr DAL877.bin DALIMP.bin LEVELNO OUT.txt [K] [--signs signs.json]
Пишет сценарий смоука: на каждой точке «flag dalimp 0» снимок, «flag dalimp 1» снимок, камера и дальность те же (tools/far1177/dalimp.py — те же правила, что у движка)."""
import sys, struct, json
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent)); import dalimp as M, dal_map as D
lv, dal, dim, no, out = sys.argv[1:6]; K = int(sys.argv[6]) if len(sys.argv) > 6 and sys.argv[6].isdigit() else 5
sj = sys.argv[sys.argv.index('--signs') + 1] if '--signs' in sys.argv else None
c = M.classify(lv, sj, 50., 16., 1500); T = D.load(dal); b = c['b']; ncell, ctab = struct.unpack_from('<II', b, 80)
cells = sorted({int(c['cell'][i]) for i in c['idx']}); cc = []; cf = []
for ci in cells:
    r = struct.unpack_from('<6fIIIII', b, ctab + 48 * ci); cc.append(((r[0] + r[3]) / 2, (r[2] + r[5]) / 2)); cf.append(c['ft'].get(ci, 0))
cc = np.array(cc); cf = np.array(cf); cand = []
for j in range(T['H']):
    for i in range(T['W']):
        v = int(T['pk'][j, i])
        if v == 0xFFFF: continue
        x = T['x0'] + i * T['st']; z = T['z0'] + j * T['st']; dx = cc[:, 0] - x; dz = cc[:, 1] - z; dist = np.hypot(dx, dz); ang = np.degrees(np.arctan2(dx, dz))
        for k in range(8):
            code = (v >> (2 * k)) & 3
            if code >= 2: continue
            cap = 150. if code == 0 else 200.; dd = (ang - k * 45. + 180.) % 360. - 180.
            m = (np.abs(dd) <= 25) & (dist > cap) & (dist <= 250.)   # узкий конус: ориентиры в центре кадра
            if m.sum() >= 2: cand.append((int(m.sum()), x, z, k, float(T['yt'][j, i]) * .1, cap))
cand.sort(reverse=True); pick = []
for n, x, z, k, yt, cap in cand:
    if all(np.hypot(x - p[1], z - p[2]) > 120 for p in pick): pick.append((n, x, z, k, yt, cap))
    if len(pick) >= K: break
ln = ['# Д1 ориентиры дали L%s: точки таблицы, где ориентиры дальше урезанной дали (tools/far1177/dalimp_spots.py)' % no, 'flag camdet 1', 'scene start', 'wait 3']
ln += ['scene L%s_load' % no, 'flag golevel %s' % no, 'wait 40'] if no != '1' else ['scene L1_load', 'wait 20']
ln += ['flag repair', 'scene DI_signs']
for q, (n, x, z, k, yt, cap) in enumerate(pick):
    h = np.radians(k * 45.); fx, fz = np.sin(h), np.cos(h); y = yt + .3
    ln += ['cam off', 'goto %.1f %.1f %.3f' % (x, z, h), 'wait 6', 'cam %.1f %.1f %.1f %.3f -0.02 %.3f 60' % (x, y, z, fx, fz), 'flag dalimp 0', 'wait 4', 'shot di%d_off' % q, 'flag dalimp 1', 'wait 4', 'shot di%d_on' % q]
ln += ['cam off', 'end']; Path(out).write_text('\n'.join(ln) + '\n')
Path(out + '.json').write_text(json.dumps([dict(n=n, x=x, z=z, k=k, eye=yt, cap=cap) for n, x, z, k, yt, cap in pick], ensure_ascii=False))
for n, x, z, k, yt, cap in pick: print('L%s точка x=%.0f z=%.0f сторона %d (курс %d°) глаз %.1f м, таблица режет до %.0f м, ориентиров-клеток дальше: %d' % (no, x, z, k, k * 45, yt, cap, n))
