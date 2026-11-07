import re, sys, statistics
o = sys.argv[1]; p = []; m = []
for s in sys.argv[2:]:
    t = open('%s/m434_%s.txt' % (o, s), encoding='utf-8').read()
    a = re.search(r'на поворотах.*?крен p50 [\d.]+ p95 ([\d.]+)', t); b = re.search(r'макс крен ([\d.]+)', t)
    if a: p.append(float(a.group(1)))
    if b: m.append(float(b.group(1)))
print('%.1f %.1f' % (statistics.median(p) if p else -1, max(m) if m else -1))
