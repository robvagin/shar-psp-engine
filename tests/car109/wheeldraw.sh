#!/bin/bash
# Гейт 108 «колёса на экране по фактическому ходу подвески» (carvis.c / traffic.c / skid.c через carWheelDy / carWheelGround в car.h).
# Физика держит контакт колеса до хода −0,266 м ниже нейтрали (подвеска 1278), в воздухе колесо свисает на 0,40 м (PS2). Рисунок по прежнему сжатию comp (≥ 0) не опускался ниже нейтрали: колесо висело над землёй до 0,27 м.
# Зелёный: на уступе (вверх 30 км/ч, вниз 20 км/ч и обрыв 0,8 м на 40 км/ч — колёса в воздухе, tests/car109/curb.c) рисунок колеса отличается от хода подвески (xs − eq) не больше чем на 2 см во всех колёсо-кадрах контакта; в воздухе — тоже.
# Красный (-DCAR_WHEELDRAW108_OLD=1, рисунок по comp): колёс-кадров с ошибкой > 2 см должно быть > 0.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/c109/wd; mkdir -p $OUT
run() { cc -std=c11 -O2 -w $1 -I viewer tests/car109/curb.c viewer/car.c viewer/col.c -lm -Wl,-undefined,dynamic_lookup -o $OUT/c_$2 || { echo FAIL сборка; exit 1; }; for d in up down drop; do $OUT/c_$2 $d | grep '^draw'; done; }
new=$(run "" new); old=$(run "-DCAR_WHEELDRAW108_OLD=1" old)
echo "$new"; echo "красное:"; echo "$old"
fail=0
echo "$new" | awk '{for(i=1;i<=NF;i++){ if($i=="2"&&$(i+1)=="см:") n=$(i+2); if($i=="макс"&&!m) m=$(i+1)}; if (n+0>0) bad=1} END{exit bad}' || { echo "FAIL зелёный"; fail=1; }
echo "$old" | awk '{for(i=1;i<=NF;i++) if($i=="2"&&$(i+1)=="см:") n+=$(i+2)} END{exit !(n>0)}' && echo "красный CAR_WHEELDRAW108_OLD=1: пойман" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
