#!/bin/bash
# №1175: барьеры с вызовом BREAK (donotenter, gatesmasha, переезд, туннельный забор) при ударе машины дают частицы обломка (родная система fx.shr), на всех уровнях, где они есть.
# Зелёный: на каждом уровне все такие типы с родными частицами. Красный (обязан упасть): -DBARRIERFX1175_OLD=1 — частиц обломка нет (был звук и 6 щепок пресета).
#   tests/barrierfx1175/accept.sh [DIR_С_УРОВНЯМИ]   (по умолчанию набор test87tour/test86tour; level.shr, l2..l7/level.shr)
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/barrierfx1175; mkdir -p "$OUT"
SET="${1:-$HOME/Downloads/shar-psp-integ/build/test86tour/PSP/GAME/SHARTEST15}"
CC="cc -std=c11 -O1 -w -I viewer"; SRC="tests/barrierfx1175/host.c viewer/props.c viewer/stateprops.c viewer/col.c viewer/car.c viewer/coins.c"
b() { $CC "$@" $SRC -lm -o "$OUT/$NAME" 2> "$OUT/$NAME.build" || { echo "FAIL: сборка $NAME"; head -5 "$OUT/$NAME.build"; exit 1; }; }
NAME=green; b; NAME=old; b -DBARRIERFX1175_OLD=1
run() { for L in 1 2 3 4 5 6 7; do f=$SET/level.shr; [ $L -gt 1 ] && f=$SET/l$L/level.shr; [ -f "$f" ] || continue; echo "L$L"; "$OUT/$1" "$f"; done; }
run green > "$OUT/green.txt"; run old > "$OUT/old.txt"
tot() { awk '/^TYPES/{t+=$2} /^WITHFX/{w+=$2} END{print t+0, w+0}' "$OUT/$1.txt"; }
read gt gw < <(tot green); read ot ow < <(tot old)
echo "зелёный: типов барьеров $gt, с родными частицами $gw; красный BARRIERFX1175_OLD=1: типов $ot, с частицами $ow"
grep -h BARRIER "$OUT/green.txt" | head -12
fail=0
[ "$gt" -gt 0 ] && [ "$gw" = "$gt" ] || { echo "FAIL зелёный: не у всех барьеров частицы обломка"; fail=1; }
[ "$ow" -lt "$ot" ] && echo "красный упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
