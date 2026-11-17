#!/bin/bash
# Даль-1272: stream() раз в 2 кадра против каждого кадра (файл streamhalfold).  tests/dal1272/run_half.sh EBOOT [SPEED=45] [SLOWIO=700]
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; EB=${1:?EBOOT}; SP=${2:-45}; IO=${3:-700}
OUT=${OUT:-$W/build/dal1272h}; export OUT; mkdir -p "$OUT"; cd "$W"; export SMOKE_HEADLESS=1
for v in new old; do F="flypath=$SP slowio=$IO cellthrashold"; [ $v = old ] && F="$F streamhalfold"
  ( FLAGS="$F" nice -n 20 tools/emu_run.sh "$H/scene.txt" h_$v "$EB" 600 > "$OUT/h_$v.log" 2>&1 ) & done; wait
python3 -I "$H/check_half.py" "$OUT/out_h_new/smoke.txt" "$OUT/out_h_old/smoke.txt" | tee "$OUT/half_gate.txt"; rc=${PIPESTATUS[0]}
find "$OUT" -maxdepth 2 \( -name smoke.txt -o -name blackbox.txt -o -name headless.log \) -delete; rm -rf "$OUT"/emu_home_h_* "$OUT"/rr_h_*; exit $rc
