#!/bin/bash
# Правка A2.5 (Стена 107): перезапуск с точки: машины прежних этапов, которых не называет этап точки, не остаются в мире (стоят без ИИ и уходят); красный -DMC_REPLAYPARK_OLD=1 (машина прошлого этапа остаётся в мире).
set -u; cd "$(dirname "$0")/../.."; P="${1:-$HOME/Downloads/shar-psp-dal103/build/test106tour/PSP/GAME/SHARTEST15}"
OUT=build/stena107; mkdir -p "$OUT"; V=viewer; . tools/bots/lib.sh
b() { cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots "$@" tests/stena107/replay_park.c $BOTS_LIB -lm -o "$OUT/$NAME"; }
NAME=rp_green; b; NAME=rp_old; b -DMC_REPLAYPARK_OLD=1
r() { BW_LEVEL=2 BW_NO_TRAFFIC=1 "$OUT/$1" "$P/l2/level.shr" "$P/homer.shr" "$P/l2/level02.msn" m4 5 | tail -1; }
g=$(r rp_green); o=$(r rp_old); echo "новый: $g"; echo "старое: $o"
echo "$g" | grep -q "stale_cars=0" && echo "$o" | grep -q "stale_cars=1" && { echo PASS; exit 0; }; echo FAIL; exit 1
