#!/bin/bash
# 1347 (Роб): на паузе миссии окно задания с текстом этапа, как в оригинале. Гейт: tests/pauseobj1347/run.sh EBOOT [КОД ...] — по прогону на код этапа (миссия*100+этап)
# и на вариант (новое / PAUSEOBJ1347_OLD=1): 20 с этапа без всплытия бабла, пауза, снимок; выход из паузы, снимок. Итог: check.py. Эмулятор безголовый, не больше одного.
set -u; cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; shift; CODES=${*:-101 302}
RUN=${RUN:-$HOME/Downloads/shar-psp-fin108/tools/emu_run.sh}; SET=${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15}
OUT=${OUT:-build/pauseobj1347}; mkdir -p $OUT; OUT=$(cd $OUT && pwd)
for C in $CODES; do for O in ${VARS:-0 1}; do
  N=po1347_${C}_$O; F=$OUT/$N.scn
  { echo "scene SW_1_0"; echo "flag camdet 1"; echo "flag startcam 1"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"
    [ "$O" = 1 ] && echo "flag ${OLDFLAG:-pauseobj1347old} 1"; [ "$O" = 2 ] && echo "flag pauseobj1347old 1"
    echo "flag msnmainat $C"; echo "wait 3"; echo "press cross"; if [ "${CROSS2:-1}" = 1 ]; then echo "wait 2"; echo "press cross"; fi; [ -n "${FOOT:-}" ] && { echo "wait 1.5"; echo "foot $FOOT"; }; echo "wait ${WAITS:-20}"; [ -n "${MSNPASS:-}" ] && { echo "flag ${MSNPASS_FLAG:-msnpass} ${MSNPASS_ARG:-}"; [ "${PASSW:-0}" != 0 ] && echo "wait $PASSW"; }; echo "flag msnhud"; echo "shot a"
    echo "press start"; echo "wait 3"; echo "shot p"; echo "press start"; echo "wait 3"; echo "shot r"; echo "flag msnhud"; echo "wait 1"; echo "end"; } > $F
  SET=$SET SMOKE_HEADLESS=1 OUT=$OUT EMU_MINE=${EMU_MINE:-2} bash $RUN $F $N $EB 500 > $OUT/$N.log 2>&1; echo "run $N: $(tail -1 $OUT/$N.log)"
done; done
python3 tests/pauseobj1347/check.py $OUT $CODES
