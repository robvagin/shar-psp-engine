#!/usr/bin/env python3
"""№1060в: эпизоды выгрузки клеток при тесной куче (flag heapleft 3,2, L4). check.py ДИР_on ДИР_churn ДИР_burst
on (новое: квота байт на эпизод + загрузчик не берёт выгруженные в эпизоде): каждая клетка выгружается ≤ 1 раза за эпизод (строки MEMTRIM2: dup=0, maxper=1), загрузок клеток не больше, чем у burst (flag memtrim1060 0:
прежние всплески), пик stream() ≤ 4 мс Мака (всплески 17 мс и 50 мс не возвращаются), видимых незагруженных и MISSCELL не больше, чем у burst. churn (flag memtrim2 0 = MEMTRIM1060): красный — есть клетка, выгруженная
больше раза в эпизоде, и загрузок больше, чем у on."""
import re, sys, os
ok = True
def need(c, m):
    global ok; print(('  ok   ' if c else '  FAIL ') + m); ok &= bool(c)
def st(d):
    T = open(os.path.join(d, 'smoke.txt'), errors='replace').read().splitlines()
    ep = [dict(zip(('cells', 'unique', 'dup', 'maxper'), map(int, re.search(r'cells=(\d+) unique=(\d+) dup=(\d+) maxper=(\d+)', l).groups()))) for l in T if l.startswith('MEMTRIM2')]
    sc = [l for l in T if l.startswith('STREAMCOST')]
    loads = int(re.search(r'loads=(\d+)', sc[-1]).group(1)) if sc else 0
    mx = max((int(re.search(r'max_us=(\d+)', l).group(1)) for l in sc), default=0)
    av = sum(int(re.search(r'avg_us=(\d+)', l).group(1)) for l in sc) / max(len(sc), 1)
    mv = sum(int(m.group(1)) for l in T if l.startswith('STAT F_') for m in [re.search(r'missvis (\d+)', l)] if m)
    return dict(end=any(l.startswith('END') for l in T), ep=ep, loads=loads, mx=mx, av=av, mv=mv, miss=sum(1 for l in T if l.startswith('MISSCELL')))
a, c, b = st(sys.argv[1]), st(sys.argv[2]), st(sys.argv[3])
for n, x in (('новый', a), ('churn', c), ('burst', b)): need(x['end'], f'{n}: прогон дошёл до END')
need(len(a['ep']) >= 1, f"новый: эпизодов выгрузки {len(a['ep'])} (куча была тесной)")
need(all(e['dup'] == 0 and e['maxper'] == 1 for e in a['ep']), f"новый: каждая клетка выгружена ≤ 1 раза за эпизод (максимум {max((e['maxper'] for e in a['ep']), default=0)}, повторных {sum(e['dup'] for e in a['ep'])}); эпизодов до {max((e['cells'] for e in a['ep']), default=0)} клеток")
need(a['loads'] <= b['loads'], f"новый: загрузок клеток {a['loads']} не больше, чем у прежних всплесков (flag memtrim1060 0) {b['loads']}; churn {c['loads']}")
need(a['mx'] <= 4000, f"новый: пик stream() {a['mx']/1000:.1f} мс Мака (≈ {a['mx']*5.8/1000:.0f} мс PSP), не вернулся к 17 мс (всплески) / 50 мс")
need(a['mv'] <= b['mv'] and a['miss'] <= b['miss'], f"новый: видимых незагруженных missvis {a['mv']} и MISSCELL {a['miss']} не больше, чем у всплесков ({b['mv']} / {b['miss']})")
need(any(e['dup'] > 0 for e in c['ep']), f"красный (flag memtrim2 0): есть клетки, выгруженные несколько раз в эпизоде: повторных {sum(e['dup'] for e in c['ep'])}, максимум {max((e['maxper'] for e in c['ep']), default=0)}")
need(c['loads'] > a['loads'], f"красный: загрузок больше, чем у нового ({c['loads']} против {a['loads']})")
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
