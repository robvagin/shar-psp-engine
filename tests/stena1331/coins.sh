#!/bin/bash
# 1335 (проверка, а не правка): повтор миссии после «машина разбита» монеты не списывает. coins.scn: кошелёк 100, взрыв машины (штраф взрыва min(20, кошелёк), запись PS2 a355 — монеты разлетаются), Yes, повтор.
# Зелёное: кошелёк в момент провала = кошельку сразу после старта повтора (списания на повторе нет). Цена починки 10 берётся только в будке (phone_view.c).
#   tests/stena1331/coins.sh ПАКЕТ
cd "$(dirname "$0")/../.."; P=${1:?пакет}
SHIP_LEVELS="5" SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN=tests/stena1331/coins.scn SMOKE_MAXT=200 tools/smoke.sh "$P" >/dev/null 2>&1; D=$(ls -td build/smoke/2* | head -1)
c() { grep -a '^MSNTIMER' $D/smoke.txt | sed -n "$1p" | sed -n 's/.* coins=\([0-9]*\).*/\1/p'; }
before=$(c 1); fail=$(c 2); retry=$(c 3)
echo "кошелёк: до взрыва $before, при провале $fail, сразу после старта повтора $retry ($D)"
[ "$fail" = "$retry" ] && [ "$before" = 100 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
