#!/bin/bash
# 1331: восемь провалов подряд (служебный flag msnfail), на каждом Yes. Каждый Yes запускает миссию заново (карточка, этап 0, MSN running), лишних провалов нет (ровно 8 строк MSN failed),
# Skip на экране провала появляется только после 7 попыток (7-й и 8-й провал), а не на 4-м. Красное (flag skipcount1331old 1): Skip уже на 4-м провале — гейт падает.
#   tests/stena1331/skipcount.sh ПАКЕТ
cd "$(dirname "$0")/../.."; P=${1:?пакет}
run() { SHIP_LEVELS="5" SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=300 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
chk() { # $1 папка: печатает «провалов N, skip по провалам: ...»
  grep -a '^MSN failed' $1/smoke.txt | sed -n 's/.* tries=\([-0-9]*\) skip=\([0-9]\).*/\1:\2/p' | tr '\n' ' '; }
N=$(run tests/stena1331/loop.scn); R=$(run tests/stena1331/loop_red.scn)
n=$(chk $N); r=$(chk $R); nr=$(grep -a -c '^MSN briefing m7.*card-only' $N/smoke.txt)
echo "новое  (провал: попыток:skip): $n  | карточек после Yes: $nr ($N)"; echo "красное: $r ($R)"
nf=$(echo $n | wc -w | tr -d ' ')
ok=0; [ "$nf" = 8 ] && [ "$nr" = 8 ] && [ "$(echo $n | awk '{print $1" "$6" "$7}')" = "1:0 6:0 7:1" ] && ok=1
red=0; [ "$(echo $r | awk '{print $4}')" = "7:1" ] && red=1
[ $ok = 1 ] && [ $red = 1 ] && { echo PASS; exit 0; }; [ $red = 0 ] && echo "КРАСНЫЙ НЕ ПОЙМАН"; echo FAIL; exit 1
