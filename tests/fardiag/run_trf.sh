#!/bin/bash
# №1202: tests/fardiag/run_trf.sh УРОВЕНЬ — езда автопилотом 150 с с журналом TRFPOP/TRFSPAWN (появление машин трафика и стоящих)
W=$(cd "$(dirname "$0")/../.." && pwd); export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test88/PSP/GAME/SHARTEST15} OUT=$W/build/trf/run OVL=$W/build/g90/data FLAGS="ballast=3 ${EXTRA_FLAGS:-}" EMU_OWNER=g13-far EMU_MINE=3 \
  bash $W/tools/emu_run.sh $W/tests/fardiag/trf_L$1${SCN:-}.txt trf_L$1${TAG:-} ${EBOOT:-$W/build/trf_EBOOT.PBP} 3000
