#!/bin/bash
# Гейт №1271: рамка-телевизор у роликов, у которых её не было. tests/movietv1271/run.sh DIR_С_EBOOT — прогоняет демо простоя (tests/idle/scenario.txt, снимок demo) и ролик гэга при чужом пакете (scenario.txt, снимок mid)
# и проверяет рамку по периметру кадра. Зелёный на новом EBOOT; EBOOT из сборки -DMOVIETV1271_OLD=1 даёт красный на гэге; демо в рамке только при flag demotv (по умолчанию без рамки, как в 153)
set -u; cd "$(dirname "$0")/../.."; BAD=0
one() {   # $1 сценарий, $2 имя снимка
  local L R; L=$(SMOKE_MAXT=300 SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_SCN="$PWD/$1" tools/smoke.sh "${DIR}" 2>&1); R=$(echo "$L" | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p')
  [ -n "$R" ] || { echo "FAIL $2: нет отчёта smoke"; BAD=1; return; }
  echo -n "$2: "; .venv/bin/python tests/movietv1271/check.py "$R/$2.png" || BAD=1; grep -h "MOVIETV1271" "$R/smoke.txt" | head -2; rm -rf "$R"
}
DIR="$1"; one tests/movietv1271/scenario.txt mid   # демо простоя: рамка по умолчанию выключена (решение Роба 153), tests/idle/check.py остаётся прежним
exit $BAD
