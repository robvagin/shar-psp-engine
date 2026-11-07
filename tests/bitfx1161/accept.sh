#!/bin/bash
# №1161: удары и поломки предметов без своей системы частиц не берут спрайт дыма машины. Зелёный: щепа, пыль, листья, сено, томаты, обломки — не дым, и вызов стоит в spFxOut.
# Красный (обязан упасть): -DBITFX1161_OLD=1 — все шесть пресетов дымом, как в TEST86.   tests/bitfx1161/accept.sh
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/bitfx1161; mkdir -p "$OUT"
cat > "$OUT/t.c" <<'EOT'
#include <stdio.h>
#include "bitfx1161.h"
int main(void) { int p[] = {SPFX_DUST, SPFX_WOOD, SPFX_LEAVES, SPFX_HAY, SPFX_TOMATO, SPFX_DEBRIS}; int smoke = 0; for (int i = 0; i < 6; i++) { printf("preset %d sprite %d\n", p[i], bit1161Sprite(p[i])); smoke += bit1161Sprite(p[i]) == BIT_SMOKE; } printf("smoke %d\n", smoke); return 0; }
EOT
b() { cc -std=c11 -O1 -w -I viewer "$@" "$OUT/t.c" -o "$OUT/$NAME" && "$OUT/$NAME" > "$OUT/$NAME.txt"; }
NAME=green; b; NAME=old; b -DBITFX1161_OLD=1
g() { grep '^smoke ' "$OUT/$1.txt" | cut -d' ' -f2; }
fail=0
echo "зелёный: пресетов дымом $(g green) из 6; красный BITFX1161_OLD=1: $(g old) из 6"
[ "$(g green)" = 0 ] || { echo "FAIL зелёный: есть дым"; fail=1; }
[ "$(g old)" = 6 ] && echo "красный упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
grep -q 'fxPropBit(o.preset' viewer/main_state.h || { echo "FAIL: spFxOut не зовёт fxPropBit"; fail=1; }
grep -q 'bit1161Sprite(preset)' viewer/fx.c || { echo "FAIL: fxPropBit не по таблице"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
