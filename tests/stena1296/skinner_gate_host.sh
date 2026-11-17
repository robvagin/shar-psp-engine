#!/bin/bash
# 1309 гейт на хосте (детерминированно): Скиннер L2 m1 «оторваться» едет к стоящему игроку из 13 точек. PASS: прежнее (SK_OLD: без торможения по кривизне и по носу) даёт >= 30 ударов о стену дальше 30 м от игрока (красный контроль),
# новое — не больше трети от прежнего; доехали к игроку за 40 с не меньше 9 из 13.   tests/stena1296/skinner_gate_host.sh [SET]
cd "$(dirname "$0")/../.."; O=$(VERBOSE=1 tests/stena1296/skinner_sweep.sh "$@"); echo "$O" | grep -v "^   "
ow=$(echo "$O" | sed -n 's/^old: ударов о стену далеко от игрока \([0-9]*\).*/\1/p'); nw=$(echo "$O" | sed -n 's/^new: ударов о стену далеко от игрока \([0-9]*\).*/\1/p')
arr=$(echo "$O" | grep "^   new " | grep -vc 'прибыл=-1.0')
echo "доехали новое: $arr из 13"
S=${1:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15}
sn=$(echo "$O" | grep "^   new " | sed 's/.*остановка=\([0-9.]*\).*/\1/' | sort -n | tail -1)
sr=$(SK_NOSLIP=1 build/sk1309/host $S/l2/level.shr $S/homer.shr $S/l2/level02.msn 40 50 50 2>&1 | grep stall_frames | sed 's/.*longest=\([0-9.]*\).*/\1/')
echo "самая долгая остановка новое: $sn с; красный (потеря управления не убывает, точка 50,50): $sr с"
python3 -c "import sys; sys.exit(0 if float('${sn:-99}') <= 5.0 and float('${sr:-0}') >= 10.0 else 1)" || { echo "FAIL остановки"; exit 1; }
[ "${ow:-0}" -ge 30 ] && [ "${nw:-99}" -le $(( ow / 3 )) ] && [ "$arr" -ge 9 ] && echo PASS || { echo FAIL; exit 1; }
