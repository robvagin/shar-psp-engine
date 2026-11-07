#!/bin/bash
# №1179/1182/1183: лист «после» всех экранов бонус-игры на E, F, G, S (tools/smoke.sh, безголовый эмулятор, без звука; языки параллельно, у каждого свой набор-копия ссылок).
#   tests/bonus/sheet_run.sh EBOOT.PBP ВЫХОД [языки]    набор — build/g09native (ссылки на набор TEST + свои mini.shr и bonuslabels.txt); сценарий SCN (по умолчанию tests/bonus/sheet_after.txt)
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
EB="$1"; OUT="$2"; LANGS="${3:-E F G S}"; BASE="${SET:-build/g09native}"; SCN="${SCN:-tests/bonus/sheet_after.txt}"
mkdir -p "$OUT"
for L in $LANGS; do
  D="build/${RUNP:-g09run}_$L"; rm -rf "$D"; mkdir -p "$D"
  for f in "$BASE"/*; do b=$(basename "$f"); [ "$b" = EBOOT.PBP ] || ln -s "$ROOT/$f" "$D/$b"; done
  cp "$EB" "$D/EBOOT.PBP"
  fl=""; ex=""
  if [ $L != E ]; then fl="lang_$(echo $L | tr A-Z a-z)"; ex="lang/$L/bonuslabels.txt lang/$L/bonus/mini.shr lang/$L/ui.shr lang/$L/pmenu.shr lang/$L/rowkeys.txt lang/$L/prompts.txt lang/$L/menu.shr lang/$L/pextra.shr"; fi
  ( SMOKE_HEADLESS=1 SMOKE_FLAGS="$fl" SMOKE_EXTRA="$ex" SMOKE_SCN="$SCN" SMOKE_MAXT="${TMO:-1200}" tools/smoke.sh "$D" > "$OUT/smoke_$L.txt" 2>&1
    d=$(grep -a "^отчёт:" "$OUT/smoke_$L.txt" | sed "s/^отчёт: //; s#/summary.txt.*##"); mkdir -p "$OUT/$L"; cp "$d"/*.png "$d/smoke.txt" "$OUT/$L/" 2>/dev/null; echo "$L: $(grep -a -c . "$OUT/$L/smoke.txt" 2>/dev/null) строк журнала, снимков $(ls "$OUT/$L"/*.png 2>/dev/null | wc -l) -> $d" > "$OUT/done_$L.txt" ) &
  sleep 2
done
wait; cat "$OUT"/done_*.txt
