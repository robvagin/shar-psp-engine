#!/usr/bin/env python3
"""107b (1278, подвеска): ход колёс порта со знаком (twin --trace, колонки 16–19 (с 1): xs, в воздухе −lim) против смещения колёс записей PS2 (jump.tsv w0..w3_off2, пределы ±0,266 м).
  python3 tests/car107b/wheelcmp.py ТРАССЫ_ПОРТА   -> печатает СКО хода колёс (м) по пути от -26 до +4 м от кромки на двух заездах 70 км/ч и долю кадров отбоя (колесо ниже нейтрали, но не в полном отбое)
Выход: wheel_rms=… ; below_neutral_contact=…"""
import sys, os, math
O4 = os.environ.get('PS2TW', os.path.expanduser('~/Downloads/shar-psp/build/twins/o4'))
RUNS = {'jz16_k70': 's_ps2_w_jz16_70', 'jz17_k70': 's_ps2_w_jz17_70'}   # записи PS2 с колонками колёс (w0..w3_off2)
if os.environ.get('A1'):   # 107b: восемь заездов А1 окна PS2 (50/100 км/ч, jz2/jz4 на 70 и ~90)
    for k, r in (('jz16_k50', 'jz16_50'), ('jz16_k100', 'jz16_100'), ('jz17_k50', 'jz17_50'), ('jz17_k100', 'jz17_100'), ('jz2_k70', 'jz2_70'), ('jz2_k100', 'jz2_100'), ('jz4_k70', 'jz4_70'), ('jz4_k100', 'jz4_100')):
        RUNS[k] = 's_ps2_c107_a1_' + r
def interp(rows, key, xi, vals):
    for a, b in zip(rows, rows[1:]):
        if (a[xi] - key) * (b[xi] - key) <= 0 and a[xi] != b[xi]:
            f = (key - a[xi]) / (b[xi] - a[xi]); return [a[v] + f * (b[v] - a[v]) for v in vals]
    return None
d = sys.argv[1]; tot = 0.0; n = 0; per = {}; neg_ps2 = neg_port = fr = 0
for nm, run in RUNS.items():
    try:
        L = [l.rstrip('\n').split('\t') for l in open(f'{O4}/{run}/jump.tsv')]
        h = L[0]; ia = h.index('along'); iw = [h.index(f'w{w}_off2') for w in range(4)]
        if os.environ.get('SWAP'): iw = [iw[2], iw[3], iw[0], iw[1]]   # 108: в записи PS2 w0,w1 — задние (по тангажу покоя), у порта 0,1 — передние
        P = [[float(r[ia])] + [float(r[i]) for i in iw] for r in L[1:]]
        Q = []
        for l in open(f'{d}/{nm}.tsv'):
            p = l.rstrip('\n').split('\t'); Q.append([float(p[4])] + [float(p[15 + k]) for k in range(4)])
    except (OSError, ValueError, IndexError) as e: print('skip', nm, repr(e)); continue
    e = 0.0; m = 0
    for a in [x * 0.5 for x in range(-52, 9)]:
        x = interp(P, a, 0, (1, 2, 3, 4)); y = interp(Q, a, 0, (1, 2, 3, 4))
        if x and y:
            for k in range(4):
                e += (x[k] - y[k]) ** 2; m += 1
                fr += 1; neg_ps2 += (-0.25 < x[k] < -0.01); neg_port += (-0.25 < y[k] < -0.01)
    per[nm] = math.sqrt(e / max(m, 1)); tot += e; n += m
print('wheel_rms=%.4f (%s)' % (math.sqrt(tot / max(n, 1)), ' '.join('%s %.3f' % kv for kv in per.items())))
print('below_neutral_contact ps2=%.3f port=%.3f (доля колесо-кадров с ходом между −0,25 и −0,01 м)' % (neg_ps2 / max(fr, 1), neg_port / max(fr, 1)))
