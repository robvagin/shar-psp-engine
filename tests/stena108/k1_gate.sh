#!/bin/bash
# К1 (108, Стена): colRayFrac по клеткам вдоль отрезка == прежний обход прямоугольника, бит в бит (tests/stena108/rayk1_host.c). Зелёный: расхождений 0 на L1..L7; красный: -DCOLRAYK1_PLANT=1 даёт расхождения.
# Использование: tests/stena108/k1_gate.sh [ЧИСЛО_ЛУЧЕЙ]   (данные: build/test106*/…/level.shr основной копии dal103)
set -u; cd "$(dirname "$0")/../.."; N="${1:-100000}"; B="${K1_DATA:-$HOME/Downloads/shar-psp-dal103/build}"; T=$(mktemp -d); fail=0
cc -std=c11 -O2 -w -I viewer tests/stena108/rayk1_host.c viewer/col.c -lm -o $T/g || exit 2
cc -std=c11 -O2 -w -DCOLRAYK1_PLANT=1 -I viewer tests/stena108/rayk1_host.c viewer/col.c -lm -o $T/r || exit 2
for n in 1 2 3 4 5 6 7; do f="$B/test106/PSP/GAME/SHARTEST15/level.shr"; [ $n -gt 1 ] && f="$B/test106tour/PSP/GAME/SHARTEST15/l$n/level.shr"
  out=$($T/g "$f" "$N") || { echo "FAIL L$n: $out"; fail=1; continue; }; echo "ok   L$n ${out##*расхождений }" | cut -c1-120; done
out=$($T/r "$B/test106/PSP/GAME/SHARTEST15/level.shr" 20000); if [ $? -ne 0 ]; then echo "ok   красный контроль краснеет: $(echo "$out" | tail -1 | sed 's/.*расхождений \([0-9]*\);.*/расхождений \1/')"; else echo "FAIL красный не покраснел"; fail=1; fi
rm -rf $T; [ $fail = 0 ] && echo PASS || { echo FAIL; exit 1; }
