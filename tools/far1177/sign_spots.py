"""№1178 аудит: места вывесок уровня и камеры для снимков «было/стало» (только чтение level.shr).
  python3 -I tools/far1177/sign_spots.py LEVEL.shr SIGNS.json LEVELNO OUT_PREFIX [K] [DIST]
Находит куски с текстурами из списка вывесок, берёт K самых крупных (по одной на рисунок), для каждого ищет камеру на DIST м
(по умолчанию 80) с чистым лучом до центра вывески (лучи против всех треугольников уровня, кроме самой вывески).
Пишет OUT_PREFIX.json (места) и OUT_PREFIX.txt (сценарий смоука: одни и те же камеры для двух прогонов)."""
import sys, json
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import shr7read as S, tex_gate as T
sys.path.insert(0, str(Path(__file__).resolve().parent)); import signs as G

def seg_hits(P, a, b):
    """есть ли треугольник P (N,3,3) на отрезке a-b (Мёллер-Трамбор, двусторонний)"""
    lo = np.minimum(a, b) - .3; hi = np.maximum(a, b) + .3
    m = (P.max(axis=1) >= lo).all(1) & (P.min(axis=1) <= hi).all(1)
    if not m.any(): return 0
    Q = P[m]; d = b - a; e1 = Q[:, 1] - Q[:, 0]; e2 = Q[:, 2] - Q[:, 0]
    h = np.cross(d, e2); det = (e1 * h).sum(1); ok = np.abs(det) > 1e-9
    f = np.where(ok, 1 / np.where(ok, det, 1), 0); s = a - Q[:, 0]; u = f * (s * h).sum(1)
    q = np.cross(s, e1); v = f * (q * d).sum(1); t = f * (q * e2).sum(1)
    return int((ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > .02) & (t < .98)).sum())

def main():
    lv, lst, no, pre = sys.argv[1:5]; K = int(sys.argv[5]) if len(sys.argv) > 5 else 8; D0 = float(sys.argv[6]) if len(sys.argv) > 6 else 80.
    b = Path(lv).read_bytes(); _, sg = G.sigs(b); d = json.loads(Path(lst).read_text())
    ids = sorted({i for s in d['signs'] for i in sg.get(s['sig'], [])})
    L = S.load(lv); P = L['P']; tx = L['TEX']; pi = L['PI']
    sel = np.isin(tx, ids); Ps = P[sel]; txs = tx[sel]; pis = pi[sel]
    n = np.cross(Ps[:, 1] - Ps[:, 0], Ps[:, 2] - Ps[:, 0]); ar = .5 * np.linalg.norm(n, axis=1)
    spots = []
    for p in np.unique(pis):
        m = pis == p; A = ar[m].sum()
        if A < 1.5: continue
        c = (Ps[m].mean(axis=1) * ar[m, None]).sum(0) / A; nn = n[m].sum(0); nn /= (np.linalg.norm(nn) + 1e-9)
        ext = Ps[m].reshape(-1, 3); w = float(np.linalg.norm((ext.max(0) - ext.min(0))[[0, 2]])); h = float((ext.max(0) - ext.min(0))[1])
        spots.append(dict(piece=int(p), tex=int(txs[m][0]), area=float(A), c=c, n=nn, w=w, h=h))
    spots.sort(key=lambda s: -s['area']); seen = set(); pick = []
    other = P[~np.isin(tx, ids)]
    for s in spots:
        if s['tex'] in seen or s['area'] > 220: continue   # выше 220 м² — фасад целого дома, не вывеска
        if abs(s['n'][1]) > .6: continue   # горизонтальная поверхность: крыша, земля, не вывеска
        c = s['c']; nh = np.array([s['n'][0], 0, s['n'][2]]); nl = np.linalg.norm(nh)
        if nl < .35: nh = np.array([1., 0, 0]) if abs(s['n'][0]) > abs(s['n'][2]) else np.array([0, 0, 1.]); nl = 1
        nh = nh / nl; got = []
        for side in (1, -1):
            for D in (D0, D0 * .75, D0 * 1.25, D0 * .55):
                eye = c + side * nh * D + np.array([0, 3., 0])
                if seg_hits(other, eye, c) == 0: got.append((side, D, eye)); break
        if not got: continue
        s['views'] = got; seen.add(s['tex']); pick.append(s)
    if len(pick) > K: pick = [pick[int(round(i * (len(pick) - 1) / (K - 1)))] for i in range(K)]   # равномерно по размеру: от крупных до мелких
    out = []
    for i, s in enumerate(pick):
        for side, D, eye in s['views']:
            out.append(dict(name='s%02d%s_t%d' % (i, 'a' if side > 0 else 'b', s['tex']), tex=s['tex'], piece=s['piece'], area=round(s['area'], 1), w=round(s['w'], 1), h=round(s['h'], 1),
                            c=[round(x, 1) for x in s['c']], n=[round(x, 2) for x in s['n']], eye=[round(float(x), 1) for x in eye], D=D, side=side))
    Path(pre + '.json').write_text(json.dumps(out, ensure_ascii=False, indent=1))
    ln = ['# №1178 аудит L%s: камеры на вывески (tools/far1177/sign_spots.py); одни и те же для «было» и «стало»' % no, 'flag camdet 1', 'scene start', 'wait 3']
    if no != '1': ln += ['scene L%s_load' % no, 'flag golevel %s' % no, 'wait 40']
    else: ln += ['scene L1_load', 'wait 20']
    ln += ['flag repair']
    ln += ['scene AB_signs']
    for s in out:
        e = s['eye']; c = np.array(s['c']) - np.array(e); c /= np.linalg.norm(c)
        ln += ['cam off', 'goto %.1f %.1f 0' % (s['c'][0], s['c'][2]), 'wait 5', 'cam %.1f %.1f %.1f %.3f %.3f %.3f 60' % (e[0], e[1], e[2], c[0], c[1], c[2]), 'wait 3', 'shot ' + s['name']]
    ln += ['cam off', 'end']
    Path(pre + '.txt').write_text('\n'.join(ln) + '\n')
    print('L%s: вывесок в списке %d текстур; кусков-кандидатов %d; взято %d' % (no, len(ids), len(spots), len(out)))
    for s in out: print('  ', s['name'], 'площадь %.1f м² %.1f×%.1f' % (s['area'], s['w'], s['h']), 'центр', s['c'], 'камера', s['eye'], 'D', s['D'])

if __name__ == '__main__': main()
