#!/bin/bash
# Гейт 1301: персонаж миссии не проходит сквозь твёрдые предметы и стейтпропы (game_msn.c npcBlocksProp). Зелёный: ≥ 3 испытаний, 0 кадров внутри. Красный (-DGAME_NPC_PROPSOLID1301_OLD=1): кадры внутри > 0.   tests/homer108b/npcprop.sh
set -u; cd "$(dirname "$0")/../.."; O=build/h108b; mkdir -p $O; D=${DATA:-$HOME/Downloads/shar-psp/build}; V=viewer; . tools/bots/lib.sh
CC="cc -std=c11 -O2 -w -DSKIN_HOST -I viewer -I tools/bots"
$CC tests/homer108b/npcprop.c $BOTS_LIB -lm -Wl,-undefined,dynamic_lookup -o $O/npcprop_new || { echo FAIL сборка; exit 1; }
$CC -DGAME_NPC_PROPSOLID1301_OLD=1 tests/homer108b/npcprop.c $BOTS_LIB -lm -Wl,-undefined,dynamic_lookup -o $O/npcprop_old || { echo FAIL сборка; exit 1; }
A="$D/level.shr $D/homer.shr $D/missions/level01.msn"; fail=0
echo "новое:"; $O/npcprop_new $A || fail=1
echo "красное (GAME_NPC_PROPSOLID1301_OLD=1):"; $O/npcprop_old $A && { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; } || echo "красный пойман"
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
