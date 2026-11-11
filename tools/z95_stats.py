#!/usr/bin/env python3
"""П5 Зоны-95: сводка по smoke.txt прогона: кусок в езде (MEMBLK blk), MEMTRIM, WATCHDOG mem по who, MEMFAILSZ, ZONE3 (проливы, отказы города).
python3 tools/z95_stats.py smoke.txt [...]"""
import re, sys
for fn in sys.argv[1:]:
    L = open(fn, errors='replace').read().splitlines()
    blk = sorted(float(m.group(1)) for l in L for m in [re.match(r'MEMBLK blk=([\d.]+)', l)] if m)
    trim = sum(int(m.group(1)) for l in L for m in [re.match(r'MEMTRIM freed=(\d+)', l)] if m)
    wd = {}
    for l in L:
        m = re.match(r'WATCHDOG mem who=(\S+) .*?step=(\d+)', l)
        if m: wd[m.group(1)] = wd.get(m.group(1), 0) + 1
    spill = sum(1 for l in L if re.match(r'WATCHDOG mem .* spill', l))
    z = [l for l in L if l.startswith('ZONE3 ')]
    cf = re.search(r'city .*?fail=(\d+)', z[-1]) if z else None
    p10 = blk[len(blk) // 10] if blk else -1
    print(f"{fn}: MEMBLK n={len(blk)} min={blk[0] if blk else -1:.2f} p10={p10:.2f} МБ | MEMTRIM={trim} | MEMFAILSZ строк={sum(1 for l in L if 'MEMFAILSZ' in l)} | WATCHDOG mem по who={wd} | spill={spill} | city fail={cf.group(1) if cf else '-'}")
