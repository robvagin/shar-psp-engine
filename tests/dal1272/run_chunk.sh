#!/bin/bash
# Даль-1272: stream() кусками против полного прохода (файл streamchunkold). tests/dal1272/run_chunk.sh EBOOT [SPEED=45] [SLOWIO=700]
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; EB=${1:?EBOOT}; SP=${2:-45}; IO=${3:-700}
OUT=${OUT:-$W/build/dal1272c}; export OUT; mkdir -p "$OUT"; cd "$W"; export SMOKE_HEADLESS=1
for v in new old; do F="flypath=$SP slowio=$IO cellthrashold"; [ $v = old ] && F="$F streamchunkold"
  ( FLAGS="$F" nice -n 20 tools/emu_run.sh "$H/scene.txt" c_$v "$EB" 600 > "$OUT/c_$v.log" 2>&1 ) & done; wait
python3 -I "$H/check_chunk.py" "$OUT/out_c_new/smoke.txt" "$OUT/out_c_old/smoke.txt" | tee "$OUT/chunk_gate.txt"; rc=${PIPESTATUS[0]}
find "$OUT" -maxdepth 2 \( -name smoke.txt -o -name blackbox.txt -o -name headless.log \) -delete; rm -rf "$OUT"/emu_home_c_* "$OUT"/rr_c_*; exit $rc
