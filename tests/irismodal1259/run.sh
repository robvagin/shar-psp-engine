#!/bin/bash
# Будка-93 (1259): экран провала миссии не под закрытым ирисом. tests/irismodal1259/run.sh EBOOT [SET] -> PASS/FAIL (зелёный и красный контроль; эмулятор x2, безголово, ballast 5; уровень L5)
set -u; cd "$(dirname "$0")/../.."; E=${1:?EBOOT}; export SET=${2:-$HOME/Downloads/shar-psp-t93/build/test96tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 SHIP_LEVELS="2 3 4 5 6 7" FLAGS="ballast=5" OUT=${OUT:-$PWD/build/irismodal1259} PATH=$PWD/.venv/bin:$PATH
mkdir -p "$OUT"; rc=0
for m in g r; do fl=""; mode=green; [ $m = r ] && { fl="flag irismodal1259 0\nflag irisstuck1258 0"; mode=red; }
  sed "s/__FLAGS__/$fl/" tests/irismodal1259/scn.txt > "$OUT/s_$m.txt"; printf '%b' "$(cat "$OUT/s_$m.txt")" > /dev/null
  python3 - "$OUT/s_$m.txt" <<'PY'
import sys; p=sys.argv[1]; s=open(p).read().replace('\\n','\n'); open(p,'w').write(s)
PY
  rm -rf "$OUT/out_$m"; nice -n 20 tools/emu_run.sh "$OUT/s_$m.txt" $m "$E" 400 > "$OUT/$m.log" 2>&1
  echo "$m: $(python3 tests/irismodal1259/check.py "$OUT/out_$m" $mode | tr '\n' ' ')"
  python3 tests/irismodal1259/check.py "$OUT/out_$m" $mode >/dev/null || rc=1
  rm -rf "$OUT/out_$m" "$OUT/emu_home_$m"; done
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
