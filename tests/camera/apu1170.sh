#!/bin/bash
# №1170: после облёта камеры старта миссии глаз игровой камеры не подходит к герою ближе 2,2 м.
#   tests/camera/apu1170.sh НАБОР        зелёный: L5 (Апу, настоящий путь игрока), L2 (Барт), L3 (Лиза), L6 (Барт) — ближе 2,2 м нигде нет
#   tests/camera/apu1170.sh --old НАБОР  красный контроль: тот же L5 с «flag camexit1170old 1» (переход по прямой, как в TEST87) ОБЯЗАН упасть (0,98 м)
# НАБОР — папка набора данных с EBOOT (сборка с флагом по умолчанию); уровни 2–7 берутся из SHIP_LEVELS
set -u; cd "$(dirname "$0")/../.."
OLD=0; [ "${1:-}" = --old ] && { OLD=1; shift; }
SRC="${1:?набор}"; export SHIP_LEVELS="2 3 4 5 6 7"; TMP=$(mktemp -d); rc=0
run() {   # имя сценарий
  SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_SCN="$2" SMOKE_MAXT=300 tools/smoke.sh "$SRC" > "$TMP/$1.log" 2>&1
  D=$(sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p' "$TMP/$1.log"); [ -n "$D" ] || { echo "$1: нет отчёта smoke"; rc=1; return; }
  echo -n "$1: "; python3 tests/camera/apu1170_blend.py "$D" | sed 's/^[^:]*: //'; [ ${PIPESTATUS[0]} = 0 ] || rc=1
}
if [ $OLD = 1 ]; then
  sed 's/^flag camlog$/flag camlog\nflag camexit1170old 1/' tests/camera/apu1170_exit_scenario.txt > "$TMP/old.txt"; run L5_old "$TMP/old.txt"
  [ $rc = 1 ] && { echo "красный контроль: упал, как и должен"; exit 0; }; echo "красный контроль НЕ упал"; exit 1
fi
run L5 tests/camera/apu1170_exit_scenario.txt
for L in 2 3 6; do printf '%s\n' "scene A1170_LV$L" "flag camdet 1" "flag camlog" "flag startcam 1" "flag golevel $L" "wait 9" "end" > "$TMP/lv$L.txt"; run L$L "$TMP/lv$L.txt"; done
exit $rc
