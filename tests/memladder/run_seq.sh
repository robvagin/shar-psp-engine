#!/bin/bash
# Правка 435 (лестница памяти) + 432 (MISSION SELECT): приёмка на Маке, безголовый эмулятор. tests/memladder/run.sh НАБОР
#   НАБОР — папка с EBOOT.PBP и данными (символьные ссылки на набор тура + свой EBOOT; уровни 2–7: SHIP_LEVELS). Красный контроль — тот же EBOOT и тот же сценарий с «flag memladder 0» (прежние частные обходы).
#   s1 starve: потребитель уровня забирает всё освобождённое (flag boothstarve), меню открывается с первой попытки только через отложенный блок лестницы
#   s2 fuzz:   бот меню (случайные последовательности кнопок и состояний прохождения, куча раздроблена, отказы пакета), куча = как у PSP (SMOKE_BALLAST=9)
#   s4 break:  отрицательный контроль самопроверки меню: 5 видов порчи (flag msnselbreak 1..5); красный — flag msnselchkoff 1 (проверка молчит)
#   s6 tour:   (Лестница-96, Fable шаг 9) тур L4 с зонами на ballast 5: WATCHDOG mem who=auto <= 2. Красный контроль s6red — НАБОР2 (второй аргумент): EBOOT, собранный с -DMEMLADRES0_OLD=1 -DMEMLADAUTO_OLD=1 -DMEMLADNPC_OLD=1
#   s9 npc:    (Лестница-96, просьба дирижёра 08.11) просьба who=npc на L4 при форсе 250 м (pano917cap), ballast 5: цельный кусок <= 2 с. Красный контроль s9red - тот же набор, сценарий s9_ladnpc_red.txt (flag ladnpcold 1)
#   s11 owner: (Лестница-96, небо и машина игрока, видео Роба Build 97) flag ownerdrop 1/2 выгружает клетки-владельцы: новое - сторож перечитал, SKYDRAW notres=0 за <= 2 строки; красное (flag ownerold 1, тот же EBOOT) неба нет до конца
#   s13 owner1: (Лестница-96, корень отказа клеток неба и машины: CP_MEM берёт область «город», она забита) область «город» 4 МБ (zone3sz) забита flag cityfill, ownerdrop; новое - владельцы из кучи, сторож перечитал, неба нет 0 кадров; красное (flag ownerheap 0) notres=69 до конца
#   s15 toon:  (Лестница-96, разбор журналов Инспектора) L2, миссии 0-3 подряд (нужны flag msnpass/bubshots - integ/test96): оболочки брошенных моделей уходят вместе с моделью, toon <= 20 блоков в «миссии»; красное flag toondrop 0 - 33
#   s16 ringfit: (Лестница-96, тур 99) входы L3, L5, L6 подряд на ballast 5: allocfail за заход <= 4 и BB bad <= 4 (круг старта по месту области города, RINGFIT_OLD); красное flag ringfit 0
#   s16 красный L5: s16cred (flag ringhero 0 + cityfit 0) - на integ L5 allocfail +30; зелёное 3-4. s17 ringhero: круг старта у Гомера (RINGHERO_OLD), released 0 на дальних стартах, красное (flag ringhero 0) 77-234
#   s3 rows:   6/7 строк на уровнях L4, L2, L6 при разных масках пройденных
# Эмулятор один за раз (run_seq.sh); запуски со сдвигом 3 с (имя папки результата — по секундам). Последняя строка PASS/FAIL.
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; SET=${1:?набор}; SET2=${2:-}; cd $W; export SHIP_LEVELS="2 3 4 5 6 7" PATH=$W/.venv/bin:$PATH
[ -x /Library/Developer/CommandLineTools/usr/bin/git ] && export DEVELOPER_DIR=/Library/Developer/CommandLineTools
run() { # имя сценарий maxt [ballast]
  ( [ -n "$4" ] && export SMOKE_BALLAST=$4; SMOKE_HEADLESS=1 SMOKE_FILEDATA="${FD:-meaudio=3}" SMOKE_SCN=$H/$2 SMOKE_MAXT=$3 tools/smoke.sh $SET > $OUT/$1.out 2>&1; grep -o "build/smoke/[0-9-]*" $OUT/$1.out | head -1 > $OUT/$1.dir ); }
OUT=${OUT:-$W/build/memladder}; mkdir -p $OUT; rc=0
run s1new s1_starve.txt 300; run s1red s1_starve_red.txt 300
run s2new s2_fuzz.txt 900 9
run s3rows s3_rows.txt 500; run s4new s4_break.txt 300
run s4red s4_break_red.txt 300
run s6new s6_tour_l4.txt 420 5
run s9new s9_ladnpc.txt 420 5
if [ -n "$SET2" ]; then SET_SAVE=$SET; SET=$SET2; run s6red s6_tour_l4.txt 420 5; SET=$SET_SAVE; fi
run s9red s9_ladnpc_red.txt 420 5
run s11new s11_owner.txt 420 5
run s11red s11_owner_red.txt 420 5
FD="meaudio=3 zone3sz=3072_3072_4096" run s13new s13_l5_start.txt 420 5
FD="meaudio=3 zone3sz=3072_3072_4096" run s13red s13_l5_start_red.txt 420 5
run s15new s15_toondrop.txt 600 5
run s15red s15_toondrop_red.txt 600 5
FD="meaudio=3 zone3sz=3072_3072_4096" run s16new s16_ringfit.txt 260 5
FD="meaudio=3 zone3sz=3072_3072_4096" run s16red s16_ringfit_red.txt 260 5
FD="meaudio=3 zone3sz=3072_3072_4096" run s16cred s16_cityfit_red.txt 260 5
run s17new s17_ringhero.txt 330 5
run s17red s17_ringhero_red.txt 330 5
for x in "s1new starve G" "s1red starve R" "s2new fuzz G" "s3rows rows G" "s4new break G" "s4red break R" "s6new tour G" ${SET2:+"s6red tour R"} "s9new npc G" "s9red npc R" "s11new owner G" "s11red owner R" "s13new owner1 G" "s13red owner1 R" "s15new toon G" "s15red toon R" "s16new ringfit G" "s16red ringfit R" "s16cred ringfit R" "s17new ringhero G" "s17red ringhero R"; do set -- $x
  echo "$1 (${3/G/зелёное ожидается}${3/R/красное ожидается}):"; python3 -I $H/check.py $W/$(cat $OUT/$1.dir)/smoke.txt $2 > $OUT/$1.check; cr=$?; cat $OUT/$1.check
  if [ $3 = G ] && [ $cr != 0 ]; then rc=1; fi; if [ $3 = R ] && [ $cr = 0 ]; then echo "  красный контроль НЕ покраснел"; rc=1; fi
done
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
