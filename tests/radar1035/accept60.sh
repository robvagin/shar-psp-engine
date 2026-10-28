#!/bin/bash
# №1035 приёмка (Роб, сверка с PS2: радар в помещениях — только рамка-кольцо, внутри прозрачно): дом Симпсонов и Kwik-E-Mart (L1) входом через дверь, затем foot наружу (выход из комнаты); новое и flag radar1035 0 (= TEST72) на одном EBOOT.
# RADAR1035_OLD=1 / файл radar1035old / flag radar1035 0 — как TEST72. accept60.sh EBOOT (тур с -DZF932_OLD=1). Красный контроль: разбор с кадрами «прежнее вместо нового» падает. Последняя строка PASS / FAIL
D=$(cd "$(dirname "$0")" && pwd); I=$(cd "$D/../.." && pwd); EB=${1:-}; export OUT=${OUT:-$I/build/r1035a}; export EMU_OWNER=${EMU_OWNER:-Управление} EMU_MINE=${EMU_MINE:-2}
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH; export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15}
[ -f "$EB" ] || { echo "нет EBOOT $EB"; echo FAIL; exit 1; }
mkdir -p $OUT; rm -rf $OUT/out_a[12] $OUT/rr_a[12]; R=$I/tools/emu_run.sh
bash $R $D/rooms_scenario.txt a1 "$EB" 150 >/dev/null 2>&1 & bash $R $D/rooms_old_scenario.txt a2 "$EB" 150 >/dev/null 2>&1 & wait
rc=0; python3 -W ignore -I $D/check.py $OUT/out_a1 $OUT/out_a2 || rc=1
echo "  красный контроль: разбор с кадрами прежнего вместо нового должен упасть"
if python3 -W ignore -I $D/check.py $OUT/out_a2 $OUT/out_a2 >/dev/null 2>&1; then echo "  КРАСНЫЙ НЕ КРАСНЫЙ"; rc=1; else echo "  ok   красный пойман"; fi
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
