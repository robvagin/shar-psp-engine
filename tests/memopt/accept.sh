#!/bin/bash
# ТЕСТ90 память (окно оптимизации, ветка ag/opt): три приёмки с красным контролем, каждая — зелёный (по умолчанию) и красный (выключатели *_OLD файлами рядом с EBOOT).
#  1) chain  — смены уровня L1→L4→L5→L6→L5→L4→L7. Красный = TEST89 (unloadfirstold feunloadfirstold pinfreeold collretryold): наибольший кусок после выгрузки перед L7 = 1 785 856 Б при ~20 МБ
#              свободных (число в число с журналом PSP TEST89); при чуть более тесной куче (ballast на 1 МБ больше, сборка с LEAK696_TRACE) — «COLL stream error=load» и MEMFAILSZ 1 607 136 Б, как на PSP.
#              Зелёный: PINFREE на каждой выгрузке, ни одного отказа, кусок после выгрузки ≥ 4 МБ, после загрузки L7 ≥ 3,3 МБ (запас вдвое к 1,61 МБ пакета гэгов).
#  2) cf     — «flag collfail 1»: пакет гэгов без памяти один раз. Красный (collretryold): «COLL stream error=load». Зелёный: COLLRETRY loaded=1 и «COLL ok».
#  3) fe     — финал L7 → главное меню → бонус-игра. Красный (TEST89, все четыре файла): отказ гостиной 5 200 032 Б и FERETRY841. Зелёный: ни того ни другого.
#   tests/memopt/accept.sh [EBOOT] [НАБОР]   BALLAST=5 (сборка без LEAK696_TRACE; с ней 3)   ONLY=chain|cf|fe   SKIP_RUN=1 — разбор готовых build/memopt_*.txt
#   нужны уровни 2–7 в наборе (по умолчанию набор тура TEST90); прогон: chain ≈ 5 мин, cf ≈ 1,5 мин, fe до предела 900 с (после «end» в экране бонус-игры эмулятор сам не выходит — режет предел; результат в журнале уже есть)
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
EB="${1:-viewer/build_op/EBOOT.PBP}"; SET="${2:-$HOME/Downloads/shar-psp-integ/build/test90tour/PSP/GAME/SHARTEST15}"; BAL="${BALLAST:-5}"; ONLY="${ONLY:-all}"
export DEVELOPER_DIR=/Library/Developer/CommandLineTools
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
for f in "$SET"/*; do b=$(basename "$f"); case $b in EBOOT.PBP|scenario.txt) ;; *) ln -s "$f" "$T/$b";; esac; done; cp "$EB" "$T/EBOOT.PBP"
BASEF="prereadold pfetchold bbsyncold regprehystold resumeringold mefxpair hudon"
run() {  # $1 метка, $2 сценарий, $3 флаги-файлы, $4 предел времени прогона, с (по умолчанию 3000)
  local out; out=$(SHIP_LEVELS="2 3 4 5 6 7" SMOKE_FLAGS="$BASEF $3" SMOKE_HEADLESS=1 SMOKE_BALLAST=$BAL SMOKE_SCN=tests/memopt/$2 SMOKE_MAXT=${4:-3000} tools/smoke.sh "$T" 2>&1 | grep -o 'build/smoke/[0-9-]*' | head -1)
  [ -n "$out" ] || { echo "FAIL: смоук $1 не дал каталога"; exit 1; }
  grep -a -E '^(LEVELUP|LEVEL ev=|PINFREE|UNLOADFIRST|COLL |COLLRETRY|MEMFAILSZ|FERETRY841|BONUS ev=load )' "$out/smoke.txt" > "$ROOT/build/memopt_$1.txt"
}
mkdir -p build
if [ -z "${SKIP_RUN:-}" ]; then
  [ "$ONLY" = all -o "$ONLY" = chain ] && { run chain_green chain_l7.txt ""; run chain_red chain_l7.txt "unloadfirstold feunloadfirstold pinfreeold collretryold"; }
  [ "$ONLY" = all -o "$ONLY" = cf ] && { run cf_green collretry.txt ""; run cf_red collretry.txt "collretryold"; }
  [ "$ONLY" = all -o "$ONLY" = fe ] && { run fe_green fe_bonus.txt "" 900; run fe_red fe_bonus.txt "unloadfirstold feunloadfirstold pinfreeold collretryold" 900; }
fi
fail=0
mx7() { grep -a '^LEVEL ev=load n=7' "$1" | tail -1 | sed -n 's/.* mx=\([0-9]*\).*/\1/p'; }   # наибольший кусок после загрузки L7, байт
if [ "$ONLY" = all -o "$ONLY" = chain ]; then
  G=build/memopt_chain_green.txt; R=build/memopt_chain_red.txt
  mxfree7() { grep -a '^LEVEL ev=free n=7' "$1" | tail -1 | sed -n 's/.* mx=\([0-9]*\).*/\1/p'; }   # наибольший кусок сразу после выгрузки L4 перед L7, байт (PSP TEST89: 1 785 856)
  gfree=$(grep -ac '^LEVEL ev=free' $G); gpin=$(grep -ac '^PINFREE' $G); gerr=$(grep -ac 'COLL stream error' $G); gfail=$(grep -ac '^MEMFAILSZ' $G); gmx=$(mx7 $G); gmf=$(mxfree7 $G)
  rerr=$(grep -ac 'COLL stream error' $R); rfail=$(grep -ac '^MEMFAILSZ' $R); rpin=$(grep -ac '^PINFREE' $R); rmx=$(mx7 $R); rmf=$(mxfree7 $R)
  echo "chain: зелёный выгрузок=$gfree PINFREE=$gpin COLL-ошибок=$gerr MEMFAILSZ=$gfail mx после выгрузки перед L7=${gmf:-?} Б, после загрузки L7=${gmx:-?} Б"
  echo "chain: красный (TEST89) COLL-ошибок=$rerr MEMFAILSZ=$rfail PINFREE=$rpin mx после выгрузки перед L7=${rmf:-?} Б, после загрузки L7=${rmx:-?} Б (PSP: 1785856 / 1277952)"
  [ "$gfree" -ge 6 ] && [ "$gpin" -eq "$gfree" ] && [ "$gerr" -eq 0 ] && [ "$gfail" -eq 0 ] && [ "${gmf:-0}" -ge 4000000 ] && [ "${gmx:-0}" -ge 3300000 ] || { echo "FAIL chain: зелёный не зелёный"; fail=1; }
  if [ "${rmf:-9999999}" -lt 2600000 ] && [ "$rpin" -eq 0 ]; then echo "chain: красный упал, как должен (дробление: кусок после выгрузки < 2,6 МБ при ~20 МБ свободных; COLL-ошибок=$rerr)"; else echo "FAIL chain: красный не красный (контроль слепой)"; fail=1; fi
fi
if [ "$ONLY" = all -o "$ONLY" = cf ]; then
  G=build/memopt_cf_green.txt; R=build/memopt_cf_red.txt
  gok=$(grep -ac 'COLLRETRY level=4 .* loaded=1' $G); g2=$(grep -ac 'COLLRETRY level=5 .* loaded=0' $G); rerr=$(grep -ac 'COLL stream error' $R); rretry=$(grep -ac '^COLLRETRY' $R)
  echo "cf: зелёный повтор принят (L4)=$gok, второй отказ подряд честно ошибка (L5)=$g2; красный COLL-ошибок=$rerr COLLRETRY=$rretry"
  [ "$gok" -ge 1 ] && [ "$g2" -ge 1 ] || { echo "FAIL cf: зелёный не зелёный"; fail=1; }
  if [ "$rerr" -ge 1 ] && [ "$rretry" -eq 0 ]; then echo "cf: красный упал, как должен"; else echo "FAIL cf: красный не красный"; fail=1; fi
fi
if [ "$ONLY" = all -o "$ONLY" = fe ]; then
  G=build/memopt_fe_green.txt; R=build/memopt_fe_red.txt
  gf=$(grep -ac '^MEMFAILSZ n=.* bytes=5200032' $G); gr=$(grep -ac '^FERETRY841' $G); rf=$(grep -ac '^MEMFAILSZ n=.* bytes=5200032' $R); rr=$(grep -ac '^FERETRY841' $R)
  echo "fe: зелёный MEMFAILSZ(5200032)=$gf FERETRY841=$gr; красный MEMFAILSZ(5200032)=$rf FERETRY841=$rr"
  [ "$gf" -eq 0 ] && [ "$gr" -eq 0 ] || { echo "FAIL fe: зелёный не зелёный"; fail=1; }
  if [ "$rf" -ge 1 ] || [ "$rr" -ge 1 ]; then echo "fe: красный упал, как должен"; else echo "FAIL fe: красный не красный (куча эмулятора чище PSP)"; fail=1; fi
fi
[ $fail -eq 0 ] && echo PASS || exit 1
