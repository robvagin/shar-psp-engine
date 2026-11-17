#!/bin/bash
# 107б P0 остаток: спрайты ключей/будки после сна. tests/p0marks/run.sh EBOOT_ТУРА [SET] -> PASS/FAIL
# Сцена: L1, flag simsleep 8 (сон, 8 отказов открытий подряд), flag wsreset 1 (спрайты читаются заново и первые чтения отказывают), через 17 с.
# Новое: после сна phonespr.bin снова загружен (atlas=1); красный wsp0old: атлас не возвращается до перезапуска игры.
cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; export SET=${2:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=1 OUT=${OUT:-$PWD/build/p0marks}; mkdir -p "$OUT"
for v in "new:" "old:wsp0old"; do n=${v%%:*}; x=${v#*:}; FLAGS="$x" nice -n 20 tools/emu_run.sh tests/p0marks/scene.txt $n "$EB" 200 > "$OUT/$n.log" 2>&1; done
bad=0
for n in new old; do f="$OUT/out_$n/smoke.txt"; ok=$(awk '/^SCENE M1/{a=1} a && /^WSPRITE phonespr.bin atlas=1/{c++} END{print c+0}' "$f"); retry=$(grep -c "^WSPRITE retry" "$f"); echo "$n: после сна phonespr.bin загружен раз: $ok, retry=$retry"
  if [ $n = new ]; then [ "$ok" -ge 1 ] || { echo "FAIL новое: атлас будки не вернулся"; bad=1; }
  else [ "$ok" = 0 ] || { echo "КРАСНЫЙ НЕ ПОЙМАН: без лечения атлас вернулся"; bad=1; }; fi; done
rm -rf "$OUT"/rr_* "$OUT"/emu_home_*; [ $bad = 0 ] && echo PASS || echo FAIL; exit $bad
