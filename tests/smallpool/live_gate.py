#!/usr/bin/env python3
"""Гейт «утечек мелочи при выгрузке уровня» (Опора-93, Build 95): строки `SMALLPOOL free pages= live= livemax= over=` в smoke.txt (пишет levelLog на LEVEL ev=free).
После выгрузки в пуле остаются только постоянные блоки (~20: стек потоков, мьютексы, ленивые состояния модулей); они не должны расти от уровня к уровню:
  живых <= 40, прирост между соседними выгрузками <= 2, over = 0 (пул не переполнен).   live_gate.py smoke.txt   -> PASS/FAIL
Красный контроль: --selftest подаёт журнал, где живые растут 20 -> 60 -> 120 (должен FAIL)."""
import re, sys
def check(lines):
    v = []
    for l in lines:
        m = re.match(r'SMALLPOOL free pages=(\d+) live=(\d+) livemax=(\d+) over=(\d+)', l)
        if m: v.append((int(m[2]), int(m[4])))
    if len(v) < 2: return False, 'мало строк SMALLPOOL free: %d' % len(v)
    bad = [x for x in v if x[0] > 40 or x[1] != 0]
    grow = [(a[0], b[0]) for a, b in zip(v, v[1:]) if b[0] - a[0] > 2]
    return (not bad and not grow), 'живых после выгрузок %s, прирост>2: %s, не-пустое over/сверх 40: %s' % ([x[0] for x in v], grow, bad)
if __name__ == '__main__':
    if sys.argv[1:] == ['--selftest']:
        ok, msg = check(['SMALLPOOL free pages=1 live=20 livemax=30 over=0', 'SMALLPOOL free pages=1 live=60 livemax=60 over=0', 'SMALLPOOL free pages=1 live=120 livemax=120 over=0'])
        print('красный:', msg, '->', 'FAIL ожидается, получено', 'PASS' if ok else 'FAIL'); sys.exit(0 if not ok else 1)
    ok, msg = check(open(sys.argv[1], errors='replace')); print(msg); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
