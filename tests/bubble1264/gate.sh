#!/bin/bash
# Гейт 1264/1267/1268: tests/bubble1264/gate.sh EBOOT — прогоны по одному (эмулятор один), затем check.py
set -u; cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; R=tests/bubble1264/run.sh
for O in 0 1; do CODE=-302 PASSES=1 WAITP=6 $R $EB 1 3 p0s2 $O; done
PASSES=1 WAITP=10 $R $EB 1 3 2 0
for O in 0 2; do AVD=2.5 CODE=302 PASSES=1 WAITP=10 $R $EB 1 3 s2avd $O; done
for O in 0 4; do WAIT2=0.01 CODE=-302 CODE2=-303 $R $EB 1 3 p0s2d $O; done
python3 tests/bubble1264/check.py
