"""№1185: «машины на линии» без эмулятора — следы гонки полигона (tools/bonus/race_host --trace: настоящие модули движка, водитель ИИ по дорогам трассы) против линии секторов (sector_host -d).
   sector_trace.py ПАПКА   (в ней boundaries.txt и trace1.tsv … trace7.tsv)   -> по трассе: расстояние машин до линии пути, м (медиана, 95 %, максимум); PASS, если 95 % <= 4 м и максимум <= 12 м.
   --break: линия сдвинута на 15 м — проверка обязана упасть (красный контроль)."""
import math, sys
from pathlib import Path
d = Path(sys.argv[1]); brk = '--break' in sys.argv
B = {}
for l in (d / 'boundaries.txt').read_text().splitlines():
    p = l.split()
    if p and p[0] == 'B': B.setdefault(int(p[1]), []).append((float(p[3]) + (15 if brk else 0), float(p[4])))
def dist(pt, poly):
    best = 1e9
    for a, b in zip(poly, poly[1:]):
        ex, ez = b[0] - a[0], b[1] - a[1]; l2 = ex * ex + ez * ez; t = max(0, min(1, ((pt[0] - a[0]) * ex + (pt[1] - a[1]) * ez) / l2)) if l2 else 0
        best = min(best, math.hypot(pt[0] - a[0] - ex * t, pt[1] - a[1] - ez * t))
    return best
bad = 0
for t in range(1, 8):
    f = d / f'trace{t}.tsv'
    if t not in B or not f.exists(): print(f'FAIL b{t}: нет данных'); bad += 1; continue
    ds = []
    for l in f.read_text().splitlines():
        p = l.split('\t')
        if len(p) < 8 or p[0] == '#': continue
        if float(p[0]) < 4.0: continue   # решётка старта до «GO»
        ds.append(dist((float(p[2]), float(p[4])), B[t]))
    ds.sort(); n = len(ds); med, p95, mx = ds[n // 2], ds[int(n * .95)], ds[-1]
    ok = p95 <= 4.0 and mx <= 12.0; bad += not ok
    print(f"{'ok  ' if ok else 'FAIL'} b{t}: {n} точек машин (4 водителя), расстояние до линии секторов: медиана {med:.1f} м, 95 % {p95:.1f} м, максимум {mx:.1f} м")
print('ИТОГ PASS' if not bad else f'ИТОГ FAIL ({bad})'); sys.exit(1 if bad else 0)
