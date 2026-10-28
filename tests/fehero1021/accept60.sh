#!/bin/bash
# №1021-герои приёмка (Роб 26.10: «Гомера с обводкой как в игре, проходящим персонажам просто поставить этот скин — наш пресет»; «подсветка поверх контура или под ним, но крупнее»): главное меню в Cartoon — Гомер и герои сценок рисуются путём игры (skinDrawPal: ровный цвет, контур-оболочка),
# подсветка дивана под Гомером крупнее на 15 %, остальные подсветки после героев. FEHERO1021_OLD=1 / файл fehero1021old / flag fehero1021 0 — как TEST71. accept60.sh EBOOT (тур с -DZF932_OLD=1 -DCARFILL930_OLD=1). Последняя строка PASS / FAIL
D=$(cd "$(dirname "$0")" && pwd); I=$(cd "$D/../.." && pwd); EB=${1:-}; export OUT=${OUT:-$I/build/h1021a}; export EMU_OWNER=${EMU_OWNER:-Управление} EMU_MINE=${EMU_MINE:-2}
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH; export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15}
[ -f "$EB" ] || { echo "нет EBOOT $EB"; echo FAIL; exit 1; }
mkdir -p $OUT; rm -rf $OUT/out_a[123] $OUT/rr_a[123]; R=$I/tools/emu_run.sh
bash $R $D/menu_scenario.txt a1 "$EB" 200 >/dev/null 2>&1 & bash $R $D/glowall_scenario.txt a2 "$EB" 120 >/dev/null 2>&1 & wait
bash $R $D/glowall_old_scenario.txt a3 "$EB" 120 >/dev/null 2>&1
rc=0; python3 -W ignore -I $D/check.py $OUT/out_a1 $OUT/out_a2 $OUT/out_a3 || rc=1
echo "  красный контроль: разбор, где новые кадры подменены прежними, должен упасть"
if python3 -W ignore -I $D/check.py $OUT/out_a1 $OUT/out_a3 $OUT/out_a3 >/dev/null 2>&1 && false; then :; fi
# подмена: меню как есть, но 'new' = 'old' — собираем копию папки меню
rm -rf $OUT/red_menu; mkdir -p $OUT/red_menu; for t in a b c; do cp $OUT/out_a1/${t}_old.png $OUT/red_menu/${t}_old.png; cp $OUT/out_a1/${t}_old.png $OUT/red_menu/${t}_new.png; done
if python3 -W ignore -I $D/check.py $OUT/red_menu $OUT/out_a3 $OUT/out_a3 >/dev/null 2>&1; then echo "  КРАСНЫЙ НЕ КРАСНЫЙ"; rc=1; else echo "  ok   красный пойман"; fi
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
