#!/bin/bash
# №1191 (Роб 04.11: «от дома Симпсонов налево, у арки с кольцом машина застревает, как будто невидимый блок»): проход между домами у J9-5 L1 с летающим гаечным ключом (тип 78) — тело «куста» №1137 у ключа тормозило машину
# (минимальная скорость у объекта 5 м/с против 10,6 на газу). Исправление — g01 fc46cd2b (BUSHCLS1201_OLD): тело только у настоящих кустов (ATC 10, масса > 0). Тест целым кадром игры (tools/bots/world.h):
# зелёный (исправленное): мин. скорость у объекта ≥ 9 м/с; красный (-DBUSHCLS1201_OLD=1, как TEST85…90): < 7. Нужен код с fc46cd2b.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
SET="${1:-$HOME/Downloads/shar-psp-integ/build/test88tour/PSP/GAME/SHARTEST15}"
export DEVELOPER_DIR=/Library/Developer/CommandLineTools PATH=/Library/Developer/CommandLineTools/usr/bin:$PATH SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
grep -q BUSHCLS1201_OLD viewer/stateprops.c || { echo "FAIL: нет исправления g01 (BUSHCLS1201_OLD в stateprops.c)"; exit 1; }
mkdir -p build/bots; ln -sf "$SET/level.shr" build/level.shr; [ -f build/homer.shr ] || ln -sf "$HOME/Downloads/shar-psp/build/homer.shr" build/homer.shr
bash -c 'V=viewer; . tools/bots/lib.sh; C2="cc -std=c11 -O2 -w -DSKIN_HOST -I viewer -I tools/bots"
  $C2 tests/arch1191/pass.c $BOTS_LIB -lm -o build/bots/pass_new && $C2 -DBUSHCLS1201_OLD=1 tests/arch1191/pass.c $BOTS_LIB -lm -o build/bots/pass_old' || { echo "FAIL: сборка"; exit 1; }
gv=$(build/bots/pass_new | awk '/мин.скорость/{print $11}' | sort -n | head -1); rv=$(build/bots/pass_old | awk '/мин.скорость/{print $11}' | sort -n | head -1)
gp=$(build/bots/pass_new | tail -1); rp=$(build/bots/pass_old | tail -1); fail=0
echo "новое: мин.скорость у объекта $gv м/с ($gp); TEST89: $rv м/с ($rp)"
awk -v a="$gv" 'BEGIN{exit !(a >= 9)}' || { echo "FAIL: новое тормозит у объекта"; fail=1; }
awk -v a="$rv" 'BEGIN{exit !(a < 7)}' || { echo "FAIL: красный не тормозит: контроль слепой"; fail=1; }
[ $fail -eq 0 ] && echo PASS || exit 1
