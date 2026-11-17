#!/bin/bash
# Гейт 1330: «машина лежит подолгу» (автосброс игрока). Красный контроль = -DSV_FLIPEMA1330_OLD=1 -DSV_LEANREST1330_OLD=1.
#  A (ema.c): вверх колёсами в покое, мгновенные скорость и вращение нули: по оригиналу (скользящее среднее 1/15 от 0,6 рад/с) условие сброса верно на 11-м кадре; старое — на 1-м.
#  B (sweep.c): 400 случайных кувырков над плитой (seed 1330) для cletu_v и marge_v; считаем машины, что остались лежать (подъём < 0,6 через 12 с без сброса). Новое: не больше 1 на машину (ползёт быстрее 1 м/с),
#     сброс сработал у большинства легших; старое: у cletu_v не меньше 15 лежат бесконечно (на двух колёсах под 64°), сброса нет.
cd "$(dirname "$0")/../.."; V=viewer; O=${TMPDIR:-/tmp}/flip1330; mkdir -p $O; fail=0
bld() { cc -std=c11 -O2 -w $1 -I $V tests/flip1330/$2.c $V/car.c $V/col.c -lm -Wl,-undefined,dynamic_lookup -o $O/$2_$3 || { echo "FAIL сборка"; exit 1; }; }
OLD="-DSV_FLIPEMA1330_OLD=1 -DSV_LEANREST1330_OLD=1"
bld "" ema new; bld "$OLD" ema old; bld "" sweep new; bld "$OLD" sweep old
n=$($O/ema_new | sed 's/.*кадре \([0-9]*\).*/\1/'); o=$($O/ema_old | sed 's/.*кадре \([0-9]*\).*/\1/')
echo "A: сброс вверх колёсами в покое: новое кадр $n (ждём 11), прежнее кадр $o (ждём 1)"
[ "$n" = 11 ] || { echo "FAIL A новое"; fail=1; }; [ "$o" = 1 ] || { echo "FAIL A красный не пойман"; fail=1; }
lying() { grep "не сработал" | sed 's/.*up=\([-0-9.]*\).*/\1/' | awk '$1 < 0.6' | wc -l | tr -d ' '; }
for car in cletu_v marge_v; do
  rn=$($O/sweep_new $car 400); ro=$($O/sweep_old $car 400)
  ln=$(echo "$rn" | lying); lo=$(echo "$ro" | lying); fn=$(echo "$rn" | tail -1 | sed 's/.*сброс сработал \([0-9]*\).*/\1/'); fo=$(echo "$ro" | tail -1 | sed 's/.*сброс сработал \([0-9]*\).*/\1/')
  echo "B $car: лежат без сброса через 12 с: новое $ln (сброс сработал $fn), прежнее $lo (сброс сработал $fo)"
  [ "$ln" -le 1 ] || { echo "FAIL B $car: лежат $ln"; fail=1; }
  if [ $car = cletu_v ]; then [ "$lo" -ge 15 ] || { echo "FAIL B красный не пойман: прежнее лежат $lo < 15"; fail=1; }; [ "$fn" -ge 15 ] || { echo "FAIL B сработало мало: $fn"; fail=1; }; fi
done
[ $fail = 0 ] && { echo "ГЕЙТ tests/flip1330: PASS"; exit 0; }; echo "ГЕЙТ tests/flip1330: FAIL"; exit 1
