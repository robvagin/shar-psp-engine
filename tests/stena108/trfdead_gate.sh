#!/bin/bash
# TRFDEAD (107b, Стена): строка ящика, когда машина трафика стала телом без возврата на полосу (где, почему, сколько стоит). trfdead_gate.sh PACK
set -u; cd "$(dirname "$0")/../.."; P=$1; fail=0
run() { SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=300 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
chk() { local f="$1/smoke.txt" want="$2" pat="$3" what="$4" n; n=$(grep -cE "$pat" "$f"); if { [ "$want" = yes ] && [ "$n" -gt 0 ]; } || { [ "$want" = no ] && [ "$n" = 0 ]; }; then echo "  ok   $what"; else echo "  FAIL $what (строк $n)"; fail=1; fi; }
R=$(run tests/stena108/trfdead_dead.txt); echo "развёрнутая на 180° машина:"; chk "$R" yes "^TRFDEAD start trf#0 .*why=7" "TRFDEAD start, причина 7 (курс > 90°)"; chk "$R" yes "^TRFDEAD end trf#0 " "игрок уехал дальше 75 м: тело убрано (TRFDEAD end), не стоит вечно"; grep -m2 "^TRFDEAD" "$R/smoke.txt" | cut -c1-160 | sed 's/^/    /'
R=$(run tests/stena108/trfdead_alive.txt); echo "стоит вдоль полосы (красный контроль):"; chk "$R" no "^TRFDEAD start" "строки нет"
[ $fail = 0 ] && echo PASS || { echo FAIL; exit 1; }
