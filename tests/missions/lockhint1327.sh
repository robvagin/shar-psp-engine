#!/bin/bash
# 1327: после запертого этапа (нужной машины/костюма нет) игра ставит паузу и показывает окно подсказки с Continue и текстом «что купить, где и за сколько»
#   (строки 49..58 страницы GenericPrompt диска). Десять этапов buycar/buyskin вступлений (L1m3 L2m6 L3m4 L3m5 L4m3 L4m6 L5m3 L5m4 L6m6 L7m2): номер подсказки 0..9 по порядку.
#   tests/missions/lockhint1327.sh ПАКЕТ [RED]   — RED=1: flag lockhint1327old 1, окна быть не должно (строки LOCKHINT open нет), гейт обязан упасть
# Случаи «уровень:номер миссии с 0:этап buycar:ключевое слово текста». Запуск с этапа buycar − 2 (talkto), msnpass ×2 (разговор, запертый диалог); в журнале ждём
#   «LOCKHINT open idx=N ok=1» и «SVMSG key=lockhint slot=N … text=…», потом «LOCKHINT closed» после ✕ Continue. Снимок hint_L<ур>_<N> — окно с текстом.
cd "$(dirname "$0")/../.."; P=${1:?пакет}; RED=${RED:-0}; T=${TMPDIR:-/tmp}/lh1327.$$; mkdir -p $T
CASES="1:3:9:PLOW_KING 2:5:6:MR._PLOW 3:3:2:SCHOOL_BUS 3:4:3:COOL 4:2:2:INMATE 4:5:4:POLICE 5:2:2:CUSTOM 5:3:3:AMERICAN 6:5:3:GLOBEX 7:1:3:ZOMBIE"
fail=0; red=0; n=0
for L in ${ONLY:-1 2 3 4 5 6 7}; do
  S=$T/l$L.scn; { echo "flag saves"; echo "scene g1327"; [ $RED = 1 ] && echo "flag lockhint1327old 1"; echo "flag golevel $L"; echo "wait 30"; } > $S
  for c in $CASES; do IFS=: read cl mi st kw <<<"$c"; [ $cl = $L ] || continue
    echo "flag msnmainat -$((mi*100+st-2))"; echo "wait 6"; echo "flag msnpass"; echo "wait 5"; echo "flag msnpass"; echo "wait 4"; echo "shot hint_L${cl}_$mi"; echo "press cross"; echo "wait 3"; done >> $S
  echo end >> $S
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN=$S SMOKE_MAXT=500 tools/smoke.sh "$P" >/dev/null 2>&1; D=$(ls -td build/smoke/2* | head -1)
  grep -a -E '^(LOCKHINT open|LOCKHINT closed|SVMSG key=lockhint)' $D/smoke.txt > $T/h$L.txt
  for c in $CASES; do IFS=: read cl mi st kw <<<"$c"; [ $cl = $L ] || continue; idx=$(( $(echo $CASES | tr " " "\n" | grep -n "^$cl:$mi:" | cut -d: -f1) - 1 )); lbl="L$cl m$([ $cl = 1 ] && echo $mi || echo $((mi+1))) подсказка $idx"
    op=$(grep -a -c "^LOCKHINT open idx=$idx ok=1" $T/h$L.txt); tx=$(grep -a "^SVMSG key=lockhint slot=$idx " $T/h$L.txt | grep -a -c -i "$(echo $kw | tr _ ' ' | cut -c1-8)\|purchase\|need"); cl_=$(grep -a -c "^LOCKHINT closed" $T/h$L.txt)
    if [ $RED = 1 ]; then if [ "$op" != 0 ]; then echo "$lbl: окно есть при выключателе (красный не пойман)"; red=1; else echo "$lbl: красный пойман, окна нет"; fi
    else if [ "$op" = 1 ] && [ "$tx" -ge 1 ]; then echo "$lbl: окно открыто, текст есть — ок"; else echo "$lbl: окна нет (open=$op text=$tx)"; fail=1; fi; fi
  done
  [ $RED = 1 ] || { c0=$(grep -a -c "^LOCKHINT closed" $T/h$L.txt); echo "L$L: закрытий после Continue: $c0"; }
  O=build/lockhint1327; [ $RED = 1 ] && O=build/lockhint1327_red; mkdir -p $O; cp -c $D/hint_L*.png $O/ 2>/dev/null
done
rm -rf $T
if [ $RED = 1 ]; then [ $red = 0 ] && { echo "красный пойман на всех"; exit 0; } || { echo FAIL; exit 1; }; fi
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
