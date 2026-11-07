#!/bin/bash
# Данные-93: прогон EBOOT на клоне набора в эмуляторе. tools/d93_run.sh EBOOT КЛОН СЕК [сценарий.txt] [OUT.png]
# Клон-тур (scenario.txt уже есть) или игровой (без сценария: режим игрока, экран снимается). Журнал: КЛОН/smoke.txt
EB=$1; C=$2; SEC=$3; SCN=${4:-}; PNG=${5:-}
cp "$EB" "$C/EBOOT.PBP"; rm -f "$C"/smoke.txt "$C"/shot_*.ok "$C"/blackbox.txt "$C"/shar_report.txt
if [ -n "$SCN" ]; then cp "$SCN" "$C/scenario.txt"; touch "$C/smokelog"; fi
if [ -n "$SCN" ] || [ -f "$C/scenario.txt" ]; then PPSSPP_SHOTS="$C/smoke.txt" PPSSPP_SHOTDIR="$C" nice -n 20 "$(dirname "$0")/ppsspp_run.sh" "$C/EBOOT.PBP" "$SEC" "$PNG"
else nice -n 20 "$(dirname "$0")/ppsspp_run.sh" "$C/EBOOT.PBP" "$SEC" "$PNG"; fi
