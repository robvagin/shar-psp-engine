#!/bin/bash
# №1069 приёмка (тур TEST79 на PSP 30.10: L5 без звука — sfx.sab 2 131 776 Б при наибольшем куске кучи 1,91 МБ; Роб: «у Апу не было звука»).
#   sizes      банки sfx.sab L1–L7 и бонуса в наборе влезают в постоянное место SAB_SLOT_BYTES (viewer/audio.c) — иначе место растёт в игре
#   new        flag sndcapsim 1.9 (банк звука из кучи больше 1,9 МБ не выделяется, как раздробленная куча PSP), L1 → L5 → L2 → L4: звук на каждом
#   red        то же на прежнем коде с запуска (файл sndslotold рядом с EBOOT + flag sndslotold 1: постоянного места нет) — обязан покраснеть: уровень без звука
#   cleannew / cleanold  без отказа: звук у всех обоими путями; цена места — свободная куча после загрузки, new против old
# Запуск: tests/sndslot1069/accept.sh [EBOOT] ; REUSE=1 — готовые прогоны не повторять. Последняя строка PASS/FAIL
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
E=${1:-$W/viewer/build_g80tour/EBOOT.PBP}; OUT=${OUT:-$W/build/snd1069}; mkdir -p $OUT/run; rc=0
export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test79tour/PSP/GAME/SHARTEST15} EMU_OWNER=${EMU_OWNER:-snd1069} EMU_MINE=${EMU_MINE:-4}
[ -f "$E" ] || { echo "нет EBOOT $E"; echo FAIL; exit 1; }
CAP=$(sed -n 's/^#define SAB_SLOT_BYTES (\([0-9]*\)u << 10).*/\1/p' $W/viewer/audio.c); CAP=$((CAP * 1024))
MAX=0; for b in $SET/audio/sfx.sab $SET/l[2-7]/audio/sfx.sab $SET/bonus/audio/sfx.sab; do [ -f $b ] || continue; s=$(stat -f %z $b); [ $s -gt $MAX ] && MAX=$s; done
[ $CAP -gt 0 ] && [ $MAX -gt 0 ] && [ $MAX -le $CAP ] && echo "sizes: ok наибольший банк $MAX Б ≤ место $CAP Б" || { echo "sizes: FAIL наибольший банк $MAX Б, место $CAP Б"; rc=1; }
for m in new red cleannew cleanold; do
  [ "${REUSE:-0}" = 1 ] && [ -f $OUT/run/out_$m/smoke.txt ] && continue
  python3 $H/gen.py $m 1 5 2 4 > $OUT/$m.txt
  echo "$m" ; done | xargs -P 4 -n 1 bash -c 'case $0 in red|cleanold) F=sndslotold;; *) F=;; esac; FLAGS=$F OUT='"$OUT"'/run bash '"$W"'/tools/emu_run.sh '"$OUT"'/$0.txt $0 '"$E"' ${MAXT:-600} >'"$OUT"'/$0.log 2>&1'
for m in new red cleannew cleanold; do echo "$m:"; o=$(python3 $H/check.py $m $OUT/run/out_$m/smoke.txt 2>$OUT/heap_$m.txt); r=$?; echo "$o" | sed 's/^/  /' | cut -c1-230; [ $r = 0 ] || rc=1; done
echo "цена места (куча после загрузки, new − old): $(paste -d' ' $OUT/heap_cleannew.txt $OUT/heap_cleanold.txt | awk '{printf "%s %+d КБ; ", $2, $3 - $6}')"
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
