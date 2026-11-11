#!/bin/bash
# №1248в (Кольцо-96): allrisk 98 флагует «RESET возвращает в ловушку» L7 G5-3. Разбор: RESET из двора выносит на 28,4 м к устью (x -13), бот снова едет в тот же двор за целью m5 (-48,-293, y 32: у Бернса, пешком), и две посадки подряд рядом считались «ловушкой».
# Счёт теперь: «в то же место» = сдвиг от места застревания < 8 м (ARRESET_OLD=1 — прежний счёт).
#   tests/pocket1248/allrisk_reset_gate.sh [DIR с L7/level.shr, L7/player.shr, L7/level07.msn, sandbox/roads-l7.bin]
# красное: прежний счёт флагует G5-3; зелёное: новый не флагует G5-3 и флагует F12-9 (там RESET честно возвращает на 0,3 м — контроль, что детектор не ослеп).
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}; O=${OUT_DIR:-build/bots}; mkdir -p "$O"
D=${1:-$HOME/Downloads/shar-psp-opora93mech98/build/in98}; V=viewer; . tools/bots/lib.sh
CF="-std=c11 -O2 -w -DSKIN_HOST -I $V -I tools/bots"
cc $CF tools/bots/allrisk.c $BOTS_LIB $V/camsafe.c $V/carzone.c -lm -o "$O/allrisk_new" && cc $CF -DARRESET_OLD=1 tools/bots/allrisk.c $BOTS_LIB $V/camsafe.c $V/carzone.c -lm -o "$O/allrisk_old" || { echo "FAIL: сборка"; exit 1; }
run() { BW_LEVEL=7 nice -n 20 "$1" "$D/L7/level.shr" "$D/L7/player.shr" "$D/sandbox/roads-l7.bin" "$D/L7/level07.msn" --mode progress --seed 1 --secs 3600 2>/dev/null | grep "^CASE.pocket_trap" | awk -F'\t' '{print $6}' | sort | tr '\n' ' '; }
o=$(run "$O/allrisk_old"); n=$(run "$O/allrisk_new"); echo "прежний счёт, места: $o"; echo "новый счёт, места:   $n"; bad=0
case "$o" in *G5-3*) ;; *) echo "FAIL: красный контроль — прежний не флагует G5-3"; bad=1;; esac
case "$n" in *G5-3*) echo "FAIL: новый всё ещё флагует G5-3"; bad=1;; esac
case "$n" in *F12-9*) ;; *) echo "FAIL: новый ослеп — не флагует F12-9 (RESET на 0,3 м)"; bad=1;; esac
rm -f "$O/allrisk_new" "$O/allrisk_old"
[ $bad = 0 ] && echo "ГЕЙТ OK" || { echo "ГЕЙТ FAIL"; exit 1; }
