#!/bin/bash
# Гейт 1301b: удешевление проверки предметов у персонажей миссий (game_msn.c npcBlocksProp).
# 1) поведение (хост): Мардж не проходит сквозь предметы и стейтпропы: зелёный на новом, красный на -DGAME_NPC_PROPSOLID1301_OLD=1 (прежняя дыра 1301);
# 2) то же поведение, что с -DNPCPROP1301B_OLD=1 (проверка в каждом шаге): строки испытаний новое и выключенное совпадают. Цена — tests/stena1301b/perf.sh (пара на эмуляторе, красный flag npcprop1301bold 1).
#   tests/stena1301b/gate.sh
set -u; cd "$(dirname "$0")/../.."; O=build/h1301b; mkdir -p $O; D=${DATA:-$HOME/Downloads/shar-psp/build}; V=viewer; . tools/bots/lib.sh
CC="cc -std=c11 -O2 -w -DSKIN_HOST -I viewer -I tools/bots"
T=tests/stena1301b/npcprop_b.c
$CC $T $BOTS_LIB -lm -Wl,-undefined,dynamic_lookup -o $O/new || { echo FAIL сборка; exit 1; }
$CC -DNPCPROP1301B_OLD=1 $T $BOTS_LIB -lm -Wl,-undefined,dynamic_lookup -o $O/b_old || { echo FAIL сборка; exit 1; }
$CC -DGAME_NPC_PROPSOLID1301_OLD=1 $T $BOTS_LIB -lm -Wl,-undefined,dynamic_lookup -o $O/red || { echo FAIL сборка; exit 1; }
A="$D/level.shr $D/homer.shr $D/missions/level01.msn"; fail=0
echo "новое:"; $O/new $A || fail=1
echo "1301b выключено (NPCPROP1301B_OLD=1, поведение 1301 как в 108):"; $O/b_old $A || fail=1
[ "$($O/new $A | grep '^P ')" = "$($O/b_old $A | grep '^P ')" ] || { echo "новое и NPCPROP1301B_OLD=1 расходятся"; fail=1; }
echo "красное (GAME_NPC_PROPSOLID1301_OLD=1):"; $O/red $A && { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; } || echo "красный пойман"
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
