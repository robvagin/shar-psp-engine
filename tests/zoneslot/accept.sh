#!/bin/bash
# TEST91 1А шаг 1: хост-тест постоянного слота. Зелёный: пакет следующего уровня по тем же адресам. Красный (-DZS_NOREUSE, слот не освобождается): обязан упасть.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/zoneslot; mkdir -p "$OUT"
export DEVELOPER_DIR=/Library/Developer/CommandLineTools PATH=/Library/Developer/CommandLineTools/usr/bin:$PATH SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
CC="cc -std=c11 -O1 -w"
$CC tests/zoneslot/host.c -o $OUT/green 2>$OUT/green.build || { echo "FAIL: сборка"; head -3 $OUT/green.build; exit 1; }
$CC -DZS_NOREUSE tests/zoneslot/host.c -o $OUT/red 2>$OUT/red.build || { echo "FAIL: сборка красного"; exit 1; }
g=$($OUT/green); gr=$?; r=$($OUT/red); rr=$?
echo "зелёный: $g (код $gr); красный: $(echo "$r" | head -2 | tr '\n' ' ') (код $rr)"
[ $gr -eq 0 ] || { echo "FAIL: зелёный не зелёный"; exit 1; }
[ $rr -ne 0 ] || { echo "FAIL: красный не упал (контроль слепой)"; exit 1; }
grep -q 'zsAlloc(&zsUi' viewer/ui.c && grep -q 'pakKind = 1; int r = pakRead' viewer/ui.c && grep -q 'zsFree(&zsUi, p)' viewer/ui.c || { echo "FAIL: ui.c не подключён к слотам"; exit 1; }
echo PASS
