#!/usr/bin/env python3
"""№ скорость 250: средние числовые поля строк PHSEC/PHSUB/PHDRAW/RFAST/DCITY по сценам двух туров и их разность. tour_fields.py А.txt Б.txt сцена[,сцена…] [мин_разность]
Поля с суффиксом _us переводятся в мс. Печатает поля, где |Б−А| ≥ порога (по умолчанию 0,15), по убыванию."""
import re, sys, collections
def load(f):
    S = collections.defaultdict(lambda: collections.defaultdict(list))
    rx = re.compile(r'(\w+)=(-?[\d.]+)(?=[ /]|$)')
    for ln in open(f, errors='replace'):
        k = ln.split(' ', 1)[0]
        if k not in ('PHSEC', 'PHSUB', 'PHDRAW', 'RFAST', 'DCITY'): continue
        m = re.search(r'(?:scene|sc)=(\S+)', ln)
        if not m: continue
        sc = m.group(1)[2:] if m.group(1).startswith('F_') else m.group(1)
        for a, b in rx.findall(ln):
            v = float(b)
            if a.endswith('_us'): a, v = a[:-3] + '_ms', v / 1000
            S[sc][k + '.' + a].append(v)
    return S
A, B = load(sys.argv[1]), load(sys.argv[2]); th = float(sys.argv[4]) if len(sys.argv) > 4 else .15; ONLY_MS = len(sys.argv) > 5
for sc in sys.argv[3].split(','):
    sc = sc[2:] if sc.startswith('F_') else sc; a, b = A.get(sc), B.get(sc)
    if not a or not b: print(f'== {sc}: нет в одном из туров'); continue
    rows = []
    for k in set(a) & set(b):
        ma, mb = sum(a[k]) / len(a[k]), sum(b[k]) / len(b[k])
        if ONLY_MS and not (k.startswith('PHSEC.') or k.endswith('_ms') or k.split('.')[1] in ('props','sprops','bill','coll','coins','sp_loop','sp_pose','bb_tail','bb_loop','pb_t','car_pc','trf_cars')): continue
        if abs(mb - ma) >= th: rows.append((mb - ma, k, ma, mb))
    print(f'== {sc} (отсчётов А {len(a.get("PHSEC.build", []))}, Б {len(b.get("PHSEC.build", []))})')
    for d, k, ma, mb in sorted(rows, key=lambda r: -abs(r[0]))[:22]: print(f'  {k:28s} {ma:8.2f} -> {mb:8.2f}  ({d:+.2f})')
