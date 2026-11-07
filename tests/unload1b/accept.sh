#!/bin/bash
# TEST90 шаг 1В (№ архитектура памяти, §1.4 1В): уровень выгружается ДО газеты и музыки загрузки, гостиная меню читается после выгрузки, PVS L1/L4 лежит в участке.
# Зелёный (новое по умолчанию): в журнале UNLOADFIRST site=levelup на каждой смене, LEVEL ev=free раньше LOADNEWS, UNLOADFIRST site=fe и нет FERETRY841 (куча «раздроблена» flag fragmenu 4).
# Красный (обязан упасть): флаги-файлы unloadfirstold feunloadfirstold (= TEST89): ни одного UNLOADFIRST, LOADNEWS раньше ev=free. (FERETRY841 в эмуляторе не воспроизводится: куча 29 МБ чистая даже с fragmenu; на PSP это отказ 5,2 МБ — строка информативная, не условие.)
#   tests/unload1b/accept.sh [EBOOT] [НАБОР_ТУРА]   (нужны SHIP_LEVELS-уровни в наборе; по умолчанию набор test88tour)
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
EB="${1:-viewer/build_t90a/EBOOT.PBP}"; SET="${2:-$HOME/Downloads/shar-psp-integ/build/test88tour/PSP/GAME/SHARTEST15}"
export DEVELOPER_DIR=/Library/Developer/CommandLineTools
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
for f in "$SET"/*; do b=$(basename "$f"); case $b in EBOOT.PBP|scenario.txt) ;; *) ln -s "$f" "$T/$b";; esac; done; cp "$EB" "$T/EBOOT.PBP"
run() {  # $1 метка, $2 флаги-файлы
  local out; out=$(SHIP_LEVELS="2 3 4 5 6 7" SMOKE_FLAGS="$2" SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_SCN=tests/unload1b/scenario.txt SMOKE_MAXT=900 tools/smoke.sh "$T" 2>&1 | grep -o 'build/smoke/[0-9-]*' | head -1)
  [ -n "$out" ] || { echo "FAIL: смоук $1 не дал каталога"; exit 1; }; grep -a -E '^(LEVELUP|UNLOADFIRST|LOADNEWS|LEVEL ev=free|FERETRY841|MEMFAILSZ|LVASOFT)' "$out/smoke.txt" > "$ROOT/build/unload1b_$1.txt"
}
mkdir -p build; if [ -z "${SKIP_RUN:-}" ]; then run green ""; run red "unloadfirstold feunloadfirstold"; fi   # SKIP_RUN=1 — только разбор готовых build/unload1b_*.txt
G=build/unload1b_green.txt; R=build/unload1b_red.txt
ng=$(grep -c 'UNLOADFIRST site=levelup' $G); nr=$(grep -c 'UNLOADFIRST' $R)
# порядок: после каждого LEVELUP первой идёт выгрузка (LEVEL ev=free) и только потом газета (LOADNEWS)
order() { awk '/^LEVELUP/{w=1;f=0;next} w&&/^LEVEL ev=free/{f=1} w&&/^LOADNEWS/{ if(f) ok++; else bad++; w=0 } END{printf "%d %d\n", ok+0, bad+0}' "$1"; }
read gok gbad < <(order $G); read rok rbad < <(order $R)
gfe=$(grep -c 'UNLOADFIRST site=fe' $G); gretry=$(grep -c FERETRY841 $G); rretry=$(grep -c FERETRY841 $R)
echo "зелёный: UNLOADFIRST(levelup)=$ng, порядок выгрузка→газета ok=$gok bad=$gbad, fe=$gfe, FERETRY841=$gretry; красный: UNLOADFIRST=$nr, порядок ok=$rok bad=$rbad, FERETRY841=$rretry"
fail=0
[ "$ng" -ge 3 ] && [ "$gbad" -eq 0 ] && [ "$gok" -ge 3 ] && [ "$gfe" -ge 1 ] && [ "$gretry" -eq 0 ] || { echo "FAIL: зелёный не зелёный"; fail=1; }
if [ "$nr" -eq 0 ] && [ "$rbad" -ge 1 ]; then echo "красный упал, как должен"; else echo "FAIL: красный не красный (контроль слепой)"; fail=1; fi
[ $fail -eq 0 ] && echo PASS || exit 1
