#!/usr/bin/env python3
"""Правка Роба 1200: тангаж и vy порта против записей PS2 на трамплинах. Читает трассы twin (tests/jumps/twin.c --trace DIR, колонки: t x y z along v vy grounded pitch roll wheels comp0..3).
  python3 tests/jumps/pitchcmp.py DIR_ТРАСС_ПОРТА
1) кромка: тангаж и vy на кромке и за 3 м до неё, 12 прыжков (эталон pitch_ps2.tsv, правка 353; jz16_k50/k100 в нём нет);
2) ход по склону: СКО тангажа (2°) и vy (0,5 м/с) по пути от -26 до 0 м до кромки на четырёх заездах 70 км/ч (трассы PS2 build/twins/o4/*, PS2TW=каталог o4)."""
import sys, os, math
HERE = os.path.dirname(os.path.abspath(__file__))
PITCH_PS2 = os.path.join(HERE, '..', '..', 'docs/handoff/ДЕЛЕГАТ_2026-10-05/patches/agent353_new/tests/jumps/pitch_ps2.tsv')
O4 = os.environ.get('PS2TW', os.path.expanduser('~/Downloads/shar-psp/build/twins/o4'))
RUNS = dict(jz16='s_ps2_w_jz16_70', jz17='s_ps2_w_jz17_70', jz2='s_ps2_v99_jz2_70', jz4='s_ps2_v99_jz4_70')

def interp(rows, key, xi, vals):
    for a, b in zip(rows, rows[1:]):
        if (a[xi] - key) * (b[xi] - key) <= 0 and a[xi] != b[xi]:
            f = (key - a[xi]) / (b[xi] - a[xi]); return [a[v] + f * (b[v] - a[v]) for v in vals]
    return None

def lip_table(d, verbose=False):
    ps = {}
    for l in open(PITCH_PS2):
        if l.startswith('#') or l.startswith('удар'): continue
        p = l.rstrip('\n').split('\t')
        if len(p) >= 8: ps.setdefault(p[0], []).append((float(p[1]), float(p[2]), float(p[5]), float(p[7])))   # t, тангаж, путь от кромки, vy
    tot = [0.0] * 4
    for k in sorted(ps):
        try: pr = [(float(l.split('\t')[4]), float(l.split('\t')[8]), float(l.split('\t')[6])) for l in open(f'{d}/{k}.tsv')]   # along, тангаж, vy
        except OSError: continue
        for j, a in enumerate((0.0, -3.0)):
            x = interp(ps[k], a, 2, (1, 3)); y = interp(pr, a, 0, (1, 2))
            if x and y:
                tot[2 * j] += abs(y[0] - x[0]); tot[2 * j + 1] += abs(y[1] - x[1])
                if verbose: print('  %-9s кромка%+.0f м: тангаж PS2 %5.1f порт %5.1f | vy PS2 %5.2f порт %5.2f' % (k, a, x[0], y[0], x[1], y[1]))
    return tot

def trace_rms(d):
    tot = 0.0; n = 0; per = {}
    for nm, run in RUNS.items():
        try:
            P = []
            for l in list(open(f'{O4}/{run}/jump.tsv'))[1:]:
                p = l.rstrip('\n').split('\t'); P.append((float(p[9]), float(p[10]), float(p[6])))
            Q = [(float(l.split('\t')[4]), float(l.split('\t')[8]), float(l.split('\t')[6])) for l in open(f'{d}/{nm}_k70.tsv')]
        except (OSError, ValueError, IndexError): continue
        e = 0.0; m = 0
        for a in [-26 + i for i in range(27)]:
            x = interp(P, a, 0, (1, 2)); y = interp(Q, a, 0, (1, 2))
            if x and y: e += ((x[0] - y[0]) / 2.0) ** 2 + ((x[1] - y[1]) / 0.5) ** 2; m += 1
        per[nm] = math.sqrt(e / max(m, 1)); tot += e; n += m
    return math.sqrt(tot / max(n, 1)), per

if __name__ == '__main__':
    d = sys.argv[1]; v = '-v' in sys.argv
    t = lip_table(d, v)
    print('кромка: Σ|Δтангаж| %.1f° Σ|Δvy| %.2f м/с | за 3 м до кромки: Σ|Δтангаж| %.1f° Σ|Δvy| %.2f м/с' % (t[0], t[1], t[2], t[3]))
    r, per = trace_rms(d)
    print('ход по склону -26..0 м: СКО %.2f (%s)' % (r, ' '.join('%s %.1f' % kv for kv in per.items())))
