#!/bin/bash
# 107в П4: контакт машины с предметом по объёмам из данных диска (PVL1), L1. Нужен level.shr, выгруженный новым tools/psp_export.py (с таблицей PVL1): LEVEL по умолчанию build/p4/level.shr. Зелёный: нарушений не больше 3 % заездов
# (точно задел — не задел; точно мимо — задел). Красный (propsVol107Old=1, контакт по стволу/кругу/тонкой коробке): нарушений больше 10 %.   tests/propvol107/gate.sh [LEVEL.shr]
set -u; cd "$(dirname "$0")/../.."; L="${1:-build/p4/level.shr}"; OUT=build/propvol107; mkdir -p $OUT
cc -std=c11 -O2 -w -DSKIN_HOST -I viewer tests/propvol107/host.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/car.c viewer/coins.c -lm -o $OUT/g || { echo "FAIL: сборка"; exit 1; }
g() { grep "^$2 " $OUT/$1.txt | cut -d' ' -f2; }
$OUT/g $L 0 > $OUT/new.txt; $OUT/g $L 1 > $OUT/old.txt
echo "новое: типов $(g new types), заездов $(g new runs), нарушений $(g new viol) (не задел $(g new missed), призрак $(g new ghost)); красное (по стволу/кругу): нарушений $(g old viol) из $(g old runs) (не задел $(g old missed), призрак $(g old ghost))"
awk -v v=$(g new viol) -v r=$(g new runs) -v ov=$(g old viol) -v or=$(g old runs) 'BEGIN{exit !(r>100 && v<=.03*r && ov>=.10*or)}' && { echo PASS; exit 0; }; echo FAIL; exit 1
