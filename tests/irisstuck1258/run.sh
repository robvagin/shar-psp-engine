#!/bin/bash
# Будка-93 (1258): сторож вечного ириса. tests/irisstuck1258/run.sh EBOOT [SET]  -> PASS/FAIL (зелёный и красный контроль, эмулятор ×2, безголово, ballast 5)
set -u; cd "$(dirname "$0")/../.."; E=${1:?EBOOT}; export SET=${2:-$HOME/Downloads/shar-psp-t93/build/test96tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 SHIP_LEVELS="2 3 4 5 6 7" FLAGS="ballast=5" OUT=${OUT:-$PWD/build/irisstuck1258} PATH=$PWD/.venv/bin:$PATH
mkdir -p "$OUT"; rc=0
for m in g r; do sw=1; [ $m = r ] && sw=0; sed "s/SW/$sw/" tests/irisstuck1258/scn.txt > "$OUT/s_$m.txt"; rm -rf "$OUT/out_$m"
  nice -n 20 tools/emu_run.sh "$OUT/s_$m.txt" $m "$E" 300 > "$OUT/$m.log" 2>&1
  mode=green; [ $m = r ] && mode=red; echo "$m: $(python3 tests/irisstuck1258/check.py "$OUT/out_$m/smoke.txt" $mode | tail -2 | tr '\n' ' ')"
  python3 tests/irisstuck1258/check.py "$OUT/out_$m/smoke.txt" $mode >/dev/null || rc=1
  rm -rf "$OUT/out_$m" "$OUT/emu_home_$m"; done
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
