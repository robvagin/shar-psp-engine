#!/bin/bash
# №1068 приёмка (второй тур TEST78 на PSP 29.10: отказ памяти PVS1 L4 2 753 824 Б при наибольшем куске 2,59 МБ).
#   host       tests/pvschunk/host.c на level.shr L1–L7: каждый список клетки из кусков = списку целого пула; красные --red-lookup / --red-plan
#   new        flag pvscapsim 2 (куча не даёт мягких кусков > 2 МБ, как раздробленная на PSP), L1 → L4 → L5: L1/L4 PVS1 кусками, сумма пула = файлу; L5 в запасе участка
#   red        то же на выключателе flag pvschunkold 1 (прежний код) — обязан покраснеть: уровень без PVS
#   cleannew / cleanold  без отказа: оба пути дают PVS, кадр (ktris/draws по каждой загрузке) обязан совпасть
# Запуск: tests/pvschunk1068/accept.sh [EBOOT] ; REUSE=1 — готовые прогоны не повторять. Последняя строка PASS/FAIL
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
E=${1:-$W/viewer/build_g79tour/EBOOT.PBP}; OUT=${OUT:-$W/build/pvs1068}; mkdir -p $OUT/run; rc=0
export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test78tour/PSP/GAME/SHARTEST15} EMU_OWNER=${EMU_OWNER:-pvs1068} EMU_MINE=${EMU_MINE:-4}
[ -f "$E" ] || { echo "нет EBOOT $E"; echo FAIL; exit 1; }
PY="$W/.venv/bin/python3 -W ignore -I"
HB=$OUT/host; cc -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -std=c99 -O2 -Wall -o $HB $W/tests/pvschunk/host.c || { echo FAIL; exit 1; }
LV=($SET/level.shr $SET/l2/level.shr $SET/l3/level.shr $SET/l4/level.shr $SET/l5/level.shr $SET/l6/level.shr $SET/l7/level.shr)
echo "host:"; $HB "${LV[@]}" | sed "s#$SET/##; s/^/  /" ; [ ${PIPESTATUS[0]} = 0 ] || rc=1
for r in --red-lookup --red-plan; do $HB $r "${LV[@]}" > $OUT/host$r.txt; c=$?; n=$(grep -c '^RED-OK' $OUT/host$r.txt)
  [ $c = 0 ] && [ $n = 7 ] && echo "  ok красный $r краснеет на 7 уровнях" || { echo "  FAIL красный $r: $n из 7"; rc=1; }; done
for m in new red cleannew cleanold; do
  [ "${REUSE:-0}" = 1 ] && [ -f $OUT/run/out_$m/smoke.txt ] && continue
  python3 $H/gen.py $m 1 4 5 > $OUT/$m.txt
  echo "$m" ; done | xargs -P 4 -n 1 bash -c 'OUT='"$OUT"'/run bash '"$W"'/tools/emu_run.sh '"$OUT"'/$0.txt $0 '"$E"' ${MAXT:-600} >'"$OUT"'/$0.log 2>&1'
for m in new red cleannew cleanold; do echo "$m:"; o=$($PY $H/check.py $m $OUT/run/out_$m/smoke.txt $SET 2>$OUT/frames_$m.txt); r=$?; echo "$o" | sed 's/^/  /' | cut -c1-230; [ $r = 0 ] || rc=1; done
# кадр: на целой куче куски и прежний блок дают одинаковые списки — ktris/draws по каждой загрузке совпадают
if cmp -s $OUT/frames_cleannew.txt $OUT/frames_cleanold.txt && [ -s $OUT/frames_cleannew.txt ]; then echo "ok кадр: кусками = одним блоком на всех загрузках ($(tr '\n' ' ' < $OUT/frames_cleannew.txt | sed 's/FRAME //g'))"
else echo "FAIL кадр кусками отличается от прежнего"; paste $OUT/frames_cleannew.txt $OUT/frames_cleanold.txt; rc=1; fi
echo "сведения: без PVS (red) против кусками (new): $(paste -d' ' $OUT/frames_red.txt $OUT/frames_new.txt | awk '{printf "%s ktris %s→%s draws %s→%s; ", $2, $3, $7, $4, $8}')"
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
