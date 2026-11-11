#!/bin/bash
# №1248ж (Кольцо-96): RESET CAR на ВСЕХ семи уровнях: во всех точках осей дорог трафика (шаг 12 м) машину ставят на полосу, RESET, оценка итоговой постановки:
#   ВНУТРИ — поставленный кузов выталкивается из твёрдого (> 0,8 м); СТЕНА — впереди ближе 6 м (путь по склону, обрыв считается концом); ВОДА/ОБРЫВ — зона DEATH под машиной или колёс нет.
# Новое (кандидаты по полосе назад/вперёд + разворот лицом наружу) против прежнего (-DRESET_CAND_OLD=1 -DRESET_WALLFACE_OLD=1). tools/bots/pocket.c, режим resetsweep.
#   tests/pocket1248/reset_all_gate.sh [DIR с L1..L7/level.shr, player.shr (L1 homer.shr)]
# красное: прежнее даёт ВНУТРИ >= 1 (L4 E5-2), СТЕНА >= 5 (L7 F12), ВОДА/ОБРЫВ >= 1; зелёное: новое ВНУТРИ 0 и СТЕНА 0 на всех уровнях, ВОДА/ОБРЫВ <= 1 (остаток L7 F12-6 у обрыва южного конца дорог 20/21/24/25, ящик b745 — у Коллизии).
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}; O=${OUT_DIR:-build/bots}; mkdir -p "$O"
D=${1:-$HOME/Downloads/shar-psp-opora93mech98/build/in98}; V=viewer; . tools/bots/lib.sh; CF="-std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots"
cc $CF tools/bots/pocket.c $BOTS_LIB -lm -o "$O/pk_rn" && cc $CF -DRESET_CAND_OLD=1 -DRESET_WALLFACE_OLD=1 tools/bots/pocket.c $BOTS_LIB -lm -o "$O/pk_ro" || { echo "FAIL: сборка"; exit 1; }
bad=0; oi=0; ow=0; od=0; ni=0; nw=0; nd=0
for L in L1 L2 L3 L4 L5 L6 L7; do P="$D/$L/player.shr"; [ -f "$P" ] || P="$D/$L/homer.shr"
  n=$(nice -n 20 "$O/pk_rn" "$D/$L/level.shr" "$P" resetsweep 12 4 2>/dev/null | grep RESETSWEEP); o=$(nice -n 20 "$O/pk_ro" "$D/$L/level.shr" "$P" resetsweep 12 4 2>/dev/null | grep RESETSWEEP)
  g() { echo "$1" | sed -E "s/.*$2 ([0-9]+).*/\1/"; }
  crash=""; [ -n "$o" ] || { o="RESETSWEEP точек 0: внутри 1, стена < 6 м впереди 0, вода/обрыв 1;"; crash=" (прежнее УПАЛО в хост-стенде на точке L4 E5-2: RESET в воду/твёрдое, зачтено как внутри 1 + вода/обрыв 1)"; }
  a=$(g "$n" "внутри"); b=$(g "$n" "впереди"); c=$(g "$n" "обрыв"); a0=$(g "$o" "внутри"); b0=$(g "$o" "впереди"); c0=$(g "$o" "обрыв"); tp=$(echo "$n" | sed -E 's/.*точек ([0-9]+).*/\1/')
  echo "$L: точек $tp | новое: внутри $a, стена $b, вода/обрыв $c | прежнее: внутри $a0, стена $b0, вода/обрыв $c0$crash"
  ni=$((ni+a)); nw=$((nw+b)); nd=$((nd+c)); oi=$((oi+a0)); ow=$((ow+b0)); od=$((od+c0)); done
echo "ИТОГО новое: внутри $ni, стена $nw, вода/обрыв $nd | прежнее: внутри $oi, стена $ow, вода/обрыв $od"
[ $ni = 0 ] || { echo "FAIL: новое оставляет кузов внутри твёрдого ($ni)"; bad=1; }
[ $nw = 0 ] || { echo "FAIL: новое ставит лицом к стене ближе 6 м ($nw)"; bad=1; }
[ $nd -le 1 ] || { echo "FAIL: новое ставит в обрыв/воду ($nd > 1)"; bad=1; }
[ $oi -ge 1 ] || { echo "FAIL: красный контроль — прежнее не ставит внутрь"; bad=1; }
[ $ow -ge 5 ] || { echo "FAIL: красный контроль — прежнее не ставит лицом к стене"; bad=1; }
[ $od -ge 1 ] || { echo "FAIL: красный контроль — прежнее не ставит в обрыв/воду"; bad=1; }
rm -f "$O/pk_rn" "$O/pk_ro"
[ $bad = 0 ] && echo "ГЕЙТ OK" || { echo "ГЕЙТ FAIL"; exit 1; }
