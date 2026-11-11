#!/bin/bash
# №1223: «нигде нет коллизии без рисунка» для живых объектов ANM1 L1–L7 (tools/bots/aodraw.c): каждый живой объём в пределах полудиагонали + 1,5 м от вершин позы рисунка во все фазы клипа.
#   tests/airbox1223/aodraw.sh [DIR с level.shr (L1), l2..l7/level.shr и homer.shr]
# Зелёное: все 7 уровней RESULT OK. Красное: сборка с -DAO_CLUSTER_SYNC_OLD=1 (кластеры объекта идут разным временем, как до правки) даёт FAIL на L6 (кран: платформа коллизии в 17 м от крюка).
set -e; cd "$(dirname "$0")/../.."; D=${1:-$HOME/Downloads/shar-psp-t93/build/test96tour/PSP/GAME/SHARTEST15}; O=${OUT_DIR:-build/bots}; mkdir -p "$O"
V=viewer; . tools/bots/lib.sh
CF="-std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots"
cc $CF tools/bots/aodraw.c $BOTS_LIB -lm -o "$O/aodraw"; cc $CF -DAO_CLUSTER_SYNC_OLD=1 tools/bots/aodraw.c $BOTS_LIB -lm -o "$O/aodraw_old"
bad=0
for L in 1 2 3 4 5 6 7; do LV=$D/level.shr; [ $L = 1 ] || LV=$D/l$L/level.shr
  r=$("$O/aodraw" "$LV" "$D/homer.shr" 2>&1 | grep -a "^RESULT" | tail -1); echo "зелёное L$L: $r"; [ "$r" = "RESULT OK" ] || bad=1; done
r=$("$O/aodraw_old" "$D/l6/level.shr" "$D/homer.shr" 2>&1 | grep -a "^RESULT" | tail -1); echo "красное L6 (OLD): $r"; [ "$r" = "RESULT FAIL" ] || bad=1
[ $bad = 0 ] && echo "ГЕЙТ OK" || { echo "ГЕЙТ FAIL"; exit 1; }
