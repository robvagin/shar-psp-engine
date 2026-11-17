#!/bin/bash
# Гейт 1264 (Бабл, 107) A2.9/A2.10/Б.6/Б.7: уличная гонка. tests/racehold1264/gate.sh EBOOT [индекс миссии=9 (L1 sr1)] [уровень=1]
#   варианты: 0 новое · 1 racehold1264old (пешеходы/стоящие как были) · 2 racefence1264old (заборы сразу). Журнал build/rh1264/<имя>.txt, проверка check.py
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; MI=${2:-9}; L=${3:-1}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py
ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; mkdir -p build/rh1264
for V in 0 1 2; do
  N=rh1264_${V}_L${L}_i${MI}; F=build/rh1264/$N.scn
  { echo "scene SW_${L}_0"; echo "flag camdet 1"; [ "$L" != 1 ] && echo "flag golevel $L"; echo "flag trflog"
    [ "$V" = 1 ] && echo "flag racehold1264old 1"; [ "$V" = 2 ] && echo "flag racefence1264old 1"
    echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 6"; echo "flag msnmain $MI"; echo "wait 3"; echo "press cross"; echo "wait 16"; echo "end"; } > $F
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $F > build/rh1264/$N.log 2>&1
  cp -f $ROOT/build/twins/scen/full/$N/smoke.txt build/rh1264/$N.txt
done
python3 tests/racehold1264/check.py L${L}_i${MI}
