#!/bin/bash
# Будка-93 (1257): ожидание гэгов комнаты не залипает после отмены чтения. tests/gaghold1257/host.sh [collect-l1.shr]; красный: GAGHOLD_RED=1
set -eu; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
F="${1:-$HOME/Downloads/shar-psp-t93/build/test96tour/PSP/GAME/SHARTEST15/collect-l1.shr}"; D="${TMPDIR:-/tmp}/gh1257.$$"; mkdir -p "$D"
LIB="viewer/coll_stream.c viewer/gag_skin.c viewer/collect.c viewer/stateprops.c viewer/skin.c viewer/homer.c viewer/col.c viewer/car.c viewer/coins.c viewer/ticket.c viewer/props.c viewer/watchdog.c"
cc -std=c11 -O1 -g -DSKIN_HOST -Iviewer tests/gaghold1257/host.c $LIB -lm -Wl,-U,_uiFlash -o "$D/new"
cc -std=c11 -O1 -g -DSKIN_HOST -DGAGPEND1257_OLD=1 -Iviewer tests/gaghold1257/host.c $LIB -lm -Wl,-U,_uiFlash -o "$D/old"
echo "новое:"; "$D/new" "$F" | tail -4 || { rm -rf "$D"; echo FAIL; exit 1; }
echo "красный (GAGPEND1257_OLD=1), обязан упасть:"; if "$D/old" "$F" | tail -3; then :; fi
"$D/old" "$F" >/dev/null && { echo "КРАСНЫЙ НЕ ПОЙМАН"; rm -rf "$D"; exit 1; } || echo "упал, как должен"
rm -rf "$D"; echo PASS
