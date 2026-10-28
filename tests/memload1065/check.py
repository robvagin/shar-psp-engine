#!/usr/bin/env python3
"""№1065: check.py smoke.txt new|red УРОВНИ... — на каждой загрузке уровня snd=1/N (банк звука открыт) и газета в кадре (SCRFILL между LEVELUP и LEVEL ev=load).
new: все проверки PASS; red: обязана упасть хотя бы одна (красный контроль). Вывод: строки PASS/FAIL, итог OK/ALARM."""
import re, sys
f, mode = sys.argv[1], sys.argv[2]; L = open(f, errors='replace').read().splitlines()
loads = [(i, l) for i, l in enumerate(L) if l.startswith('LEVEL ev=load')]
rc = 0; bad = 0
print(f"загрузок уровня: {len(loads)}")
prev = 0
for k, (i, l) in enumerate(loads):
    n = int(re.search(r'n=(\d+)', l).group(1)); snd = re.search(r'snd=(\d+)/(\d+)', l)
    seg = L[prev:i + 1]; prev = i + 1
    scr = [x for x in seg if x.startswith('SCRFILL screen=0')]
    need_scr = any(x.startswith('LEVELUP') for x in seg)   # первая загрузка из сценария — без газеты
    scr_ok = (not need_scr) or any(int(re.search(r'draws=(\d+)', x).group(1)) > 0 for x in scr)
    snd_ok = snd and snd.group(1) == '1' and int(snd.group(2)) == n
    m = [x for x in seg if x.startswith(('SCROPEN1065', 'MEMFAILSZ', 'SNDRETRY1065'))]
    print(f"  L{n}: звук {'ok' if snd_ok else 'FAIL snd=' + (snd.group(0) if snd else '?')}  газета {'ok' if scr_ok else 'FAIL (нет SCRFILL)'}  " + ' | '.join(x[:90] for x in m))
    bad += (not snd_ok) + (not scr_ok)
print("итого нарушений:", bad)
if mode == 'new': print('OK' if bad == 0 else 'ALARM'); sys.exit(0 if bad == 0 else 1)
print('OK красный краснеет' if bad else 'ALARM красный не покраснел'); sys.exit(0 if bad else 1)
