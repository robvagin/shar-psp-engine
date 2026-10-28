#!/bin/bash
# №1061: пакеты машин с правилом колёс: traffic.shr (флаги качества, как tools/final-build.sh), msncars-lN.shr (БЕЗ флагов качества, как final-build.sh и tests/booth1056/accept60.sh: с флагами пакеты расходились с набором на +230 КБ)
# и боковые файлы carflat930/carflatm930, уровни 1..7 -> OUT/ (раскладка набора: l1 в корне, lN/ для N>=2). Старое: TIRE1061_OLD=1 в окружении
cd "$(dirname "$0")/.." || exit 1
OUT=${1:?OUT}; mkdir -p "$OUT"
for L in 1 2 3 4 5 6 7; do
  D=$OUT; [ $L = 1 ] || D=$OUT/l$L; mkdir -p "$D"
  ( SHR_TEX_CAP256=1 SHR_SMALL_MIPS=1 SHR_LAYERS=1 .venv/bin/python tools/traffic_export.py --level $L --out "$D/traffic.shr" 2>&1 | grep -E "КБ —|Traceback|Error"
    .venv/bin/python tools/msncar_export.py --level $L --out "$D/msncars-l$L.shr" 2>&1 | grep -E "Traceback|Error"
    .venv/bin/python tools/car_flat.py "$D/traffic.shr" --sidecar "$D/carflat930.bin" | tail -1
    .venv/bin/python tools/car_flat.py "$D/msncars-l$L.shr" --sidecar "$D/carflatm930.bin" | tail -1 ) > "$OUT/log_$L.txt" 2>&1 &
done; wait; cat "$OUT"/log_*.txt
