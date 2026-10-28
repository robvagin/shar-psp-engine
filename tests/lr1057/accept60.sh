#!/bin/bash
# №1057 + №1058 приёмка (Роб 27.10: «зажать L и R, и оно будет кликать вправо-влево, докликать до своей тачки»; «в Mission Select уровень/город влево-вправо ещё и L R»).
# №1057 будка: зажатые L R листают машины с повтором (пауза 0,30 с, шаг 0,11 с); одиночное касание — один шаг; ◄ ► повторяются как после №706. Откат BOOTHLR1057_OLD=1 / файл boothlr1057old / flag boothlr1057 0 (L R без повтора).
# №1058 Mission Select в паузе: уровень L R (одиночные) как ◄ ►; откат MSLR1058_OLD=1 / файл mselr1058old / flag mselr1058 0.
# Эмулятор: набор тура TEST73, EBOOT собирается из этого дерева (ACC_SKIP_BUILD=1 — взять готовый $OUT/EBOOT.PBP). Красный: будка — файл boothlr1057old (прогон без повтора L R), Mission Select — flag mselr1058 0. Последняя строка PASS/FAIL
set -u; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export PSPDEV=$W/pspdev PATH=$W/pspdev/bin:$PATH
OUT=${OUT:-$W/build/lr1057/acc}; mkdir -p $OUT; export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test73tour/PSP/GAME/SHARTEST15} EMU_OWNER=${EMU_OWNER:-Управление} EMU_MINE=${EMU_MINE:-4}
if [ "${ACC_SKIP_BUILD:-0}" != 1 ]; then
  psp-cmake -S $W/viewer -B $W/viewer/build_lr -DSHAR_RELEASE=ON -DSHAR_RELEASE_KEEP_DIAG=ON -DSHAR_RELEASE_KEEP_HUD=ON -DSHAR_RELEASE_KEEP_SMOKE=ON \
    -DCMAKE_C_FLAGS="-DJITTER_STRIPS_OLD=1 -DJITTER_CAR_OLD=1 -DFE_DEMO_PS2_OLD=1 -DZF932_OLD=1 -DCARFILL930_OLD=1 -DBB_SYNC_OLD=1" > $OUT/cfg.log 2>&1 && cmake --build $W/viewer/build_lr -j6 > $OUT/build.log 2>&1 || { echo "сборка не удалась ($OUT/build.log)"; echo FAIL; exit 1; }
  cp $W/viewer/build_lr/EBOOT.PBP $OUT/EBOOT.PBP
fi
E=$OUT/EBOOT.PBP; R=$W/tools/emu_run.sh; rc=0
sed 's/^flag msnhud 1$/flag msnhud 1/;s/^wait 1$/wait 1/' $H/ms_select.txt | awk '/^press start$/ && !d {print "flag mselr1058 0"; d=1} {print}' > $OUT/ms_old.txt
( OUT=$OUT bash $R $H/booth_hold.txt bnew $E 300 >/dev/null 2>&1 ) & ( FLAGS="boothlr1057old" OUT=$OUT bash $R $H/booth_hold.txt bold $E 300 >/dev/null 2>&1 ) &
( OUT=$OUT bash $R $H/ms_select.txt snew $E 500 >/dev/null 2>&1 ) & ( OUT=$OUT bash $R $OUT/ms_old.txt sold $E 500 >/dev/null 2>&1 ) & wait
for p in "booth bnew" "ms snew"; do set -- $p; echo "  зелёный $1:"; python3 -W ignore -I $H/check.py $1 $OUT/out_$2/smoke.txt || rc=1; done
for p in "booth bold" "ms sold"; do set -- $p; echo "  красный контроль $1 (выключатель = прежнее поведение):"; python3 -W ignore -I $H/check.py $1 $OUT/out_$2/smoke.txt old || rc=1; done
echo "  красный должен упасть на зелёной проверке:"
for p in "booth bold" "ms sold"; do set -- $p; if python3 -W ignore -I $H/check.py $1 $OUT/out_$2/smoke.txt >/dev/null 2>&1; then echo "    $1: КРАСНЫЙ НЕ КРАСНЫЙ"; rc=1; else echo "    ok $1: зелёная проверка на прежнем падает"; fi; done
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
