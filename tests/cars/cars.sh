#!/bin/bash
# Гейт «машина уровня в движке» (29.09): у каждого уровня своя машина из данных — имя в CAR1 v2, ходовые из её .con,
# кузов, сиденье и дверь из модели, камера FollowCameraData. Каждая проверка сначала КРАСНАЯ на подложенном плохом
# входе, потом ЗЕЛЁНАЯ на настоящих данных (закон 5). Уровни — build/levels/l<N>/level.shr (tests/cars/build_levels.sh).
#   1 машина, .con, камера, кузов, сиденье, дверь   tests/cars/params_host (viewer/car.c) + tests/cars/car_check.py
#       красные: чужое имя машины в CAR1 (level.shr уровня 1 как уровень 2), устаревшая таблица .con (масса honor_v),
#                сиденье за бортом, точка у двери внутри кузова
#   2 уровень 1 как до 29.09: всё, что движок берёт о машине (CarParams), побайтно = carDefaults (famil_v)
#       красный: крыша famil_v выше на 1 см
#   3 езда, выход и посадка на машине уровня в мире ботов   tests/cars/drive_host (tools/bots/world.c, homer.c, car.c)
#       красные: level.shr уровня 1 с машиной «honor_v» (мир ждёт famil_v), дверь внутри кузова (Гомер не дойдёт)
#   tests/cars/cars.sh [уровни, по умолчанию 1..7]   -> PASS / FAIL, журнал build/gates/cars/
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
PY=.venv/bin/python; G=build/gates/cars; mkdir -p "$G"
LEVELS=${*:-1 2 3 4 5 6 7}
red=0; nred=0; green=0; ngreen=0
# красный засчитан, только если проверка упала своим «ПРОВАЛ»/«FAIL» (падение программы с трассировкой — не красный)
R() { nred=$((nred + 1)); local n="$1"; shift; if "$@" > "$G/red_$n.txt" 2>&1; then echo "КРАСНЫЙ НЕ ПОЙМАН: $n";
  elif grep -q Traceback "$G/red_$n.txt" || ! grep -qE 'ПРОВАЛ|FAIL' "$G/red_$n.txt"; then echo "КРАСНЫЙ НЕ ПО ДЕЛУ: $n — $(tail -1 "$G/red_$n.txt" | cut -c1-100)";
  else red=$((red + 1)); echo "красный ($n): $(grep -m1 -E 'ПРОВАЛ|FAIL' "$G/red_$n.txt" | sed 's/^ *//' | cut -c1-120)"; fi; }
plant() { "$@" > /dev/null 2>&1 || { echo "FAIL: не подложить: $*"; exit 1; }; }
Gn() { ngreen=$((ngreen + 1)); local n="$1"; shift; if "$@" > "$G/green_$n.txt" 2>&1; then green=$((green + 1)); echo "зелёный ($n): $(tail -1 "$G/green_$n.txt" | cut -c1-120)"; else echo "НЕ ЗЕЛЁНЫЙ: $n — $(grep -m1 -E 'ПРОВАЛ|FAIL' "$G/green_$n.txt" | sed 's/^ *//' | cut -c1-120)"; fi; }
lvl() { [ -f "build/levels/l$1/level.shr" ] && echo "build/levels/l$1/level.shr"; }
car_of() { sed -n 's/^[^/]*InitLevelPlayerVehicle *( *"\([^"]*\)".*/\1/p' "hit-and-run-web/source/game/scripts/missions/level0$1/leveli.mfk" | head -1; }

# сборка проверочных программ: настоящая и с подложенной устаревшей таблицей .con (масса honor_v 1500 вместо 1200)
CC="cc -std=c11 -O2 -Wall -Wno-unused-function -Wno-misleading-indentation"
$CC -I viewer tests/cars/params_host.c viewer/car.c viewer/col.c -lm -o $G/params_host || exit 1
mkdir -p $G/stale; cp viewer/car.c viewer/car.h viewer/col.h $G/stale/; plant $PY tests/cars/car_check.py --plant-con viewer/carparams.h $G/stale/carparams.h
$CC -I $G/stale -I viewer $G/stale/car.c tests/cars/params_host.c viewer/col.c -lm -o $G/params_host_stale || exit 1
V=viewer; . tools/bots/lib.sh
$CC -DSKIN_HOST -I viewer -I tools/bots tests/cars/drive_host.c $BOTS_LIB -lm -o $G/drive_host || exit 1
HOMER=build/homer.shr
tools/fresh_homer.sh || exit $?   # 04.11 (gates): свежесть build/homer.shr (штамп входов; устарел — пересборка, нельзя — код 77 НЕ СВИДЕТЕЛЬ, не FAIL)

chk() {   # chk HOST LEVEL SHR — параметры движка и сверка с диском
  "$1" "$3" > "$3.params.txt" || return 1
  $PY tests/cars/car_check.py --level "$2" "$3" "$3.params.txt"
}

# 1 машина уровня: красные
p1=$(lvl 1); p2=$(lvl 2)
if [ -n "$p1" ] && [ -n "$p2" ]; then
  cp "$p1" $G/l1_as_l2.shr; R name_l1_as_l2 chk $G/params_host 2 $G/l1_as_l2.shr
  plant $PY tests/cars/car_check.py --plant-name "$p2" $G/name_l2.shr famil_v; R name_planted_l2 chk $G/params_host 2 $G/name_l2.shr
  R con_stale_l2 chk $G/params_host_stale 2 "$p2"
  plant $PY tests/cars/car_check.py --plant-seat "$p2" $G/seat_l2.shr; R seat_outside_l2 chk $G/params_host 2 $G/seat_l2.shr
  plant $PY tests/cars/car_check.py --plant-door "$p2" $G/door_l2.shr; R door_inside_l2 chk $G/params_host 2 $G/door_l2.shr
fi
# 1 машина уровня: зелёные
for L in $LEVELS; do p=$(lvl $L); [ -n "$p" ] || { echo "нет level.shr уровня $L"; continue; }; Gn "car_l$L" chk $G/params_host $L "$p"; done

# 2 уровень 1 как до 29.09
if [ -n "$p1" ]; then
  $G/params_host --defaults > $G/defaults.txt
  $G/params_host "$p1" > $G/l1.params.txt
  $PY - "$p1" $G/roof_l1.shr <<'EOF'
import struct, sys
d = bytearray(open(sys.argv[1], 'rb').read()); c1, = struct.unpack_from('<I', d, 100); io, = struct.unpack_from('<I', d, c1 + 24)
o = c1 + io + 16 + 8 * 4; v, = struct.unpack_from('<f', d, o); struct.pack_into('<f', d, o, v + 0.01)   # bodyHi — 9-е число кузова
open(sys.argv[2], 'wb').write(bytes(d))
EOF
  $G/params_host $G/roof_l1.shr > $G/roof_l1.params.txt
  R l1_roof $PY tests/cars/car_check.py --identity $G/roof_l1.params.txt $G/defaults.txt
  Gn l1_identity $PY tests/cars/car_check.py --identity $G/l1.params.txt $G/defaults.txt
fi

# 2б уровень 1 — та же игра: ввод «водителя» 90 с (газ, руль, ручник, выход △, пешком, посадка △, RESET CAR) — код до 29.09
#    (CARS_BASE, по умолчанию 12f4ee7) с прежним level.shr и новый код с новым: одинаковые игрок, скорость, монеты, предметы
#    (отпечаток мира не сравниваем: в структурах машины и Гомера новые поля). Красный: новый код, крыша famil_v +1 см
pold=build/levels/l1/level_pre_car1v2.shr; [ -f "$pold" ] || pold=build/level_pre_car1v2.shr
if [ -n "$p1" ] && [ -f "$pold" ]; then
  B=$G/base; mkdir -p $B; git archive "${CARS_BASE:-12f4ee7}" viewer tools/bots | tar -x -C $B || { echo "FAIL: нет исходников ${CARS_BASE:-12f4ee7}"; exit 1; }
  (cd $B && OUT_DIR="$ROOT/$G/base_bin" BINS=game_headless bash tools/bots/build.sh > /dev/null) || exit 1
  OUT_DIR=$G/new_bin BINS=game_headless bash tools/bots/build.sh > /dev/null || exit 1
  $G/new_bin/game_headless "$p1" $HOMER --gen 7 90 > $G/drive_input.txt
  same() { "$1" "$2" $HOMER --input $G/drive_input.txt 2>/dev/null | sed 's/отпечаток [0-9a-f]*//' > "$3"; }
  same $G/base_bin/game_headless "$pold" $G/l1_before.txt
  cmp_run() { same $G/new_bin/game_headless "$1" "$2"
    if cmp -s $G/l1_before.txt "$2"; then echo "уровень 1: игра та же — $(grep -m1 '^игрок' "$2")"
    else echo "  ПРОВАЛ: игра разошлась"; diff $G/l1_before.txt "$2" | head -6; echo FAIL; return 1; fi; }
  R l1_same_roof cmp_run $G/roof_l1.shr $G/l1_roof_after.txt
  Gn l1_same cmp_run "$p1" $G/l1_after.txt
fi

# 3 езда, выход и посадка
if [ -n "$p1" ] && [ -n "$p2" ]; then
  plant $PY tests/cars/car_check.py --plant-name "$p1" $G/honor_l1.shr honor_v; R drive_wrong_car $G/drive_host $G/honor_l1.shr $HOMER famil_v
  R drive_door_inside $G/drive_host $G/door_l2.shr $HOMER "$(car_of 2)"
fi
for L in $LEVELS; do p=$(lvl $L); [ -n "$p" ] || continue; Gn "drive_l$L" $G/drive_host "$p" $HOMER "$(car_of $L)"; done

echo "красных поймано $red из $nred, зелёных $green из $ngreen"
[ $red = $nred ] && [ $green = $ngreen ] && echo "PASS" || { echo "FAIL"; exit 1; }
