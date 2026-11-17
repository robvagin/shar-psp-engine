#!/bin/bash
# Даль-1272: облёт L1 с медленной картой, новое против старого (файл dal1272old).  tests/dal1272/run.sh EBOOT [SPEED=45] [SLOWIO=700]; на 30 и 400 старое в одном прогоне из двух дыр не даёт — красный контроль нестабилен
# Окружение как у tools/emu_run.sh (SET набор, OUT рабочая папка). Результат: $OUT/dal1272_gate.txt, последняя строка PASS/FAIL
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; EB=${1:?EBOOT}; SP=${2:-45}; IO=${3:-700}
OUT=${OUT:-$W/build/dal1272}; export OUT; mkdir -p "$OUT"; cd "$W"; export SMOKE_HEADLESS=1
for v in new old; do F="flypath=$SP slowio=$IO cellthrashold"; [ $v = old ] && F="$F dal1272old"
  ( FLAGS="$F" nice -n 20 tools/emu_run.sh "$H/scene.txt" g_$v "$EB" 600 > "$OUT/g_$v.log" 2>&1 ) & done; wait
python3 -I "$H/check.py" "$OUT/out_g_new/smoke.txt" "$OUT/out_g_old/smoke.txt" | tee "$OUT/dal1272_gate.txt"; rc=${PIPESTATUS[0]}
find "$OUT" -maxdepth 2 \( -name smoke.txt -o -name blackbox.txt -o -name headless.log \) -delete; rm -rf "$OUT"/emu_home_g_* "$OUT"/rr_g_*; exit $rc
