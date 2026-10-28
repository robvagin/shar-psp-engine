#!/usr/bin/env python3
"""Стейтпропы по типам (строки SPTOP): средние экземпляры/куски на кадр по типу в сцене, два тура. sptop_cmp.py А.txt Б.txt сцена"""
import re, sys, collections
def load(f, sc):
    d = collections.defaultdict(lambda: [0.0, 0.0, 0, ''])
    n = 0
    for ln in open(f, errors='replace'):
        if not ln.startswith('SPTOP') or ('sc=F_' + sc + ' ') not in ln + ' ': continue
        n += 1
        for m in re.finditer(r'\[t(\d+) inst=([\d.]+) pcs=([\d.]+) of (\d+) j(\d+) s(\d+) k(\d+) b(\d+) pv(\d+)\]', ln):
            t = m.group(1); d[t][0] += float(m.group(2)); d[t][1] += float(m.group(3)); d[t][2] = int(m.group(4)); d[t][3] = f'j{m.group(5)} s{m.group(6)} k{m.group(7)} b{m.group(8)}'
    return d, n
sc = sys.argv[3]; A, na = load(sys.argv[1], sc); B, nb = load(sys.argv[2], sc)
print(f'{sc}: строк А {na}, Б {nb}'); print('тип  инст/с А→Б   кусков/с А→Б  кусков в типе  суставы/состояния')
rows = []
for t in set(A) | set(B):
    a = A.get(t, [0, 0, 0, '']); b = B.get(t, [0, 0, 0, ''])
    rows.append((b[1] / max(nb, 1) - a[1] / max(na, 1), t, a, b))
for d, t, a, b in sorted(rows, reverse=True)[:12]:
    print(f't{t:>3s}  {a[0]/max(na,1):6.1f} → {b[0]/max(nb,1):6.1f}   {a[1]/max(na,1):6.1f} → {b[1]/max(nb,1):6.1f}   {max(a[2],b[2]):3d}   {b[3] or a[3]}')
