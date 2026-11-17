#!/bin/bash
# 108: сравнение вариантов подвески/центра масс по ВСЕМ записям PS2 с колёсами (10 заездов А1/jz) и склону: СКО хода колёс (обычное и с перестановкой пар), СКО склона, twin вне допуска
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; V=viewer; OUT=build/c109/var; mkdir -p $OUT
LV=${LV:-$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15/level.shr}
HOST="$V/car.c $V/col.c $V/props.c $V/stateprops.c $V/coins.c"
name=$1; shift
mkdir -p $OUT/$name
cc -std=c11 -O2 -w -DSKIN_HOST -I $V "$@" tests/jumps/twin.c $HOST -lm -Wl,-undefined,dynamic_lookup -o $OUT/$name/twin || { echo "FAIL сборка $name"; exit 1; }
$OUT/$name/twin "$LV" --trace $OUT/$name > $OUT/$name/twin.txt 2>&1
echo "== $name [$*] twin вне допуска: $(LC_ALL=C grep -a -c ' FAIL' $OUT/$name/twin.txt)"
echo -n " колёса: "; A1=1 python3 tests/car107b/wheelcmp.py $OUT/$name | head -1 | cut -c1-60; echo -n " колёса (пары переставлены): "; SWAP=1 A1=1 python3 tests/car107b/wheelcmp.py $OUT/$name | head -1 | cut -c1-60
echo -n " склон: "; python3 tests/car107b/slopecmp.py $OUT/$name | tail -2 | tr '\n' ' ' | cut -c1-200; echo
python3 tests/jumps/pitchcmp.py $OUT/$name | head -2 | cut -c1-200
