#!/bin/bash
# Круг 7 (g11, P11-15): таблица звуков подвижных объектов сверена с оригиналом по именам. Гейт без запуска игры: препроцессор раскрывает таблицу в viewer/sound.c, имена сравниваются с ожидаемым списком
# (имена объектов с диска игры: платформы, люки, кран, робо-рука, лифт, грузовик, дверь, библиотека) и с реальными именами на уровнях (L6 "crane", L5 "cranes" звука не имеет).
# Красный: -DANIMCRANE_SV_OLD=1 — в таблице "cranes", кран L6 остаётся без звука.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/animsound; mkdir -p "$OUT"
names() { python3 - "$@" <<'PY'
import re, subprocess, sys
src = open('viewer/sound.c', encoding='utf-8').read()
i = src.index('static const struct { const char *obj, *clip; int pos; float mn, mx; } T[]'); j = src.index('};', i)
block = src[i:j]
flags = [a for a in sys.argv[1:]]
crane = 'cranes' if '-DANIMCRANE_SV_OLD=1' in flags else 'crane'
block = block.replace('ANIMCRANE_NAME', '"%s"' % crane)
print(' '.join(sorted(set(re.findall(r'\{"([A-Za-z0-9_]+)",', block)))))
PY
}
EXPECT="Aplatform1Trans Aplatform2Trans Aplatform6Trans Splatfrom7Trans Splatform1Trans Splatform2Trans Splatform4Trans Splatform5Trans Splatform6Trans TD crane elevator hdoortrans1 library roboarm trapdoor1Trans truck1"
G=$(names); O=$(names -DANIMCRANE_SV_OLD=1)
echo "зелёный: $G"
fail=0
[ "$(echo $G | tr ' ' '\n' | LC_ALL=C sort | tr '\n' ' ')" = "$(echo $EXPECT | tr ' ' '\n' | LC_ALL=C sort | tr '\n' ' ')" ] || { echo "FAIL зелёный: список не совпал с оригиналом"; echo "ожидалось: $EXPECT"; fail=1; }
[ "$O" != "$G" ] && echo "красный ANIMCRANE_SV_OLD=1: упал, как должен — в таблице cranes" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
# люки: любое имя, начинающееся на TD (L7: TD01…TD12), попадает в запись «TD»; другие — только точное совпадение
cat > "$OUT/td.c" <<'EOT'
#include <stdio.h>
#include "sound.h"
int main(void) { printf("td03 %d\ntd %d\ntruck %d\ntdx_other %d\nplatform_case %d\n", sndAnimNameMatch("TD", "TD03"), sndAnimNameMatch("TD", "TD"), sndAnimNameMatch("truck1", "TD03"), sndAnimNameMatch("truck1", "truck1"), sndAnimNameMatch("Splatform1Trans", "splatform1trans")); return 0; }
EOT
tb() { cc -std=c11 -O2 -w -DSKIN_HOST -I viewer "$@" "$OUT/td.c" -o "$OUT/$NAME" && "$OUT/$NAME" > "$OUT/$NAME.txt"; }
NAME=tdg; tb; NAME=tdo; tb -DANIMTD_SV_OLD=1
tg() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
echo "люки: TD03 $(tg tdg td03), TD $(tg tdg td), чужое имя $(tg tdg truck), truck1 $(tg tdg tdx_other), регистр $(tg tdg platform_case)"
[ "$(tg tdg td03)" = 1 ] && [ "$(tg tdg td)" = 1 ] && [ "$(tg tdg truck)" = 0 ] && [ "$(tg tdg tdx_other)" = 1 ] && [ "$(tg tdg platform_case)" = 1 ] || { echo "FAIL зелёный: люки"; fail=1; }
[ "$(tg tdo td03)" = 0 ] && echo "красный ANIMTD_SV_OLD=1: упал, как должен — TD03 в таблицу не попадает" || { echo "КРАСНЫЙ ANIMTD НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
