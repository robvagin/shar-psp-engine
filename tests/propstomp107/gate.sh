#!/bin/bash
# 107б П12: прыжок сверху (стомп) по простому предмету 800 / масса, пинок 400 / масса (оригинал; у стейтпропов уже так). Гейт: функция скорости вылета propsHomerKickSpeed (props.h): ящик 100 кг — пинок 4 м/с, стомп 8 м/с,
# масса 0 — как 100 кг; game.c её вызывает (проверка текстом). Красный (-DPROPSTOMP107_OLD=1): стомп 4 м/с.   tests/propstomp107/gate.sh
set -u; cd "$(dirname "$0")/../.."; OUT=build/propstomp107; mkdir -p $OUT
cat > $OUT/t.c <<'C'
#include <stdio.h>
#include "props.h"
unsigned homerSolidFrame = 0; void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
int main(void) { printf("%.2f %.2f %.2f %.2f\n", propsHomerKickSpeed(0, 100.f), propsHomerKickSpeed(1, 100.f), propsHomerKickSpeed(1, 0.f), propsHomerKickSpeed(1, 1000.f)); return 0; }
C
cc -std=c11 -O2 -w -I viewer $OUT/t.c viewer/props.c viewer/col.c viewer/car.c -lm -o $OUT/g 2>$OUT/g.err || cc -std=c11 -O2 -w -DSKIN_HOST -I viewer $OUT/t.c viewer/props.c viewer/col.c viewer/car.c -lm -o $OUT/g 2>>$OUT/g.err || { echo "FAIL: сборка"; head -3 $OUT/g.err; exit 1; }
cc -std=c11 -O2 -w -DPROPSTOMP107_OLD=1 -I viewer $OUT/t.c viewer/props.c viewer/col.c viewer/car.c -lm -o $OUT/r 2>>$OUT/g.err
G=$($OUT/g); R=$($OUT/r); echo "зелёный (пинок 100, стомп 100, стомп 0, стомп 1000): $G | красный: $R"
fail=0; [ "$G" = "4.00 8.00 8.00 0.80" ] || { echo "FAIL зелёный"; fail=1; }; [ "$R" = "4.00 4.00 4.00 0.40" ] && echo "красный PROPSTOMP107_OLD=1: стомп 4 м/с, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
grep -q "propsHomerKickSpeed(e.type == HOMER_EV_STOMP" viewer/game.c || { echo "FAIL: game.c не вызывает функцию"; fail=1; }
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
