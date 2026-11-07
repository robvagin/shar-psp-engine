#!/bin/bash
# TEST90 MEMMAP: таблица живых выделений против эталона. Зелёный: число живых и килобайты по классам совпадают на 300 тысячах операций. Красный (-DMM_NODEL, удаления не учитываются): обязан упасть.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/memmap; mkdir -p "$OUT"
export DEVELOPER_DIR=/Library/Developer/CommandLineTools PATH=/Library/Developer/CommandLineTools/usr/bin:$PATH SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
CC="cc -std=c11 -O1 -w"
$CC tests/memmap/host.c -o $OUT/green 2>$OUT/g.build || { echo "FAIL: сборка"; head -3 $OUT/g.build; exit 1; }
$CC -DMM_NODEL tests/memmap/host.c -o $OUT/red 2>$OUT/r.build || { echo "FAIL: сборка красного"; exit 1; }
g=$($OUT/green); gr=$?; r=$($OUT/red); rr=$?
echo "зелёный: $(echo "$g" | tail -1) (код $gr); красный: $(echo "$r" | grep FAIL | head -1) (код $rr)"
[ $gr -eq 0 ] || { echo "$g" | tail -4; echo "FAIL: зелёный не зелёный"; exit 1; }
[ $rr -ne 0 ] || { echo "FAIL: красный не упал (контроль слепой)"; exit 1; }
grep -q 'mmAdd(' viewer/memwatch.c && grep -q 'mmDel(' viewer/memwatch.c && grep -q -- '--wrap=free' viewer/CMakeLists.txt || { echo "FAIL: memmap не подключён к memwatch.c / free не обёрнут"; exit 1; }
echo PASS
