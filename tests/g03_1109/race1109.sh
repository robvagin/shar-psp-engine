#!/bin/bash
# Правка Роба 1109, окно g03-aicars: tests/g03_1109/race1109_host.c на настоящих трассах (BNS1 из build/bonus, переэкспорт: python3 tools/bonus/export.py --track N --bns-file ... --shr ...).
# Красный: bonus.c со старыми правилами (-DBR_SEQ_OLD=1 -DBR_CHEAT_OLD=1 -DBR_POSDIST_OLD=1) — тест обязан упасть; зелёный проходит.
cd "$(dirname "$0")/../.."
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
O=${OUT_DIR:-build/g03_1109}; mkdir -p "$O"; V=viewer; DIR=${1:-build/bonus}
SRC="tests/g03_1109/race1109_host.c $V/bonus.c $V/msncar.c $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c $V/props.c $V/stateprops.c $V/animobj.c $V/coins.c"
B="cc -std=c11 -O1 -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V"; LNK="-lm -Wl,-U,_uiFlash"
OBJS=""; for f in $SRC; do o="$O/n_$(basename $f .c).o"; $B -c $f -o $o 2>/dev/null || { echo "сборка $f"; exit 1; }; OBJS="$OBJS $o"; done; cc $OBJS $LNK -o "$O/race_new" || exit 1
OBJS=""; for f in $SRC; do o="$O/o_$(basename $f .c).o"; if [ "$(basename $f)" = bonus.c ]; then $B -DBR_SEQ_OLD=1 -DBR_CHEAT_OLD=1 -DBR_POSDIST_OLD=1 -c $f -o $o 2>/dev/null || exit 1; else $B -c $f -o $o 2>/dev/null || exit 1; fi; OBJS="$OBJS $o"; done; cc $OBJS $LNK -o "$O/race_old" || exit 1
"$O/race_new" "$DIR" | tee "$O/green.txt" | cut -c1-260; G=${PIPESTATUS[0]}
"$O/race_old" "$DIR" > "$O/red.txt" 2>&1; nred=$(grep -c "^CHECK.*FAIL" "$O/red.txt"); ntot=$(grep -c "^CHECK" "$O/red.txt")
echo "красный (старые правила): упало $nred из $ntot"; grep "^CHECK.*OK" "$O/red.txt" | cut -c1-150
[ "$G" = 0 ] && [ "$nred" -ge "$ntot" ] && echo PASS || { echo "FAIL (зелёный код $G, красный $nred из $ntot)"; exit 1; }
