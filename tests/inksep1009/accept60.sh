#!/bin/bash
# №1009 приёмка: контур героя поверх машины трафика не пропадает (ободок ближнего ложится на тело дальней машины). accept60.sh NEW_EBOOT [red]
#   Сцена L1: машина трафика стоит, перед ней Гомер пешком на зазоре 1,2 и 2,2 м; два кадра с одного места одним EBOOT: o — flag inksep1009 0 (прежнее), n — flag inksep1009 1.
#   Мера: чёрных точек (все каналы < 70) в окне вокруг Гомера и бампера: у «стало» не меньше, чем у «было», на 25 и больше на обоих зазорах; и что «стало» не добавило чёрного вне окна (рамка вокруг: разница <= 15 точек).
#   «red» — красный контроль: «стало» тоже flag inksep1009 0 -> точек поровну -> FAIL. Последняя строка PASS / FAIL
D=$(cd "$(dirname "$0")" && pwd); I=$(cd "$D/../.." && pwd); RUN=${RUN:-$I/tools/emu_run.sh}; export OUT=${OUT:-/tmp/inksep1009_out}; export EMU_OWNER=${EMU_OWNER:-Мультик}; export EMU_MINE=${EMU_MINE:-2}
EB=$1; MODE=green; [ "${2:-}" = red ] && MODE=red
[ -f "$EB" ] || { echo "нет EBOOT $EB"; echo FAIL; exit 1; }
mkdir -p $OUT; rm -rf $OUT/out_acc; python3 -I $D/gen_scenario.py $MODE homer > $OUT/s_$MODE.txt
$RUN $OUT/s_$MODE.txt acc "$EB" 300 >/dev/null 2>&1
python3 -I - "$OUT/out_acc" <<'PYEOF'
import sys
from PIL import Image
import numpy as np
d = sys.argv[1]
ok_all = True
def chk(ok, s):
    global ok_all; ok_all &= bool(ok); print('  %s %s' % ('ok  ' if ok else 'FAIL', s))
def dark(nm, box):
    a = np.asarray(Image.open('%s/%s.bmp' % (d, nm)).convert('RGB'), int); x0, y0, x1, y1 = box
    return int((a[y0:y1, x0:x1].max(axis=2) < 70).sum())
try:
    for g in ('h12', 'h22'):
        o, n = dark('o_' + g, (215, 115, 280, 185)), dark('n_' + g, (215, 115, 280, 185))
        chk(n >= o + 25, 'зазор %s: чёрных точек у Гомера и бампера было %d, стало %d (нужно стало >= было + 25)' % (g, o, n))
        ro, rn = dark('o_' + g, (0, 200, 120, 272)), dark('n_' + g, (0, 200, 120, 272))
        chk(abs(rn - ro) <= 15, 'зазор %s: чёрных точек вне окна (угол кадра) было %d, стало %d (нужно разница <= 15)' % (g, ro, rn))
except OSError: print('нет кадров'); print('FAIL'); sys.exit(1)
print('PASS' if ok_all else 'FAIL'); sys.exit(0 if ok_all else 1)
PYEOF
