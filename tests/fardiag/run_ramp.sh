#!/bin/bash
# tests/fardiag/run_ramp.sh УРОВЕНЬ old|new — один прогон A/B рампы границы дали (покой + езда 300 с); old = файл dalrampold
W=$(cd "$(dirname "$0")/../.." && pwd); export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
F="ballast=3"; [ "$2" = old ] && F="ballast=3 dalrampold"
SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test88/PSP/GAME/SHARTEST15} OUT=$W/build/ramp/run OVL=$W/build/g90/data FLAGS="$F" EMU_OWNER=g13-far EMU_MINE=3 \
  bash $W/tools/emu_run.sh $W/tests/fardiag/ramp_L$1.txt ramp_L$1_$2 ${EBOOT:-$W/build/ramp_EBOOT.PBP} 3000
