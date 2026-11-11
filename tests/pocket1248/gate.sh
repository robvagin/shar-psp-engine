#!/bin/bash
# №1248 (Кольцо-96): L7 G5-2 «как в яме, заехал и не могу выехать». Двор у Power Plant (стены с трёх сторон, вход с востока) — тупик короткого съезда (дороги 26/27, 25 м), на оси полосы четыре куста-шара.
#   tests/pocket1248/gate.sh [DIR с homer.shr и l7/level.shr]
# 1) RESET CAR из 20 точек двора: новое — ни разу не остаётся во дворе (RESET_DEADEND_OLD=0), прежнее (сборка -DRESET_DEADEND_OLD=1) — остаётся (красное);
# 2) водитель-бот из 64 стартов двора выезжает за x > -17 (физика двора: не ловушка, если рулить и сдавать назад);
# 3) справка (не гейт): заезд с востока на 15 м/с — потеря скорости на кустах; tools/bots/pocket.c: режимы dump reach run escape reset roads sp prof deadends.
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}; D=${1:-$HOME/Downloads/shar-psp-t93/build/test96tour/PSP/GAME/SHARTEST15}; O=${OUT_DIR:-build/bots}; mkdir -p "$O"
V=viewer; . tools/bots/lib.sh; CF="-std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots"
cc $CF tools/bots/pocket.c $BOTS_LIB -lm -o "$O/pocket" && cc $CF -DRESET_DEADEND_OLD=1 tools/bots/pocket.c $BOTS_LIB -lm -o "$O/pocket_old" || { echo "FAIL: сборка"; exit 1; }
L="$D/l7/level.shr"; H="$D/homer.shr"; bad=0
n=$("$O/pocket" "$L" "$H" reset | tail -1); o=$("$O/pocket_old" "$L" "$H" reset | tail -1)
echo "новое:   $n"; echo "прежнее: $o"
case "$n" in *"в кармане 0 из"*) ;; *) echo "FAIL: новое оставляет во дворе"; bad=1;; esac
case "$o" in *"в кармане 0 из"*) echo "FAIL: красный контроль не поймал (прежнее тоже выводит)"; bad=1;; esac
e=$("$O/pocket" "$L" "$H" escape | tail -1); echo "выезд водителем-ботом: $e"; case "$e" in *"выехала 64 из 64"*) ;; *) echo "FAIL: бот не выехал"; bad=1;; esac
echo "справка, заезд с востока 15 м/с:"; PK_D=6 "$O/pocket" "$L" "$H" run 15 | head -1 | cut -c1-200
[ $bad = 0 ] && echo "ГЕЙТ OK" || { echo "ГЕЙТ FAIL"; exit 1; }
