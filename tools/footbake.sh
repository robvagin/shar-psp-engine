#!/bin/bash
# Модели-96: запечь поправку стоп (tools/footbake.c) в CHR1-файлы: tools/footbake.sh FILE...  Бинарник собирается в build/tools/footbake (хост-cc), пересобирается при смене skin.c/skin.h/footbake.c
set -e
cd "$(dirname "$0")/.."
B=build/tools/footbake; mkdir -p build/tools
if [ ! -x "$B" ] || [ viewer/skin.c -nt "$B" ] || [ viewer/skin.h -nt "$B" ] || [ tools/footbake.c -nt "$B" ] || [ viewer/traffic.c -nt "$B" ]; then
  cc -std=gnu99 -O2 -w -DSKIN_HOST -Iviewer tools/footbake.c viewer/skin.c viewer/traffic.c viewer/car.c viewer/col.c viewer/watchdog.c -lm -o "$B.$$" && mv "$B.$$" "$B"
fi
exec "$B" "$@"
