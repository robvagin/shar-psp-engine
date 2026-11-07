"""№1202: появление машин трафика: py -I tests/fardiag/trf_an.py out_old out_new [сцена] — TRFPOP на краю дальности рисования в минуту, рождения (TRFSPAWN) в кадре, цена (PHDRAW trf, PHSEC x_trf, build)."""
import sys, re, statistics as st
from pathlib import Path
def an(d, scene):
    pop = []; sp = []; trf = []; ktr = []; xt = []; bld = []; secs = set()
    for l in open(Path(d) / 'smoke.txt', errors='replace'):
        if l.startswith('TRFPOP '):
            m = re.search(r' t=([\d.]+) (car|park)=\d+ d=([\d.]+) R=([\d.]+) born=\d dd=(\d+)', l)
            if m: pop.append((float(m[1]), m[2], float(m[3]), float(m[4]), int(m[5])))
        elif l.startswith('TRFSPAWN t='):
            m = re.search(r'd=([\d.]+) ang=\d+ dp=[\d.]+ inframe=(\d)', l)
            if m: sp.append((float(m[1]), int(m[2])))
        elif l.startswith('PHDRAW ') and scene in l:
            m = re.search(r' trf=(\d+)/([\d.]+)/', l)
            if m: trf.append(int(m[1])); ktr.append(float(m[2]))
        elif l.startswith('PHSEC ') and scene in l:
            m = re.search(r' build=([\d.]+) .*x_trf=([\d.]+)', l)
            if m: bld.append(float(m[1])); xt.append(float(m[2]))
        elif re.match(r'^\d+\.\d+ ' + scene + ' ', l): secs.add(l.split()[0])
    return pop, sp, trf, ktr, xt, bld, len(secs)
old, new = sys.argv[1:3]; scene = sys.argv[3] if len(sys.argv) > 3 else 'F_L5_trf'
for nm, d in (('old', old), ('new', new)):
    pop, sp, trf, ktr, xt, bld, ns = an(d, scene); dd = max((p[4] for p in pop), default=0)
    edge = [p for p in pop if p[2] >= p[4] - 6]; mins = max(ns, 1) / 60.
    print('%-3s dd=%d: появлений в кадре %d, на краю дальности (d ≥ dd−6) %d (%.1f в минуту), рождений %d в кадре %d | trf вызовов %.1f ктрис %.2f | x_trf %.2f мс, build %.2f мс'
          % (nm, dd, len(pop), len(edge), len(edge) / mins, len(sp), sum(s[1] for s in sp), st.mean(trf) if trf else 0, st.mean(ktr) if ktr else 0, st.mean(xt) if xt else 0, st.mean(bld) if bld else 0))
