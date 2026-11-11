#!/bin/bash
# №1248е (Кольцо-96): L7 G5-2, «машину тормозит и застреваешь из-за решётки/барабана на полу» — четыре стейтпропа типа 76 (cls 2, два цилиндра r 0,97/0,36) в чаше двора (−0,19 м) в начальном состоянии ничего не рисуют (vis = 0), но твёрдые.
# Закон 1223: твёрдость по тому же условию, что рисунок. SPINVIS1248_OLD=1 — прежнее. Хост-стенд tools/bots/pocket.c, режим thru.
#   tests/pocket1248/invis_gate.sh [DIR с homer.shr и l7/level.shr]
# красное: Build 97 (оба OLD: -DSPINVIS1248_OLD=1 -DBUSHSOFT1248_OLD=1) встаёт в чаше, v у точки 0,0; только SPINVIS_OLD — замедляет до 3,5; зелёное: новое проезжает чашу в обе стороны без замедления (v у точки >= 8 при заезде за 4 м).
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}; D=${1:-$HOME/Downloads/shar-psp-t93/build/test96tour/PSP/GAME/SHARTEST15}; O=${OUT_DIR:-build/bots}; mkdir -p "$O"
V=viewer; . tools/bots/lib.sh; CF="-std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots"
cc $CF tools/bots/pocket.c $BOTS_LIB -lm -o "$O/pk_n" && cc $CF -DSPINVIS1248_OLD=1 tools/bots/pocket.c $BOTS_LIB -lm -o "$O/pk_i" && cc $CF -DSPINVIS1248_OLD=1 -DBUSHSOFT1248_OLD=1 tools/bots/pocket.c $BOTS_LIB -lm -o "$O/pk_97" || { echo "FAIL: сборка"; exit 1; }
L="$D/l7/level.shr"; H="$D/homer.shr"; bad=0
t() { "$O/$1" "$L" "$H" thru -32.5 "$2" "$3" 4 8 2>&1 | head -1; }
vp() { echo "$1" | sed -E 's/.*v у точки ([0-9.]+),.*/\1/'; }
for z in -297.2 -299.5; do for h in 270 90; do
  n=$(t pk_n $z $h); i=$(t pk_i $z $h); o=$(t pk_97 $z $h); vn=$(vp "$n"); vi=$(vp "$i")
  echo "z $z курс $h: новое v ${vn} | только SPINVIS_OLD v ${vi} | Build 97: ${o:0:75}"
  case "$n" in *"ПРОШЛА"*) ;; *) echo "FAIL: новое не проехало"; bad=1;; esac
  awk "BEGIN{exit !($vn >= 8)}" || { echo "FAIL: новое замедляется в чаше (v $vn)"; bad=1; }
  awk "BEGIN{exit !($vi <= 3.6)}" || { echo "FAIL: красный контроль — с невидимыми цилиндрами не замедляет (v $vi)"; bad=1; }
  case "$o" in *"НЕ ПРОШЛА"*) ;; *) echo "FAIL: красный контроль — Build 97 проехал"; bad=1;; esac
done; done
n=$("$O/pk_n" "$L" "$H" invisp 2>&1 | tail -1); echo "семейство (L7): $n"
rm -f "$O/pk_n" "$O/pk_i" "$O/pk_97"
[ $bad = 0 ] && echo "ГЕЙТ OK" || { echo "ГЕЙТ FAIL"; exit 1; }
