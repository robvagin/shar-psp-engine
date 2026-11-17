#!/bin/bash
# Гейт №1269: tests/briefpic1269/run.sh DIR_С_EBOOT_И_ДАННЫМИ. Зелёный на новом EBOOT; EBOOT из сборки -DBRIEFPIC1269_OLD=1 даёт красный
set -u; cd "$(dirname "$0")/../.."
L=$(SMOKE_MAXT=300 SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_SCN="$PWD/tests/briefpic1269/scenario.txt" tools/smoke.sh "$1" 2>&1); R=$(echo "$L" | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p')
[ -n "$R" ] || { echo "FAIL: нет отчёта smoke"; exit 1; }
.venv/bin/python tests/briefpic1269/check.py "$R"; rc=$?; rm -rf "$R"; exit $rc
