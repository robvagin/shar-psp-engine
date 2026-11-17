#!/usr/bin/env python3
import re, sys, os
out = sys.argv[1]
def res(v):
    t = open(os.path.join(out, 'out_ch1359_%d' % v, 'smoke.txt'), errors='replace').read()
    m = re.search(r'^MSNARTCHAIN1359 n=(\d+) pumps=(\d+) ms=([\d.]+)', t, re.M)
    return (int(m.group(1)), int(m.group(2)), float(m.group(3)), len(re.findall(r'^WATCHDOG mem who=msnart', t, re.M))) if m else None
ok = True
def chk(cond, why):
    global ok; ok &= bool(cond); print('  %-100s %s' % (why, 'ok' if cond else 'FAIL'))
new, old = res(0), res(1)
print('новое %s, старое %s  (записей, кадров насоса, мс, WATCHDOG mem msnart)' % (new, old))
chk(new and old, 'обе строки MSNARTCHAIN1359 есть')
if new and old:
    chk(new[0] == old[0] == 10, 'в очереди те же 10 записей')
    chk(new[1] <= old[1] / 2, 'новое: кадров насоса вдвое меньше (%d против %d)' % (new[1], old[1]))
    chk(new[2] <= old[2] * .6, 'новое: время опустошения очереди не больше 60 %% старого (%.0f против %.0f мс)' % (new[2], old[2]))
    chk(new[3] <= old[3], 'новое: отказов чтения (WATCHDOG mem msnart) не больше, чем на старом (%d против %d)' % (new[3], old[3]))
print('MSNARTCHAIN1359', 'PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
