#!/bin/bash
# №1005 приёмка: плитка пешеходной зоны L3 остаётся родной текстурой. accept60.sh NEW_EBOOT [red]   (OVL — папка с новыми flat912.bin: build/t72/xwalk999)
#   Мера: (1) журнал WALK1005: у «стало» на L3 строк с причиной 11 не меньше 1 и все родные, на остальных уровнях строк нет; у «было» (файл walk1005old) строки есть, родных 0;
#   (2) кадр L3 у «стало» отличается от «было» (не меньше 3000 заметно изменившихся точек: швы плитки); (3) кадр L6 (та же картинка плитки, Барт) у «стало» и «было» совпадает (до 20 точек шума HUD) — соседний уровень не тронут.
#   «red» — красный контроль: «стало» тоже с файлом walk1005old -> родных 0, кадры равны -> FAIL. Последняя строка PASS / FAIL
D=$(cd "$(dirname "$0")" && pwd); I=$(cd "$D/../.." && pwd); RUN=${RUN:-$I/tools/emu_run.sh}; export OUT=${OUT:-/tmp/walk1005_out}; export EMU_OWNER=${EMU_OWNER:-Мультик}; export EMU_MINE=${EMU_MINE:-2}
export OVL=${OVL:-$I/build/t72/xwalk999}
EB=$1; RED=0; [ "${2:-}" = red ] && RED=1
[ -f "$EB" ] || { echo "нет EBOOT $EB"; echo FAIL; exit 1; }
[ -d "$OVL" ] || { echo "нет данных $OVL"; echo FAIL; exit 1; }
mkdir -p $OUT; rm -rf $OUT/out_acc_new $OUT/out_acc_old
( [ $RED = 1 ] && export FLAGS=walk1005old; $RUN $D/scenario_accept.txt acc_new "$EB" 500 >/dev/null 2>&1 ) &
( FLAGS=walk1005old $RUN $D/scenario_accept.txt acc_old "$EB" 500 >/dev/null 2>&1 ) &
wait
python3 -I - "$OUT/out_acc_new" "$OUT/out_acc_old" <<'PYEOF'
import sys, re
from PIL import Image
import numpy as np
dn, do = sys.argv[1], sys.argv[2]
rx = re.compile(r'WALK1005 level=(\d+) on=(\d) rows=(\d+) native=(\d+)')
def rd(d):
    try: t = open(d + '/smoke.txt', encoding='utf8', errors='replace').read()
    except OSError: return None
    return {int(m.group(1)): (int(m.group(2)), int(m.group(3)), int(m.group(4))) for m in rx.finditer(t)}
n, o = rd(dn), rd(do)
if not n or not o: print('нет журнала'); print('FAIL'); sys.exit(1)
ok_all = True
def chk(ok, s):
    global ok_all; ok_all &= bool(ok); print('  %s %s' % ('ok  ' if ok else 'FAIL', s))
chk(sorted(n) == [1, 2, 3, 4, 5, 6, 7], 'уровни в журнале «стало»: %s' % sorted(n))
chk(n.get(3, (0, 0, 0))[1] >= 1 and n[3][2] == n[3][1], 'L3 «стало»: строк причины 11 %d, родных %d (нужно >= 1 и все родные)' % (n.get(3, (0, 0, 0))[1], n.get(3, (0, 0, 0))[2]))
chk(all(v[1] == 0 for k, v in n.items() if k != 3), 'другие уровни «стало»: строк причины 11 нет (%s)' % {k: v[1] for k, v in n.items() if k != 3})
chk(o.get(3, (0, 0, 0))[1] >= 1 and o[3][2] == 0, 'L3 «было»: строк %d, родных %d (нужно >= 1 и 0)' % (o.get(3, (0, 0, 0))[1], o.get(3, (0, 0, 0))[2]))
def img(d, nm): return np.asarray(Image.open('%s/%s.bmp' % (d, nm)).convert('RGB'), int)
try:
    a, b = img(dn, 'A_L3_W'), img(do, 'A_L3_W'); df = int((np.abs(a - b).sum(axis=2) > 24).sum())
    c, e = img(dn, 'A_L6_W'), img(do, 'A_L6_W'); d6 = int((np.abs(c - e).sum(axis=2) > 24).sum())
except OSError: print('нет кадров'); print('FAIL'); sys.exit(1)
chk(df >= 3000, 'кадр L3: заметно изменившихся точек %d (нужно >= 3000)' % df)
chk(d6 <= 20, 'кадр L6: заметно разных точек «стало» и «было» %d (нужно <= 20, соседний уровень не тронут)' % d6)
print('PASS' if ok_all else 'FAIL'); sys.exit(0 if ok_all else 1)
PYEOF
