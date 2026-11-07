#!/bin/bash
# Нижняя граница радиуса разговора в комнате (L2 m2, talkto Moe): по данным комнаты Moes Гомер подходит к Мо не ближе 2,26 м (tools/cartest/talkreach.c, залив 0,5 м, код EBOOT), радиус разговора из скрипта 2,0. Зелёный: в комнате радиус разговора больше
# достижимого сближения, вне комнат прежний. Красный (-DMSN_TALKROOMFLOOR_OLD=1, как TEST89): разговор недостижим — обязан упасть. Бот L2 m2 (tools/bots/missioner): TEST89 «застрял пешком» на стадии 1.6 talkto, новое проходит её.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/talkroomfloor; mkdir -p "$OUT" build/bots
SET="${1:-$HOME/Downloads/shar-psp-integ/build/test88tour/PSP/GAME/SHARTEST15}"; HOMER="${HOMER:-$HOME/Downloads/shar-psp/build/homer.shr}"
export DEVELOPER_DIR=/Library/Developer/CommandLineTools PATH=/Library/Developer/CommandLineTools/usr/bin:$PATH SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
bash -c 'V=viewer; . tools/bots/lib.sh; C2="cc -std=c11 -O2 -w -DSKIN_HOST -I viewer -I tools/bots"
  $C2 tools/cartest/talkreach.c $BOTS_LIB -lm -o build/bots/talkreach && $C2 tests/talkroomfloor/host.c $BOTS_LIB -lm -o build/talkroomfloor/green && $C2 -DMSN_TALKROOMFLOOR_OLD=1 tests/talkroomfloor/host.c $BOTS_LIB -lm -o build/talkroomfloor/red' || { echo "FAIL: сборка"; exit 1; }
m=$(build/bots/talkreach "$SET/l2/interiors-l2.bin" "$HOMER" 53.1 -20.0 200.6 Moes | sed -E 's/.*ближе всего 3D ([0-9.]+) м.*/\1/')
[ -n "$m" ] || { echo "FAIL: talkreach не дал числа"; exit 1; }
g=$($OUT/green "$m"); gr=$?; r=$($OUT/red "$m"); rr=$?
echo "достижимое сближение $m м; зелёный: $g (код $gr); красный: $r (код $rr)"
[ $gr -eq 0 ] || { echo "FAIL: зелёный не зелёный"; exit 1; }
[ $rr -ne 0 ] || { echo "FAIL: красный не упал (контроль слепой)"; exit 1; }
echo PASS
