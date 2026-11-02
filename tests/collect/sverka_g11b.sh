#!/bin/bash
# Гейт сверки g11b на Маке: осы (W1–W4). Зелёный — настоящий код; красные контроли — прежнее поведение обязано упасть.
#   tests/collect/sverka_g11b.sh [PACK]   по умолчанию build/collect-l1.shr
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
PACK="${1:-build/collect-l1.shr}"; OUT=build/collect; mkdir -p "$OUT"; V=viewer
. tools/bots/lib.sh
b() { cc -std=c11 -O2 -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V -I tools/bots "$@" tests/collect/sverka_g11b.c $BOTS_LIB -lm -o "$OUT/sverka_$N" 2> "$OUT/sverka_$N.build.txt" || { echo "FAIL: сборка $N"; head -5 "$OUT/sverka_$N.build.txt"; exit 1; }; }
N=green; b
N=red_aggr; b -DWASP_AGGR_OLD=1
N=red_kmh; b -DWASP_BOLT_KMH_OLD=1
"$OUT/sverka_green" "$PACK" | tee "$OUT/sverka_green.txt"; g=${PIPESTATUS[0]}
"$OUT/sverka_red_aggr" "$PACK" > "$OUT/sverka_red_aggr.txt"; ra=$?
"$OUT/sverka_red_kmh" "$PACK" > "$OUT/sverka_red_kmh.txt"; rk=$?
echo "зелёный rc=$g, красный WASP_AGGR_OLD rc=$ra, красный WASP_BOLT_KMH_OLD rc=$rk"
[ "$g" = 0 ] && [ "$ra" = 1 ] && [ "$rk" = 1 ] && echo "PASS: зелёный проходит, оба красных краснеют" || { echo "FAIL"; exit 1; }
# круг 5 (P11-Д5): рамка забора в луче осы
cc -std=c11 -O1 -DSKIN_HOST -I $V tests/collect/fencebox_g11b.c $V/col.c -lm -o "$OUT/fencebox" 2> "$OUT/fencebox.build.txt" && "$OUT/fencebox" || { echo "FAIL fencebox"; exit 1; }
