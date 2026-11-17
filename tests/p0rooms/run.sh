#!/bin/bash
# 107б P0 остаток: комната (rgBad) после сна. tests/p0rooms/run.sh EBOOT_ТУРА [SET] -> PASS/FAIL
# Сцена: L1, дверь Kwik-E-Mart; flag simsleep 6 перед входом (чтение комнаты отказывает) — дверь не впускает; через 15 с с отхода вход заново.
# Новое: после сна комната пробуется заново (ROOM retry) и вход удаётся (ROOMANIM parse); красный p0roomold: err=2 («битая, больше не пробуем») до смены уровня.
cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; export SET=${2:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=1 OUT=${OUT:-$PWD/build/p0rooms}; mkdir -p "$OUT"
for v in "new:" "old:p0roomold"; do n=${v%%:*}; x=${v#*:}; FLAGS="ballast=5 $x" nice -n 20 tools/emu_run.sh tests/p0rooms/scene.txt $n "$EB" 200 > "$OUT/$n.log" 2>&1; done
bad=0
for n in new old; do f="$OUT/out_$n/smoke.txt"; en=$(awk '/^SCENE R_retry/{a=1} /^SCENE R_end/{a=0} a && /^ROOMANIM parse/{c++} END{print c+0}' "$f"); e2=$(awk '/^SCENE R_retry/{a=1} /^SCENE R_end/{a=0} a && /^ROOM ev=fail room=[A-Za-z]* err=2/{c++} END{print c+0}' "$f"); rt=$(grep -c "^ROOM retry" "$f"); echo "$n: после повторного подхода входов $en, отказов err=2 $e2, ROOM retry $rt"
  if [ $n = new ]; then [ "$en" -ge 1 ] && [ "$e2" = 0 ] || { echo "FAIL новое: дверь не впустила после сна"; bad=1; }
  else [ "$e2" -ge 1 ] || { echo "КРАСНЫЙ НЕ ПОЙМАН: без лечения дверь впустила"; bad=1; }; fi; done   # старое: дверь отказывает (err=2); потом комнату открывает только перезагрузка уровня
rm -rf "$OUT"/rr_* "$OUT"/emu_home_*; [ $bad = 0 ] && echo PASS || echo FAIL; exit $bad
