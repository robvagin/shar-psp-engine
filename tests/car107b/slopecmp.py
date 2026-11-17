#!/usr/bin/env python3
"""107b: ход по склону порта против записей PS2 А1 (s_ps2_c107_a1_*, 8 заездов) и s_ps2_w_jz16/17_70: СКО тангажа (2°) и vy (0,5 м/с) по пути -26..0 м до кромки и vy/тангаж на кромке.
  python3 tests/car107b/slopecmp.py ТРАССЫ_ПОРТА_twin"""
import sys, os, math
O4 = os.environ.get('PS2TW', os.path.expanduser('~/Downloads/shar-psp/build/twins/o4'))
RUNS = {'jz16_k70': 's_ps2_w_jz16_70', 'jz17_k70': 's_ps2_w_jz17_70'}
for k, r in (('jz16_k50', 'jz16_50'), ('jz16_k100', 'jz16_100'), ('jz17_k50', 'jz17_50'), ('jz17_k100', 'jz17_100'), ('jz2_k70', 'jz2_70'), ('jz2_k100', 'jz2_100'), ('jz4_k70', 'jz4_70'), ('jz4_k100', 'jz4_100')): RUNS[k] = 's_ppp' and 's_ps2_c107_a1_' + r
def interp(rows, key, vals):
    for a, b in zip(rows, rows[1:]):
        if (a[0] - key) * (b[0] - key) <= 0 and a[0] != b[0]:
            f = (key - a[0]) / (b[0] - a[0]); return [a[v] + f * (b[v] - a[v]) for v in vals]
d = sys.argv[1]; tot = 0.0; n = 0; per = {}; lip = [0.0, 0.0]; nl = 0
for nm, run in RUNS.items():
    try:
        L = [l.rstrip('\n').split('\t') for l in open(f'{O4}/{run}/jump.tsv')]; h = L[0]
        ia, it, iv = h.index('along'), h.index('тангаж'), h.index('vy')
        P = [(float(r[ia]), float(r[it]), float(r[iv])) for r in L[1:]]
        Q = [(float(p[4]), float(p[8]), float(p[6])) for p in (l.split('\t') for l in open(f'{d}/{nm}.tsv'))]
    except (OSError, ValueError, IndexError): continue
    e = 0.0; m = 0
    for a in [-26 + i for i in range(27)]:
        x = interp(P, a, (1, 2)); y = interp(Q, a, (1, 2))
        if x and y: e += ((x[0] - y[0]) / 2.0) ** 2 + ((x[1] - y[1]) / 0.5) ** 2; m += 1
    x = interp(P, 0.0, (1, 2)); y = interp(Q, 0.0, (1, 2))
    if x and y: lip[0] += abs(x[0] - y[0]); lip[1] += abs(x[1] - y[1]); nl += 1
    per[nm] = math.sqrt(e / max(m, 1)); tot += e; n += m
print('склон_СКО=%.3f (%s)' % (math.sqrt(tot / max(n, 1)), ' '.join('%s %.1f' % kv for kv in per.items())))
print('кромка: Σ|Δтангаж| %.1f° Σ|Δvy| %.2f м/с по %d заездам' % (lip[0], lip[1], nl))
