#!/bin/bash
# Гейт 99 «гэг Обсерватории» (L3). tests/gag99/run.sh EBOOT. Набор SET (как у tools/emu_run.sh), безголово, ballast 5. Зелёные: place 3072 КБ (как есть) и урезанное 2816 КБ (проливов нет, GAGPLAN > 0); красный: 2816 + файл gagplan99old.
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; EB=${1:?EBOOT}; OUT=${OUT:-$W/build/gag99}; export OUT; mkdir -p "$OUT"; cd "$W"
export SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SHIP_LEVELS="2 3 4 5 6 7"; rc=0
one() { local n=$1 fl=$2 ck=$3 ex=$4 o=$OUT/out_$1
  FLAGS="$fl" nice -n 20 tools/emu_run.sh "$H/scene.txt" "$n" "$EB" 400 > "$OUT/$n.log" 2>&1
  grep -E "^(GAGPLAN|WATCHDOG|ROOM ev=enter)" "$o/smoke.txt" > "$o/key.txt"; python3 -I "$H/check.py" "$o/key.txt" $ck > "$OUT/gag99_$n.txt" 2>&1; local c=$?
  find "$o" -type f \( -name smoke.txt -o -name blackbox.txt -o -name headless.log -o -name '*.bmp' \) -delete; rm -rf "$OUT/emu_home_$n"
  echo "$n: $(tail -1 "$OUT/gag99_$n.txt") (ожидается $([ $ex = G ] && echo PASS || echo FAIL))"
  if [ $ex = G ] && [ $c != 0 ]; then rc=1; fi; if [ $ex = R ] && [ $c = 0 ]; then echo "  красный контроль НЕ покраснел"; rc=1; fi; }
one g3072 "" "" G
one g2816 "zone3sz=3072_2816_7168" --forced G
one r2816 "zone3sz=3072_2816_7168 gagplan99old" --forced R
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
