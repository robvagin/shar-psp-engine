#!/bin/bash
# Гейт «чёрный ящик у игрока не пишет синхронно» (аудит диагностики 16.10). Безголовый эмулятор, тур-сборка (SHAR_RELEASE + KEEP_DIAG + KEEP_SMOKE).
#   tools/bb_sync_gate.sh EBOOT.PBP НАБОР [ВЫХОД]            зелёный: счётчик синхронных записей ящика на пути игрока (NEW GAME 35 с) = 0, кроме fail/melad;
#                                                              после «flag freeze» ящик хранит последнюю строку (tools/bb_last_line.txt)
#   BB_EXPECT_FAIL=1 tools/bb_sync_gate.sh …                 красный контроль: EBOOT старого кода (или сборка -DBB_SYNC_OLD=1) обязан упасть по синхронным записям
. "$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)/linux_env.sh"   # Linux: обёртки stat/cp/sed под синтаксис macOS (tools/linux_env.sh)
set -u
cd "$(dirname "$0")/.."; ROOT=$PWD
EB=${1:?EBOOT.PBP}; PK=${2:?НАБОР (папка данных)}; OUT=${3:-$ROOT/build/bb_sync_gate/$(date +%H%M%S)}
[ -f "$EB" ] && [ -d "$PK" ] || { echo "ЯЩИК: FAIL нет EBOOT или набора"; exit 2; }
mkdir -p "$OUT" "$ROOT/build"; TMP="$ROOT/build/bbgate_pkg_$$"
cleanup() { [ -n "${TMP:-}" ] && [ -d "$TMP" ] && rm -rf "${TMP:?}"; }; trap cleanup EXIT
cp -cR "$PK" "$TMP" 2>/dev/null || cp -R "$PK" "$TMP"; cp "$EB" "$TMP/EBOOT.PBP"
run() {   # run ИМЯ СЦЕНАРИЙ МАКС_С
  ( SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_SCN="$2" SMOKE_MAXT=$3 nice -n 20 tools/smoke.sh "$TMP" ) > "$OUT/$1.out" 2>&1
  D="$ROOT/build/smoke/$(ls -t "$ROOT/build/smoke" | head -1)"; cp -R "$D" "$OUT/$1"
}
[ "${BB_ONLY_LAST:-0}" = 1 ] || run new "$ROOT/tools/frame_gate_new.txt" 160
run last "$ROOT/tools/bb_last_line.txt" 300
LOGS=(); for r in new; do [ "${BB_ONLY_LAST:-0}" = 1 ] && break; [ -f "$OUT/$r/smoke.txt" ] && LOGS+=("$OUT/$r/smoke.txt"); done
RC=0
if [ "${BB_EXPECT_FAIL:-0}" = 1 ]; then python3 -I tools/bb_sync_gate.py "${LOGS[@]:-}" --expect-fail | tee "$OUT/bb_sync_gate.txt"; RC=${PIPESTATUS[0]}
else python3 -I tools/bb_sync_gate.py "${LOGS[@]:-}" | tee "$OUT/bb_sync_gate.txt"; RC=${PIPESTATUS[0]}; fi
BB="$OUT/last/blackbox.txt"
if [ -f "$BB" ] && grep -q "BBTEST_LAST_LINE" "$BB"; then echo "ЯЩИК ПОСЛЕ ЗАВИСАНИЯ: PASS (последняя строка BBTEST_LAST_LINE на месте: $(grep -m1 -o 'STALL[^|]*' "$BB" | head -c 60))"
else echo "ЯЩИК ПОСЛЕ ЗАВИСАНИЯ: FAIL (в $BB нет BBTEST_LAST_LINE)"; [ "${BB_EXPECT_FAIL:-0}" = 1 ] || RC=1; fi
exit $RC
