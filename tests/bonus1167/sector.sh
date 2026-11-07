#!/bin/bash
# №1185: гейт кодов секторов трасс бонус-игры (viewer/bnssector.h, tests/bonus1167/sector_host.c) на настоящих данных build/bonus/b0N.bns (или BNS_DIR).
#   tests/bonus1167/sector.sh        зелёный: все 7 трасс; красный (-DBSC_BREAK) обязан упасть
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; export DEVELOPER_DIR="${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}"
BNS="${BNS_DIR:-$HOME/Downloads/shar-psp-integ/build/test87tour/PSP/GAME/SHARTEST15/bonus}"; V=viewer; O=build/bonus1167/bin; mkdir -p $O
SRC="$V/bonus.c $V/msncar.c $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c $V/props.c $V/stateprops.c $V/animobj.c $V/coins.c"
F="-std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V"
cc $F tests/bonus1167/sector_host.c $SRC -lm -Wl,-U,_uiFlash -o $O/sector_host || { echo "FAIL: сборка"; exit 2; }
cc $F -DBSC_BREAK tests/bonus1167/sector_host.c $SRC -lm -Wl,-U,_uiFlash -o $O/sector_host_red || { echo "FAIL: красная сборка"; exit 2; }
FILES=""; for i in 1 2 3 4 5 6 7; do FILES="$FILES $BNS/b0$i.bns"; done
export SEC_DIR="${SEC_DIR:-$ROOT/build/secdata}"; [ -f "$SEC_DIR/b01.sec" ] || tools/bonus/sec_export.sh "$BNS" "$SEC_DIR" >/dev/null
echo "== зелёный"; $O/sector_host $FILES; g=$?
echo "== красный (-DBSC_BREAK)"; $O/sector_host_red $FILES | tail -3; r=${PIPESTATUS[0]}
[ $g -eq 0 ] && [ $r -ne 0 ] && { echo "ИТОГ: PASS (зелёный прошёл, красный пойман)"; exit 0; }
echo "ИТОГ: FAIL (зелёный rc=$g, красный rc=$r — должен быть не 0)"; exit 1
