#!/bin/bash
# Гейт гейта: tools/run_outcome.py различает ENDED / TIMEOUT / HANG на настоящем эмуляторе (04.11, окно gates). Три прогона одного EBOOT:
#   1. короткий честный сценарий с end, предел с запасом            → ENDED   (код 0)  — зелёный: на исправной игре исход не краснеет
#   2. тот же движок, длинный сценарий, предел 25 с                  → TIMEOUT (код 3)  — игра шла до обрыва; это «не измерено», не зависание
#   3. сценарий с flag hang (главный поток встаёт намертво)          → HANG    (код 4)  — красный контроль: зависание НЕ маскируется под TIMEOUT
# Использование: tests/gates/outcome.sh EBOOT.PBP НАБОР   (НАБОР — папка данных, как у frame_gate.sh; безголовый эмулятор; EBOOT — тур-конфигурация с KEEP_SMOKE)
set -u
cd "$(dirname "$0")/../.."; ROOT=$PWD
EB=${1:?EBOOT.PBP}; PK=${2:?НАБОР}
TMP="$ROOT/build/gate_outcome_$$"; trap 'rm -rf "${TMP:?}"' EXIT
cp -cR "$PK" "$TMP" 2>/dev/null || cp -R "$PK" "$TMP"; cp "$EB" "$TMP/EBOOT.PBP"
printf 'scene spawn\nwait 3\nshot spawn\nend\n' > "$TMP/ok_scenario.txt"
LOAD1=$(sysctl -n vm.loadavg | tr -d '{}' | awk '{print $1}'); NCPU=$(sysctl -n hw.ncpu)
SC=$(awk -v l="$LOAD1" -v n="$NCPU" 'BEGIN{r=l/n; print (r>=2)?3:((r>=1)?2:1)}')
fail=0
one() {   # ИМЯ СЦЕНАРИЙ ПРЕДЕЛ ОЖИДАЕМЫЙ_КОД
  ( SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_SCN="$2" SMOKE_MAXT=$3 nice -n 20 tools/smoke.sh "$TMP" ) > "$ROOT/build/gate_outcome_$1.out" 2>&1
  D="$ROOT/build/smoke/$(ls -t "$ROOT/build/smoke" | head -1)"
  line=$(python3 "$ROOT/tools/run_outcome.py" "$D"); rc=$?
  if [ "$rc" = "$4" ]; then echo "outcome $1: ok · код $rc · $line"; else echo "outcome $1: FAIL · ждали код $4, получили $rc · $line"; fail=1; fi
}
one green "$TMP/ok_scenario.txt" $((120 * SC)) 0
one timeout "$ROOT/tests/gates/slow_scenario.txt" 25 3
one hang "$ROOT/tests/gates/hang_scenario.txt" $((60 * SC)) 4
[ $fail = 0 ] && echo "outcome: PASS (ENDED / TIMEOUT / HANG различаются; масштаб нагрузки x$SC)" || echo "outcome: FAIL"
exit $fail
