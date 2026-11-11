#!/bin/bash
# №1223: неподвижные ящики списка Опоры «коллизия без картинки» (9 из 103) против рисунка SPR1: зелёное — у каждого есть вершина рисунка в полудиагональ + 1,5 м;
# яма L4 908 (159,7 −42,0 186,3: на 41 м под землёй, достать нельзя) в гейт не входит. Красное: стена-ящик L4 №100 (город без SPR1 рядом) -> FAIL.
set -e; cd "$(dirname "$0")/../.."; D=${1:-$HOME/Downloads/shar-psp-t93/build/test96tour/PSP/GAME/SHARTEST15}; O=${OUT_DIR:-build/bots}; mkdir -p "$O"
V=viewer; . tools/bots/lib.sh; cc -std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots tools/bots/boxpic.c $BOTS_LIB -lm -o "$O/boxpic"
bad=0; run() { L=$1; shift; LV=$D/level.shr; [ $L = 1 ] || LV=$D/l$L/level.shr; r=$("$O/boxpic" "$LV" "$D/homer.shr" "$@" 2>&1 | grep -a "^RESULT"); echo "L$L ящики $*: $r"; [ "$r" = "RESULT OK" ] || bad=1; }
run 1 1581; run 2 961; run 4 1574; run 5 958; run 7 1210 1211 1213 1219 1253 1281 1285 1290
r=$("$O/boxpic" "$D/l4/level.shr" "$D/homer.shr" 100 2>&1 | grep -a "^RESULT"); echo "красное L4 ящик 100: $r"; [ "$r" = "RESULT FAIL" ] || bad=1
[ $bad = 0 ] && echo "ГЕЙТ OK" || { echo "ГЕЙТ FAIL"; exit 1; }
