#!/bin/bash
# Гейт З2 (109): пересборка листа предметов клетки берёт кандидатов из списков по клеткам, а не обходит все экземпляры.
#   tests/pbidx109/gate.sh DIR [УРОВНИ]   DIR — набор с EBOOT.PBP (как для tools/smoke.sh), по умолчанию уровни «1 2 3 5 6» (на L4 и L7 в езде по прямой сборок листов предметов нет: гейт был бы пустым)
# Безголовый эмулятор, езда 2×22 с по прямой, flag pbchk 1: рядом с каждой сборкой считается полный обход, всё, что он взял бы, обязано быть среди кандидатов (строка PBCHK idx_bad).
# Зелёный: idx_bad=0 при idx_calls>0 (сборки были и шли по спискам); красный контроль (flag pbidxbreak 1: список теряет последнего кандидата): idx_bad>=1; выключатель pbidx109old: idx_calls=0, всё как до 109.
# Печатает цену сборок (PBAKE: сумма мкс эмулятора) по спискам и полным обходом: для отчёта, не условие гейта.
set -u; cd "$(dirname "$0")/../.."; SRC="$1"; shift; LV="${*:-1 2 3 5 6}"; BAD=0
one() {   # уровень режим -> строка итога
  local n=$1 mode=$2 sc; sc=$(mktemp /tmp/pbi.XXXXXX)
  # флаги ДО смены уровня: первые сборки листов идут сразу после загрузки уровня (на L3, L6 все 123 за первые секунды), позже флаги ничего бы не проверили
  { echo "flag nosave"; [ "$mode" != old ] && echo "flag pbchk 1"; [ "$mode" = break ] && echo "flag pbidxbreak 1"; [ "$n" != 1 ] && { echo "flag golevel $n"; echo "wait 40"; }
    printf 'scene a\nwait 3\nflag autostraight 22\nwait 24\nscene b\nflag stop\nflag autostraight 22\nwait 24\nend\n'; } > "$sc"
  local fl=""; [ "$mode" = old ] && fl=pbidx109old
  local R; R=$(SMOKE_FLAGS="$fl" SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_MAXT=500 SMOKE_SCN="$sc" tools/smoke.sh "$SRC" 2>&1 | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p'); rm -f "$sc"
  case "$R" in /*) ;; *) R="$PWD/$R" ;; esac
  local pc; pc=$(grep -a '^PBCHK' "$R/smoke.txt" | tail -1)
  local tot; tot=$(grep -a '^PBAKE' "$R/smoke.txt" | awk '{for(i=1;i<=NF;i++){split($i,a,"=");v[a[1]]=a[2]} t+=v["ms"]*1000; n++} END{print n+0, t+0}')
  echo "$pc builds_total_us=$tot" | sed 's/force_cells=[0-9]* //'
}
fld() { echo "$1" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }
for n in $LV; do
  a=$(one $n new); b=$(one $n break); c=$(one $n old)
  ok=1
  [ "$(fld "$a" idx_bad)" = 0 ] && [ "$(fld "$a" idx_calls)" -gt 0 ] 2>/dev/null && [ "$(fld "$a" dyn_bad)" = 0 ] && [ "$(fld "$a" vis_bad)" = 0 ] || ok=0
  [ "$(fld "$b" idx_bad)" -ge 1 ] 2>/dev/null || { echo "L$n красный контроль НЕ пойман: $b"; ok=0; }
  [ "$(fld "$c" idx_calls)" = 0 ] || ok=0
  [ $ok = 1 ] && echo "L$n PASS  списки: $a" || { echo "L$n FAIL  списки: $a | красный: $b | старое: $c"; BAD=1; }
  [ $ok = 1 ] && echo "      красный (idx_bad>=1): $(fld "$b" idx_bad)   старое (полный обход): $c"
done
[ $BAD = 0 ] && echo PASS || echo FAIL; exit $BAD
