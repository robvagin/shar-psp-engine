#!/bin/sh
# Собрать и запустить tools/mcp_bake.c:  tools/mcp_bake.sh LEVEL.shr TRAFFIC.shr OUT.shr  (или ... --check FILE)
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; V="$ROOT/viewer"; B="$ROOT/build/mcp_bake"
[ -n "${SDKROOT:-}" ] || ! [ -d /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk ] || export SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk   # №1086: cc без принятой лицензии Xcode берёт заголовки отсюда
if [ ! -x "$B" ] || [ "$ROOT/tools/mcp_bake.c" -nt "$B" ] || [ "$V/msncar.c" -nt "$B" ] || [ "$V/col.c" -nt "$B" ] || [ "$V/car.c" -nt "$B" ] || [ "$V/traffic.c" -nt "$B" ]; then
  cc -std=c11 -O2 -ffp-contract=off -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I "$V" -o "$B" "$ROOT/tools/mcp_bake.c" \
     "$V/msncar.c" "$V/car.c" "$V/col.c" "$V/traffic.c" "$V/homer.c" "$V/skin.c" "$V/mission/msn_file.c" "$V/mission/msn_ops.c" -lm
fi
exec "$B" "$@"
