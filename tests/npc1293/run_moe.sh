#!/bin/bash
# 108 правка 1314 (Мо в таверне не виден на этапе «TALK TO MOE», L2 m2): tests/npc1293/run_moe.sh EBOOT [SET] -> PASS/FAIL
# Путь: L2, flag msnmainat 105 (этап «ENTER MOE'S»), вход в таверну △ → этап 6 «TALK TO MOE». Новое: строк «NPCVIS hide moe» нет; красный flag npcroom1314 0 — строки идут (тег интерьера moe1 ≠ комната Moes).
cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; export SET=${2:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=1 SHIP_LEVELS="1 2 3 4 5 6 7" OUT=${OUT:-$PWD/build/npc1293}; mkdir -p "$OUT"; rm -rf "$OUT"/out_mn "$OUT"/out_mo
FLAGS="" nice -n 20 tools/emu_run.sh tests/npc1293/scene_moe_msn5.txt mn "$EB" 300 > "$OUT/mn.log" 2>&1
FLAGS="" nice -n 20 tools/emu_run.sh tests/npc1293/scene_moe_msn5_old.txt mo "$EB" 300 > "$OUT/mo.log" 2>&1
n=$(grep -c "^NPCVIS hide moe" "$OUT/out_mn/smoke.txt"); o=$(grep -c "^NPCVIS hide moe" "$OUT/out_mo/smoke.txt"); st=$(grep -c "^MSN stage m2.1.6" "$OUT/out_mn/smoke.txt")
echo "этап TALK TO MOE достигнут $st раз; скрыт Мо: новое $n строк, прежнее $o строк"
[ "$st" -ge 1 ] && [ "$n" = 0 ] && [ "$o" -ge 3 ] && echo PASS || { echo FAIL; exit 1; }
