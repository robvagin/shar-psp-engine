#!/bin/bash
# Гейт машин миссий с ИИ на Маке (tests/aicar/host.c): сначала красные на заведомо плохом входе, потом зелёный.
#   tests/aicar/host.sh [LEVEL.shr TRAFFIC.shr LEVELNN.msn MSNCARS.shr]   (по умолчанию — уровень 1 из build/)
# Красные (каждый обязан упасть своей проверкой):
#   --plant-offroad         последняя точка каждого пути этапа — в 60 м от дорог («точки пути вне дорог»)
#   -DMC_BREAK_SPEED=1      скорость не из .con (60 м/с)                      («быстрее TopSpeed .con»)
#   -DMC_BREAK_ROADS=1      дороги не используются: прямо к точке сквозь дома   («кадры дальше 6 м от оси полос» / «доехали»)
#   -DMC_BREAK_HEADING=1    машина ставится курсом персонажа (без +π)          («старт смотрит на первую точку пути»)
# Зелёный — все проверки на настоящих данных. Журнал: build/gates/aicar/host_<время>.txt
cd "$(dirname "$0")/../.."
LV=${1:-build/level.shr}; TR=${2:-build/traffic.shr}; MS=${3:-build/missions/level01.msn}; MC=${4:-build/msncars-l1.shr}
V=viewer; SRC="tests/aicar/host.c $V/msncar.c $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c $V/mission/msn_file.c $V/mission/msn_ops.c $V/watchdog.c"
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V"
mkdir -p build/aicar build/gates/aicar; LOG=build/gates/aicar/host_$(date +%Y%m%d-%H%M%S).txt
fail=0
red() {   # имя, что должно упасть, флаги сборки, ключи запуска
  local name="$1" want="$2" defs="$3"; shift 3
  $CC $defs $SRC -lm -o build/aicar/host_red || { echo "сборка $name не удалась"; fail=1; return; }
  local out; out=$(build/aicar/host_red "$LV" "$TR" "$MS" "$MC" "$@" 2>&1); echo "$out" >> "$LOG"
  local r; r=$(echo "$out" | grep '^RESULT')
  if echo "$r" | grep -q "FAIL.*$want"; then echo "красный $name: пойман ($want)"; else echo "красный $name: НЕ пойман ($r)"; fail=1; fi
}
echo "== красные" | tee -a "$LOG"
red "подброс точки вне дорог" "вне дорог" "" --plant-offroad
red "скорость не по .con"     "TopSpeed" "-DMC_BREAK_SPEED=1"
red "без дорог"               "оси полос\|доехали" "-DMC_BREAK_ROADS=1"
red "курс без +π"             "старт смотрит" "-DMC_BREAK_HEADING=1"
echo "== зелёный" | tee -a "$LOG"
$CC $SRC -lm -o build/aicar/host || exit 1
build/aicar/host "$LV" "$TR" "$MS" "$MC" 2>&1 | tee -a "$LOG" | tail -9
grep -q '^RESULT: PASS' "$LOG" || fail=1
echo "журнал: $LOG"
[ $fail = 0 ] && echo "ГЕЙТ tests/aicar/host: PASS" || echo "ГЕЙТ tests/aicar/host: FAIL"
exit $fail
