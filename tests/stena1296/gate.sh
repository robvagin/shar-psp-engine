#!/bin/bash
# 1296/1300: у персонажа одна точка по состоянию. L2: пока сюжетная m1 держит Милхауса, его бонусный двойник (выдаватель гонки sr1 в другом месте) скрыт и △ у него гонку не открывает; после Abort — так же.
# Красный: -DMSN_BONUSTWIN1296_OLD=1 / аргумент 1 (flag bonustwin1296old) — прежнее: у места гонки △ открывает гонку (bonus_near ≥ 0).   tests/stena1296/gate.sh [level.shr homer.shr level02.msn]
cd "$(dirname "$0")/../.."; V=viewer; . tools/bots/lib.sh
D=$HOME/Downloads/shar-psp; L=${1:-$HOME/Downloads/shar-psp-int107b/build/test108tour/PSP/GAME/SHARTEST15/l2/level.shr}; H=${2:-$D/build/homer.shr}; M=${3:-$D/build/missions/level02.msn}
cc -std=c11 -O2 -DSKIN_HOST -I viewer -I tools/bots tests/stena1296/cancel_host.c $BOTS_LIB -lm -o /tmp/c1296.$$ || exit 1
echo "== новое:"; /tmp/c1296.$$ "$L" "$H" "$M" 0; rn=$?
echo "== прежнее (обязано показать гонку у места выдавателя):"; /tmp/c1296.$$ "$L" "$H" "$M" 1; ro=$?
echo "== новое, реальный разговор → TalkCancel (двойник не переезжает на место m1):"; /tmp/c1296.$$ "$L" "$H" "$M" 0 talk; tn=$?
echo "== прежнее, тот же путь (обязано переставить двойника и открыть гонку):"; /tmp/c1296.$$ "$L" "$H" "$M" 1 talk; to=$?
rm -f /tmp/c1296.$$
[ $rn = 0 ] && [ $ro = 0 ] && [ $tn = 0 ] && [ $to = 0 ] && echo PASS || { echo FAIL; exit 1; }
