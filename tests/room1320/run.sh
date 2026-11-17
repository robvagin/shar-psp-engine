#!/bin/bash
# 1320: tests/room1320/run.sh EBOOT [SET]. new (зелёный), old через flag roomreload1320 0 (красный контроль: new-проверка на нём краснеет), старая сборка не нужна.
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; cd $W; E=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); O=${OUT:-$W/build/room1320}; mkdir -p $O/ovl_empty
export SET=${2:-$W/build/test108tour/PSP/GAME/SHARTEST15} OUT=$O OVL=$O/ovl_empty SMOKE_HEADLESS=1 SHIP_LEVELS="2 3 4 5 6 7" EMU_OWNER=Room1320 EMU_MINE=1
python3 $H/gen.py 5 > $O/new.txt; python3 $H/gen.py 5 old > $O/old.txt; bad=0
for n in new old; do bash tools/emu_run.sh $O/$n.txt $n $E 300 > /dev/null 2>&1; python3 $H/check.py $O/out_$n/smoke.txt $n || bad=1; done
if python3 $H/check.py $O/out_old/smoke.txt new > /dev/null 2>&1; then echo "FAIL красный контроль не покраснел"; bad=1; else echo "RED ok (старое краснеет на проверке нового)"; fi
[ $bad = 0 ] && echo "ROOM1320 PASS" || echo "ROOM1320 FAIL"; exit $bad
