#!/usr/bin/env python3
"""№1148: цвета блика нитро бонус-игры в viewer/spritelight.h (nitroGlintBatch1148) = данные нитро (art/missions/generic/nitro.p3d, плашки BQG nitroShape3/4/5), в той же записи, что цвета ключа
(wrench.p3d: wrenchShape46 -> 0xFF2685, wrenchShape4 -> 0xFF5B48, wrenchShape5 -> 0xFF4640 в wrGlowDraw). Диск — только чтение (hit-and-run-web/source/game/art).
Красный контроль: --wrong подменяет цвет нитро (обязан упасть)."""
import os, re, struct, sys
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MAIN = os.environ.get('SHAR_MAIN', os.path.expanduser('~/Downloads/shar-psp'))
sys.path.insert(0, os.path.join(MAIN, 'hit-and-run-web/tools'))
import p3d
from pathlib import Path
ART = Path(MAIN) / 'hit-and-run-web/source/game/art'
fails = 0; total = 0
def chk(ok, msg):
    global fails, total
    total += 1; fails += not ok; print(('PASS ' if ok else 'FAIL ') + msg)
def bq(f):
    out = {}
    def walk(c):
        if c.id == 0x17001:
            d = c.data; n = d[4]; name = d[5:5 + n].decode('latin1').strip('\0 '); o = 5 + n
            # строка режима (имя, выровнено), затем цвет 4 Б: сверяем по маркеру 'AAX'
            k = d.index(b'AAX', o) if b'AAX' in d[o:] else -1
            raw = d[k + 3 + 13:k + 3 + 17] if k >= 0 else b''
            out[name] = raw
        for k_ in c.children: walk(k_)
    walk(p3d.read(f)); return out
def const(raw): return '0x%02X%02X%02Xu' % (raw[0], raw[1], raw[2])   # запись цвета в коде: байты данных подряд (ключ: ff 26 85 -> 0xFF2685)
nit = bq(ART / 'missions/generic/nitro.p3d'); wr = bq(ART / 'wrench.p3d')
src = open(os.path.join(ROOT, 'viewer/spritelight.h'), encoding='utf-8').read()
body = src[src.index('static void nitroGlintBatch1148'):src.index('static void wrGlowDraw')]
wbody = src[src.index('static void wrGlowDraw'):]
chk(len(nit) == 3 and all(len(v) == 4 for v in nit.values()), 'нитро: три плашки %s' % sorted(nit))
chk(const(wr['wrenchShape46']) == '0xFF2685u' and const(wr['wrenchShape4']) == '0xFF5B48u', 'ключ: запись цвета данных = запись в коде (%s, %s)' % (const(wr['wrenchShape46']), const(wr['wrenchShape4'])))
chk('0xFF2685u' in wbody and '0xFF5B48u' in wbody, 'ключ: эти цвета стоят в wrGlowDraw')
big, second = const(nit['nitroShape3']), const(nit['nitroShape4'])
if '--wrong' in sys.argv: big = '0xEE237Du'
chk(big in body, 'нитро: большая звезда = nitroShape3 = %s в коде' % big)
chk(second in body, 'нитро: вторая звезда = nitroShape4 = %s в коде' % second)
chk('gvMode970 = 1' in body and 'WRENCH970_S' in body and 'WRENCH970_A' in body, 'нитро: ход слоёв и размер / прозрачность как у ключа (gvMode970, WRENCH970_S, WRENCH970_A)')
print('%s строк %d, упало %d' % ('FAIL' if fails else 'PASS', total, fails)); sys.exit(1 if fails else 0)
