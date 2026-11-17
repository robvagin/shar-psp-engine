#!/bin/bash
# Гейт 109 А5: крен в установившемся круге на ровной плите (famil_v, полный руль) против записей PS2 окна PS2 (s_ps2_c107_a5_twowheel_{40,60,80}_R, медианы окон по 8 кадров после входа в круг):
# крен на боковое ускорение у PS2 0,138 / 0,154 / 0,145 °/(м/с²); у порта на тех же скоростях круга (47 / 70 / 62 км/ч) должно быть 0,12…0,17.
# Красные контроли: -DCAR_TYRE_HUB_OLD=1 (прежняя модель шины) — 0,078 (мягче в 2 раза); -DCAR_TYRE_LEVER_OLD=1 — 0,357 (кладёт на бок).
# Прежний вывод «порт мягче PS2 в 2–3 раза» сравнивал максимумы (выбросы входа в руль и шум записи); по медианам крена на ускорение расхождения нет.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/c109; mkdir -p $OUT
run() { cc -std=c11 -O2 -w $1 -I viewer tests/car109/a5b.c viewer/car.c viewer/col.c -lm -Wl,-undefined,dynamic_lookup -o $OUT/a5_$2 || { echo "FAIL сборка"; exit 1; }; $OUT/a5_$2 | grep '^a5b'; }
in_range() { awk -v x="$1" 'BEGIN{exit !(x>=0.12 && x<=0.17)}'; }
fail=0
new=$(run "" new); echo "$new"
for r in $(echo "$new" | awk '{print $NF}'); do in_range "$r" || { echo "FAIL зелёный: $r вне 0,12…0,17"; fail=1; }; done
for v in CAR_TYRE_HUB_OLD CAR_TYRE_LEVER_OLD; do o=$(run "-D$v=1" $v); echo "красный $v: $(echo "$o" | awk '{print $NF}' | tr '\n' ' ')"
  bad=0; for r in $(echo "$o" | awk '{print $NF}'); do in_range "$r" || bad=1; done; [ $bad = 1 ] || { echo "КРАСНЫЙ НЕ ПОЙМАН: $v"; fail=1; }; done
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
