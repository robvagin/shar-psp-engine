"""Дальность Д1, снимки «было/стало» точно на ориентир: башня или щит из dalimp.bin дальше урезанной дали, чистый луч, камера целится в его центр.
  python3 -I tools/far1177/dalimp_aim.py LEVEL.shr DAL877.bin DALIMP.bin LEVELNO OUT.txt [K]
Из узлов таблицы дали берутся те, где в сторону ориентира таблица режет (150/200 м), а ориентир дальше этой дали, не дальше 240 м и не закрыт геометрией; лучшие K по угловой высоте."""
import sys, struct, json
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent)); import dalimp as M, dal_map as D, sign_spots as SS
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import shr7read as S
lv, dal, dim, no, out = sys.argv[1:6]; K = int(sys.argv[6]) if len(sys.argv) > 6 else 4
c = M.classify(lv, None, 50., 99., 1500); T = D.load(dal); idx = c['idx']; bb = c['bb']; ctr = (bb[idx, :3] + bb[idx, 3:]) / 2; h = bb[idx, 4] - bb[idx, 1]
L_ = S.load(lv); P = L_['P']; best = []; wd = np.hypot(bb[idx, 3] - bb[idx, 0], bb[idx, 5] - bb[idx, 2])
for n in range(len(idx)):
    cx, cy, cz = ctr[n]; cand = []
    for j in range(T['H'] - 1):
        for i in range(T['W'] - 1):
            nodes = [(j + a_, i + b_) for a_ in (0, 1) for b_ in (0, 1)]
            if any(int(T['pk'][q]) == 0xFFFF for q in nodes): continue
            x = T['x0'] + (i + .5) * T['st']; z = T['z0'] + (j + .5) * T['st']; dist = np.hypot(cx - x, cz - z)
            if dist < 190 or dist > 248: continue
            yaw = np.arctan2(cx - x, cz - z); d8 = int(np.floor(yaw * 4. / np.pi + .5)) % 8; need = 0   # как dalNeed движка: худшее из 4 узлов и трёх сторон
            for q in nodes:
                v = int(T['pk'][q])
                for e in (-1, 0, 1):
                    cc_ = (v >> (2 * ((d8 + e + 8) % 8))) & 3; cc_ = 2 if cc_ == 3 else cc_; need = max(need, cc_)
            if need >= 2: continue
            cap = 150. if need == 0 else 200.
            if dist <= cap + 40: continue
            ytop = max(float(T['yt'][q]) for q in nodes) * .1; ey = ytop - .2   # глаз не выше самого высокого глаза бота (иначе таблица не действует)
            cand.append((np.degrees(np.arctan2(h[n], dist)), x, ey, z, dist, cap))
    cand.sort(reverse=True)
    for ang, x, ey, z, dist, cap in cand[:6]:
        a = np.array([x, ey, z]); bpt = np.array([cx, cy, cz]); u = (bpt - a) / np.linalg.norm(bpt - a); bpt = bpt - u * min(.5 * wd[n] + 6., 40.)   # до ближней стены, не внутрь
        if SS.seg_hits(P, a, bpt): continue   # закрыто геометрией
        best.append((ang, n, x, ey, z, dist, cap)); break
best.sort(reverse=True); pick = []
for ang, n, x, ey, z, dist, cap in best:
    if all(np.hypot(x - p[2], z - p[4]) > 150 for p in pick): pick.append((ang, n, x, ey, z, dist, cap))
    if len(pick) >= K: break
ln = ['# Д1 ориентиры дали L%s: камера целится в ориентир дальше урезанной дали (tools/far1177/dalimp_aim.py)' % no, 'flag camdet 1', 'scene start', 'wait 3']
ln += ['scene L%s_load' % no, 'flag golevel %s' % no, 'wait 40'] if no != '1' else ['scene L1_load', 'wait 20']
ln += ['flag repair', 'scene DI_aim']
for q, (ang, n, x, ey, z, dist, cap) in enumerate(pick):
    cx, cy, cz = ctr[n]; d = np.array([cx - x, cy - ey, cz - z]); d /= np.linalg.norm(d); hd = np.arctan2(d[0], d[2])
    ln += ['cam off', 'goto %.1f %.1f %.3f' % (x, z, hd), 'wait 6', 'cam %.1f %.1f %.1f %.3f %.3f %.3f 60' % (x, ey, z, d[0], d[1], d[2]), 'flag dalimp 0', 'wait 4', 'shot da%d_off' % q, 'flag dalimp 1', 'wait 4', 'shot da%d_on' % q]
    print('L%s цель %d: ориентир (%.0f, %.0f, %.0f) высота %.0f м, до него %.0f м, таблица режет до %.0f м, угловая высота %.1f°' % (no, q, cx, cy, cz, h[n], dist, cap, ang))
ln += ['cam off', 'end']; Path(out).write_text('\n'.join(ln) + '\n')
