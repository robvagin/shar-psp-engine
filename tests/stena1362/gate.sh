#!/bin/bash
# 1362 (Роб: «всё нужно загрузить во время загрузки с газетой, не открывать раньше, чем надо»): три загрузки уровня L5, L6, L7 подряд. Новое: после газеты ждать клеток нет (LOADCELLS left=0) и машины миссии не читаются
# после раскрытия (нет BBOP car+ между TFSM levelup open и следующей загрузкой). Красный (flag loadwait1362old 1): остаются клетки около старта (left>0) и машина фазы читается уже под игрой.
# Запуск: tests/stena1362/gate.sh PACK  (набор в рабочей копии со ссылками, EBOOT этой сборки; уровни 5 6 7)
set -u; cd "$(dirname "$0")/../.."; P=$1
run() { SHIP_LEVELS="5 6 7" SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=400 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
chk() { awk '/^LOADCELLS/{split($3,a,"="); if (a[2]+0>0) left+=a[2]} /^TFSM levelup open/{o=1;next} /^LOADCELLS|^LEVEL ev=free/{o=0} o&&/^BBOP car\+/{car++} END{printf "%d %d\n", left+0, car+0}' "$1/smoke.txt"; }
Rn=$(run tests/stena1362/new.scn); Ro=$(run tests/stena1362/old.scn)
read ln cn <<< "$(chk $Rn)"; read lo co <<< "$(chk $Ro)"
echo "  новое: клеток не дождались $ln, машин прочитано после раскрытия $cn;  прежнее: клеток $lo, машин $co"
[ "$ln" = 0 ] && [ "$cn" = 0 ] && { [ "$lo" -gt 0 ] || [ "$co" -gt 0 ]; } && echo PASS || { echo FAIL; exit 1; }
