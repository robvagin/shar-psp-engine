#!/bin/bash
# Гейт 109 1279: касательное трение при ударе машина-машина (car.c carHitCar, CARHITFRIC1279_OLD / flag hitfric1279old, μ = CARHIT_MU 0,3).
# Запись PS2 (окно PS2, s_ps2_c107_v_ram_r1pc10_th45_v40 и th90_v40; цель R1PC10 стоит, газ у игрока держится): 45°/40 — игрок 0,7 км/ч через 25 кадров (0,9 и 0,8 через 50 и 100), цель смещена на 0,97 м;
# 90°/40 — игрок 14,3 км/ч через 25 кадров, цель 2,44 м за 50 кадров. Стенд tests/fiz105/carram.c (тот же carHitCar/carUpdate, цель — стоящая minivana на ручнике), угол 45°/90° к оси цели.
# Зелёный: 45°/40 — скорость игрока через 0,5 с ≤ 12 км/ч (PS2 ≈ 1) и цель за 1 с в 0,5…1,6 м (PS2 0,97); 90°/40 не хуже, чем без трения (±1 км/ч, ±0,3 м).
# Красный (-DCARHITFRIC1279_OLD=1, как было): 45°/40 игрок через 0,5 с = 20 км/ч → упал.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/c109; mkdir -p $OUT
run() { cc -std=c11 -O2 -w $1 -I viewer tests/fiz105/carram.c viewer/car.c viewer/col.c -lm -Wl,-undefined,dynamic_lookup -o $OUT/ram_$2 || { echo "FAIL сборка"; exit 1; }
  $OUT/ram_$2 minivana | python3 -c "
import re,sys
for l in sys.stdin:
    m=re.search(r'(\d+)° (\d+) км/ч.*игрок через 0\.5 с (\d+) км/ч.*смещена за 0\.5 с ([\d.]+) м, за 1 с ([\d.]+) м',l)
    if m and m.group(2)=='40': print(m.group(1),m.group(3),m.group(5))
"; }
new=$(run "" new); old=$(run "-DCARHITFRIC1279_OLD=1" old)
g() { echo "$1" | awk -v a="$2" '$1==a {print $2, $3}'; }
n45=($(g "$new" 45)); o45=($(g "$old" 45)); n90=($(g "$new" 90)); o90=($(g "$old" 90))
echo "новое 45°/40: игрок ${n45[0]} км/ч, цель за 1 с ${n45[1]} м (PS2 ≈1 км/ч, 0,97 м); старое: игрок ${o45[0]}, цель ${o45[1]}; 90°/40 новое ${n90[0]} км/ч ${n90[1]} м, старое ${o90[0]} км/ч ${o90[1]} м (PS2 14, 2,4 м)"
fail=0
awk -v v="${n45[0]}" -v d="${n45[1]}" 'BEGIN{exit !(v<=12 && d>=0.5 && d<=1.6)}' || { echo "FAIL зелёный 45°"; fail=1; }
awk -v a="${n90[0]}" -v b="${o90[0]}" -v c="${n90[1]}" -v d="${o90[1]}" 'BEGIN{x=a-b;if(x<0)x=-x;y=c-d;if(y<0)y=-y;exit !(x<=1 && y<=0.3)}' || { echo "FAIL зелёный 90° испорчен"; fail=1; }
awk -v v="${o45[0]}" 'BEGIN{exit !(v<=12)}' && { echo "КРАСНЫЙ НЕ ПОЙМАН (старое тоже ≤ 12)"; fail=1; } || echo "красный CARHITFRIC1279_OLD=1: упал, как должен"
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
