#!/bin/bash
# build/homer.shr свежий или пересобран (04.11, окно gates; tools/fresh.sh): гейты (tests/homer/host.sh, tests/cars/cars.sh …) зовут его вместо «если файл есть — берём».
# Код 0 — файл свежий (проверен штамп) или пересобран и проштампован; 77 — «НЕ СВИДЕТЕЛЬ»: файл устарел/без штампа, а пересобрать нельзя (нет .venv или диска). HOMER_FRESH_OLD=1 — прежнее (файл как есть).
cd "$(dirname "$0")/.."
[ "${HOMER_FRESH_OLD:-0}" = 1 ] && { [ -f build/homer.shr ] && exit 0; }
tools/fresh.sh check build/homer.shr tools/homer_export.py > /dev/null 2>&1 && exit 0
{ source .venv/bin/activate && python tools/homer_export.py > /dev/null; } 2>/dev/null && [ -f build/homer.shr ] && tools/fresh.sh stamp build/homer.shr tools/homer_export.py > /dev/null && exit 0
echo "НЕ СВИДЕТЕЛЬ: build/homer.shr устарел или без штампа, пересобрать не удалось (нужны .venv и диск hit-and-run-web/source/game)"; exit 77
