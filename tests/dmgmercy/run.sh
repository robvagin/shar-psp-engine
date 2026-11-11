#!/bin/bash
# Опора-93: гейт «пощада один раз» (car.c dmgApply, DMGMERCY_OLD). tests/dmgmercy/run.sh  - новое обязано пройти, с -DDMGMERCY_OLD=1 обязано упасть (красный контроль)
set -u; cd "$(dirname "$0")/../.."; export PATH=/Library/Developer/CommandLineTools/usr/bin:$PATH DEVELOPER_DIR=/Library/Developer/CommandLineTools SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
V=viewer; LIB="tools/bots/world.c $V/game.c $V/col.c $V/props.c $V/stateprops.c $V/coins.c $V/homer.c $V/watchdog.c $V/watchdog_game.c $V/skin.c $V/rooms.c $V/roomcam.c $V/collect.c $V/traffic.c $V/msncar.c $V/game_msn.c $V/shop.c $V/mission/msn_file.c $V/mission/msn_run.c $V/mission/msn_ops.c $V/mission/msn_game.c $V/msn_audio.c tools/bots/msn_audio_host.c $V/animobj.c"
O=${OUT:-build/dmgmercy}; mkdir -p $O; rc=0
cc -std=c11 -O1 -w -DSKIN_HOST -I viewer -I tools/bots tests/dmgmercy/host.c $LIB -lm -o $O/new || exit 2
cc -std=c11 -O1 -w -DSKIN_HOST -DDMGMERCY_OLD=1 -I viewer -I tools/bots tests/dmgmercy/host.c $LIB -lm -o $O/old || exit 2
echo "== новое (обязано пройти)"; $O/new && echo "ЗЕЛЁНОЕ" || { echo "ОШИБКА: новое упало"; rc=1; }
echo "== DMGMERCY_OLD=1 (красный контроль, обязано упасть)"; $O/old > $O/old.txt; if [ $? -ne 0 ]; then grep FAIL $O/old.txt | head -4; echo "КРАСНОЕ на OLD, как и должно"; else echo "ОШИБКА: на OLD не упало"; rc=1; fi
exit $rc
