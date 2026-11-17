#!/bin/bash
# 1320б: чистая перезагрузка в миссии идёт долго; (1) кадры прелоадера идут из ожиданий загрузчика, самый долгий промежуток без кадра ≤ 0,5 с (на PSP было 8,7 с без кадров до чтения уровня);
# (2) сторож не пишет STALL на штатной долгой работе без кадров (запас +20 с). Тур срочного 15.11: STALL2 phase=rooms на 5-й секунде перезагрузки 14 с.
# Стенд: сценарий room1320 (L4 m3.1.1, flag tffail 5, дверь Kwik-E-Mart) + служебно «flag reloadwait1320 3» (3 с ожидания загрузчика до выгрузки) и «flag reloadstall1320 7» (7 с без кадров после выгрузки).
# Случаи: pump — reloadwait 3: новое RELOADGAP max_ms ≤ 500; красное (flag stallgrace1320 0 = STALLGRACE1320_OLD, крючок выключен) max_ms ≥ 2500.
#         grace — reloadstall 7: новое BBSTALL нет; красное (stallgrace1320 0) BBSTALL ms ≥ 5000.   tests/stall1320/run.sh EBOOT_ТУРА [НАБОР]
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; cd $W; E=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); O=${OUT:-$W/build/stall1320}; mkdir -p $O/ovl_empty
export SET=${2:-$W/build/test108tour/PSP/GAME/SHARTEST15} OUT=$O OVL=$O/ovl_empty SMOKE_HEADLESS=1 SHIP_LEVELS="2 3 4 5 6 7" EMU_OWNER=Stall1320 EMU_MINE=1
mk() { # имя, строки перед tffail
  python3 $H/../room1320/gen.py 5 | python3 -c "import sys; ex=sys.argv[1].replace('|','\n'); print(sys.stdin.read().replace('flag tffail 5', ex+'\nflag tffail 5',1),end='')" "$2" > $O/$1.txt; }
mk pump_new "flag reloadwait1320 3"; mk pump_old "flag stallgrace1320 0|flag reloadwait1320 3"
mk grace_new "flag reloadstall1320 7"; mk grace_old "flag stallgrace1320 0|flag reloadstall1320 7"
bad=0
for n in pump_new pump_old grace_new grace_old; do bash tools/emu_run.sh $O/$n.txt $n $E 300 > /dev/null 2>&1; f=$O/out_$n/smoke.txt
  cr=$(grep -ac '^CLEANRELOAD96 ms=' $f); en=$(grep -ac '^ROOM ev=enter room=KwikEMart' $f); bs=$(grep -a '^BBSTALL' $f | head -1 | cut -c1-60)
  mx=$(grep -a '^RELOADGAP total' $f | head -1 | sed -n 's/.*max_ms=\([0-9]*\).*/\1/p'); mx=${mx:--1}
  echo "$n: перезагрузок $cr, вход в комнату $en, max_ms=$mx, ${bs:-BBSTALL нет}"
  [ "$cr" -ge 1 ] && [ "$en" -ge 1 ] || { echo "FAIL $n: перезагрузка или вход не прошли"; bad=1; }
  case $n in
   pump_new) [ "$mx" -ge 0 ] && [ "$mx" -le 500 ] || { echo "FAIL pump_new: промежуток без кадра $mx мс > 500"; bad=1; };;
   pump_old) [ "$mx" -ge 2500 ] || { echo "КРАСНЫЙ НЕ ПОЙМАН (pump_old: max_ms=$mx < 2500)"; bad=1; };;
   grace_new) [ -z "$bs" ] || { echo "FAIL grace_new: сторож сработал"; bad=1; };;
   grace_old) [ -n "$bs" ] || { echo "КРАСНЫЙ НЕ ПОЙМАН (grace_old: BBSTALL нет)"; bad=1; };;
  esac
done
[ $bad = 0 ] && echo "STALL1320 PASS" || echo "STALL1320 FAIL"; exit $bad
