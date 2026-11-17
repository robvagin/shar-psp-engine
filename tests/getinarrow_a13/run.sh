#!/bin/bash
# A1.3: tests/getinarrow_a13/run.sh DIR_С_EBOOT. Зелёный: в журнале MSN getin-arrow drawn; EBOOT из сборки -DMVGETINARROW_OLD=1 — красный
set -u; cd "$(dirname "$0")/../.."
L=$(SMOKE_MAXT=300 SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_SCN="$PWD/tests/getinarrow_a13/scenario.txt" tools/smoke.sh "$1" 2>&1); R=$(echo "$L" | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p')
[ -n "$R" ] || { echo "FAIL: нет отчёта smoke"; exit 1; }
if grep -q "MSN stage m0.0.1.*obj=getin\|obj=getin" "$R/smoke.txt"; then :; else echo "FAIL: этап getin не наступил"; rm -rf "$R"; exit 1; fi
l=$(grep -m1 "MSN getin-arrow drawn" "$R/smoke.txt"); rm -rf "$R"
[ -n "$l" ] && { echo "OK: $l"; exit 0; } || { echo "FAIL: стрелки getin нет (нет строки MSN getin-arrow drawn)"; exit 1; }
