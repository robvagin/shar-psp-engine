#!/bin/bash
# ПРИЁМКА №1007б (окно «Небо», ветка t72/zones1007): тон площадок и парковок у асфальта по куску (сайдкар zones1007p.bin, множитель цвета вершин), в игре «flag zones1007 0» / файл zones1007old / ZONES1007_OLD=1 — как в TEST71.
#   1 сайдкар  tools/check_sidecars.py --only zones1007: 7 из 7 файлов совпали с выходом генератора (тона — tests/zones1007/finals из дампа движка)
#   2 сборка   EBOOT тура
#   3 движок   accept.txt (парковки L1, L2, L4, L5, L7: виды сверху, сбоку и Гомер на площадке) на данных с сайдкаром и на тех же данных с файлом zones1007old
#   4 проверка check.py: сайдкар подгружен, на видах сверху площадка другого тона, остальной кадр цел; красное: «старый против старого» обязан упасть; лист «TEST71 / стало»
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; H=$ROOT/tests/zones1007
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH
PY=$ROOT/.venv/bin/python; SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15}; DATA=${DATA:-$ROOT/build/z1007/ovl}
A=${ACC_OUT:-$ROOT/build/z1007/accept}; rm -rf "$A"; mkdir -p "$A"; export EMU_OWNER=${EMU_OWNER:-Небо} OUT=$A EMU_MINE=${EMU_MINE:-2} SET OVL=$DATA
FAIL=0; say() { echo "[$(date +%H:%M:%S)] $*"; }
res() { if [ "$1" = 0 ]; then say "  ok   $2"; else say "  ПАДАЕТ $2"; FAIL=1; fi; }
red() { if [ "$1" != 0 ]; then say "  ok   красное: $2"; else say "  ПАДАЕТ красное не упало: $2"; FAIL=1; fi; }
say "1 сайдкар"; $PY tools/check_sidecars.py "$SET" "$DATA" --only zones1007 --out "$A/chk" > "$A/sidecars.txt" 2>&1; tail -2 "$A/sidecars.txt"; res $? "check_sidecars zones1007: 7 из 7"
say "2 сборка"; bash $H/mk.sh new "" > /dev/null; res $? "EBOOT тура"; EN=$ROOT/viewer/build_zn_new/EBOOT.PBP
say "3 движок: с сайдкаром и красный (файл zones1007old), около 7 минут"
( bash tools/emu_run.sh $H/accept.txt new "$EN" 500 > $A/emu_new.log 2>&1 ) & ( FLAGS="zones1007old" bash tools/emu_run.sh $H/accept.txt red "$EN" 500 > $A/emu_red.log 2>&1 ) & wait
for v in new red; do [ -s "$A/out_$v/smoke.txt" ] && grep -q "^END" "$A/out_$v/smoke.txt"; res $? "прогон $v дошёл до END"; done
say "4 проверка"
$PY -I $H/check.py $A/out_new $A/out_red | tee $A/check.txt; [ "$(tail -1 $A/check.txt)" = PASS ]; res $? "сайдкар работает, площадки другого тона"
$PY -I $H/check.py $A/out_red $A/out_red > $A/check_red.txt; [ "$(tail -1 $A/check_red.txt)" = PASS ]; red $? "старый против старого не проходит"
$PY -I $H/sheet.py $A/out_new $A/out_red $A/sheet_TEST71_stalo.png
[ $FAIL = 0 ] && echo PASS || echo FAIL
