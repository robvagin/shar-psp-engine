#!/usr/bin/env python3
"""Гейт Z0-init (Опора-93, Build 95; Fable А.4б): на каждой выгрузке уровня (`FREEMAP free a= kb=` и `LEVEL ev=free ... heap= ... mx=`) свободная куча должна лежать немногими большими кусками.
  --max-chunks N  не больше N кусков >= 0,4 МБ (по умолчанию 2);  --ratio R  mx >= R * free (по умолчанию 0,0: только отчёт; Fable: 0,9 для блока уровня)
  freemap_gate.py smoke.txt [--max-chunks 2] [--ratio 0.9]   -> PASS/FAIL
Красный контроль: журнал сборки без Z0-init (ZONE0 лениво): 4 куска >= 0,4 МБ -> FAIL."""
import re, sys, argparse
ap = argparse.ArgumentParser(); ap.add_argument('log'); ap.add_argument('--max-chunks', type=int, default=2); ap.add_argument('--ratio', type=float, default=0.0); a = ap.parse_args()
chunks = []; cur = []; ok = True; n = 0
for l in open(a.log, errors='replace'):
    m = re.match(r'FREEMAP free a=\w+ kb=(\d+)', l)
    if m: cur.append(int(m[1])); continue
    m = re.match(r'LEVEL ev=free n=(\d).*? heap=(\d+) .*?mx=(\d+)', l)
    if m:
        big = [c for c in cur if c >= 400]; free = int(m[2]) / 1048576; mx = int(m[3]) / 1048576; r = mx / free if free else 0
        bad = len(big) > a.max_chunks or r < a.ratio; n += 1; ok &= not bad
        print(f"перед L{m[1]}: free {free:.1f} МБ, mx {mx:.2f} МБ ({100*r:.0f}%), кусков >=0,4 МБ: {len(big)} {big}  {'FAIL' if bad else 'ok'}"); cur = []
if not n: print('нет строк LEVEL ev=free'); ok = False
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
