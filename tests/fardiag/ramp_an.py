"""Рампа границы дали: сравнение прогонов old (dalrampold) и new. python3 -I tests/fardiag/ramp_an.py ПАПКА_out_old ПАПКА_out_new [RIDE_SCENE]
Печатает: смены дальности и скачки ≥ 25 м за кадр (DAL877 chg / jump25 за езду), доля 2-с окон со сдвигом средней дальности ≥ 25 м, цену кадра по PHSEC (build, x_city) и cull_us (RFAST), FPS p5,
и побайтное сравнение снимков покоя rest_*.png."""
import sys, re, hashlib, statistics as st
from pathlib import Path
import numpy as np
def parse(d, scene):
    ch = jm = 0; dd = []; bld = []; city = []; cull = []; fps = []
    for l in open(Path(d) / 'smoke.txt', errors='replace'):
        if l.startswith('DAL877 frames') and ' chg=' in l:
            m = re.search(r'frames=(\d+) capped=(\d+) avg_dd=(\d+).* chg=(\d+) jump25=(\d+)', l)
            if m and int(m[1]) > 20: dd.append(int(m[3])); ch += int(m[4]); jm += int(m[5])
        elif l.startswith('PHSEC ') and ('scene=F_' + scene in l or 'scene=' + scene in l):
            m = re.search(r' build=([\d.]+) .*x_city=([\d.]+)', l)
            if m: bld.append(float(m[1])); city.append(float(m[2]))
        elif l.startswith('RFAST ') and ('scene=' + scene in l or 'scene=F_' + scene in l):
            m = re.search(r'cull_us=(\d+)', l)
            if m: cull.append(int(m[1]))
        elif re.match(r'^\d+\.\d+ \S+ [\d.]+ ', l) and scene in l.split()[1]:
            fps.append(float(l.split()[2]))
    return dict(ch=ch, jm=jm, dd=np.array(dd), bld=bld, city=city, cull=cull, fps=fps)
old, new = sys.argv[1:3]; scene = sys.argv[3] if len(sys.argv) > 3 else 'ride'
for nm, d in (('old', old), ('new', new)):
    r = parse(d, scene); dd = r['dd']; sw = np.abs(np.diff(dd)) if len(dd) > 1 else np.array([0])
    print('%-3s смен дальности %d, скачков ≥25 м за кадр %d | окон 2 с %d, со сдвигом средней ≥25 м: %d (%.0f%%), ≥50 м: %d | build мс %.2f x_city %.2f cull_us %.0f | FPS p5 %.1f'
          % (nm, r['ch'], r['jm'], len(dd), (sw >= 25).sum(), 100 * (sw >= 25).mean() if len(sw) else 0, (sw >= 50).sum(), st.mean(r['bld']) if r['bld'] else 0, st.mean(r['city']) if r['city'] else 0, st.mean(r['cull']) if r['cull'] else 0, np.percentile(r['fps'], 5) if r['fps'] else 0))
same = diff = 0
for p in sorted(Path(old).glob('rest_*.png')):
    q = Path(new) / p.name
    if q.exists():
        a, b = hashlib.md5(p.read_bytes()).hexdigest(), hashlib.md5(q.read_bytes()).hexdigest(); same += a == b; diff += a != b; print('  покой', p.name, 'побайтно то же' if a == b else 'ОТЛИЧАЕТСЯ')
print('снимков покоя: те же %d, отличаются %d' % (same, diff))
