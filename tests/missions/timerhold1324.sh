#!/bin/bash
# 1324 (б): отсчёт и таймер этапа ждут готовый мир под игроком. L5 m7 (таймер 30 с), после ✕ Continue мир «не готов» 4 с (flag scenehold 4, служебная пауза сцены города).
# Новое: через 2 с остаток 30,0 (стоит), строка TIMERHOLD1324 waited≈4000 мс; потом остаток идёт. Красное (flag timerhold1324old 1): остаток уже меньше 29,5, строки нет.
#   tests/missions/timerhold1324.sh ПАКЕТ
cd "$(dirname "$0")/../.."; P=${1:?пакет}
run() { SHIP_LEVELS="5" SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=120 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
rem1() { grep -a '^MSNTIMER' $1/smoke.txt | sed -n '1p' | sed -n 's/.* rem=\([0-9.]*\).*/\1/p'; }
rem2() { grep -a '^MSNTIMER' $1/smoke.txt | sed -n '2p' | sed -n 's/.* rem=\([0-9.]*\).*/\1/p'; }
wt() { grep -a '^TIMERHOLD1324 waited' $1/smoke.txt | tail -1 | sed -n 's/.*waited=\([0-9]*\)ms.*/\1/p'; }
N=$(run tests/missions/timerhold1324.scn); R=$(run tests/missions/timerhold1324_red.scn)
n1=$(rem1 $N); n2=$(rem2 $N); w=$(wt $N); r1=$(rem1 $R); r2=$(rem2 $R); rw=$(wt $R)
echo "новое: через 2 с rem=$n1, через 7 с rem=$n2, waited=${w:-нет} ($N)"; echo "красное: через 2 с rem=$r1, через 7 с rem=$r2, waited=${rw:-нет} ($R)"
ok=$(awk -v a="$n1" -v b="$n2" -v w="${w:-0}" 'BEGIN{print (a>=29.9 && b<29.9 && w>=2500 && w<=5000) ? 1 : 0}')
red=$(awk -v a="$r1" -v w="${rw:-0}" 'BEGIN{print (a<29.5 && w==0) ? 1 : 0}')
[ $ok = 1 ] && [ $red = 1 ] && { echo PASS; exit 0; }; [ $red = 0 ] && echo "КРАСНЫЙ НЕ ПОЙМАН"; echo FAIL; exit 1
