"""№1177: опись дальних кусков города по дальности от камеры (только чтение level.shr).
python3 -I tools/far1177/census.py LEVEL.shr X Z HEADING_RAD [RANGE]
Дальность до клетки по её ящику (как boxD2 в движке), пирамида как в tools/budget.py (cone 0.60/0.80)."""
import sys, struct, numpy as np
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import budget as B, tex_gate as T

def cells(d, L):
    off, = struct.unpack_from('<I', d, 84); n, = struct.unpack_from('<I', d, 80)
    out = []
    for i in range(n):
        r = struct.unpack_from('<6fIIII', d, off + 48 * i)
        out.append(dict(bb=np.array(r[:6]), first=r[6], n=r[7], owner=r[8], off=r[9]))
    return out

def census(path, x, z, h, rng=250., far_d=90.):
    d = Path(path).read_bytes(); L = B.load(path); R = L['recs']; C = cells(d, L)
    recs, *_ = T.walk(d)
    own = np.array([r['owner'] for r in R]); fl = np.array([r['flags'] for r in R]); tex = np.array([r['tex'] for r in R])
    ni = np.array([r['ni'] for r in R]) // 3; nf = np.array([r['nf'] for r in R]) // 3; cellid = np.array([r['cell'] for r in R])
    hm = L['hm']; gx = int((x - L['hox']) // L['cell']); gz = int((z - L['hoz']) // L['cell'])
    y = 3.0
    if 0 <= gx < L['W'] and 0 <= gz < L['H']:
        hs = hm[gz, gx]; hs = hs[hs != -32768]; y = (hs.min() * .01 if len(hs) else 0.) + 3.0
    e = np.array([x - np.sin(h) * 7.5, y, z - np.cos(h) * 7.5]); f = np.array([np.sin(h), -0.15, np.cos(h)]); f /= np.linalg.norm(f)
    res = {}
    bins = [(0, 50), (50, 100), (100, 150), (150, 200), (200, 251)]
    stat = {b: dict(cells=0, pieces=0, tris_near=0, tris_far=0, tiny=0, texs=set(), prim=0) for b in bins}
    for ci, c in enumerate(C):
        lo, hi = c['bb'][:3], c['bb'][3:]
        q = np.clip(e, lo, hi); dd = np.linalg.norm(q - e)
        if dd > rng: continue
        ctr = (lo + hi) / 2; rad = .5 * np.linalg.norm(hi - lo); dv = ctr - e; fw = dv @ f; side = np.sqrt(max(0, dv @ dv - fw * fw))
        if side * .6 - fw * .8 > rad: continue
        idx = [k for k in range(c['first'], c['first'] + c['n']) if own[k] == 0 and ni[k] > 0 and not ((fl[k] & 2) and not (fl[k] & 4))]
        if not idx: continue
        for b in bins:
            if b[0] <= dd < b[1]:
                s = stat[b]; s['cells'] += 1; s['pieces'] += len(idx)
                far = dd > far_d
                s['tris_near'] += int(ni[idx].sum()); s['tris_far'] += int(np.where(nf[idx] > 0, nf[idx], ni[idx]).sum())
                s['tiny'] += int((np.where(far, np.where(nf[idx] > 0, nf[idx], ni[idx]), ni[idx]) < 50).sum())
                s['texs'].update(int(t) for t in tex[idx]); s['prim'] += len(idx)
    return stat, e, y

if __name__ == '__main__':
    p = sys.argv[1]; x, z, h = map(float, sys.argv[2:5]); rng = float(sys.argv[5]) if len(sys.argv) > 5 else 250.
    stat, e, y = census(p, x, z, h, rng)
    print('глаз', np.round(e, 1), 'дальность', rng)
    print('%-9s %6s %7s %9s %9s %6s %6s' % ('м', 'клеток', 'кусков', 'тр.ближн', 'тр.дальн', 'мелких', 'текстур'))
    tot = dict(cells=0, pieces=0, near=0, far=0, tiny=0)
    for b, s in stat.items():
        print('%-9s %6d %7d %9d %9d %6d %6d' % ('%d-%d' % b, s['cells'], s['pieces'], s['tris_near'], s['tris_far'], s['tiny'], len(s['texs'])))
