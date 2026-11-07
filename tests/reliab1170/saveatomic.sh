#!/bin/bash
# №1170: гейт «слот сохранения не пустеет при обрыве питания». Красный контроль первым (SAVEAT_OLD=1 обязан упасть), затем зелёный.
cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}
O=build/reliab1170; mkdir -p $O
V=viewer; . tools/bots/lib.sh   # общий список исходников мира (нужны collect.c и миссии для savegame.c)
CC="cc -std=c11 -O1 -Wall -Wno-unused-function -I tests/reliab1170/stub -I viewer -I tools/bots -DSKIN_HOST"
build() { $CC "$@" tests/reliab1170/saveatomic.c viewer/savefile.c $BOTS_LIB viewer/savegame.c -lm -o $O/$OUT 2> $O/cc_$OUT.txt || { echo "FAIL: сборка $OUT (см. $O/cc_$OUT.txt)"; exit 1; }; }
OUT=old; build -DSAVEAT_OLD=1; $O/old > $O/old.txt && { echo "FAIL: красный контроль (SAVEAT_OLD=1) не упал"; cat $O/old.txt; exit 1; }
OUT=new; build; $O/new > $O/new.txt || { cat $O/new.txt; echo "FAIL: новая запись теряет слот"; exit 1; }
echo "RESULT: OK — красный (прежний порядок): $(tail -1 $O/old.txt); зелёный: $(tail -1 $O/new.txt)"
