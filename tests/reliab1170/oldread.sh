#!/bin/bash
# №1170: гейт «записи прежних раскладок читаются» (риск 7). Красный первым: -DSAVEOLDREAD_OLD=1 обязан упасть; потом зелёный.
cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}
O=build/reliab1170; mkdir -p $O
V=viewer; . tools/bots/lib.sh
CC="cc -std=c11 -O1 -w -I viewer -I tools/bots -DSKIN_HOST"
$CC -DSAVEOLDREAD_OLD=1 tests/reliab1170/oldread.c $BOTS_LIB viewer/savegame.c -lm -o $O/old_red 2> $O/cc_old_red.txt || { echo "FAIL: сборка красного"; exit 1; }
$O/old_red > $O/old_red.txt && { echo "FAIL: красный контроль (SAVEOLDREAD_OLD=1) не упал"; exit 1; }
$CC tests/reliab1170/oldread.c $BOTS_LIB viewer/savegame.c -lm -o $O/old_green 2> $O/cc_old_green.txt || { echo "FAIL: сборка"; exit 1; }
$O/old_green > $O/old_green.txt || { cat $O/old_green.txt; echo "FAIL: прежние записи не читаются"; exit 1; }
echo "RESULT: OK — красный (чтение выключено): $(tail -1 $O/old_red.txt); зелёный: $(tail -1 $O/old_green.txt)"
