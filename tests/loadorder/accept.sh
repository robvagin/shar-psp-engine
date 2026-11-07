#!/bin/bash
# ag/loadorder (№1209): приёмка «игра не идёт дальше, пока не готов город у игрока». Прогоны последовательно, медленная карта slowio (КБ/с, по умолчанию 600).
#   SET=<набор тура: EBOOT.PBP, l2..l7, movies>  tests/loadorder/accept.sh [КБ/с]   -> последняя строка PASS или FAIL
# Зелёный = сценарий с новым поведением проходит tools/loadorder_check.py (код 0); красный = тот же сценарий с флагами отката (*_OLD) обязан дать код 1.
# Сценарии (каждый в паре new / old): r7 (L7: перенос миссии в школу и выход дверью — дыра рядом), r10 (L4: Гомер в кармане комнат, пока мир города, потом msnRoomSync — падение). EBOOT — сборка с -DSHAR_RELEASE_KEEP_SMOKE=ON.
set -u
W=$(cd "$(dirname "$0")/../.." && pwd); SLOW=${1:-600}; SET=${SET:-$W/build/lo_set}; export SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_MAXT=${SMOKE_MAXT:-900}
run() {   # сценарий -> папка отчёта
  SMOKE_SCN="$W/tests/loadorder/$1" SMOKE_FILEDATA="slowio=$SLOW" "$W/tools/smoke.sh" "$SET" 2>&1 | grep -o "build/smoke/[0-9-]*" | head -1
}
bad=0
check() {   # сценарий-пара: имя проверки
  case "$1" in r7*) python3 "$W/tools/loadorder_check.py" "$2" ;; r10*) python3 "$W/tests/loadorder/check_r10.py" "$2" ;; esac
}
for pair in "r7_l7_school_exit.txt r7_l7_school_exit_old.txt" "r10_msnroomsync_fall.txt r10_msnroomsync_fall_old.txt"; do
  set -- $pair
  d=$(run "$1"); echo "new $1: $d"; check "$1" "$W/$d/smoke.txt" || { echo "  !! зелёный не зелёный"; bad=1; }
  d=$(run "$2"); echo "old $2: $d"; check "$2" "$W/$d/smoke.txt" && { echo "  !! красный не краснеет"; bad=1; }
done
[ $bad -eq 0 ] && echo PASS || echo FAIL
