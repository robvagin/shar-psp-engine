#!/bin/bash
# Гейт 1325 (машина ИИ миссии в зоне DEATH / вверх колёсами вне дороги возвращается, как игрок). tests/aicar1325/gate.sh [LEVEL.shr TRAFFIC.shr MSNCARS.shr]
# Зелёный: новое — зона DEATH на 1-м кадре в apex, переворот через 1 с на ось полосы, покой на дороге — тихо.
# Красный: -DMC_AIRESET1325_OLD=1 (прежнее: зон нет, переворот на месте) — прогон обязан пройти свои «прежнее»-проверки и ПРОВАЛИТЬ проверки нового (зона DEATH).
cd "$(dirname "$0")/../.."
B=${HOME}/Downloads/shar-psp/build; LV=${1:-$B/level.shr}; TR=${2:-$B/traffic.shr}; MC=${3:-$B/msncars-l1.shr}
V=viewer; SRC="tests/aicar1325/host.c $V/msncar.c $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c $V/mission/msn_file.c $V/mission/msn_ops.c $V/watchdog.c"
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -w -DSKIN_HOST -I $V"
O=${TMPDIR:-/tmp}/aicar1325; mkdir -p $O; fail=0
$CC $SRC -lm -Wl,-undefined,dynamic_lookup -o $O/new || exit 1
$CC -DMC_AIRESET1325_OLD=1 $SRC -lm -Wl,-undefined,dynamic_lookup -o $O/old || exit 1
$CC -DMC_AIRESET1325B_OLD=1 $SRC -lm -Wl,-undefined,dynamic_lookup -o $O/oldb || exit 1
echo "== красный (прежнее): зона DEATH должна НЕ сработать"
out=$($O/old "$LV" "$TR" "$MC" 2>&1); echo "$out" | grep -E "^  (ok|FAIL)|RESULT|зон DEATH"
echo "$out" | grep -q "FAIL.*зона DEATH: машина ИИ возвращена" && echo "красный: пойман (старое не возвращает из зоны)" || { echo "красный: НЕ пойман"; fail=1; }
echo "$out" | grep -q "ok.*прежнее: на месте" && echo "красный: переворот на месте (прежнее) подтверждён" || { echo "красный: переворот прежнего не подтверждён"; fail=1; }
echo "== красный 1325б (как после 1325: ближайшая полоса, трафик в точке остаётся)"
out=$($O/oldb "$LV" "$TR" "$MC" 2>&1); echo "$out" | grep -E "^  (ok|FAIL)|соперник"
echo "$out" | grep -q "ok .*прежнее: на ближайшей к месту переворота" && echo "$out" | grep -q "ok .*прежнее: трафик в точке остался" && echo "красный 1325б: подтверждён" || { echo "красный 1325б: НЕ подтверждён"; fail=1; }
echo "== зелёный (новое)"
out=$($O/new "$LV" "$TR" "$MC" 2>&1); echo "$out" | grep -E "^  (ok|FAIL)|RESULT|зон DEATH|    "
echo "$out" | grep -q "^RESULT: PASS" || fail=1
for L in 2 3 4 5 6 7; do D=$B/levels/l$L   # те же проверки на остальных уровнях (зелёный)
  [ -f $D/level.shr ] || continue; o=$($O/new $D/level.shr $D/traffic.shr $D/msncars-l$L.shr 2>&1); echo "  L$L: $(echo "$o" | grep -E "^RESULT")"; echo "$o" | grep -q "^RESULT: PASS" || fail=1; done
[ $fail = 0 ] && echo "ГЕЙТ tests/aicar1325: PASS" || echo "ГЕЙТ tests/aicar1325: FAIL"
exit $fail
