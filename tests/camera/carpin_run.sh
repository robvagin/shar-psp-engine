#!/bin/sh
# Аудит 04.11: камера пешком у своей машины за спиной героя (carpin_host.c). Норма обязана пройти, прежнее (-DCARPIN_OLD=1, как TEST89) обязано упасть.
#   sh tests/camera/carpin_run.sh [level.shr homer.shr]   (по умолчанию — набор TEST88 в shar-psp-integ)
SDK=${SDK:-/Library/Developer/CommandLineTools/SDKs/MacOSX26.sdk}; T=${TMPDIR:-/tmp}; L=${1:-$HOME/Downloads/shar-psp-integ/build/test88/PSP/GAME/SHARTEST15/level.shr}; H=${2:-$HOME/Downloads/shar-psp-integ/build/test88/PSP/GAME/SHARTEST15/homer.shr}; rc=0
build() { cc -isysroot "$SDK" -std=c99 -O2 -w -DSKIN_HOST $1 -I viewer tests/camera/carpin_host.c viewer/homer.c viewer/skin.c viewer/col.c viewer/car.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -o "$2" || exit 2; }
build "" "$T/carpin_n"; build "-DCARPIN_OLD=1" "$T/carpin_o"
"$T/carpin_n" "$L" "$H" >"$T/carpin_n.txt" 2>&1 && echo "норма: PASS (ждали) — $(head -1 "$T/carpin_n.txt")" || { echo "норма: FAIL (ОШИБКА)"; cat "$T/carpin_n.txt"; rc=1; }
"$T/carpin_o" "$L" "$H" >"$T/carpin_o.txt" 2>&1 && { echo "красный CARPIN_OLD=1: ПРОШЁЛ (ОШИБКА: проверка не ловит прежнее)"; rc=1; } || echo "красный CARPIN_OLD=1: упал (ждали) — $(head -1 "$T/carpin_o.txt")"
exit $rc
