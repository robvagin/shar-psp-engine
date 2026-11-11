#!/bin/bash
# Подписи кнопок на карточках брифинга внутри рамки (правило ui.c briefFit). Сценарий tests/flow/brieffit_scn.txt (L4, гонка на ставку: △ Cancel Mission).
#   tests/flow/brieffit.sh DIR   — проверка каталога прогона смоука (build/smoke/<время>) зелёным EBOOT: отступ >= 8 px с обеих сторон
#   tests/flow/brieffit.sh DIR red — тот же сценарий EBOOT с -DBRIEFFIT_OLD=1: обязан упасть (отступ справа < 8)
set -u; ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
if [ "${2:-}" = red ]; then python3 tests/flow/brieffit_check.py "$1" && { echo "КРАСНЫЙ НЕ ПОЙМАН"; exit 1; } || { echo "красный BRIEFFIT_OLD упал, как должен"; echo PASS; exit 0; }; fi
python3 tests/flow/brieffit_check.py "$1" && echo PASS
