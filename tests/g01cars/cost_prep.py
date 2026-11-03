#!/usr/bin/env python3
# сверка g01-cars №1159: счётчики обходов физики машины (цена на кадр), без правки рабочего кода.
#   cost_prep.py ПАПКА_viewer_КОПИИ  — в КОПИИ car.c вставляет счётчики: вызовов carUpdate, подшагов и обращений к миру (все col*-вызовы из car.c)
#   и пишет costnames.h. Рабочий viewer/car.c не трогается: счётчики живут только в копии (build/g1159/<вид>/viewer)
import re, sys, os
d = sys.argv[1]; p = os.path.join(d, 'car.c'); s = open(p, encoding='utf-8').read()
col = open(os.path.join(d, 'col.h'), encoding='utf-8').read()
decl = set(re.findall(r'\b(col[A-Z][A-Za-z0-9]*)\s*\(', col))
used = sorted(set(re.findall(r'\b(col[A-Z][A-Za-z0-9]*)\s*\(', s)) & decl)
head = 'extern unsigned long cntQ[64], cntSub, cntCalls;\n' + ''.join('#define %s(...) (cntQ[%d]++, %s(__VA_ARGS__))\n' % (n, i, n) for i, n in enumerate(used))
head += '#define substep(...) (cntSub++, substep(__VA_ARGS__))\n'
key = re.search(r'^void carUpdate\(Car \*c, const ColWorld \*w, const CarInput \*in, float dt\) \{', s, re.M)
if not key: sys.exit('нет carUpdate')
# обёртки действуют только после всех определений col*/substep: ставим перед carUpdate; вызовы выше (в substep) считаем отдельной вставкой после include
inc = [m for m in re.finditer(r'^#include .*$', s, re.M) if 'car.h' in m.group(0)][0]   # после car.h (он тянет col.h)
wr = ''.join('#define %s(...) (cntQ[%d]++, %s(__VA_ARGS__))\n' % (n, i, n) for i, n in enumerate(used))
s = s[:inc.end()] + '\n' + wr + s[inc.end():]
key = re.search(r'^void carUpdate\(Car \*c, const ColWorld \*w, const CarInput \*in, float dt\) \{', s, re.M)
s = s[:key.start()] + '#define substep(...) (cntSub++, substep(__VA_ARGS__))\n' + s[key.start():key.end()] + ' cntCalls++;' + s[key.end():]
s = 'extern unsigned long cntQ[64], cntSub, cntCalls;\n' + s
open(p, 'w', encoding='utf-8').write(s)
open(os.path.join(d, 'costnames.h'), 'w').write('static const char *costNames[] = {%s};\n#define COSTN %d\n' % (','.join('"%s"' % n for n in used), len(used)))

# кто вызывает carUpdate: 0 игрок (game.c), 1 трафик на ходу, 2 припаркованные проснувшиеся, 3 машины миссий (msncar.c), 4 бонус-игра
for fn, k in (('game.c', 0), ('traffic.c', 1), ('msncar.c', 3), ('bonus_play.h', 4)):
    q = os.path.join(d, fn)
    if not os.path.exists(q): continue
    t = open(q, encoding='utf-8').read()
    if fn == 'traffic.c':
        t = t.replace('carUpdate(&P->car', 'CUWHO(2, &P->car').replace('carUpdate(&c->car', 'CUWHO(1, &c->car')
    else: t = re.sub(r'\bcarUpdate\(', 'CUWHO(%d, ' % k, t)
    t = '#include <time.h>\nextern unsigned long cntWho[8], cntNs[8], cntSubWho[8], cntSub, cntQ[64];\n#define CUWHO(k, ...) do { unsigned long t0_ = clock_gettime_nsec_np(CLOCK_UPTIME_RAW); cntWho[k]++; unsigned long s0_ = cntSub; carUpdate(__VA_ARGS__); cntSubWho[k] += cntSub - s0_; cntNs[k] += clock_gettime_nsec_np(CLOCK_UPTIME_RAW) - t0_; } while (0)\n' + t
    open(q, 'w', encoding='utf-8').write(t)
