#!/bin/bash
# 1350 (Роб): рамка задания на паузе миссии — по центру экрана по горизонтали, под рамкой меню паузы, не заходит на меню, размер и вид как у HUD (не деформируется).
#   tests/pauseobj1350/run.sh EBOOT [КОД ...]   (по умолчанию 302 короткий текст, 303 длинный: рамка растёт вверх)
# Прогоны на код: 0 новое, 1 PAUSEOBJPOS1350_OLD=1 (как 1347, на месте HUD), 2 без окна задания (эталон меню). Итог: check.py
set -u; cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; shift; CODES=${*:-302 303}
OUT=${OUT:-build/pauseobj1350} OLDFLAG=pauseobjpos1350old VARS="0 1 2" tests/pauseobj1347/run.sh $EB $CODES > /dev/null 2>&1
python3 tests/pauseobj1350/check.py ${OUT:-build/pauseobj1350} $CODES
