#!/bin/bash
# №999 приёмка: переход на брусчатке в цвет остальной дороги (все уровни). accept60.sh NEW_EBOOT [red]   (OVL — папка с новыми flat912.bin: build/t72/xwalk999)
#   Мера: журнал XWALK999TEX (средний цвет насыщенных точек текстуры перехода и чистой дороги-донора, палитра после тона и №979): у «стало» расстояние до донора не больше 14 у каждой текстуры перехода и их не меньше 3 на уровнях 1..7,
#   у «было» (файл xwalk999old) в L6 у каждой текстуры перехода расстояние не меньше 25 хотя бы у одной; донор «стало» = донор «было» (чистая дорога не тронута); кадр L6 X1 различается.
#   «red» — красный контроль: «стало» тоже с файлом xwalk999old -> расстояния как у «было» -> FAIL. Последняя строка PASS / FAIL
D=$(cd "$(dirname "$0")" && pwd); I=$(cd "$D/../.." && pwd); RUN=${RUN:-$I/tools/emu_run.sh}; export OUT=${OUT:-/tmp/xwalk999_out}; export EMU_OWNER=${EMU_OWNER:-Мультик}; export EMU_MINE=${EMU_MINE:-2}
export OVL=${OVL:-$I/build/t72/xwalk999}
EB=$1; RED=0; [ "${2:-}" = red ] && RED=1
[ -f "$EB" ] || { echo "нет EBOOT $EB"; echo FAIL; exit 1; }
[ -d "$OVL" ] || { echo "нет данных $OVL"; echo FAIL; exit 1; }
mkdir -p $OUT; rm -rf $OUT/out_acc_new $OUT/out_acc_old
( [ $RED = 1 ] && export FLAGS=xwalk999old; $RUN $D/scenario_accept.txt acc_new "$EB" 400 >/dev/null 2>&1 ) &
( FLAGS=xwalk999old $RUN $D/scenario_accept.txt acc_old "$EB" 400 >/dev/null 2>&1 ) &
wait
python3 -I - "$OUT/out_acc_new" "$OUT/out_acc_old" <<'PYEOF'
import sys, re
from PIL import Image
import numpy as np
dn, do = sys.argv[1], sys.argv[2]
rx = re.compile(r'XWALK999TEX level=(\d+) tex=(\d+) donor=(\d+) own=(\d+),(\d+),(\d+)\((\d+)\) don=(\d+),(\d+),(\d+)\((\d+)\)')
def rd(d):
    try: t = open(d + '/smoke.txt', encoding='utf8', errors='replace').read()
    except OSError: return None
    out = {}
    for m in rx.finditer(t):
        g = [int(x) for x in m.groups()]; out[(g[0], g[1])] = (g[2], tuple(g[3:6]), g[6], tuple(g[7:10]), g[10])
    return out
n, o = rd(dn), rd(do)
if not n or o is None: print('нет данных журнала'); print('FAIL'); sys.exit(1)
ok_all = True
def chk(ok, s):
    global ok_all; ok_all &= bool(ok); print('  %s %s' % ('ok  ' if ok else 'FAIL', s))
dist = lambda a, b: sum((x - y) ** 2 for x, y in zip(a, b)) ** .5
lv = sorted({k[0] for k in n})
chk(lv == [1, 2, 3, 4, 5, 6, 7] or len(n) >= 3, 'текстур перехода «стало»: %d на уровнях %s (нужно не меньше 3)' % (len(n), lv))
bad = [(k, round(dist(v[1], v[3]), 1)) for k, v in n.items() if v[2] > 0 and v[4] > 0 and dist(v[1], v[3]) > 14]
chk(not bad, 'переход «стало» в тон донора (до 14): худшие %s' % sorted(bad, key=lambda x: -x[1])[:4])
mx = max((dist(v[1], v[3]) for k, v in n.items() if v[2] > 0 and v[4] > 0), default=999)
print('  самое большое расстояние «стало» %.1f' % mx)
l6 = {k: dist(v[1], v[3]) for k, v in o.items() if k[0] == 6 and v[4] > 0 and v[2] > 0}
chk(l6 and max(l6.values()) >= 25, 'L6 «было»: расстояние до донора %s (хотя бы одно не меньше 25)' % {k[1]: round(v) for k, v in l6.items()})
chk(all(k in o and o[k][3] == v[3] for k, v in n.items()), 'донор-дорога «стало» = «было» (чистая дорога не тронута)')
try:
    a = np.asarray(Image.open(dn + '/A_L6_X1.bmp').convert('RGB'), int); b = np.asarray(Image.open(do + '/A_L6_X1.bmp').convert('RGB'), int)
    df = int((np.abs(a - b).sum(axis=2) > 24).sum())
except OSError: print('нет кадра'); print('FAIL'); sys.exit(1)
chk(df >= 1500, 'кадр L6 X1: пикселей, изменившихся заметно: %d (нужно не меньше 1500)' % df)
print('PASS' if ok_all else 'FAIL'); sys.exit(0 if ok_all else 1)
PYEOF
