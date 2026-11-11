#!/bin/bash
# Гейт правки 1241: красный -DOLDCARHIT1241_OLD=1 обязан упасть на A и B, потом зелёный. Данных игры не нужно.
set -u; cd "$(dirname "$0")/../.."; mkdir -p build/oldcarhit
export DEVELOPER_DIR=/Library/Developer/CommandLineTools
V=viewer; SRC="tests/oldcarhit/host.c $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c $V/watchdog.c"
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -Wno-stringop-truncation -DSKIN_HOST -I $V"
$CC -DOLDCARHIT1241_OLD=1 $SRC -lm -o build/oldcarhit/red || exit 2
$CC $SRC -lm -o build/oldcarhit/green || exit 2
build/oldcarhit/red > build/oldcarhit/red.log; r=$?
build/oldcarhit/green > build/oldcarhit/green.log; g=$?
cat build/oldcarhit/red.log | sed 's/^/  красный: /'; cat build/oldcarhit/green.log | sed 's/^/  зелёный: /'
[ $r = 1 ] && grep -q "FAIL A:" build/oldcarhit/red.log && grep -q "FAIL A2:" build/oldcarhit/red.log && grep -q "FAIL B:" build/oldcarhit/red.log || { echo "КРАСНЫЙ КОНТРОЛЬ СЛЕП"; exit 1; }
[ $g = 0 ] || { echo "ЗЕЛЁНЫЙ ПАДАЕТ"; exit 1; }
echo "PASS oldcarhit (красный падает на A и B, зелёный чист)"
