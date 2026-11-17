#!/bin/bash
# О12 (108, Стена): свет не обходит клетки, пока не загрузилась новая; результат тот же. slscan_gate.sh PACK_NEW [red PACK_RED]
# PASS: пропусков >= 200 и расхождений 0 (SLSCANCHECK). Красный: EBOOT с -DSLSCAN108_BREAK=1 (пропуск без учёта загрузок) — расхождения обязаны появиться
set -u; cd "$(dirname "$0")/../.."; P=$1; [ "${2:-}" = red ] && P=${3:?нужен пакет красного контроля}
SMOKE_HEADLESS=1 SMOKE_SCN=tests/stena108/slscan_check.txt SMOKE_MAXT=300 tools/smoke.sh "$P" >/dev/null 2>&1; d=$(ls -td build/smoke/2* | head -1)
l=$(grep "SLSCANCHECK" $d/smoke.txt | tail -1); echo "  $l"; sk=$(echo "$l" | sed 's/.*skipped=\([0-9]*\).*/\1/'); mis=$(echo "$l" | sed 's/.*mismatches=\([0-9]*\).*/\1/')
if [ "${2:-}" = red ]; then [ "${mis:-0}" -gt 0 ] && { echo "  ok   красный краснеет: расхождений $mis"; echo PASS; } || { echo "  FAIL красный не покраснел"; echo FAIL; exit 1; }
else [ "${sk:-0}" -ge 200 ] && [ "${mis:-1}" = 0 ] && { echo "  ok   пропусков $sk, расхождений 0"; echo PASS; } || { echo "  FAIL пропусков ${sk:-?}, расхождений ${mis:-?}"; echo FAIL; exit 1; }; fi
