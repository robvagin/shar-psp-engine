#!/bin/sh
# №1162: мир «мультик» уровня 1 (SHR_TOON=1, №679) и его линии контура: tools/toon_make.sh ПАПКА -> ПАПКА/level_toon.shr и ПАПКА/toonlines.shr.
# Тот же рецепт, что у level.shr (tools/build_level.sh: PVS, обход, порядок секций, значения по умолчанию №1162), только с SHR_TOON=1; линии пишет сам psp_export
# рядом с уровнем (<уровень>.toonlines). Публичная сборка раньше эти два файла пропускала (правила «true»), и режим Cartoon не получал ни мира, ни контура
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
OUT="$1"; mkdir -p "$OUT"
SHR_TOON=1 LEVEL="${LEVEL:-1}" tools/build_level.sh "$OUT/level_toon.shr" > "$OUT/toon_build.log" 2>&1 || { tail -5 "$OUT/toon_build.log"; exit 1; }
[ -s "$OUT/level_toon.shr.toonlines" ] || { echo "toon_make: нет линий контура (level_toon.shr.toonlines)"; exit 1; }
mv "$OUT/level_toon.shr.toonlines" "$OUT/toonlines.shr"
rm -f "$OUT/level_toon.shr.texsrc.json" "$OUT/toon_build.log"
