#!/bin/bash
# Гейт 1279 «забор не тормозит правилом CAR_CRASH_ANG» (car.c, сверка по записям PS2 окна PS2: s_ps2_c107_b1_th45_v80_dirp и др., забор L1 f008, газ держится после касания).
# Числа PS2 (чистые записи, доля скорости от входной через 0,5 с / через 1 с): 20°/80: 1,04/1,09; 30°/80: 0,93/1,02; 45°/80: 0,56/0,69.
# Зелёный (новое): 45°/80 в пределах 0,12 от PS2 на обоих отсчётах, 20°/80 в пределах 0,12. Красный (-DCAR_FENCEANG1279_OLD=1): 45°/80 = 0,05/0,07 (стоит). 30°/80 остаётся ниже PS2 на 0,19/0,12 (решатель стены, не это правило): записано, не гейтится.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/c107b; mkdir -p $OUT; V=viewer
LV=${1:-$HOME/Downloads/shar-psp-dal103/build/test106/PSP/GAME/SHARTEST15/level.shr}; [ -f "$LV" ] || { echo "FAIL: нет $LV"; exit 1; }
for k in new old; do F=(); [ $k = old ] && F=(-DCAR_FENCEANG1279_OLD=1)
  cc -std=c11 -O2 -w ${F[@]+"${F[@]}"} -I $V tests/car107b/fence1279.c $V/car.c $V/col.c -lm -Wl,-undefined,dynamic_lookup -o $OUT/f79_$k || { echo "FAIL: сборка"; exit 1; }
  $OUT/f79_$k "$LV" | grep '^fence' > $OUT/f79_$k.txt; done
g() { awk -v a="$2" -v b="$3" -v c="$4" '$2==a && $3==b {print $c}' $OUT/f79_$1.txt; }
near() { awk -v x="$1" -v y="$2" -v t="$3" 'BEGIN{d=x-y; if(d<0)d=-d; exit !(d<=t)}'; }
echo "новое: 45°/80 $(g new 45 80 4)/$(g new 45 80 5) (PS2 0,56/0,69), 20°/80 $(g new 20 80 4)/$(g new 20 80 5) (PS2 1,04/1,09), 30°/80 $(g new 30 80 4)/$(g new 30 80 5) (PS2 0,93/1,02); старое 45°/80 $(g old 45 80 4)/$(g old 45 80 5)"
fail=0
{ near "$(g new 45 80 4)" 0.56 0.12 && near "$(g new 45 80 5)" 0.69 0.12 && near "$(g new 20 80 4)" 1.04 0.12 && near "$(g new 20 80 5)" 1.09 0.12; } || { echo "FAIL зелёный"; fail=1; }
{ ! near "$(g old 45 80 4)" 0.56 0.12; } && echo "красный CAR_FENCEANG1279_OLD=1: упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
