#!/bin/bash
# Измерение (не гейт): Смитерс m7 против игрока по настоящим дорогам L1, сравнение с радаром PS2 (tools/research/race10/rival_radar.py 2393 132 OUT.tsv).
#   tests/aicar/racereal.sh [Vp м/с=24] [IN98_L1=~/Downloads/shar-psp-opora93mech98/build/in98/L1] [RADAR.tsv]
cd "$(dirname "$0")/../.."
OUT=build/aicar; mkdir -p "$OUT"; V=viewer; VP=${1:-24}; L=${2:-$HOME/Downloads/shar-psp-opora93mech98/build/in98/L1}; R=${3:-$OUT/rival_m7.tsv}
[ -f "$R" ] || nice -n 20 python3 tools/research/race10/rival_radar.py 2393 132 "$R" > /dev/null
cc -std=c11 -O2 -Wno-unused-function -DSKIN_HOST -I $V tests/aicar/racereal_host.c $V/msncar.c $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c $V/watchdog.c -lm -Wl,-U,_uiFlash -o "$OUT/racereal" || exit 1
nice -n 20 "$OUT/racereal" "$L/level.shr" "$L/traffic.shr" "$R" "$VP" "$OUT/racereal.tsv"
