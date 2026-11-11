#!/bin/bash
# Вписывание ВСЕХ заданий, провалов, подсказок и брифингов уровня в рамки при данных выпуска (flag uifit, viewer/msn_view.c): итог по уровню UIFIT total=N over=M и список «over».
#   RELDATA=<data выпуска> tests/bubble1247/fit.sh EBOOT [уровни]      (по одному, один эмулятор)
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; LV=${2:-1 2 3 4 5 6 7}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; mkdir -p build/bub1247
for L in $LV; do
  N=bub1251fit_L$L; F=build/bub1247/$N.scn
  { echo "scene SW_${L}_0"; [ "$L" != 1 ] && echo "flag golevel $L"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag uifit"; echo "wait 2"; echo "end"; } > $F
  if [ -n "${RELDATA:-}" ]; then OV="ui.shr=$RELDATA/ui.shr,ui.shr.fonts=$RELDATA/ui.shr.fonts,pmenu.shr=$RELDATA/pmenu.shr,pmenu.shr.fonts=$RELDATA/pmenu.shr.fonts"; [ -f "$RELDATA/l$L/ui.shr" ] && OV="$OV,l$L/ui.shr=$RELDATA/l$L/ui.shr"; [ -f "$RELDATA/l$L/pmenu.shr" ] && OV="$OV,l$L/pmenu.shr=$RELDATA/l$L/pmenu.shr,l$L/pmenu.shr.fonts=$RELDATA/l$L/pmenu.shr.fonts"; export SCEN_OVERRIDE="$OV"; fi
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $F > build/bub1247/$N.log 2>&1
  f=$ROOT/build/twins/scen/full/$N/smoke.txt
  echo "== L$L: $(grep -a 'UIFIT total' $f | tail -1)"; grep -a 'UIFIT over' $f | cut -c1-200
  rm -rf $ROOT/build/twins/scen/full/$N
done
echo ALLDONE
