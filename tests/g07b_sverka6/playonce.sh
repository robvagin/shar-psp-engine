#!/bin/bash
# Окно g07b-msnview, круг 6: PlayOnce (viewer/animobj.c) + код действия в экспорте (tools/animobj_export.py ACTIONS['PlayOnce'] = AO_PLAYONCE).
cd "$(dirname "$0")/../.."
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
O=${OUT_DIR:-build/g07b_sverka6}; mkdir -p "$O"; V=viewer
SRC="tests/g07b_sverka6/playonce_host.c $V/animobj.c $V/stateprops.c $V/col.c $V/props.c $V/coins.c $V/car.c"
B="cc -std=c11 -O1 -Wno-unused-function -Wno-misleading-indentation -I $V"; LNK="-lm -Wl,-U,_uiFlash"
$B $SRC $LNK -o "$O/po_new" 2>"$O/po.log" || { tail -5 "$O/po.log"; exit 1; }
"$O/po_new" | tail -10; POS=${PIPESTATUS[0]}
NEG=1
for f in AO_PLAYONCE_OLD=1 PLAYONCE_LEAVE_OLD=1; do
  $B -D$f $SRC $LNK -o "$O/po_old" 2>/dev/null || { NEG=0; continue; }
  n=$("$O/po_old" | grep -c "FAIL  "); if [ "$n" = 0 ]; then echo "красный контроль $f НЕ упал"; NEG=0; else echo "контроль $f: красных проверок $n"; fi
done
# код действия: экспорт и движок должны совпадать
EXP=$(grep -o "'PlayOnce': *[0-9]*" tools/animobj_export.py | grep -o "[0-9]*$"); ENG=$(python3 -c "
import re;s=open('viewer/animobj.h').read();m=re.search(r'enum \{([^}]*)\};\s*//?.*AO_PLAYONCE',s) or re.search(r'enum \{(AO_NONE[^}]*)\}',s);n=[x.strip() for x in m.group(1).split(',')];print(n.index('AO_PLAYONCE'))")
echo "код PlayOnce: экспорт $EXP, движок $ENG"; [ "$EXP" = "$ENG" ] || { echo "коды не совпали"; POS=1; }
[ "$POS" = 0 ] && [ $NEG = 1 ] && echo PASS || { echo "FAIL"; exit 1; }
