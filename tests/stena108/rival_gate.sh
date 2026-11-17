#!/bin/bash
# Скиннер в гонке L1 m1 срезает дворами (107b, Стена; слово Роба 1284). rival_gate.sh PACK_NEW PACK_OLD (PACK_OLD — сборка с -DMC_RIVAL_ROAD108_OLD=1). Нужна строка MCRIVAL shortcuts=1 / 0
set -u; cd "$(dirname "$0")/../.."; fail=0
for k in 1 2; do P=$([ $k = 1 ] && echo "$1" || echo "$2"); SMOKE_HEADLESS=1 SMOKE_SCN=tests/stena108/rival_scn.txt SMOKE_MAXT=300 tools/smoke.sh "$P" >/dev/null 2>&1; d=$(ls -td build/smoke/2* | head -1); l=$(grep -m1 "^MCRIVAL" $d/smoke.txt); echo "  ${l:-нет строки MCRIVAL}"
  if [ $k = 1 ]; then case "$l" in *"shortcuts=1"*) echo "  ok   новое: Скиннер срезает дворами";; *) echo "  FAIL"; fail=1;; esac; else case "$l" in *"shortcuts=0"*) echo "  ok   КРАСНЫЙ (прежнее): только по дороге";; *) echo "  FAIL"; fail=1;; esac; fi; done
[ $fail = 0 ] && echo PASS || { echo FAIL; exit 1; }
