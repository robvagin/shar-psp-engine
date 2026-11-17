#!/usr/bin/env python3
import re, sys, glob, os
d = sys.argv[1]; ok = True
def chk(c, why):
    global ok; ok &= bool(c); print('  %-112s %s' % (why, 'ok' if c else 'FAIL'))
def parse(p):
    if not os.path.exists(p): return None
    t = open(p).read(); m = re.search(r'гэгов (\d+), заданий района (\d+), кадров до оседания района (\d+).*байт (\d+) район (\d+), сонов (\d+), опросов (\d+)', t); return tuple(map(int, m.groups())) if m else None
sums = {}
for model in ('card', 'fast'):
    for p in sorted(glob.glob(os.path.join(d, 'new_%s*.txt' % model))):
        l = re.search(r'(\d)\.txt', p).group(1); n, o = parse(p), parse(os.path.join(d, 'old_%s%s.txt' % (model, l)))
        if not n or not o: chk(False, '%s L%s: строки есть' % (model, l)); continue
        s = sums.setdefault(model, [0, 0]); s[0] += n[2]; s[1] += o[2]
        chk(n[3:5] == o[3:5], '%s L%s: итоговые байты те же (по гэгу %d, район %d)' % (model, l, n[3], n[4]))
        chk(n[1] == o[1], '%s L%s: то же число заданий района (%d)' % (model, l, n[1]))
        chk(n[5] == 0, '%s L%s: ни одного сна в коде гэгов (сонов %d)' % (model, l, n[5]))
        if model == 'card':
            chk(n[2] <= o[2], 'card L%s: кадров до оседания не больше, чем у OLD: %d против %d' % (l, n[2], o[2]))
            chk(n[6] <= o[6] * 1.1 + 10, 'card L%s: опросов не больше, чем у OLD (+10 %%): новое %d, OLD %d' % (l, n[6], o[6]))
        elif o[2] >= 8: chk(n[2] <= o[2] * .8, 'fast L%s: мгновенная карта, район оседает быстрее: %d кадров против %d (не больше 80 %%)' % (l, n[2], o[2]))
if 'fast' in sums: chk(sums['fast'][0] <= sums['fast'][1] * .6 and sums['fast'][1] > 0, 'fast: сумма кадров района новое %d, OLD %d (не больше 60 %%)' % tuple(sums['fast']))
reds = [parse(p) for p in sorted(glob.glob(os.path.join(d, 'red_card*.txt')))]
reds = [r for r in reds if r]
if reds: chk(any(r[5] > 0 for r in reds), 'красный контроль: прежняя версия со сном (d954071c) даёт сонов > 0, проверка «сонов 0» на ней краснеет (%d)' % sum(r[5] for r in reds))
else: chk(False, 'красный контроль: нет прогона прежней версии')
print('COLLCHAIN1364', 'PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
