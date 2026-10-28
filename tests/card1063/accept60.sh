#!/bin/bash
# №1063 accept60: свечение карточки векторным пятном оригинала (1,6 м, радиальный градиент), старого блика-звезды и размытого спрайта нет; красный — flag card1063old 1 (блик-звезда №839 как в TEST74).
# Аргумент 1 — EBOOT тура (с CARD1063), SET — набор данных (test74tour). ≤ 2 эмулятора. Последняя строка GATE PASS / GATE FAIL
H=$(cd "$(dirname "$0")" && pwd); E=${1:?EBOOT}; export OUT=${OUT:-$H/out} EMU_OWNER=${EMU_OWNER:-Камера} EMU_MINE=2; rc=0; mkdir -p $OUT
RUN=${RUN:-$HOME/Downloads/shar-psp-integ/tools/emu_run.sh}
OUT=$OUT/n bash $RUN $H/look.txt n $E 300 >/dev/null 2>&1 & OUT=$OUT/o bash $RUN $H/look_old.txt n $E 300 >/dev/null 2>&1 & wait
OUT=$OUT/r bash $RUN $H/look_old.txt n $E 300 >/dev/null 2>&1 & OUT=$OUT/fn bash $RUN $H/family.txt n $E 900 >/dev/null 2>&1 & wait
OUT=$OUT/fo bash $RUN $H/family_old.txt n $E 900 >/dev/null 2>&1
echo "== №1063 зелёный"; python3 $H/check.py $OUT/n/out_n $OUT/o/out_n || rc=1
echo "== №1063 семейство L1–L7 (зелёный)"; python3 $H/check_family.py $OUT/fn/out_n || rc=1
echo "== №1063 красный (flag card1063old 1)"; python3 $H/check_family.py $OUT/fo/out_n --expect fail || { echo "КРАСНЫЙ КОНТРОЛЬ СЕМЕЙСТВА НЕ КРАСНЫЙ"; rc=1; }
echo "-- кадры"; python3 $H/check.py $OUT/r/out_n $OUT/o/out_n --expect fail || { echo "КРАСНЫЙ КОНТРОЛЬ НЕ КРАСНЫЙ"; rc=1; }
[ $rc = 0 ] && echo "GATE PASS" || echo "GATE FAIL"; exit $rc
