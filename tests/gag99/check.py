#!/usr/bin/env python3
"""Гейт 99 «гэг Обсерватории получает место»: check.py key.txt [--forced]. key.txt: строки GAGPLAN / WATCHDOG / ROOM ev=enter из smoke.
Всегда: вход в Observatory состоялся и строк WATCHDOG mem who=gag нет. --forced (место урезано до 2816 КБ файлом zone3sz): ещё GAGPLAN heap_n > 0 (гэги взяты из кучи сразу, не проливом).
Красный контроль: тот же запуск с файлом gagplan99old - проливы WATCHDOG who=gag, GAGPLAN нет -> FAIL."""
import re, sys
L = open(sys.argv[1], errors='replace').read().splitlines(); forced = '--forced' in sys.argv; ok = True
enter = [l for l in L if l.startswith('ROOM ev=enter room=Observatory')]
c = bool(enter); print(f"1) вход в Обсерваторию: {'есть' if enter else 'НЕТ'}: {'ok' if c else 'FAIL'}"); ok &= c
wd = [l for l in L if l.startswith('WATCHDOG mem who=gag')]; c = not wd; print(f"2) WATCHDOG mem who=gag: {len(wd)} (0): {'ok' if c else 'FAIL'}"); ok &= c
gp = [re.search(r'heap_n=(\d+) heap_kb=(\d+)', l) for l in L if l.startswith('GAGPLAN')]; gp = [m for m in gp if m]
if forced:
    n = int(gp[-1].group(1)) if gp else 0; c = n > 0; print(f"3) гэги из кучи по плану (GAGPLAN heap_n): {n}{' / %s КБ' % gp[-1].group(2) if gp else ''} (>0): {'ok' if c else 'FAIL'}"); ok &= c
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
