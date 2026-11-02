#!/bin/sh
# Проверка P18-04 (тип поверхности земли) на CPU Мака, без эмулятора. tests/g12/surface.sh level.shr
# Зелёный: на сетке 10 м по L1 есть трава и грунт (как в отчёте: трава 38 %, грунт 15 % земли; на сетке 10 м: трава > 200, грунт >= 3 точек), spawn (220,172) — трава.
# Красный контроль: сборка с -DSURFACE_OLD=1 (везде дорога) обязана ПРОВАЛИТЬСЯ на том же условии.
set -eu
LV="$1"; D="$(cd "$(dirname "$0")/../.." && pwd)"; T="${TMPDIR:-/tmp}/g12_surf.$$"; mkdir -p "$T"
export SDKROOT="${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}"
PTS=$(awk 'BEGIN{for(x=-300;x<=420;x+=10)for(z=-300;z<=420;z+=10)printf "%d %d ",x,z}')
build() { cc -std=c11 -O2 -Wall $2 -I "$D/viewer" "$D/tests/g12/surface_test.c" "$D/viewer/surface.c" "$D/viewer/col.c" -lm -o "$T/$1"; }
check() { # бинарник -> 0, если трава и грунт найдены
  "$T/$1" "$LV" $PTS | tail -1 | awk '{g=$5; d=$17; print $0; exit !(g>200 && d>=3)}'
}
build new ""; build old "-DSURFACE_OLD=1"
echo "== новый"; check new && echo "ЗЕЛЁНЫЙ: PASS" || { echo "FAIL (новый)"; exit 1; }
echo "== старый (красный контроль)"; if check old; then echo "FAIL: старый код тоже проходит, гейт ничего не доказывает"; exit 1; else echo "красный контроль: старый краснеет, как и должен"; fi
echo "== spawn (220,172): $("$T/new" "$LV" 220 172 | head -1)"
rm -rf "$T"
