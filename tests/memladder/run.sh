#!/bin/bash
# Правка 435 (лестница памяти) + 432 (MISSION SELECT): приёмка на Маке, безголовый эмулятор. tests/memladder/run.sh НАБОР
#   НАБОР — папка с EBOOT.PBP и данными (символьные ссылки на набор тура + свой EBOOT; уровни 2–7: SHIP_LEVELS). Красный контроль — тот же EBOOT и тот же сценарий с «flag memladder 0» (прежние частные обходы).
#   s1 starve: потребитель уровня забирает всё освобождённое (flag boothstarve), меню открывается с первой попытки только через отложенный блок лестницы
#   s2 fuzz:   бот меню (случайные последовательности кнопок и состояний прохождения, куча раздроблена, отказы пакета), куча = как у PSP (SMOKE_BALLAST=9)
#   s4 break:  отрицательный контроль самопроверки меню: 5 видов порчи (flag msnselbreak 1..5); красный — flag msnselchkoff 1 (проверка молчит)
#   s3 rows:   6/7 строк на уровнях L4, L2, L6 при разных масках пройденных
# Эмуляторов не больше 2 разом; запуски со сдвигом 3 с (имя папки результата — по секундам). Последняя строка PASS/FAIL.
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; SET=${1:?набор}; cd $W; export SHIP_LEVELS="2 3 4 5 6 7" PATH=$W/.venv/bin:$PATH
[ -x /Library/Developer/CommandLineTools/usr/bin/git ] && export DEVELOPER_DIR=/Library/Developer/CommandLineTools
run() { # имя сценарий maxt [ballast]
  ( [ -n "$4" ] && export SMOKE_BALLAST=$4; SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_SCN=$H/$2 SMOKE_MAXT=$3 tools/smoke.sh $SET > $OUT/$1.out 2>&1; grep -o "build/smoke/[0-9-]*" $OUT/$1.out | head -1 > $OUT/$1.dir ); }
OUT=${OUT:-$W/build/memladder}; mkdir -p $OUT; rc=0
run s1new s1_starve.txt 300 & sleep 3; run s1red s1_starve_red.txt 300 & wait
run s2new s2_fuzz.txt 420 9 & wait
run s3rows s3_rows.txt 500 & sleep 3; run s4new s4_break.txt 300 & wait
run s4red s4_break_red.txt 300 & wait
for x in "s1new starve G" "s1red starve R" "s2new fuzz G" "s3rows rows G" "s4new break G" "s4red break R"; do set -- $x
  echo "$1 (${3/G/зелёное ожидается}${3/R/красное ожидается}):"; python3 -I $H/check.py $W/$(cat $OUT/$1.dir)/smoke.txt $2 > $OUT/$1.check; cr=$?; cat $OUT/$1.check
  if [ $3 = G ] && [ $cr != 0 ]; then rc=1; fi; if [ $3 = R ] && [ $cr = 0 ]; then echo "  красный контроль НЕ покраснел"; rc=1; fi
done
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
