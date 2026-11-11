#!/bin/bash
# Гейт правки 139(2): правило зон стоящих машин (TRAFFIC_ZONE_PARK, по умолчанию выключено). Мак, CPU, без эмулятора.
#  (а) данные: tools/zones_check.py — прочтение строк загрузчиков сверено с AddTeleportDest level.mfk; красный — «;»/«:» наоборот;
#  (б) движок: tests/parkzone/host.c (viewer/traffic.c с -DTRAFFIC_ZONE_PARK=1) на build/level.shr + build/traffic.shr.
#      Красные: маски ворот наоборот (swap) — места у ворот в загруженных файлах пропадают; -DTRZ_PLANT_NOVIEW — без
#      проверки кадра стоящая исчезает на глазах. Потом зелёный. Итог: build/parkzone/*.txt
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
OUT=build/parkzone; mkdir -p "$OUT"; PY=.venv/bin/python; [ -x "$PY" ] || PY=python3
red=0
$PY tools/zones_check.py --swap > "$OUT/red_data_swap.txt" 2>&1
if [ $? -ne 0 ]; then echo "красный (а, «;»/«:» наоборот): упал, как должен"; red=$((red + 1)); else echo "КРАСНЫЙ НЕ ПОЙМАН: (а) swap прошёл"; fi
$PY tools/zones_check.py > "$OUT/green_data.txt" 2>&1; rcA=$?
build() { cc -std=c11 -O2 -w -DSKIN_HOST -DTRAFFIC_ZONE_PARK=1 "$@" -I viewer tests/parkzone/host.c viewer/traffic.c viewer/skin.c \
          viewer/col.c viewer/car.c viewer/watchdog.c -lm -o "$OUT/host$1" 2> "$OUT/build.txt" || { echo "FAIL: сборка"; head -5 "$OUT/build.txt"; exit 1; }; }
build -DGREEN; build -DTRZ_PLANT_NOVIEW
"$OUT/host-DGREEN" build/level.shr build/traffic.shr swap > "$OUT/red_swap.txt" 2>&1
if [ $? -ne 0 ] && grep -q "FAIL (3)" "$OUT/red_swap.txt"; then echo "красный (б, маски наоборот): упал, как должен"; red=$((red + 1))
else echo "КРАСНЫЙ НЕ ПОЙМАН: маски наоборот прошли"; fi
"$OUT/host-DTRZ_PLANT_NOVIEW" build/level.shr build/traffic.shr > "$OUT/red_noview.txt" 2>&1
if [ $? -ne 0 ] && grep -q "FAIL (1)" "$OUT/red_noview.txt"; then echo "красный (б, без проверки кадра): упал, как должен"; red=$((red + 1))
else echo "КРАСНЫЙ НЕ ПОЙМАН: без проверки кадра прошло"; fi
"$OUT/host-DGREEN" build/level.shr build/traffic.shr > "$OUT/green.txt" 2>&1; rcB=$?
grep "RESULT" "$OUT/green_data.txt"; grep "исчезло\|FAIL\|RESULT" "$OUT/green.txt"
if [ $red -eq 3 ] && [ $rcA -eq 0 ] && [ $rcB -eq 0 ]; then echo "PASS: красный 3/3 пойман, зелёный (а) и (б)"; exit 0; fi
echo "FAIL: красный $red/3, зелёный данные rc=$rcA, движок rc=$rcB"; exit 1
