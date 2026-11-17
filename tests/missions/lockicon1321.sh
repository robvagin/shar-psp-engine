#!/bin/bash
# 1321: после запертого этапа (L2 m6, прогулка: доктор → запертый диалог → «купи Mr. Plow у Гомера») в углу значок продавца Гомера, а не прежний значок доктора.
#   tests/missions/lockicon1321.sh ПАКЕТ   — безголовый эмулятор, SHIP_LEVELS=2 (нужен набор с журналом MSNICON: сборка выпуска смоука + данные L2)
# Зелёный: после двух msnpass последний MSNICON want="homer". Красный (flag lockicon1321old 1 = MSN_LOCKICON1321_OLD): want="drnick", гейт обязан упасть.
cd "$(dirname "$0")/../.."; P=${1:?пакет}
run() { SHIP_LEVELS=2 SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=300 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
want() { grep -a '^MSNICON' $1/smoke.txt | tail -1 | sed -n 's/.*want="\([^"]*\)".*pixok=\([-0-9]*\).*/\1 pixok=\2/p'; }
N=$(run tests/missions/lockicon1321.scn); R=$(run tests/missions/lockicon1321_red.scn)
n=$(want $N); r=$(want $R); echo "новое: $n ($N)"; echo "красное: $r ($R)"
grep -a '^MSN stage m6.0.6' $N/smoke.txt | head -1
case "$n" in homer*) ok=1;; *) ok=0;; esac; case "$r" in drnick*) red=1;; *) red=0;; esac
[ $ok = 1 ] && [ $red = 1 ] && { echo PASS; exit 0; }; [ $red = 0 ] && echo "КРАСНЫЙ НЕ ПОЙМАН"; echo FAIL; exit 1
