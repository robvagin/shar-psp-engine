#!/bin/bash
# Правило ship_rules для notoon.txt одного уровня (tools/notoon_list.py считает все семь за раз, а правило даёт один файл):
#   tools/notoon_rule.sh N LEVEL_SHR OUT [PRIOR]   PRIOR — прежний файл (build/notoon.txt, build/levels/lN/notoon.txt): нет texsrc — прежний остаётся как есть («оставлен»), нет и его — ошибка. texsrc лежит рядом с level.shr (ship.sh кладёт <имя>.texsrc.json); кэш имён текстур NOTOON_CACHE (по умолчанию build/notoon_names.json, при первом запуске строится по p3d диска)
set -eu
N="${1:?номер уровня}"; SHR="${2:?level.shr}"; OUT="${3:?выход}"; PRIOR="${4:-}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; D="$(cd "$(dirname "$SHR")" && pwd)"; TS="$D/$(basename "$SHR").texsrc.json"
if [ ! -s "$TS" ]; then
  if [ -n "$PRIOR" ] && [ -s "$PRIOR" ]; then cp "$PRIOR" "$OUT"; echo "notoon_rule: L$N: нет $TS, notoon.txt оставлен как есть ($PRIOR)"; exit 0; fi
  echo "notoon_rule: L$N: нет $TS (его пишет tools/psp_export.py рядом с level.shr) и нет прежнего notoon.txt"; exit 1
fi
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
mkdir -p "$T/l$N" "$T/out"; ln -s "$D/$(basename "$SHR")" "$T/l$N/level.shr"; ln -s "$TS" "$T/l$N/level.shr.texsrc.json"
python3 "$ROOT/tools/notoon_list.py" --texsrc "$T" --out "$T/out" --cache "${NOTOON_CACHE:-$ROOT/build/notoon_names.json}" >&2
if [ "$N" = 1 ]; then SRC="$T/out/notoon.txt"; else SRC="$T/out/l$N/notoon.txt"; fi
[ -s "$SRC" ] || { echo "notoon_rule: L$N: файла нет (число текстур level.shr и texsrc.json не сошлось?)"; exit 1; }
cp "$SRC" "$OUT"
