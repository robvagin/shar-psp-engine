#!/bin/bash
# Гейт окна Гомер 107b, пункт «невидимое пешком» (сверка Fable-2 П7/П8; homer.c homerSolidSprops): стейтпроп твёрдый для пешего там, где его объём (как у машины) ушёл за суставом,
# и не твёрдый в состоянии, где ничего не рисуется. Данные: уровни 1–7 из выпуска 106. Зелёный: новое — П7 все выталкивают, П8 с рисунком выталкивает, без рисунка нет.
# Красный: -DHOMERSP107B_OLD=1 — П7 не выталкивает ни одного (объём в позе кадра 0), П8 без рисунка выталкивает.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/h107b; mkdir -p "$OUT"; V=viewer
D=${SHR_DATA:-$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15}
ARGS=("$D/level.shr" "$D/objnames.shr"); for n in 2 3 4 5 6 7; do ARGS+=("$D/l$n/level.shr" "$D/l$n/objnames.shr"); done
for f in "${ARGS[@]}"; do [ -f "$f" ] || { echo "FAIL: нет $f"; exit 1; }; done
b() { cc -std=c99 -O2 -w -DSKIN_HOST "$@" -I $V tests/homer107b/pedsolid.c $V/homer.c $V/watchdog.c $V/skin.c $V/col.c $V/car.c $V/props.c $V/stateprops.c $V/coins.c -lm -o "$OUT/ped_$NAME" && "$OUT/ped_$NAME" "${ARGS[@]}" > "$OUT/ped_$NAME.txt"; }
NAME=new; b || { echo "FAIL: сборка"; exit 1; }; NAME=old; b -DHOMERSP107B_OLD=1 -DHOMERP9107_OLD=1 || exit 1
g() { grep "^$2 " "$OUT/ped_$1.txt" | cut -d' ' -f2; }
echo "новое: П7 $(g new p7_pushed)/$(g new p7_total), П8 контроль $(g new p8_control_pushed)/$(g new p8_total), без рисунка выталкивает $(g new p8_noart_pushed); старое: П7 $(g old p7_pushed)/$(g old p7_total), без рисунка выталкивает $(g old p8_noart_pushed); П9 целый $(g new p9_whole_pushed)/$(g new p9_total), отреагировавший новое $(g new p9_reacted_pushed) против старого $(g old p9_reacted_pushed)"
fail=0
{ [ "$(g new p9_total)" -gt 0 ] && [ "$(g new p9_whole_pushed)" = "$(g new p9_total)" ] && [ "$(g new p9_reacted_pushed)" = 0 ] && [ "$(g old p9_reacted_pushed)" -gt 0 ]; } || { echo "FAIL П9 (целый твёрдый, отреагировавший проходим, старое держит)"; fail=1; }
[ "$(g new p7_total)" -gt 0 ] && [ "$(g new p7_pushed)" = "$(g new p7_total)" ] && [ "$(g new p8_total)" -gt 0 ] && [ "$(g new p8_control_pushed)" = "$(g new p8_total)" ] && [ "$(g new p8_noart_pushed)" = 0 ] || { echo "FAIL зелёный"; fail=1; }
[ "$(g old p7_pushed)" -lt "$(g old p7_total)" ] && [ "$(g old p8_noart_pushed)" -gt 0 ] && echo "красный HOMERSP107B_OLD=1: упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
