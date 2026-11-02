#!/bin/bash
# №1131 (g11): капли кислоты АЭС заметнее и ярче. Хост (арифметика viewer/acid1131.h, без эмулятора). Зелёный: столб непрозрачнее (альфа сверху 150 против 70, снизу 70 против 22, пятно 120 против 50), капли идут:
# на столбе 15 м и трубе 4 м хотя бы одна капля есть не меньше чем в 40 % моментов, капли в пределах столба, хвост 0,5–1,9 м, падение без подскоков. Красный (обязан упасть): -DACID1131_OLD=1.
# Снимки до/после для Роба: сценарий tests/acid1131/scenario.txt в эмуляторе (флаг acid1131old 1 — до, 0 — после).
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/acid1131; mkdir -p "$OUT"
b() { cc -std=c11 -O2 -w -I viewer "$@" tests/acid1131/host.c -lm -o "$OUT/$NAME" && "$OUT/$NAME" > "$OUT/$NAME.txt"; }
NAME=green; b; NAME=old; b -DACID1131_OLD=1
g() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
ok() { [ "$(g $1 top)" -ge 150 ] && [ "$(g $1 low)" -ge 70 ] && [ "$(g $1 pool)" -ge 120 ] && [ "$(g $1 active_big_pct)" -ge 40 ] && [ "$(g $1 active_pipe_pct)" -ge 40 ] && [ "$(g $1 bad)" = 0 ] && [ "$(g $1 nonmono)" = 0 ]; }
echo "зелёный: альфа столба $(g green top)/$(g green low), пятна $(g green pool); капли есть: столб 15 м $(g green active_big_pct) %, труба 4 м $(g green active_pipe_pct) %; вне столба $(g green bad), подскоков $(g green nonmono)"
fail=0; ok green || { echo "FAIL зелёный"; fail=1; }
ok old && { echo "КРАСНЫЙ ACID1131_OLD НЕ ПОЙМАН"; fail=1; } || echo "красный ACID1131_OLD=1: упал, как должен — альфа $(g old top)/$(g old low), капель $(g old drops_seen)"
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
