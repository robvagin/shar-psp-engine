#!/bin/bash
# 1357 (Роб): «поговорить, чтобы начать миссию, — △, отменить — тоже △; жму много раз и отменяю миссию». Гейт: серия △ в 1 кадр через кадр от начала разговора (L2, Милхаус: последний этап вступления,
# ✕ Start Mission / △ Cancel). Старое (INPUTHINTS1357_OLD=1): серия отменяет миссию (TALKCANCEL). Новое: пока полосы разговора не раскрыты (подсказок нет) ✕/△ не считаются — отмены нет;
# после появления подсказок ✕ принимается (TALKSKIP), потом серия ✕ на карточке: Continue не раньше 0,4 с после загрузки карточки (старое: сразу).
#   tests/input1357/run.sh EBOOT   → build/input1357/*; итог check.py
set -u; cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}
RUN=${RUN:-$HOME/Downloads/shar-psp-fin108/tools/emu_run.sh}; SET=${SET:-$HOME/Downloads/shar-psp-int107b/build/test108/PSP/GAME/SHARTEST15}
OUT=${OUT:-build/input1357}; mkdir -p $OUT; OUT=$(cd $OUT && pwd)
for O in 0 1 2; do N=in1357_$O; F=$OUT/$N.scn
  { echo "scene SW_2_0"; echo "flag camdet 1"; echo "flag startcam 1"; echo "flag golevel 2"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"
    [ "$O" != 0 ] && echo "flag inputhints1357old 1"
    echo "flag msngo"; echo "wait 0.2"
    if [ "$O" != 2 ]; then for i in 1 2 3; do echo "press triangle 0.03"; echo "wait 0.067"; done; else echo "press triangle 0.03"; echo "wait 0.4"; fi
    echo "shot talk"; echo "wait 1.2"; echo "shot prompt"; echo "press cross 0.03"
    for i in $(seq 1 60); do echo "wait 0.067"; echo "press cross 0.03"; done
    echo "wait 3"; echo "shot card"; echo "flag msnhud"; echo "wait 1"; echo "end"; } > $F
  SET=$SET SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 OUT=$OUT EMU_MINE=${EMU_MINE:-1} bash $RUN $F $N $EB 600 > $OUT/$N.log 2>&1; echo "run $N: $(tail -1 $OUT/$N.log)"
done
python3 tests/input1357/check.py $OUT
