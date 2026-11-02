#!/bin/bash
# №1133: хост-тест смены машины в будке без затемнения + пятно-свечение. Мак, CPU; PSP и эмулятор не нужны.
#   booth_swap.sh        — зелёный (новое) и красные контроли: BOOTHFADE1133_OLD=1 (затемнение смены) и BOOTHGLOW1133_OLD=1 (пятно) обязаны упасть
set -u
export PATH=~/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; T=tests/g07b_sverka8; OUT=build/g07b_sverka8; mkdir -p $OUT/data; cp $T/phonecars_test.txt $OUT/data/phonecars.txt
b() { cc -std=gnu11 -O1 -DSKIN_HOST -Wno-misleading-indentation $2 -I $T/stub -I viewer $T/booth_swap_host.c -lm -o $OUT/$1 2> $OUT/$1.build.txt || { echo "FAIL: сборка $1"; head -5 $OUT/$1.build.txt; exit 1; }; }
rc=0
b swap_new ""; if $OUT/swap_new $OUT/data/ > $OUT/swap_new.txt; then echo "зелёный: $(tail -1 $OUT/swap_new.txt)"; else echo "FAIL: новое не прошло"; cat $OUT/swap_new.txt | head; rc=1; fi
for d in BOOTHFADE1133_OLD BOOTHGLOW1133_OLD; do
  b red_$d "-D$d=1"; if $OUT/red_$d $OUT/data/ > $OUT/red_$d.txt; then echo "КРАСНЫЙ НЕ ПОЙМАН: $d=1 прошёл"; rc=1; else echo "красный ($d=1): упал, как должен — $(grep -m1 FAIL: $OUT/red_$d.txt | cut -c7-100)"; fi
done
# данные: пятно за машиной остаётся с отдельным тегом (движок его прячет); красный — PHONE_HUSK_OLD=1 (пятно без тега)
PY=~/Downloads/shar-psp/.venv/bin/python
if $PY $T/glow_check.py > $OUT/glow.txt 2>&1; then echo "зелёный (данные): $(tail -1 $OUT/glow.txt)"; else echo "FAIL: данные"; tail -3 $OUT/glow.txt; rc=1; fi
if PHONE_HUSK_OLD=1 $PY $T/glow_check.py > $OUT/glow_red.txt 2>&1; then echo "КРАСНЫЙ НЕ ПОЙМАН: PHONE_HUSK_OLD=1"; rc=1; else echo "красный (PHONE_HUSK_OLD=1): упал, как должен"; fi
exit $rc
