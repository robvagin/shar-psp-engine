#!/bin/bash
# Круг 7 (g11): частица ATC подвижного предмета — порог импульса 5 × масса и «не разбит». Гейт на хосте: ящик 125 кг, машина 1500 кг: на 3 м/с частицы нет, на 8 м/с есть, обломок вместо предмета — нет;
# лёгкий предмет (урна 15 кг) — то же порог от 4,5 м/с (приведённая масса почти равна массе предмета, порог от массы не зависит). Красный: -DPROPFXIMP_SV_OLD=1 — частица на любой скорости и вместе с обломком.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/propfx; mkdir -p "$OUT"
cat > "$OUT/t.c" <<'EOT'
#include <stdio.h>
#include "props.h"
int main(void) { printf("box3 %d\nbox8 %d\nbroken8 %d\nbin8 %d\nbin3 %d\n", propFxOk(1500.f, 125.f, 3.f, 0), propFxOk(1500.f, 125.f, 8.f, 0), propFxOk(1500.f, 125.f, 8.f, 1), propFxOk(1500.f, 15.f, 8.f, 0), propFxOk(1500.f, 15.f, 3.f, 0)); return 0; }
EOT
b() { cc -std=c11 -O2 -w -I viewer "$@" "$OUT/t.c" -o "$OUT/$NAME" && "$OUT/$NAME" > "$OUT/$NAME.txt"; }
NAME=green; b; NAME=old; b -DPROPFXIMP_SV_OLD=1
g() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
echo "зелёный: ящик 3 м/с $(g green box3), 8 м/с $(g green box8), с обломком $(g green broken8); урна 8 м/с $(g green bin8), 3 м/с $(g green bin3)"
ok() { [ "$(g $1 box3)" = 0 ] && [ "$(g $1 box8)" = 1 ] && [ "$(g $1 broken8)" = 0 ] && [ "$(g $1 bin8)" = 1 ] && [ "$(g $1 bin3)" = 0 ]; }
fail=0; ok green || { echo "FAIL зелёный"; fail=1; }
ok old && { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; } || echo "красный PROPFXIMP_SV_OLD=1: упал, как должен — ящик 3 м/с $(g old box3), с обломком $(g old broken8)"
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
