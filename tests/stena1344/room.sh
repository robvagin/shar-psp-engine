#!/bin/bash
# 1344: комната под закрытым экраном двери ждёт моделей персонажей (медленное чтение: flag npcdelay 1800 = модель готова через 1,8 с). Дом Симпсонов и Kwik-E-Mart (L1), таверна Мо (L2).
# Новое: строка ROOMREADY waited≥1500 мс left=0 (персонаж встал до раскрытия). Красное (flag roomready1344old 1, предел 1,5 с): left≥1 cap — открыли без персонажа, он встал на глазах (гейт обязан упасть).
#   tests/stena1344/room.sh ПАКЕТ [комнаты: house kwik moe]
cd "$(dirname "$0")/../.."; P=${1:?пакет}; shift; ROOMS=${*:-house kwik moe}; fail=0
for r in $ROOMS; do
  for v in "" _red; do
    lv=1; [ $r = moe ] && lv=2
    SHIP_LEVELS="2" SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN=tests/stena1344/$r$v.scn SMOKE_MAXT=200 tools/smoke.sh "$P" >/dev/null 2>&1; D=$(ls -td build/smoke/2* | head -1)
    line=$(grep -a '^ROOMREADY' $D/smoke.txt | head -1); w=$(echo "$line" | sed -n 's/.*waited=\([0-9]*\)ms.*/\1/p'); l=$(echo "$line" | sed -n 's/.*left=\([0-9]*\).*/\1/p')
    if [ -z "$v" ]; then ok=$(awk -v w="${w:-0}" -v l="${l:-9}" 'BEGIN{print (w>=1500 && l==0) ? 1 : 0}'); echo "$r новое: ${line:-нет ROOMREADY} => $([ $ok = 1 ] && echo ok || echo ПЛОХО)"; [ $ok = 1 ] || fail=1
    else red=$(awk -v l="${l:-0}" 'BEGIN{print (l>=1) ? 1 : 0}'); echo "$r красное: ${line:-нет ROOMREADY} => $([ $red = 1 ] && echo поймано || echo НЕ-ПОЙМАНО)"; [ $red = 1 ] || fail=1; fi
  done
done
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
