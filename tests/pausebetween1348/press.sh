#!/bin/bash
# 1348, хвост: из паузы миссии в окне между этапами (BETWEEN) нажать RESTART MISSION и CANCEL MISSION, без чёрного экрана и цикла.
#   tests/pausebetween1348/press.sh EBOOT   (этап 302 goto; BETWEEN входом не зависит от игрока: «flag msnpasshold 8» засчитывает этап и держит окно; для сравнения CANCEL из обычной паузы RUNNING)
# Проверки: RESTART — одно «MSN pause-restart», миссия идёт дальше (MSNHUD phase=1 status=1), кадры не чёрные; CANCEL — одно «MSN pause-abort», итог тот же, что у CANCEL из RUNNING
# (phase/stage одинаковы), кадры не чёрные, повторных стартов нет. Красный контроль наличия пунктов — run.sh (на PAUSEBETWEEN1348_OLD=1 строк RESTART/CANCEL в паузе BETWEEN нет).
set -u; cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}
RUN=${RUN:-$HOME/Downloads/shar-psp-fin108/tools/emu_run.sh}; SET=${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15}
OUT=build/pausebetween1348/press; mkdir -p $OUT; OUT=$(cd $OUT && pwd)
scn() {  # имя код ожидание_до_паузы шаги_вниз
  { echo "scene SW_1_0"; echo "flag camdet 1"; echo "flag startcam 1"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag msnmainat $2"; echo "wait 3"; echo "press cross"; echo "wait $3"; [ "${6:-0}" = 1 ] && echo "flag msnpasshold 8"
    echo "press start"; echo "wait 2"; for i in $(seq 1 $4); do echo "press down"; echo "wait 0.5"; done; echo "press cross"; echo "wait 2"; [ "$5" = 1 ] && { echo "press cross"; }
    echo "wait 3"; echo "shot s1"; echo "flag msnhud"; echo "wait 8"; echo "shot s2"; echo "flag msnhud"; echo "end"; } > $OUT/$1.scn
  SET=$SET SMOKE_HEADLESS=1 OUT=$OUT EMU_MINE=${EMU_MINE:-2} bash $RUN $OUT/$1.scn $1 $EB 500 > $OUT/$1.log 2>&1; echo "run $1: $(tail -1 $OUT/$1.log)"
}
scn restart_btw 302 3 1 1 1; scn cancel_btw 302 3 2 0 1; scn cancel_run 302 3 2 0 0; scn restart_run 302 3 1 1 0
python3 tests/pausebetween1348/press_check.py $OUT
