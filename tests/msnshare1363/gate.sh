#!/bin/bash
# Гейт №1363: общие ресурсы миссий уровня (cards, wrench, missgen, двери, значки msnicons: список msnres, «L» или две и более миссий) на старте второй миссии НЕ читаются заново.
#   tests/msnshare1363/gate.sh НАБОР EBOOT.PBP [УРОВНИ]   безголовый эмулятор, ballast 5; миссии: flag msn, flag msnsel 1, flag msnsel 2 (сцена M2 начинается перед вторым выбором)
# Зелёный: ни один файл, прочитанный на 1-й миссии, не читается снова на 2-й (строки MSNREQ art: полный путь, пишется при начале чтения), и MSNSHARE1363 hits > 0.
# Красный контроль: flag msnartshare1363 0 до старта миссий — на старте второй миссии общие читаются заново (число > 0).
set -u; cd "$(dirname "$0")/../.."; BASE="$1"; EB="$2"; shift 2; LV="${*:-1 4}"; OUT=build/msnshare1363; mkdir -p "$OUT"; BAD=0
run() {   # уровень режим
  local n=$1 mode=$2; local S="$OUT/set_$n$mode" sc="$OUT/sc_$n$mode.txt"; rm -rf "$S"; cp -Rc "$BASE" "$S"; cp "$EB" "$S/EBOOT.PBP"
  { echo "flag nosave"; [ $mode = old ] && echo "flag msnartshare1363 0"; [ $n != 1 ] && { echo "flag golevel $n"; echo "wait 50"; } || echo "wait 15"
    echo "flag msn"; echo "wait 20"; echo "scene M1"; echo "flag msnsel 1"; echo "wait 25"; echo "scene M2"; echo "flag msnsel 2"; echo "wait 25"; echo "flag msnshare1363stat"; echo "wait 2"; echo "end"; } > "$sc"
  local R; R=$(SMOKE_EXTRA="msnres-l1.shr l2/msnres-l2.shr l3/msnres-l3.shr l4/msnres-l4.shr l5/msnres-l5.shr l6/msnres-l6.shr l7/msnres-l7.shr" SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_MAXT=400 SMOKE_SCN="$sc" tools/smoke.sh "$S" 2>&1 | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p')
  case "$R" in /*) ;; *) R="$PWD/$R" ;; esac; rm -rf "$S"
  awk '/^SCENE M2/{f=1} f' "$R/smoke.txt" > "$OUT/L${n}_${mode}_after.txt"; awk '/^SCENE M1/{f=1} /^SCENE M2/{f=0} f' "$R/smoke.txt" > "$OUT/L${n}_${mode}_m1.txt"
  grep -a "^MSNSHARE1363" "$R/smoke.txt" | tail -1 > "$OUT/L${n}_${mode}_stat.txt"
}
reads() { grep -a "^MSNREQ art" "$1" | awk '{print $3}' | sort -u; }   # полные пути записей, которые реально начали читать с карты (msnartReq стоит прямо перед чтением)
cnt() { comm -12 <(reads "$OUT/L$1_$2_m1.txt") <(reads "$OUT/L$1_$2_after.txt") | wc -l | tr -d ' '; }   # прочитано и на 1-й, и на 2-й миссии
for n in $LV; do
  run $n new; run $n old
  a=$(cnt $n new); b=$(cnt $n old); m1=$(reads "$OUT/L${n}_new_m1.txt" | wc -l | tr -d " "); st=$(cat "$OUT/L${n}_new_stat.txt")
  echo "L$n: файлов прочитано на 1-й миссии (новый) $m1; из них снова на 2-й: новый $a, старый $b | ${st:-нет строки MSNSHARE1363}"
  [ "$a" = 0 ] || { echo "  FAIL L$n: новый читает общие заново ($a)"; BAD=1; }
  [ "$b" -gt 0 ] 2>/dev/null && echo "  красный пойман L$n: старый читает $b" || { echo "  КРАСНЫЙ НЕ ПОЙМАН L$n"; BAD=1; }
  h=$(echo "$st" | sed -n 's/.* hits=\([0-9]*\).*/\1/p'); [ "${h:-0}" -gt 0 ] 2>/dev/null || { echo "  FAIL L$n: hits=0 (кэш не использовался)"; BAD=1; }
done
[ $BAD = 0 ] && echo PASS || echo FAIL; exit $BAD
