#!/bin/bash
# Куча уровня после загрузки: два набора (EBOOT до / после), уровни 1 и 2. Красный контроль гейта «статической памяти нет»: у сборки со статическими массивами отсечения (143 КБ) куча меньше на ~143 КБ.
#   tests/racecull1298/heap.sh НАБОР_ДО НАБОР_ПОСЛЕ [мин. прирост байт, по умолчанию 100000]   PASS — куча после больше на не меньше порога на обоих уровнях
set -u; cd "$(dirname "$0")/../.."; A="$1"; B="$2"; MIN="${3:-100000}"; BAD=0
heap() { local set=$1 n=$2 sc; sc=$(mktemp /tmp/hp.XXXXXX); printf 'flag level %s\nscene h%s\nwait 8\nend\n' "$n" "$n" > "$sc"
  local R; R=$(SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_MAXT=400 SMOKE_SCN="$sc" tools/smoke.sh "$set" 2>&1 | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p'); rm -f "$sc"; case "$R" in /*) ;; *) R="$PWD/$R" ;; esac
  grep -a '^LEVEL ev=load' "$R/smoke.txt" | tail -1 | sed -n 's/.* heap=\([0-9]*\).*/\1/p'; }
for n in 1 2; do a=$(heap "$A" $n); b=$(heap "$B" $n); d=$((b - a)); echo "L$n куча после загрузки: до $a, после $b, прирост $d"; [ "$d" -ge "$MIN" ] || { echo "FAIL L$n: прирост меньше $MIN"; BAD=1; }; done
[ $BAD = 0 ] && echo PASS || echo FAIL; exit $BAD
