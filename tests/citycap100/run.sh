#!/bin/bash
# 100 (дирижёр, cellcap на L5): потолок дальности по ёмкости области города (CITYCAP100_OLD / файл citycap100old). tests/citycap100/run.sh EBOOT [g r]   LV=5|6 (по умолчанию 5)
# Уровни с областью города: 2, 5, 6 (L4 на куче, область выключена - потолок не работает, check.py для него не нужен). g - зелёный (правка включена), r - красный контроль (+ файл citycap100old, ожидается упор cellcap/city-more). Загрузка уровня LV, 2 мин езды; безголово, nice 20, эмулятор один.
# Результат: $OUT/citycap_<имя>L<LV>.txt (check.py), последняя строка PASS/FAIL. Окружение как у гейтов Будки: набор test96tour, ballast 5, без horizon.shr (иначе потолок дали по зонам режет 250 м)
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; EB=${1:?EBOOT}; shift; RUNS=${*:-g r}; LV=${LV:-5}
OUT=${OUT:-$W/build/citycap100}; export OUT SMOKE_HEADLESS=1 SMOKE_BALLAST=${SMOKE_BALLAST:-5} SHIP_LEVELS="2 3 4 5 6 7"; mkdir -p "$OUT"; cd "$W"
export SET=${SET:-$HOME/Downloads/shar-psp-t93/build/test96tour/PSP/GAME/SHARTEST15}; rc=0
[ -n "${OVL:-}" ] || { mkdir -p "$OUT/ovl_empty"; export OVL=$OUT/ovl_empty; }
sed "s/\${LV}/$LV/; s/L5_/L${LV}_/" "$H/scene.txt" > "$OUT/scene_L$LV.txt"
for n in $RUNS; do case $n in g) fl="";; r) fl="citycap100old";; esac; nm=$n$LV
  FLAGS="$fl" nice -n 20 tools/emu_run.sh "$OUT/scene_L$LV.txt" "$nm" "$EB" 700 > "$OUT/$nm.log" 2>&1; o=$OUT/out_$nm
  grep -E "^(DDREG|CITYCAP|CITYMEM|SCENE|END|WATCHDOG|MEMFAILSZ|MISSCELL|LEVEL ev=load)" "$o/smoke.txt" > "$o/key.txt"
  python3 -I "$H/check.py" "$o/key.txt" $LV $([ $n = r ] && echo red) > "$OUT/citycap_$nm.txt" 2>&1; c=$?; tail -4 "$OUT/citycap_$nm.txt"
  find "$o" -type f \( -name smoke.txt -o -name blackbox.txt -o -name headless.log -o -name '*.bmp' \) -delete; rm -rf "$OUT/emu_home_$nm" "$OUT/rr_$nm"
  if [ $n = g ] && [ $c != 0 ]; then rc=1; fi; if [ $n = r ] && [ $c = 0 ]; then echo "красный контроль НЕ покраснел"; rc=1; fi
done
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
