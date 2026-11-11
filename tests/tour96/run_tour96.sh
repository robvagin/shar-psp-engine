#!/bin/bash
# Тур 96 на эмуляторе: после строки END движок остаётся в меню бонус-игры и сам не выходит (F_BONUS_T7 последняя сцена): когда в smoke.txt появилась строка END, эмулятор убить вручную, smoke.sh дособерёт отчёт. BALLAST=N (МБ) — файл ballast; 1 даёт кучу на старте 39,1 МБ против 35,1 МБ на PSP (95c), 5 даёт 35,1.
cd "$(dirname "$0")/../.."
export SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=${BALLAST:-1} SMOKE_FLAGS="mefxpair" SMOKE_SCN=${1:?набор тура (tour-набор с scenario.txt)}/scenario.txt SMOKE_MAXT=21600 DEVELOPER_DIR=/Library/Developer/CommandLineTools
export PATH=$PWD/.venv/bin:$PATH
echo "start $(date +%H:%M:%S)"
tools/smoke.sh ${1:?набор тура (tour-набор с scenario.txt)} > build/tour96.out 2>&1
echo "end $(date +%H:%M:%S) rc=$?"; tail -5 build/tour96.out
