#!/bin/bash
# Окно g07b-msnview, круг 6: логика бонус-гонки на настоящих трассах (настоящий viewer/bonus.c). Новое — зелёное; каждый _OLD=1 — красный.
#   tests/g07b_sverka6/bonus_logic.sh [каталог с b0N.bns]   (по умолчанию ~/Downloads/shar-psp/build/bonus)
cd "$(dirname "$0")/../.."
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
O=${OUT_DIR:-build/g07b_sverka6}; mkdir -p "$O"; V=viewer
BNS=${1:-$HOME/Downloads/shar-psp/build/bonus}
SRC="tests/g07b_sverka6/bonus_logic_host.c $V/bonus.c $V/msncar.c $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c $V/props.c $V/stateprops.c $V/animobj.c $V/coins.c"
B="cc -std=c11 -O1 -Wall -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V"
LNK="-lm -Wl,-U,_uiFlash"
$B $SRC $LNK -o "$O/bl_new" 2>"$O/bl_new.log" || { tail -20 "$O/bl_new.log"; exit 1; }
POS=0; "$O/bl_new" "$BNS" | grep -E "FAIL|OK:|проверок" ; [ ${PIPESTATUS[0]} = 0 ] || POS=1
NEG=1
for f in BR_NITRO3_OLD BR_AIHORN3_OLD BR_RESPAWN_KIND_OLD BR_TRAP_LEADER_OLD BR_LAUNCH_OLD BR_GONITRO1_OLD; do
  $B -D$f=1 $SRC $LNK -o "$O/bl_old" 2>/dev/null || { echo "сборка $f не вышла"; NEG=0; continue; }
  n=$("$O/bl_old" "$BNS" | grep -c "FAIL  "); if [ "$n" = 0 ]; then echo "красный контроль $f НЕ упал"; NEG=0; else echo "контроль $f: красных проверок $n"; fi
done
for f in BR_AIFLIP_UPY=0.35f BR_AIFLIP_S=2.f; do   # соседние ложные значения: прежний порог 0,35 и вдвое долгая задержка (≈ 2 с, как было до круга 3)
  $B -D$f $SRC $LNK -o "$O/bl_old" 2>/dev/null || { echo "сборка $f не вышла"; NEG=0; continue; }
  n=$("$O/bl_old" "$BNS" | grep -c "FAIL  "); if [ "$n" = 0 ]; then echo "красный контроль $f НЕ упал"; NEG=0; else echo "контроль $f: красных проверок $n"; fi
done
if [ $POS = 0 ] && [ $NEG = 1 ]; then echo PASS; else echo "FAIL (новое: $POS, контроль: $NEG)"; exit 1; fi
