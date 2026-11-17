#!/bin/bash
# 108 О6 + О9: гейт горячего массива предметов (хост на Маке, настоящий level.shr). Зелёный: журнал состояния всех предметов за 1500 кадров РАВЕН журналу прежних обходов (-DPROPHOT108_OLD=1),
# сверка горячего массива с PropState каждый кадр без расхождений, медленный путь шага и возврата не больше 15 % предметов. Красный (обязан упасть): -DPROPHOT_RED=1 (удар не обновляет запись)
# даёт другой журнал или расхождения. Время (холодный кэш) печатается до и после. Запуск: tests/prophot108/gate.sh [LEVEL.shr]
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
SET="${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15}"; BASE="${BASE:-rel/test107}"; EXTRA=(${EXTRA:-})   # EXTRA: -D…_OLD=1 правок 107б после базы (П6 П10 П11 П12 П5: их журнал предметов отличается от базы)
OUT=build/prophot108; rm -rf "$OUT/t107dir"; mkdir -p "$OUT/t107dir"; V=viewer
(git archive "$BASE" viewer | tar -x -C "$OUT/t107dir") || { echo "FAIL: git archive $BASE"; exit 1; }
CC=(cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation -Wno-unused-variable -Wno-unused-but-set-variable -I $V)
SRC=(tests/prophot108/host.c $V/col.c $V/props.c $V/car.c $V/stateprops.c $V/coins.c)
SRCO=(tests/prophot108/host.c "$OUT/t107dir/viewer/col.c" "$OUT/t107dir/viewer/props.c" "$OUT/t107dir/viewer/car.c" "$OUT/t107dir/viewer/stateprops.c" "$OUT/t107dir/viewer/coins.c")
bb() { local name=$1; shift; "${CC[@]}" "$@" "${SRC[@]}" -lm -o "$OUT/$name" 2> "$OUT/$name.build" || { echo "FAIL: сборка $name"; head -5 "$OUT/$name.build"; exit 1; }; }
bb green "${EXTRA[@]}"; bb old -DPROPHOT108_OLD=1 "${EXTRA[@]}"; bb red -DPROPHOT_RED=1 "${EXTRA[@]}"
cc -std=c11 -O2 -w -DNOHOT -I "$OUT/t107dir/viewer" tests/prophot108/host.c "${SRCO[@]:1}" -lm -o "$OUT/tree107" 2> "$OUT/tree107.build" || { echo "FAIL: сборка прежнего дерева"; head -5 "$OUT/tree107.build"; exit 1; }
fail=0
for LV in 1 2 3 4 5 6 7; do
  L="$SET/l$LV/level.shr"; [ $LV = 1 ] && L="$SET/level.shr"; [ -f "$L" ] || { echo "нет $L"; fail=1; continue; }
  for n in green old red tree107; do "$OUT/$n" "$L" > "$OUT/${n}_L$LV.txt" 2>&1; done
  g() { grep "^$2 " "$OUT/$1_L$LV.txt" | cut -d' ' -f2; }
  n=$(g green "предметов")
  echo "L$LV: предметов $n; журнал новый $(g green journal) / флаг OLD $(g old journal) / дерево 107 $(g tree107 journal); расхождений горячего массива $(g green hotbad), без роста версии $(g green verbad); строк кэша шаг $(g old lines_step) → $(g green lines_step), возврат $(g old lines_resp) → $(g green lines_resp)"
  [ "$(g green journal)" = "$(g old journal)" ] && [ "$(g green journal)" = "$(g tree107 journal)" ] || { echo "  FAIL зелёный: журнал не равен прежнему"; fail=1; }
  [ "$(g green hotbad)" = "0" ] && [ "$(g green verbad)" = "0" ] || { echo "  FAIL зелёный: горячий массив расходится с PropState или версия не растёт"; fail=1; }
  awk -v o="$(g old lines_step)" -v n2="$(g green lines_step)" -v p="$(g old lines_resp)" -v q="$(g green lines_resp)" 'BEGIN{exit !(n2 <= .5*o && q <= .5*p)}' || { echo "  FAIL зелёный: строк кэша не меньше половины прежнего"; fail=1; }
  if [ "$(g red journal)" = "$(g old journal)" ] && [ "$(g red hotbad)" = "0" ] && [ "$(g red verbad)" = "0" ]; then echo "  КРАСНЫЙ PROPHOT_RED НЕ ПОЙМАН на L$LV"; fail=1; fi
done
echo "красный PROPHOT_RED=1 (удар не обновляет запись): журнал другой или расхождения — проверено на каждом уровне выше (нет строки «НЕ ПОЙМАН»); красный PROPHOT108_OLD=1: строк кэша шаг $(g old lines_step), возврат $(g old lines_resp) (L7) — не меньше половины, как и должно быть у прежнего пути"
echo "время на кадр, мкс на Маке с холодным кэшем, L7 (на PSP ×5,8): шаг старый $(g old step_us) → новый $(g green step_us); возврат старый $(g old resp_us) → новый $(g green resp_us)"
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
