#!/bin/bash
# №1248г (Кольцо-96): мягкий куст (BUSHSOFT1248_OLD) не даёт крена, подъёма и двух колёс. Хост-стенд tools/bots/pocket.c, режим bushroll:
# сквозь каждый мягкий куст-шар (кл. 2, высота <= 1,5 м) на 4 и 6 м/с, 8 курсов x сдвиг -1/0/+1 м; ровные проходы = где контроль (тот же проход с выключенным кустом) сам не кренится > 3° и не встаёт на два колеса.
#   красное: прежнее (-DBUSHSOFT1248_OLD=1) на L4 v6 не проезжает при проехавшем контроле >= 1 раз; зелёное: новое — 0 раз не проехало и 0 проходов с креном +3°, клевком > 3° или кадром на двух колёсах.
#   tests/pocket1248/bush_roll_gate.sh [DIR с L4/level.shr, L4/player.shr, L7/...]
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}; O=${OUT_DIR:-build/bots}; mkdir -p "$O"
D=${1:-$HOME/Downloads/shar-psp-opora93mech98/build/in98}; V=viewer; . tools/bots/lib.sh; CF="-std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots"
cc $CF tools/bots/pocket.c $BOTS_LIB -lm -o "$O/pocket_br" && cc $CF -DBUSHSOFT1248_OLD=1 tools/bots/pocket.c $BOTS_LIB -lm -o "$O/pocket_br_old" || { echo "FAIL: сборка"; exit 1; }
bad=0
r() { nice -n 20 "$O/$1" "$D/$2/level.shr" "$D/$2/player.shr" bushroll 150 "$3" 2>/dev/null | grep BUSHROLL; }
num() { echo "$1" | sed -E "s/.*$2 ([0-9]+).*/\1/"; }
for c in "L4 4" "L4 6" "L7 4"; do set -- $c; n=$(r pocket_br $1 $2); o=$(r pocket_br_old $1 $2)
  nb=$(echo "$n" | sed -E 's/.*две колеса.*|.*кадр на двух колёсах\) ([0-9]+);.*/\1/'); ns=$(echo "$n" | sed -E 's/.*контроле ([0-9]+);.*/\1/'); os=$(echo "$o" | sed -E 's/.*контроле ([0-9]+);.*/\1/')
  echo "$1 v$2: новое хуже контроля $nb, не проехало $ns | прежнее не проехало $os"
  [ "$nb" = 0 ] && [ "$ns" = 0 ] || { echo "FAIL: новое $1 v$2: $n"; bad=1; }
  [ "$1 $2" = "L4 6" ] && { [ "$os" -ge 1 ] || { echo "FAIL: красный контроль — прежнее проехало все ($o)"; bad=1; }; }
done
rm -f "$O/pocket_br" "$O/pocket_br_old"
[ $bad = 0 ] && echo "ГЕЙТ OK" || { echo "ГЕЙТ FAIL"; exit 1; }
