#!/usr/bin/env python3
"""Круг 7 (P17-16): список номеров чекпойнтов круга в движке (viewer/bonus.c brLapList) = список в экспорте (tools/bonus/track.py LAP_FWD); оба = списки оригинала, записанные в tests/g07b_sverka7/lap_host.c.
Красный контроль: --old меняет одно число в таблице движка (как будто расходятся) — тест обязан упасть."""
import re, sys, pathlib
here = pathlib.Path(__file__).resolve().parents[2]
c = (here / 'viewer/bonus.c').read_text(); i = c.index('int brLapList'); blk = c[i:c.index('};', i)]
rows = re.findall(r'\{([\d, ]+)\}', blk)
eng = {k: [int(x) for x in r.split(',') if x.strip() and int(x)] for k, r in enumerate(rows) if k > 0 and r.strip() != '0'}
sys.path.insert(0, str(here / 'tools')); sys.path.insert(0, str(here / 'tools/bonus'))
import track as T
t = (here / 'tests/g07b_sverka7/lap_host.c').read_text(); j = t.index('static const int FWD'); tb = t[j:t.index('};', j)]
tst = [[int(x) for x in r.split(',') if x.strip() and int(x)] for r in re.findall(r'\{([\d,]+)\}', tb)]
if '--old' in sys.argv: eng[2] = eng[2][:3] + [9] + eng[2][4:]
bad = [n for n in range(1, 8) if eng.get(n) != T.LAP_FWD.get(n) or eng.get(n) != tst[n]]
print('движок %s' % eng); print('расходятся трассы: %s' % bad); print('FAIL' if bad else 'OK'); sys.exit(1 if bad else 0)
