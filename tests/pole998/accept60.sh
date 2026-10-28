#!/bin/bash
# №998 приёмка (Роб 26.10, TEST71 на PSP, L5 F3-2: «красные столбы не сбиваются, сколько можно», повтор №975).
# Причина: прямой удар в ствол работал (№975), а касание вскользь (бортом, углом кузова на ходу) — нет: у вертикального ствола нормаль SAT поперёк движения, сближение по ней < SP_HIT_SPEED,
# касание есть, реакции нет. Больше всего у фонаря типа 8 (простой красный столб, cls 10, масса 1000: ствол — тонкий цилиндр без баннера); у типа 6 (с баннером) широкий объём баннера давал удар и вскользь.
# Правка: у высокого стейтпропа cls 10 касание на ходу (скорость кузова ≥ SP_HIT_SPEED) засчитывается ударом. Красный — -DPOLE998_OLD=1 (хост) и flag pole998 0 (эмулятор).
# Часть 1 (хост, tests/pole998/census.c на данных L4–L7): заезды на столб со смещением линии от ствола ±1,15 и ±1,35 м (касание краем кузова): «дыра» (касание без реакции) у высоких cls 10 — новое против прежнего.
# Часть 2 (эмулятор, аргумент 1 — EBOOT тура с -DZF932_OLD=1): удар игровой машины в каждый из 11 красных столбов перекрёстка L5 F3-2 (типа 8: 6, типа 6: 5; номера 203 204 205 207 214 218, 92 95 96 97 98): новый — упали все;
#   красный (flag pole998 0): заезд вскользь в 204, 205, 207 — стоят; прямые удары падали и раньше (контроль не ломает уже работавшее).
# Соседние гейты: tests/lamp975, fall968, poles946 — запускать отдельно. Последняя строка PASS/FAIL
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; E=${1:-}; OUT=${OUT:-$W/build/f998}; mkdir -p $OUT; rc=0
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15}; export SET
CCS="cc -std=c11 -O2 -w -DSKIN_HOST -include stdio.h -I $W/viewer $H/census.c $W/viewer/col.c $W/viewer/props.c $W/viewer/stateprops.c $W/viewer/car.c $W/viewer/coins.c -lm"
$CCS -o $OUT/cn || { echo "census не собрался"; echo FAIL; exit 1; }; $CCS -DPOLE998_OLD=1 -o $OUT/co || exit 1
holes() { for lat in 1.15 -1.15 1.35 -1.35; do LAT998=$lat $1 $SET/l$2/level.shr S 2>/dev/null | grep ЗАЕЗДЫ | sed 's/.*высокие cls10: сбил \([0-9]*\), дыра \([0-9]*\).*/\2/'; done | awk '{s+=$1} END {print s+0}'; }
tn=0; to=0
for L in 4 5 6 7; do n=$(holes $OUT/cn $L); o=$(holes $OUT/co $L); printf "  L%s: «дыр» у высоких фонарей cls 10 (заезды краем кузова, 4 смещения): прежнее %s -> новое %s\n" $L $o $n; tn=$((tn + n)); to=$((to + o)); [ "$n" -le 8 ] || { echo "  L$L: после правки слишком много дыр"; rc=1; }; done
echo "  итого дыр: прежнее $to -> новое $tn"; [ "$to" -ge 400 ] || { echo "  КРАСНЫЙ (прежнее) не красный: дыр $to < 400"; rc=1; }
for L in 5 7; do a=$(LAT998=0 $OUT/cn $SET/l$L/level.shr S 2>/dev/null | grep ЗАЕЗДЫ | sed 's/.*высокие cls10: сбил \([0-9]*\).*/\1/'); b=$(LAT998=0 $OUT/co $SET/l$L/level.shr S 2>/dev/null | grep ЗАЕЗДЫ | sed 's/.*высокие cls10: сбил \([0-9]*\).*/\1/'); echo "  L$L прямой удар в ствол: сбито $b -> $a"; [ "$a" -ge "$b" ] || { echo "  прямой удар стал хуже"; rc=1; }; done
if [ -n "$E" ]; then
  export EMU_OWNER=${EMU_OWNER:-Управление} EMU_MINE=${EMU_MINE:-2} OUT
  python3 $H/gen_scenarios.py $OUT/hit_new.txt; python3 $H/gen_scenarios.py $OUT/hit_old.txt old
  bash $W/tools/emu_run.sh $OUT/hit_new.txt gn "$E" 400 >/dev/null 2>&1 & bash $W/tools/emu_run.sh $OUT/hit_old.txt go "$E" 400 >/dev/null 2>&1; wait
  python3 $H/check.py $OUT/out_gn/smoke.txt $OUT/out_go/smoke.txt || rc=1
  python3 $H/sheet.py $OUT/out_gn $OUT/out_go $OUT/sheet_fall998.png >/dev/null 2>&1
fi
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
