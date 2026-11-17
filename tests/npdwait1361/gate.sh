#!/bin/bash
# Гейт №1361: экран загрузки не ждёт «читающегося» персонажа, которого прочитать нельзя (npd: риг без кожи, L1 m5 и L5 m1), на всех уровнях; красный: flag npd1361old (ждёт потолок 3 с на L5).
#   tests/npdwait1361/gate.sh НАБОР EBOOT.PBP [УРОВНИ]    безголовый эмулятор, ballast 5 (как PSP Роба)
# Зелёный: на каждом уровне строка NPCWAIT1076 либо нет, либо left=0/0 и ms < 500; на L5 новой нет «MSNNPC no-model npd» -> ожидание 0 мс, время загрузки (LOADSHOW) меньше, чем на старом минимум на 2,5 с.
# Красный контроль: на старом (flag npd1361old 1 до golevel) на L5 NPCWAIT ms >= 2900 и left=1/…; LOADSHOW старого больше нового минимум на 2,5 с.
set -u; cd "$(dirname "$0")/../.."; BASE="$1"; EB="$2"; shift 2; LV="${*:-1 2 3 4 5 6 7}"; OUT=build/npdwait1361; mkdir -p "$OUT"; BAD=0
run() {   # уровень режим -> файлы $OUT/L<n>_<режим>.txt
  local n=$1 mode=$2; local S="$OUT/set_$n$mode" sc="$OUT/sc_$n$mode.txt"; rm -rf "$S"; cp -Rc "$BASE" "$S"; cp "$EB" "$S/EBOOT.PBP"
  { echo "flag nosave"; [ $mode = old ] && echo "flag npd1361old 1"; if [ $n = 1 ]; then echo "flag golevel 2"; echo "wait 45"; echo "flag golevel 1"; echo "wait 50"; else echo "flag golevel $n"; echo "wait 50"; fi; echo "end"; } > "$sc"
  local R; R=$(SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_MAXT=300 SMOKE_SCN="$sc" tools/smoke.sh "$S" 2>&1 | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p')
  case "$R" in /*) ;; *) R="$PWD/$R" ;; esac; rm -rf "$S"
  grep -a -E "^(NPCWAIT1076 pending|LOADSHOW|XITION level|MSNNPC no-model|MSN npc npd)" "$R/smoke.txt" | tail -8 > "$OUT/L${n}_$mode.txt"
}
fld() { echo "$1" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }
for n in $LV; do
  run $n new; [ $n = 5 ] && run $n old || : > "$OUT/L${n}_old.txt"
  wn=$(grep -a "^NPCWAIT1076 pending" "$OUT/L${n}_new.txt" | tail -1); wo=$(grep -a "^NPCWAIT1076 pending" "$OUT/L${n}_old.txt" | tail -1)
  sn=$(grep -a "^LOADSHOW" "$OUT/L${n}_new.txt" | tail -1); so=$(grep -a "^LOADSHOW" "$OUT/L${n}_old.txt" | tail -1)
  echo "L$n новый: ${wn:-нет ожидания} | $(fld "$sn" ms) мс загрузка"; echo "L$n старый: ${wo:-нет ожидания} | $(fld "$so" ms) мс загрузка"
  mn=$(fld "$wn" ms); [ -z "$wn" ] || [ "${mn%.*}" -lt 500 ] 2>/dev/null || { echo "  FAIL L$n: на новом ждали ${mn} мс"; BAD=1; }
  if [ $n = 5 ]; then
    mo=$(fld "$wo" ms); [ -n "$wo" ] && [ "${mo%.*}" -ge 2900 ] 2>/dev/null && echo "  красный пойман L$n: старый ждал $mo мс" || { echo "  КРАСНЫЙ НЕ ПОЙМАН L$n (на старом ожидание ${mo:-нет})"; [ $n = 5 ] && BAD=1; }
    d=$(( $(fld "${so:-ms=0}" ms | cut -d. -f1) - $(fld "${sn:-ms=0}" ms | cut -d. -f1) )); [ $n = 5 ] && { [ "$d" -ge 2500 ] && echo "  L5 загрузка короче на $d мс" || { echo "  FAIL L5: выигрыш $d мс < 2500"; BAD=1; }; }  fi
done
[ $BAD = 0 ] && echo PASS || echo FAIL; exit $BAD
