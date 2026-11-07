#!/bin/sh
# Собрать и запустить tools/colbake.c:  tools/colbake.sh LEVEL.shr OUT.shr  |  LEVEL.shr --check FILE   (на Маке, процессор, без эмулятора)
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; V="$ROOT/viewer"; B="$ROOT/build/colbake"
[ -n "${SDKROOT:-}" ] || ! [ -d /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk ] || export SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
if [ ! -x "$B" ] || [ "$ROOT/tools/colbake.c" -nt "$B" ] || [ "$V/col.c" -nt "$B" ] || [ "$V/col.h" -nt "$B" ] || [ "$V/msncar.c" -nt "$B" ]; then
  cc -std=c11 -O2 -ffp-contract=off -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I "$V" -o "$B" "$ROOT/tools/colbake.c" \
     "$V/msncar.c" "$V/car.c" "$V/col.c" "$V/traffic.c" "$V/homer.c" "$V/skin.c" "$V/mission/msn_file.c" "$V/mission/msn_ops.c" -lm
fi
exec "$B" "$@"
