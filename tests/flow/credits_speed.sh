#!/bin/bash
# Скорость титров конца игры = скорости Options → View Credits (20 px Scrooby/с, запись PS2: 21,5 px/с на 512 = 20,2 на 480). Данных уровня не нужно.
# Зелёный: оба пути 11,33 px PSP/с. Красный: -DENDCRED_SPEED_OLD=1 (35 → 19,83) должен упасть.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; mkdir -p build/flow
cat > build/flow/credits_speed.c <<'C'
#include <stdio.h>
#include <math.h>
#include "flowrules.h"
int main(void) { float a = flowCreditsSpeed(0), e = flowCreditsSpeed(1);
  int ok = fabsf(a - 11.333f) < .01f && fabsf(e - 11.333f) < .01f;
  printf("меню %.2f, конец игры %.2f px/с (ждём 11,33 оба) %s\n", a, e, ok ? "OK" : "FAIL"); return ok ? 0 : 1; }
C
cc -std=c11 -Wall -I viewer build/flow/credits_speed.c -lm -o build/flow/cs_green && cc -std=c11 -DENDCRED_SPEED_OLD=1 -I viewer build/flow/credits_speed.c -lm -o build/flow/cs_red || exit 1
build/flow/cs_green || { echo FAIL green; exit 1; }
build/flow/cs_red && { echo "КРАСНЫЙ НЕ ПОЙМАН"; exit 1; } || echo "красный ENDCRED_SPEED_OLD упал, как должен"
echo PASS
