#!/bin/bash
# №1159 (g11, регресс скорости TEST83→85): счётчики обходов на данных L1–L7. Отсев снаряда осы даёт ТОТ ЖЕ результат (число попаданий и сумма долей пути), что полный обход (BOLTNEAR1159_OLD=1), при этом
# до дорогой проверки доходит меньше записей; список будок = полный перебор; для spHitCar печатаются «дошло до carContact» и «дошло до дорогой части» с телами кустов и без (BUSHFALL1137_OLD=1).
# Красный: -DBOLTNEAR1159_RED (охват урезан в 100 раз) теряет попадания — тест обязан упасть.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/objcost1159; mkdir -p "$OUT"; V=viewer
T=$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15
LV=(build/level.shr $T/l2/level.shr $T/l3/level.shr $T/l4/level.shr $T/l5/level.shr $T/l6/level.shr $T/l7/level.shr)
b() { cc -std=c11 -O2 -w -DSKIN_HOST -I $V "$@" tests/objcost1159/host.c $V/props.c $V/stateprops.c $V/col.c $V/car.c $V/coins.c -lm -o "$OUT/$NAME" && "$OUT/$NAME" "${LV[@]}" > "$OUT/$NAME.txt"; }
NAME=new; b; NAME=old; b -DBOLTNEAR1159_OLD=1 -DBUSHFALL1137_OLD=1; NAME=newfull; b -DBOLTNEAR1159_OLD=1; NAME=red; b -DBOLTNEAR1159_RED
col() { grep "^L$2 bolt" "$OUT/$1.txt" | sed -E "s/.* $3 ([0-9.]+).*/\1/"; }
tot() { grep " bolt " "$OUT/$1.txt" | sed -E "s/.* $2 ([0-9.]+).*/\1/" | awk '{s+=$1} END{printf "%.6f", s}' | sed -E 's/\.?0+$//'; }
echo "снаряд (сумма по L1–L7): попаданий отсев $(tot new hits) / полный обход $(tot newfull hits); сумма долей отсев $(tot new sum) / полный $(tot newfull sum)"
echo "дорогая проверка стейтпропов: отсев $(tot new sp_deep) из $(tot new sp_seen), полный обход $(tot newfull sp_deep); предметов: отсев $(tot new pr_deep) из $(tot new pr_seen), полный $(tot newfull pr_deep)"
echo "красный (охват ×0,01): попаданий $(tot red hits)"
echo "будки: $(grep 'booth list' $OUT/new.txt | sed -E 's/L([0-9]+) booth list ([0-9]+) .* same ([0-9]) ninst ([0-9]+)/L\1 будок \2 из \4 (совпало \3)/' | tr '\n' ';')"
echo "spHitCar с телами кустов: $(grep hitcar $OUT/new.txt | sed -E 's/L([0-9]+) hitcar calls [0-9]+ seen\/call ([0-9.]+) entered\/call ([0-9.]+) deep\/call ([0-9.]+)/L\1 вошло \3 дорогих \4/' | tr '\n' ';')"
echo "spHitCar без тел (старое): $(grep hitcar $OUT/old.txt | sed -E 's/L([0-9]+) hitcar calls [0-9]+ seen\/call ([0-9.]+) entered\/call ([0-9.]+) deep\/call ([0-9.]+)/L\1 вошло \3 дорогих \4/' | tr '\n' ';')"
fail=0
[ "$(tot new hits)" = "$(tot newfull hits)" ] && [ "$(tot new sum)" = "$(tot newfull sum)" ] || { echo "FAIL: отсев меняет результат снаряда"; fail=1; }
[ "$(tot new hits)" -gt 100 ] || { echo "FAIL: мало попаданий в проверке (отрезки не попадают)"; fail=1; }
[ "$(tot new sp_deep)" -lt "$(( $(tot newfull sp_deep) / 4 ))" ] && [ "$(tot new pr_deep)" -lt "$(( $(tot newfull pr_deep) / 4 ))" ] || { echo "FAIL: отсев не сокращает обход хотя бы в 4 раза"; fail=1; }
grep 'booth list' $OUT/new.txt | grep -q 'same 0' && { echo "FAIL: список будок расходится с перебором"; fail=1; }
[ "$(tot red hits)" -lt "$(tot newfull hits)" ] && echo "красный BOLTNEAR1159_RED: потерял попадания, как должен" || { echo "КРАСНЫЙ BOLTNEAR1159_RED НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
