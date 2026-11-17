#!/bin/bash
# 107б P0 (мой кусок): файл уровня и colring.shr после сна. tests/p0level/run.sh EBOOT_ТУРА [SET]  -> PASS/FAIL
# Сцена: L1, езда, на 12 с flag simsleep 5 (power.h: сон, kill-хуки закрывают файлы уровня и colring.shr, 5 испорченных чтений/открытий подряд, пробуждение), потом 45 с езды.
# Новое: после пробуждения загрузчик и кольцо переоткрывают файлы (COLRING reopen ok, WATCHDOG colring ioerr конечен), клеток и страниц догружается столько же, дыр видимых мало; красный: с p0levelold и p0colringold — ioerr идёт непрерывно / клетки не догружаются.
cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; export SET=${2:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=1 OUT=${OUT:-$PWD/build/p0level}; mkdir -p "$OUT"
for v in "new:" "old:p0levelold p0colringold"; do n=${v%%:*}; x=${v#*:}; FLAGS="ballast=5 $x" nice -n 20 tools/emu_run.sh tests/p0level/scene.txt $n "$EB" 200 > "$OUT/$n.log" 2>&1; done
python3 -I "$(dirname "$0")/check.py" "$OUT/out_new/smoke.txt" "$OUT/out_old/smoke.txt" one | tee "$OUT/p0level_gate.txt"; rc1=${PIPESTATUS[0]}
for v in "n40:" "o40:p0levelold p0colringold"; do n=${v%%:*}; x=${v#*:}; FLAGS="ballast=5 $x" nice -n 20 tools/emu_run.sh tests/p0level/scene40.txt $n "$EB" 200 > "$OUT/$n.log" 2>&1; done   # 40 снов подряд: дескрипторы не текут
python3 -I "$(dirname "$0")/check.py" "$OUT/out_n40/smoke.txt" "$OUT/out_o40/smoke.txt" many | tee -a "$OUT/p0level_gate.txt"; rc2=${PIPESTATUS[0]}; rm -rf "$OUT"/rr_* "$OUT"/emu_home_*; [ $rc1 = 0 ] && [ $rc2 = 0 ]; exit $?
