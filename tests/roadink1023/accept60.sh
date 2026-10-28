#!/bin/bash
# №1023 приёмка (Роб 26.10: «контур асфальта тонкий… нужен тёмный насыщенный цвет асфальта, плотный, видный, не один пиксель, без биения»): край асфальта (линии №824) — несколько проходов со сдвигом на пиксель: 3 px у глаза, 2 px до 36 м, 1 px до 46 м; перо — тёмный тон асфальта (№1008) ещё темнее на 40 %.
# ROADTHICK1023_OLD=1 / файл roadthick1023old / flag roadthick1023 0 — как TEST72 (1 px до 28 м). accept60.sh EBOOT (тур с -DZF932_OLD=1). Последняя строка PASS / FAIL
D=$(cd "$(dirname "$0")" && pwd); I=$(cd "$D/../.." && pwd); EB=${1:-}; export OUT=${OUT:-$I/build/r1023a}; export EMU_OWNER=${EMU_OWNER:-Управление} EMU_MINE=${EMU_MINE:-2}
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH; export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15}
[ -f "$EB" ] || { echo "нет EBOOT $EB"; echo FAIL; exit 1; }
mkdir -p $OUT; rm -rf $OUT/out_a[12] $OUT/rr_a[12]; R=$I/tools/emu_run.sh
bash $R $D/view_scenario.txt a1 "$EB" 200 >/dev/null 2>&1 & bash $R $D/view_old_scenario.txt a2 "$EB" 200 >/dev/null 2>&1 & wait
rc=0; python3 -W ignore -I $D/check.py $OUT/out_a1 $OUT/out_a2 || rc=1
echo "  красный контроль: разбор с кадрами прежнего вместо нового должен упасть"
if python3 -W ignore -I $D/check.py $OUT/out_a2 $OUT/out_a2 >/dev/null 2>&1; then echo "  КРАСНЫЙ НЕ КРАСНЫЙ"; rc=1; else echo "  ok   красный пойман"; fi
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
