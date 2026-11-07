#!/bin/bash
# №1186 гейт: конец разговора-старта (последний этап вступления, ✕ Start Mission) -> ирис talk_out не должен вешать видеочип.
# Использование: SET=<набор с EBOOT.PBP и данными> [LV=1] tests/g10b/talk1186.sh ; последняя строка PASS/FAIL.
# PASS: сценарий дошёл до конца (нет TIMEOUT/«нет END»), на каждый TALKSKIP start=1 есть IRIS why=talk_out phase=close f=0.00, GE NESTED = 0.
# Красный контроль: SET с EBOOT TEST88 (ирис внутри кадра) -> прогон встаёт после первого TALKSKIP (STALL phase=ui, SceGeQueue), FAIL.
set -u; T="$(cd "$(dirname "$0")/../.." && pwd)"; SET=${SET:?нужен набор}; LV=${LV:-1}
[ "$LV" -gt 1 ] && export SHIP_LEVELS="2 3 4 5 6 7"
SMOKE_HEADLESS=1 SMOKE_MAXT=${SMOKE_MAXT:-1100} SMOKE_SCN="$T/tests/smoke_scenes/talk1186_l$LV.txt" "$T/tools/smoke.sh" "$SET" > /tmp/talk1186_$$.log 2>&1
D=$(sed -n 's/^отчёт: \(.*\)\/summary.txt, листы.*/\1/p' /tmp/talk1186_$$.log | tail -1); case "$D" in /*) ;; *) D="$T/$D" ;; esac; S="$D/smoke.txt"   # папка именно этого прогона (из вывода smoke.sh), не «последняя»: рядом идут чужие прогоны
ts=$(grep -ac "^TALKSKIP start=1" "$S"); to=$(grep -ac "^IRIS why=talk_out phase=close f=0.00" "$S"); ne=$(grep -ac "^GE NESTED" "$S"); st=$(grep -ac "^BBSTALL" "$S")
end=$(grep -ac "^SCENE F_TALK1186_L${LV}_M7S2" "$S")
echo "TALKSKIP start=1: $ts; talk_out: $to; GE NESTED: $ne; STALL: $st; дошёл до последней сцены: $end ($D)"
[ "$ts" -ge 3 ] && [ "$to" = "$ts" ] && [ "$ne" = 0 ] && [ "$st" = 0 ] && [ "$end" -ge 1 ] && { echo PASS; exit 0; } || { echo FAIL; exit 1; }
