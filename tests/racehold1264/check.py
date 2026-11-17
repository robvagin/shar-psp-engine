#!/usr/bin/env python3
import re, sys, pathlib
D = pathlib.Path(__file__).resolve().parents[2] / 'build/rh1264'
tag = sys.argv[1]
def rd(v): return (D / f'rh1264_{v}_{tag}.txt').read_text(errors='replace').splitlines()
for v in (0, 1, 2):
    L = rd(v); t0 = next((float(re.search(r't=([\d.]+)', l).group(1)) for l in L if l.startswith('MSN race-props')), None)
    trf = [l for l in L if l.startswith('TRF t=')]
    def num(l, k): return int(re.search(k + r'=(\d+)', l).group(1))
    peds = [num(l, 'peds') for l in trf]; pk = [num(l, 'parkdrawn') for l in trf]; aw = [num(l,'awake') for l in trf]
    print(v, 'race-props', [l for l in L if l.startswith('MSN race-props')][:1], 'fences', [l for l in L if l.startswith('MSN race-fences')][:3], 'peds max/last', max(peds or [-1]), (peds or [-1])[-1], 'parkdrawn max/last', max(pk or [-1]), (pk or [-1])[-1])
# гейт заборов (A2.10/Б.7): новое — в строке race-props заборов 0, потом race-fences on=N/N после появления рисунка; racefence1264old — заборы сразу (N>0 без рисунка)
def props(v): return next((l for l in rd(v) if l.startswith('MSN race-props')), '')
def fen(v): return [l for l in rd(v) if l.startswith('MSN race-fences')]
n0 = int(re.search(r'fences=(\d+)', props(0)).group(1)); n2 = int(re.search(r'fences=(\d+)', props(2)).group(1))
f0 = fen(0); full = bool(f0) and re.search(r'on=(\d+)/(\d+) art=1', f0[-1]) and re.search(r'on=(\d+)/(\d+)', f0[-1]).group(1) == re.search(r'on=(\d+)/(\d+)', f0[-1]).group(2)
ok = (n0 == 0 and full and n2 > 0)
print('ЗАБОРЫ новое: в момент пропов', n0, 'потом', f0[-1] if f0 else '-', '| старое: сразу', n2, '|', 'ЗЕЛЁНЫЙ' if ok else 'КРАСНЫЙ'); sys.exit(0 if ok else 1)
