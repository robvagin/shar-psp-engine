#!/bin/bash
# 108 правка 1293 (Бёрнс в L1 m7 не виден до разговора): tests/npc1293/run.sh EBOOT [SET] -> PASS/FAIL
# Точка Бёрнса (−197,6; 3,75; 84,7) на реальных клетках L1 при наборе районов 4, 10, 11 (маска 0x608) и клетках, загруженных только из набора: NPCSCENETEST старое правило old=0 («сцена не готова» вечно, красный), новое new=1.
cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; export SET=${2:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=1 SHIP_LEVELS="1 2 3 4 5 6 7" OUT=${OUT:-$PWD/build/npc1293}; mkdir -p "$OUT"; rm -rf "$OUT"/out_bb
FLAGS="" nice -n 20 tools/emu_run.sh tests/npc1293/scene_burns.txt bb "$EB" 200 > "$OUT/bb.log" 2>&1
L=$(grep -h "^NPCSCENETEST" "$OUT/out_bb/smoke.txt" | tail -1); echo "$L"
echo "$L" | grep -q "old=0 new=1" && echo PASS || { echo FAIL; exit 1; }
