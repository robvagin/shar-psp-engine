#!/bin/bash
# 108 О13: гейт плотного массива следов шин (хост на Маке, настоящие car.c, col.c, skid.c). Зелёный: плотный массив qc сверен с исходными массивами на каждой проверке (bad 0), выбор квадов новым путём
# равен выбору прежним (два обхода) на всех проверках (diff 0, хэши равны), строк кэша PSP по модели не больше трети прежнего. Красный (обязан упасть): -DSKIDDENSE_RED=1 (qlen не обновляет qc) — bad или diff > 0.
# Запуск: tests/skid108/gate.sh [LEVEL.shr]
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
L="${1:-$HOME/Downloads/shar-psp/build/level.shr}"; [ -f "$L" ] || { echo "FAIL: нет $L"; exit 1; }
OUT=build/skid108; mkdir -p "$OUT"
CC=(cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-unused-variable -DSKID_HOST -I viewer)
SRC=(tests/skid108/host.c viewer/skid.c viewer/car.c viewer/col.c)
b() { local name=$1; shift; "${CC[@]}" "$@" "${SRC[@]}" -lm -o "$OUT/$name" 2> "$OUT/$name.build" || { echo "FAIL: сборка $name"; head -5 "$OUT/$name.build"; exit 1; }; "$OUT/$name" "$L" > "$OUT/$name.txt"; }
b green; b red -DSKIDDENSE_RED=1
g() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
fail=0
echo "проверок $(g green checks), квадов в кольце до $(g green quads_max), выбрано $(g green selected); bad $(g green bad), diff $(g green diff), хэш новый $(g green hashnew) / старый $(g green hashold)"
[ "$(g green bad)" = 0 ] && [ "$(g green diff)" = 0 ] && [ "$(g green hashnew)" = "$(g green hashold)" ] || { echo "FAIL зелёный: плотный массив или выбор расходятся с прежним"; fail=1; }
awk -v o="$(g green lines_old)" -v n="$(g green lines_new)" 'BEGIN{exit !(n <= o/3)}' || { echo "FAIL зелёный: строк кэша не меньше трети прежнего ($(g green lines_old) → $(g green lines_new))"; fail=1; }
if [ "$(g red bad)" = 0 ] && [ "$(g red diff)" = 0 ]; then echo "КРАСНЫЙ SKIDDENSE_RED НЕ ПОЙМАН"; fail=1; else echo "красный SKIDDENSE_RED=1: упал, как должен (bad $(g red bad), diff $(g red diff))"; fi
echo "строк кэша PSP за выбор (модель): $(g green lines_old) → $(g green lines_new); время выбора, мкс на Маке с холодным кэшем: $(g green us_old) → $(g green us_new) (на PSP ×5,8)"
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
