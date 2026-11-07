#!/bin/bash
# GitHub #4: снимки запасных экранов бонус-гонки на E, F, G, S. Набор — папка из ссылок (build/g09bonus) без bonus/mini.shr; build/bonus/mini.shr копии на время прогона убирается.
#   tests/bonus/lang4_run.sh EBOOT.PBP ВЫХОД_ПАПКА   -> ВЫХОД/{E,F,G,S}/*.png
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
EB="$1"; OUT="$2"; SET="${SET:-build/g09bonus}"
cp "$EB" "$SET/EBOOT.PBP"; mkdir -p "$OUT"
for L in E F G S; do
  fl=""; ex=""
  if [ $L != E ]; then fl="lang_$(echo $L | tr A-Z a-z)"; ex="lang/$L/bonuslabels.txt lang/$L/ui.shr lang/$L/pmenu.shr lang/$L/rowkeys.txt lang/$L/prompts.txt lang/$L/menu.shr lang/$L/pextra.shr"; fi
  SMOKE_HEADLESS=1 SMOKE_FLAGS="$fl" SMOKE_EXTRA="$ex" SMOKE_SCN=tests/bonus/lang4_scenario.txt SMOKE_MAXT=600 tools/smoke.sh "$SET" > "$OUT/smoke_$L.txt" 2>&1
  d=$(grep -a "^отчёт:" "$OUT/smoke_$L.txt" | sed "s/^отчёт: //; s#/summary.txt.*##")   # папка этого прогона из его же вывода (параллельные прогоны не путаются); mkdir -p "$OUT/$L"; cp "$d"/*.png "$OUT/$L/" 2>/dev/null; cp "$d/smoke.txt" "$OUT/$L/smoke.txt" 2>/dev/null
  echo "$L: $(tail -3 "$OUT/smoke_$L.txt" | head -1 | cut -c1-80) -> $d"
done
