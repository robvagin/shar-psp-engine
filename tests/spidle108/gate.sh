#!/bin/bash
# 108 О11: гейт раннего выхода шага стейтпропов по признаку trivial (stateprops.c). Одинаковый хост собирается в прежнем дереве (git archive $BASE, по умолчанию rel/test107) и в новом; на уровнях L1–L7
# настоящие данные (SET), 1800 кадров, машина едет к экземплярам. Зелёный: журнал (хэш всех полей всех SpRun и счётчиков по кадрам) НОВОГО равен журналу ПРЕЖНЕГО на всех уровнях, строк кэша PSP по модели
# не больше 60 % прежнего. Красный (обязан упасть): -DSPIDLE_RED=1 (признак не обновляется при входе в состояние) — журнал другой хотя бы на одном уровне. Запуск: tests/spidle108/gate.sh
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
SET="${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15}"; BASE="${BASE:-rel/test107}"; EXTRA=(); [ -n "${EXTRAS:-}" ] && EXTRA=($EXTRAS)   # EXTRAS: -D…_OLD=1 правок 107б, меняющих физику после базы (П5 П6 П10…); FR="${FR:-1800}"
FR="${FR:-1800}"
OUT=build/spidle108; rm -rf "$OUT/old"; mkdir -p "$OUT/old"; (git archive "$BASE" viewer | tar -x -C "$OUT/old") || { echo "FAIL: git archive $BASE"; exit 1; }
SRCN=(tests/spidle108/host.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/car.c viewer/coins.c)
SRCO=(tests/spidle108/host.c "$OUT/old/viewer/col.c" "$OUT/old/viewer/props.c" "$OUT/old/viewer/stateprops.c" "$OUT/old/viewer/car.c" "$OUT/old/viewer/coins.c")
CC=(cc -std=c11 -O2 -w -DSKIN_HOST)
"${CC[@]}" -I viewer -DSP_HAS_TRIVIAL=1 ${EXTRA[@]+"${EXTRA[@]}"} "${SRCN[@]}" -lm -o "$OUT/new" 2> "$OUT/new.build" || { echo "FAIL: сборка нового"; head -5 "$OUT/new.build"; exit 1; }
"${CC[@]}" -I "$OUT/old/viewer" "${SRCO[@]}" -lm -o "$OUT/old_bin" 2> "$OUT/old.build" || { echo "FAIL: сборка прежнего"; head -5 "$OUT/old.build"; exit 1; }
"${CC[@]}" -I viewer -DSP_HAS_TRIVIAL=1 -DSPIDLE_RED=1 ${EXTRA[@]+"${EXTRA[@]}"} "${SRCN[@]}" -lm -o "$OUT/red" 2> "$OUT/red.build" || { echo "FAIL: сборка красного"; exit 1; }
fail=0; redcaught=0; ln_o=0; ln_n=0; lv=0
for L in 1 2 3 4 5 6 7; do F="$SET/l$L/level.shr"; [ $L = 1 ] && F="$SET/level.shr"; [ -f "$F" ] || { echo "нет $F"; fail=1; continue; }
  "$OUT/new" "$F" "$FR" > "$OUT/new_L$L.txt" 2>&1; "$OUT/old_bin" "$F" "$FR" > "$OUT/old_L$L.txt" 2>&1; "$OUT/red" "$F" "$FR" > "$OUT/red_L$L.txt" 2>&1
  g() { grep "^$2 " "$OUT/$1_L$L.txt" | cut -d' ' -f2; }
  jn=$(g new journal); jo=$(g old journal); jr=$(g red journal)
  echo "L$L: экземпляров $(g new instances), в «делать нечего» $(g new trivial); журнал новый $jn / прежний $jo; строк кэша на проход $(g old lines) → $(g new lines); запись $(g old sizeof_run) → $(g new sizeof_run) Б"
  [ -n "$jn" ] && [ "$jn" = "$jo" ] || { echo "  FAIL: журнал не равен прежнему"; fail=1; }
  [ "$jr" != "$jo" ] && redcaught=$((redcaught+1))
  ln_o=$(awk -v a=$ln_o -v b=$(g old lines) 'BEGIN{print a+b}'); ln_n=$(awk -v a=$ln_n -v b=$(g new lines) 'BEGIN{print a+b}'); lv=$((lv+1))
done
# 107в: чужая машина бьёт стейтпропы (П3, spHitOther) при включённом и выключенном «делать нечего» (О11): журнал один и тот же на всех уровнях, и он отличается от прогона без чужой машины (удары были)
other_bad=0; other_hits=0
for L in 1 2 3 4 5 6 7; do F="$SET/l$L/level.shr"; [ $L = 1 ] && F="$SET/level.shr"; [ -f "$F" ] || continue
  a=$(OTHER=1 "$OUT/new" "$F" "$FR" | grep '^journal'); b=$(OTHER=1 IDLEOLD=1 "$OUT/new" "$F" "$FR" | grep '^journal'); c0=$("$OUT/new" "$F" "$FR" | grep '^journal')
  [ "$a" = "$b" ] || { echo "FAIL: чужая машина на L$L: О11 вкл $a / выкл $b"; other_bad=1; }; [ "$a" != "$c0" ] && other_hits=$((other_hits+1)); done
echo "чужая машина (П3) с О11 вкл и выкл: журнал равен на всех уровнях ($([ $other_bad = 0 ] && echo да || echo НЕТ)); удары чужой машины меняли журнал на $other_hits из 7 уровней"
[ $other_bad = 0 ] && [ $other_hits -ge 3 ] || { echo "FAIL: чужая машина"; fail=1; }
awk -v o=$ln_o -v n=$ln_n 'BEGIN{exit !(n <= .6*o)}' || { echo "FAIL: строк кэша не меньше 60 % прежнего ($ln_o → $ln_n)"; fail=1; }
[ $redcaught -ge 1 ] && echo "красный SPIDLE_RED=1: упал, как должен (журнал другой на $redcaught из $lv уровней)" || { echo "КРАСНЫЙ SPIDLE_RED НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
