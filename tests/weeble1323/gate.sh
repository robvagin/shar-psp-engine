#!/bin/bash
# 1323 гейт: «неваляшка по контакту» (WEEBLE1323_OLD). Красный контроль = -DWEEBLE1323_OLD=1 (старое правило: A_борт и B_полёт = 0).
# Стенд tests/weeble1323/w.c: A — на плите подставлен контакт (колёса борта (0,2) или диагонали (0,3) без контакта при сжатой подвеске): число шагов момента; B — трамплин 1,5 м на 70 км/ч, шаги момента в полёте;
# C — круг 80 км/ч с ручником 1 с, крен; D — прямая 60 км/ч, ложные срабатывания.
# Зелёное: cletu_v (борта): A_борт>0 A_диаг=0; famil_v (диагональ): A_борт=0 A_диаг>0; у обоих B>0, D=0, C <= 17,5 / 4,5°. Красный: старый A_борт=0 и B=0 у cletu_v.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; O=build/w1323; mkdir -p $O; fail=0
bld() { cc -std=c11 -O2 -w $1 -I viewer tests/weeble1323/w.c viewer/car.c viewer/col.c -lm -Wl,-undefined,dynamic_lookup -o $O/w_$2 || { echo "FAIL сборка"; exit 1; }; }
bld "" new; bld "-DWEEBLE1323_OLD=1" old
get() { echo "$1" | tr ' ' '\n' | grep "^$2=" | head -1 | cut -d= -f2 | sed 's/(.*//'; }
for car in cletu_v famil_v; do
  n=$($O/w_new $car); o=$($O/w_old $car); echo "новый:  $n"; echo "старый: $o"
  As=$(get "$n" A_борт); Ad=$(get "$n" A_диаг); B=$(get "$n" B_полёт); C=$(get "$n" C_крен); D=$(get "$n" D_прямая); Aso=$(get "$o" A_борт); Bo=$(get "$o" B_полёт)
  if [ "$car" = cletu_v ]; then lim=17.5; [ "${As:-0}" -gt 0 ] && [ "${Ad:-1}" -eq 0 ] || { echo "FAIL $car: пара борта должна включать, диагональ нет (A_борт=$As A_диаг=$Ad)"; fail=1; }
    [ "${Aso:-1}" -eq 0 ] && [ "${Bo:-1}" -eq 0 ] || { echo "КРАСНЫЙ НЕ ПОЙМАН: старый A_борт=$Aso B=$Bo"; fail=1; }
  else lim=4.5; [ "${As:-1}" -eq 0 ] && [ "${Ad:-0}" -gt 0 ] || { echo "FAIL $car: диагональ должна включать, борт нет (A_борт=$As A_диаг=$Ad)"; fail=1; }; fi
  [ "${B:-0}" -gt 0 ] || { echo "FAIL $car: в полёте момента нет"; fail=1; }
  [ "${D:-1}" -eq 0 ] || { echo "FAIL $car: ложные срабатывания на прямой ($D)"; fail=1; }
  awk -v c="$C" -v l="$lim" 'BEGIN{exit !(c<=l)}' || { echo "FAIL $car: крен круга $C > $lim"; fail=1; }
done
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
