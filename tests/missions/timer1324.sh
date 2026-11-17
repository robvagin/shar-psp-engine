#!/bin/bash
# 1324 (а): число таймера обратного отсчёта — секунды с отбрасыванием дроби. L5 m7 (30 с), стоять до провала: последнее показанное число перед «MSN failed 1 Timed out» — 1 (новое); красное (flag timerfloor1324old 1) — 2.
#         и первая секунда: на старте этапа 30, через секунду после старта 29 (новое) / 30 (красное) — строка MSNTIMER.
#   tests/missions/timer1324.sh ПАКЕТ   (набор с данными L5, безголовый эмулятор)
cd "$(dirname "$0")/../.."; P=${1:?пакет}
run() { SHIP_LEVELS="5" SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=200 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
last() { grep -a '^MSN failed 1' $1/smoke.txt | head -1 | sed -n 's/.*lastshown=\([-0-9]*\).*/\1/p'; }
N=$(run tests/missions/timer1324.scn); R=$(run tests/missions/timer1324_red.scn)
n=$(last $N); r=$(last $R); echo "новое: lastshown=$n ($N), красное: lastshown=$r ($R)"
[ "$n" = 1 ] && [ "$r" = 2 ] && { echo PASS; exit 0; }; [ "$r" != 2 ] && echo "КРАСНЫЙ НЕ ПОЙМАН"; echo FAIL; exit 1
