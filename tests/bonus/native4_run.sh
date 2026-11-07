#!/bin/bash
# №1179/1182/1183: снимки родных экранов бонус-игры на E, F, G, S. Набор — build/g09native (ссылки на набор TEST + метки языков); в наборах integ ничего не пишется.
#   tests/bonus/native4_run.sh EBOOT.PBP ВЫХОД_ПАПКА [языки]   -> ВЫХОД/{E,F,G,S}/*.png
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
EB="$1"; OUT="$2"; LANGS="${3:-E F G S}"; SET="${SET:-build/g09native}"
rm -f "$SET/EBOOT.PBP"; cp "$EB" "$SET/EBOOT.PBP"; mkdir -p "$OUT"
for L in $LANGS; do
  fl=""; ex=""
  if [ $L != E ]; then fl="lang_$(echo $L | tr A-Z a-z)"; ex="lang/$L/bonuslabels.txt lang/$L/bonus/mini.shr lang/$L/ui.shr lang/$L/pmenu.shr lang/$L/rowkeys.txt lang/$L/prompts.txt lang/$L/menu.shr lang/$L/pextra.shr"; fi
  SMOKE_HEADLESS=1 SMOKE_FLAGS="$fl" SMOKE_EXTRA="$ex" SMOKE_SCN="${SCN:-tests/bonus/native4_scenario.txt}" SMOKE_MAXT=900 tools/smoke.sh "$SET" > "$OUT/smoke_$L.txt" 2>&1
  d=$(grep -a "^отчёт:" "$OUT/smoke_$L.txt" | sed "s/^отчёт: //; s#/summary.txt.*##")   # папка этого прогона из его же вывода (параллельные прогоны не путаются); mkdir -p "$OUT/$L"; cp "$d"/*.png "$OUT/$L/" 2>/dev/null; cp "$d/smoke.txt" "$OUT/$L/smoke.txt" 2>/dev/null
  echo "$L: $(tail -3 "$OUT/smoke_$L.txt" | head -1 | cut -c1-80) -> $d"
done
