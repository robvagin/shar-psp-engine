#!/bin/bash
# 1321: все десять этапов buycar/buyskin вступлений (L1m3 L2m6 L3m4 L3m5 L4m3 L4m6 L5m3 L5m4 L6m6 L7m2): после запертого этапа в углу значок этапа покупки (MSNICON want = значок из данных этапа).
#   tests/missions/lockicon1321_all.sh ПАКЕТ [RED]   — RED=1: flag lockicon1321old 1, обязаны остаться значки прежнего этапа (talkto), гейт падает
# Случаи: «уровень:номер миссии с 0:этап buycar»; запуск с этапа buycar − 2 (talkto), msnpass ×2 (разговор, запертый диалог). Нужен набор с данными уровней 1–7 и журналом MSNICON.
cd "$(dirname "$0")/../.."; P=${1:?пакет}; RED=${RED:-0}; T=${TMPDIR:-/tmp}/li1321.$$; mkdir -p $T
CASES="1:3:9:barne 2:5:6:homer 3:3:2:otto 3:4:3:tshirt 4:2:2:tshirt 4:5:4:tshirt 5:2:2:homer 5:3:3:tshirt 6:5:3:kearney 7:1:3:cemetery"
fail=0; red=0
for L in ${ONLY:-1 2 3 4 5 6 7}; do
  S=$T/l$L.scn; { echo "scene g1321"; [ $RED = 1 ] && echo "flag lockicon1321old 1"; echo "flag golevel $L"; echo "wait 30"; } > $S
  for c in $CASES; do IFS=: read cl mi st ic <<<"$c"; [ $cl = $L ] || continue
    echo "flag msnmainat -$((mi*100+st-2))"; echo "wait 6"; echo "flag msnpass"; echo "wait 5"; echo "flag msnpass"; echo "wait 6"; echo "flag msnhud"; echo "wait 1"; done >> $S
  echo end >> $S
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN=$S SMOKE_MAXT=400 tools/smoke.sh "$P" >/dev/null 2>&1; D=$(ls -td build/smoke/2* | head -1)
  grep -a -E '^MSN stage m[0-9]+\.0\.[0-9]+ obj=(buycar|buyskin)' $D/smoke.txt | sed "s/^/L$L /" > $T/st$L.txt
  grep -a '^MSNICON' $D/smoke.txt | sed -n 's/.*want="\([^"]*\)".*pixok=\([-0-9]*\).*/\1 pixok=\2/p' > $T/ic$L.txt
  n=0; for c in $CASES; do IFS=: read cl mi st ic <<<"$c"; [ $cl = $L ] || continue; n=$((n+1)); got=$(sed -n "${n}p" $T/ic$L.txt)
    if [ $RED = 1 ]; then case "$got" in "$ic "*) echo "L$cl m$([ $cl = 1 ] && echo $mi || echo $((mi+1))): значок $ic есть при выключателе (красный не пойман) [$got]"; red=1;; *) echo "L$cl m$([ $cl = 1 ] && echo $mi || echo $((mi+1))): красный пойман, значок '$got'";; esac
    else case "$got" in "$ic "*) echo "L$cl m$([ $cl = 1 ] && echo $mi || echo $((mi+1))): значок $got — ок";; *) echo "L$cl m$([ $cl = 1 ] && echo $mi || echo $((mi+1))): ждали $ic, в углу '$got'"; fail=1;; esac; fi
  done
done
cat $T/st*.txt | head -12; rm -rf $T
if [ $RED = 1 ]; then [ $red = 0 ] && { echo "красный пойман на всех"; exit 0; } || { echo FAIL; exit 1; }; fi
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
