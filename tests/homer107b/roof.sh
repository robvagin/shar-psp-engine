#!/bin/bash
# Гейт окна Гомер 107b, пункт Г4/Г5 «крыша машины» (сверка Fable-2; homer.c HS_FOOT, traffic.c trafficRide): Г5 по стоящей своей машине идёт обычным шагом, Г4 едущая и поворачивающая
# машина трафика не уезжает из-под ног. Данные: L1 из выпуска 106. Красный: -DHOMERRIDE107B_OLD=1 — Г5 ползёт < 1 м за 1 с, Г4 падает с крыши.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/h107b; mkdir -p "$OUT"; V=viewer
D=${SHR_DATA:-$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15}
for f in "$D/level.shr" "$D/homer.shr"; do [ -f "$f" ] || { echo "FAIL: нет $f"; exit 1; }; done
SRC=(tests/homer107b/roof.c $V/homer.c $V/skin.c $V/col.c $V/car.c $V/props.c $V/stateprops.c $V/coins.c $V/traffic.c $V/msncar.c $V/watchdog.c $V/mission/msn_file.c $V/mission/msn_ops.c)
b() { cc -std=c11 -O2 -w -DSKIN_HOST "$@" -I $V "${SRC[@]}" -lm -o "$OUT/roof_$NAME" && "$OUT/roof_$NAME" "$D/level.shr" "$D/homer.shr" > "$OUT/roof_$NAME.txt"; }
NAME=new; b || { echo "FAIL: сборка"; exit 1; }; NAME=old; b -DHOMERRIDE107B_OLD=1 || exit 1
g() { grep "^$2 " "$OUT/roof_$1.txt" | cut -d' ' -f2; }
echo "новое: Г5 путь за 1 с $(g new g5_path_m) м, Г4 на крыше $(g new g4_stays_on_roof) (дрейф в осях машины $(g new g4_car_frame_drift_m) м); старое: Г5 $(g old g5_path_m) м, Г4 $(g old g4_stays_on_roof) (дрейф $(g old g4_car_frame_drift_m) м)"
ge() { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a>=b)}'; }; lt() { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a<b)}'; }
fail=0
{ ge "$(g new g5_path_m)" 2.0 && [ "$(g new g4_stays_on_roof)" = 1 ] && [ "$(g new g5_onCar)" = 1 ] && [ "$(g new g4_standing_on_roof)" = 1 ]; } || { echo "FAIL зелёный"; fail=1; }
{ lt "$(g old g5_path_m)" 1.0 && [ "$(g old g4_stays_on_roof)" = 0 ]; } && echo "красный HOMERRIDE107B_OLD=1: упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
