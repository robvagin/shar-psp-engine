#!/bin/bash
# Окно g07b-msnview, круг 7 (P17-16): порядок круга бонус-трасс. .bns b02, b05, b07 пересобираются экспортом (рецепт набора и так зависит от export.py и track.py).
#   tests/g07b_sverka7/lap.sh [каталог с b0N.bns и b0N.shr]   (по умолчанию ~/Downloads/shar-psp/build/bonus)
cd "$(dirname "$0")/../.."
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
O=${OUT_DIR:-build/g07b_sverka7}; mkdir -p "$O/bonus" "$O/bonus_old"; V=viewer
BNS=${1:-$HOME/Downloads/shar-psp/build/bonus}; PY=${PY:-$HOME/Downloads/shar-psp/.venv/bin/python}
for n in 2 5 7; do $PY tools/bonus/export.py --track $n --bns-file "$O/bonus/b0$n.bns" --shr "$BNS/b0$n.shr" >/dev/null 2>&1 || { echo "экспорт b0$n не вышел"; exit 1; }
  BONUS_LAP_OLD=1 $PY tools/bonus/export.py --track $n --bns-file "$O/bonus_old/b0$n.bns" --shr "$BNS/b0$n.shr" >/dev/null 2>&1; done
SRC="tests/g07b_sverka7/lap_host.c $V/bonus.c $V/msncar.c $V/car.c $V/col.c $V/traffic.c $V/homer.c $V/skin.c $V/props.c $V/stateprops.c $V/animobj.c $V/coins.c"
B="cc -std=c11 -O1 -Wno-unused-function -Wno-misleading-indentation -DSKIN_HOST -I $V"; LNK="-lm -Wl,-U,_uiFlash"
$B $SRC $LNK -o "$O/lap_new" 2>"$O/lap.log" || { tail -5 "$O/lap.log"; exit 1; }
"$O/lap_new" "$BNS" "$O/bonus" | grep -E "FAIL|OK:|проверок"; POS=${PIPESTATUS[0]}; NEG=1
$B -DBR_LAPLIST_OLD=1 $SRC $LNK -o "$O/lap_old" 2>/dev/null; n=$("$O/lap_old" "$BNS" "$O/bonus" | grep -c "FAIL  "); [ "$n" -gt 0 ] && echo "контроль BR_LAPLIST_OLD=1: красных $n" || { echo "красный контроль BR_LAPLIST_OLD НЕ упал"; NEG=0; }
n=$("$O/lap_new" "$BNS" "$O/bonus_old" | grep -c "FAIL  "); [ "$n" -gt 0 ] && echo "контроль старые .bns (BONUS_LAP_OLD=1): красных $n" || { echo "красный контроль старых .bns НЕ упал"; NEG=0; }
[ "$POS" = 0 ] && [ $NEG = 1 ] && echo PASS || { echo FAIL; exit 1; }
