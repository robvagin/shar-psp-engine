#!/bin/bash
# Шаг 7 Fable: хост-проверка «висячих указателей» (заливка 0xFF областей): зелёное — с заливкой ловит в free/release/reset; красное — без заливки молчит (проверка сама не врёт).
H=$(cd "$(dirname "$0")" && pwd); export DEVELOPER_DIR=/Library/Developer/CommandLineTools; O=${OUT:-/tmp}/z3dangle.$$
cc -O1 -DZ3_HOST -Wall -Wno-unused-function -o $O $H/dangle.c -lm || exit 2
$O scribble | tail -2; r1=${PIPESTATUS[0]}; $O off | tail -2; r2=${PIPESTATUS[0]}; rm -f $O
[ $r1 = 0 ] && [ $r2 = 0 ] && echo "PASS" || { echo FAIL; exit 1; }
