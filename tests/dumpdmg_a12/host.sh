#!/bin/bash
# A1.2: tests/dumpdmg_a12/host.sh [папка набора с level01.msn и l2..l7/level0N.msn]; красный: DUMPDMG_RED=1
set -eu; cd "$(dirname "$0")/../.."
S="${1:-$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15}"; D="${TMPDIR:-/tmp}/dmga12.$$"; mkdir -p "$D"
RED=""; [ "${DUMPDMG_RED:-0}" = 1 ] && RED="-DGAME_A12_DUMPDMG_OLD=1"
cc -std=c11 -O1 -g $RED -Iviewer/mission tests/dumpdmg_a12/host.c viewer/mission/msn_file.c viewer/mission/msn_ops.c -lm -Wl,-undefined,dynamic_lookup -o "$D/check" 2>&1 | grep -E "error" || true
f=(); for n in 1 2 3 4 5 6 7; do p="$S/l$n/level0$n.msn"; [ $n = 1 ] && p="$S/level01.msn"; f+=("$p"); done
set +e; "$D/check" "${f[@]}"; r=$?; rm -rf "${D:?}"; exit $r
