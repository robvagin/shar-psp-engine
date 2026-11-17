#!/bin/bash
# Правки 1264/1267/1268 (Бабл, 107): сообщение этапа посреди миссии. Старт миссии M с этапа S (msnmainat M*100+S), построчный журнал пузыря.
#   tests/bubble1264/run.sh EBOOT УРОВЕНЬ МИССИЯ ЭТАП [старый:0|1]  → build/bub1264/<имя>.txt
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; L=${2:-1}; M=${3:-3}; ST=${4:-3}; OLD=${5:-0}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py
ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; mkdir -p build/bub1264
N=bub1264_${OLD}_L${L}_m${M}s${ST}; F=build/bub1264/$N.scn
{ echo "scene SW_${L}_0"; echo "flag camdet 1"; echo "flag camlog"; echo "flag startcam 1"; [ "$L" != 1 ] && echo "flag golevel $L"
  echo "flag bubtrace"; [ "${AVD:-0}" != 0 ] && echo "flag avdelay $AVD"; [ "$OLD" != 0 ] && echo "flag bubble1264old $OLD"
  echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag msnmainat ${CODE:-$((M*100+ST))}"; [ -n "${CODE2:-}" ] && { echo "wait ${WAIT2:-0.3}"; echo "flag msnmainat $CODE2"; }; echo "wait 3"; echo "press cross"; echo "wait 2"; echo "press cross"; echo "wait ${WAITS:-8}"; echo "shot a"; for i in $(seq 1 ${PASSES:-0}); do echo "flag msnhud"; echo "flag msnpass"; echo "wait ${WAITP:-8}"; echo "shot p$i"; done; [ -n "${ONEOFF:-}" ] && { echo "flag uipopmsg 1"; echo "wait ${ONEOFF}"; echo "shot z"; echo "flag msnhud"; }; echo "flag msnhud"; echo "wait 4"; echo "end"; } > $F
SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $F > build/bub1264/$N.log 2>&1
cp -f $ROOT/build/twins/scen/full/$N/smoke.txt build/bub1264/$N.txt 2>/dev/null || echo "нет smoke.txt"
echo DONE $N
