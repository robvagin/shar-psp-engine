#!/bin/bash
# №1248д (Кольцо-96): RESET CAR у ящика L7 b745 (F12-9, проезд под зданием, дороги 20/21/24/25 идут сквозь ящик). Прежнее: точка RESET внутри ящика, машина вытолкнута на грань лицом к стене, v = 0. Новое: стена впереди, позади свободно — на 6 м назад лицом наружу.
#   tests/pocket1248/wallface_gate.sh [DIR с L7/level.shr, L7/player.shr]
# красное: прежнее (-DRESET_WALLFACE_OLD=1 -DRESET_CAND_OLD=1) из F12-9 при курсах 180/0/90 ставит лицом к стене ближе 6 м (путь по склону); зелёное: новое — впереди >= 6 м; контроли: открытая дорога — новое = прежнее; двор G5-2 — tests/pocket1248/gate.sh OK.
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}; O=${OUT_DIR:-build/bots}; mkdir -p "$O"
D=${1:-$HOME/Downloads/shar-psp-opora93mech98/build/in98}; V=viewer; . tools/bots/lib.sh; CF="-std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots"
cc $CF tools/bots/pocket.c $BOTS_LIB -lm -o "$O/pocket_wf" && cc $CF -DRESET_WALLFACE_OLD=1 -DRESET_CAND_OLD=1 tools/bots/pocket.c $BOTS_LIB -lm -o "$O/pocket_wf_old" || { echo "FAIL: сборка"; exit 1; }
L="$D/L7/level.shr"; H="$D/L7/player.shr"; bad=0
for hd in 180 0 90; do
  nf=$(RA_DBG=1 "$O/pocket_wf" "$L" "$H" resetat -110.5 486.4 $hd 8); of=$(RA_DBG=1 "$O/pocket_wf_old" "$L" "$H" resetat -110.5 486.4 $hd 8)
  na=$(echo "$nf" | sed -n 's/.*впереди \([0-9.]*\) м, позади.*/\1/p'); oa=$(echo "$of" | sed -n 's/.*впереди \([0-9.]*\) м, позади.*/\1/p')
  echo "F12-9 курс $hd: новое: $(echo "$nf" | grep RESETAT | cut -c1-95), впереди $na м | прежнее: $(echo "$of" | grep RESETAT | cut -c1-95), впереди $oa м"
  awk "BEGIN{exit !($na >= 6)}" || { echo "FAIL: новое смотрит в стену ближе 6 м (курс $hd): $na"; bad=1; }
  awk "BEGIN{exit !($oa < 6)}" || { echo "FAIL: красный контроль — прежнее не смотрит в стену (курс $hd): $oa"; bad=1; }
done
for p in "-7.5 516.8 90" "224 -498 0" "100 -300 45"; do set -- $p; n=$("$O/pocket_wf" "$L" "$H" resetat $1 $2 $3 8 | head -1); o=$("$O/pocket_wf_old" "$L" "$H" resetat $1 $2 $3 8 | head -1)
  [ "$n" = "$o" ] && echo "открытая дорога ($p): одинаково" || { echo "FAIL: открытая дорога ($p) разно: $n | $o"; bad=1; }; done
rm -f "$O/pocket_wf" "$O/pocket_wf_old"
bash tests/pocket1248/gate.sh >/dev/null 2>&1 && echo "двор G5-2 (gate.sh): OK" || { echo "FAIL: gate.sh двора G5-2 не прошёл"; bad=1; }
[ $bad = 0 ] && echo "ГЕЙТ OK" || { echo "ГЕЙТ FAIL"; exit 1; }
