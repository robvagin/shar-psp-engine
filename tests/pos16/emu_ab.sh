#!/bin/bash
# Модели-96, s16: сравнение снимков из эмулятора. Два набора данных (символьные ссылки на набор тура + подменённые модели «до» и «после»), одинаковый EBOOT
# (новый: читает оба вида), сценарий в обоих как scenario.txt. Безголовый PPSSPP (программная отрисовка 480x272, воспроизводится побитово), nice 20.
#   tests/pos16/emu_ab.sh SET_OLD SET_NEW OUT_DIR [--bot 0.948 | другие ключи shotdiff.py]     (в SET: EBOOT.PBP, scenario.txt, smokelog рядом с данными)
# Итог: OUT/old, OUT/new (снимки .png), печать tests/pos16/shotdiff.py. Красный контроль: добавить --shift 1 к shotdiff (должен упасть).
set -u
cd "$(dirname "$0")/../.."; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
OLD=$1; NEW=$2; OUT=$3; shift 3
for V in old new; do
  D=$([ $V = old ] && echo "$OLD" || echo "$NEW"); rm -rf "$OUT/$V" "$OUT/raw_$V"; mkdir -p "$OUT/$V" "$OUT/raw_$V"; rm -f "$D/smoke.txt"
  nice -n 20 python3 tools/smoke_headless.py "$D" "$OUT/raw_$V" 500 > "$OUT/$V.log" 2>&1
  cp "$OUT/raw_$V"/*.png "$OUT/$V/"
done
.venv/bin/python tests/pos16/shotdiff.py "$OUT/old" "$OUT/new" --top 0 "$@"
