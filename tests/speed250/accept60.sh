#!/bin/bash
# скорость 250, приём №1 (MEMTRIM1060) и приём №2 (CAMCLIPFAST1060, часть cc): выгрузка дальних клеток при тесной куче растянута на кадры (MEMTRIM1060). Маршрут L4 (F_L4_CR1, F_L4_CR1b) при дали 250 м, flag heapleft 3.2: куча тесная, как в туре TEST73 на PSP.
#   НОВЫЙ (flag memtrim1060 1): пик stream() ≤ 4 мс Мака, спайков нет, картинка и видимые незагруженные те же.  КРАСНЫЙ (flag memtrim1060 0): те же 50 выгрузок по ~70 клеток за кадр, пик 17 мс Мака.  СВОБОДНЫЙ: куча не тесная — выгрузки нет.
# Запуск: tests/speed250/accept60.sh [EBOOT] ; REUSE=1 — готовые прогоны не повторять. Последняя строка PASS/FAIL
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
E=${1:-$W/viewer/build_s250/EBOOT.PBP}; export OUT=${OUT:-$W/build/s250/acc}; mkdir -p $OUT; rc=0
[ -f "$E" ] || { echo "нет EBOOT $E"; echo FAIL; exit 1; }
for j in new:route_l4_tight_m1 red:route_l4_tight_m0 free:route_l4_free cc_f0:route_l4_cc_f0 cc_f1:route_l4_cc_f1 cc_c1:route_l4_cc_c1; do v=${j%%:*}; s=${j##*:}
  [ "${REUSE:-0}" = 1 ] && [ -s $OUT/out_$v/smoke.txt ] && continue
  ( bash $H/run.sh $v $H/$s.txt $E >/dev/null 2>&1 ) &
done; wait   # шесть прогонов: по три сразу (EMU_MINE 3)
for v in new red free; do echo "$v:"; if [ $v = new ]; then $W/.venv/bin/python3 -W ignore -I $H/check_trim.py new $OUT/out_new $OUT/out_red || rc=1; else $W/.venv/bin/python3 -W ignore -I $H/check_trim.py $v $OUT/out_$v || rc=1; fi; done
echo "cc (приём 2, счётчик CAMCLIPSTAT):"; $W/.venv/bin/python3 -W ignore -I $H/check_cc.py $OUT/out_cc_f0 $OUT/out_cc_f1 $OUT/out_cc_c1 || rc=1
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
