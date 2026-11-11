#!/bin/bash
# Титры (View Credits и конец игры): △ ○ Start Select не выходят, ✕ выходит (flowCreditsExit, хост-тест без данных); подсказки Back на экране титров нет — её прячет движок (ui.c credHideBack),
# пакет menu.shr не меняется. Красный контроль: CREDITSX_OLD=1 (△ выходит, Back на месте) падает. Эмулятор: tests/flow/credits_exit_scn.txt + credits_exit_check.py
#   tests/flow/credits_exit.sh            — хост-часть
#   tests/flow/credits_exit.sh DIR        — плюс проверка каталога прогона смоука (build/smoke/<время>)
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; mkdir -p build/flow
cat > build/flow/credits_exit.c <<'C'
#include <stdio.h>
#include "flowrules.h"
int main(void) {
  int tri = flowCreditsExit(FLOW_PAD_TRIANGLE), cir = flowCreditsExit(FLOW_PAD_CIRCLE), crs = flowCreditsExit(FLOW_PAD_CROSS), st = flowCreditsExit(0x8u), sel = flowCreditsExit(0x1u);
  int ok = !tri && !cir && crs && !st && !sel;
  printf("tri %d cir %d cross %d start %d select %d (ждём 0 0 1 0 0) %s\n", tri, cir, crs, st, sel, ok ? "OK" : "FAIL"); return ok ? 0 : 1; }
C
cc -std=c11 -Wall -I viewer build/flow/credits_exit.c -o build/flow/ce_green && cc -std=c11 -DCREDITSX_OLD=1 -I viewer build/flow/credits_exit.c -o build/flow/ce_red || exit 1
build/flow/ce_green || { echo "FAIL код зелёный"; exit 1; }
build/flow/ce_red && { echo "КРАСНЫЙ (код) НЕ ПОЙМАН"; exit 1; } || echo "красный CREDITSX_OLD (код) упал, как должен"
[ $# -ge 1 ] && { "$ROOT/.venv/bin/python" tests/flow/credits_exit_check.py "$1" || exit 1; }
echo PASS
