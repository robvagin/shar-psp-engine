"""Даль-1272: сколько треугольников города даёт пояс 150-200 м.  python3 -I tools/dal1272_belt.py LEVEL.shr [точек]
Для случайных точек земли (глаз над ними как в tools/budget.py) и 8 курсов считает треугольники в кадре при дальностях:
  пир150   пирамида кадра, дальность 150 (как в туре 106)
  пир200   пирамида, дальность 200
  шар200   круг 150 м + шар оригинала (центр на половине дальности по взгляду, радиус 100) внутри той же пирамиды
Кусок виден по ящику как в движке: расстояние до центра минус радиус, конус (0.60, 0.80). Только чтение файла."""
import sys, numpy as np
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent)); import budget as B

def run(path, n=1500, seed=1, far_from=90.):
    L = B.load(path); R = [r for r in L['recs'] if not r['owner']]
    bb = np.array([r['bb'] for r in R]); c = (bb[:, :3] + bb[:, 3:]) / 2; rad = .5 * np.linalg.norm(bb[:, 3:] - bb[:, :3], axis=1)
    ni = np.array([r['ni'] for r in R]) // 3; nf = np.array([r['nf'] for r in R]) // 3
    nf = np.where(np.array([r['flags'] for r in R]) & 2048, ni, nf)
    hm = L['hm']; rng = np.random.default_rng(seed); out = {k: [] for k in ('p150', 'p200', 's200', 'p150f', 'p200f', 's200f')}
    pts = [(gx, gz) for gz in range(L['H']) for gx in range(L['W']) if (hm[gz, gx] != -32768).any()]
    for k in rng.choice(len(pts), min(n, len(pts)), replace=False):
        gx, gz = pts[k]; hs = hm[gz, gx]; hs = hs[hs != -32768]; y = hs.min() * .01
        x = L['hox'] + (gx + .5) * L['cell']; z = L['hoz'] + (gz + .5) * L['cell']
        h = rng.uniform(0, 2 * np.pi)
        e = np.array([x - np.sin(h) * 7.5, y + 3.2, z - np.cos(h) * 7.5]); f = np.array([x, y + 1.4, z]) - e; f /= np.linalg.norm(f)
        dv = c - e; dist = np.linalg.norm(dv, axis=1); fw = dv @ f; side = np.sqrt(np.maximum(0, dist ** 2 - fw ** 2))
        pyr = ~(side * .60 - fw * .80 > rad); edge = dist - rad
        p150 = pyr & (edge <= 150); p200 = pyr & (edge <= 200)
        sc = e + f * 100.; sd = np.linalg.norm(c - sc, axis=1)
        s200 = pyr & ((edge <= 150) | (sd - rad <= 100))
        far = edge > far_from
        for key, m in (('p150', p150), ('p200', p200), ('s200', s200)):
            out[key].append(ni[m].sum()); out[key + 'f'].append(np.where(far, nf, ni)[m].sum())
    return {k: np.array(v) for k, v in out.items()}

if __name__ == '__main__':
    path = sys.argv[1]; r = run(path, int(sys.argv[2]) if len(sys.argv) > 2 else 1500)
    print(path)
    for lab, k in (('пирамида 150 м', 'p150'), ('пирамида 200 м', 'p200'), ('круг 150 + шар 200', 's200')):
        for suf, nm in (('', 'ближние версии'), ('f', 'дальние версии за 90 м')):
            t = r[k + suf]; print(f"  {lab:20s} {nm:24s} медиана {np.median(t)/1e3:5.1f}k  p90 {np.percentile(t,90)/1e3:5.1f}k  p99 {np.percentile(t,99)/1e3:5.1f}k")
    for suf, nm in (('', 'ближние'), ('f', 'дальние')):
        a = r['p150' + suf]; 
        print(f"  добавка к пир150 ({nm}): шар200 +{np.median(r['s200'+suf]-a)/1e3:.1f}k (мед) +{np.percentile(r['s200'+suf]-a,90)/1e3:.1f}k (p90); пир200 +{np.median(r['p200'+suf]-a)/1e3:.1f}k (мед) +{np.percentile(r['p200'+suf]-a,90)/1e3:.1f}k (p90)")
