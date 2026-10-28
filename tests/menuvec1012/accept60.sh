#!/bin/bash
# №1012 приёмка (Роб 26.10: «для целостности меню внутреннее перевести в векторный формат, раз мультик по умолчанию; не на всё, а на ховер и важные объекты, чтобы выделение выделялось, аккуратненько»).
# В Cartoon выделенный пункт меню (пауза, OPTIONS, PICTURE, SOUND, CAMERA, GAME, главное меню) — векторная плашка-капсула: чернильный контур, жёлтая жилка, фиолетовая заливка, блик ✦ у левого края (viewer/ui.c menuVecPlate).
# Остальное (другие пункты, рамки, подписи, значки, раскладка, шрифт) не тронуто. Выключатели: MENUVEC1012_OLD=1 / файл menuvec1012old рядом с EBOOT / flag menuvec1012 0.
# Прогоны эмулятора (EBOOT тура с -DZF932_OLD=1): а1 — Cartoon, паузу открывают и листают (кадры tests/menuvec1012/menus_scenario.txt); а2 — то же с файлом menuvec1012old (как TEST71); а3 — то же без Cartoon (flag toon 0);
# а4/а5 — главное меню новый / с файлом menuvec1012old. Гейт (check.py): плашка в каждом кадре Cartoon, её нет в а2 и а3, разница с а2 целиком внутри плашки. Красный контроль: тот же разбор с кадрами а2 вместо новых обязан упасть.
# accept60.sh EBOOT. Последняя строка PASS / FAIL
D=$(cd "$(dirname "$0")" && pwd); I=$(cd "$D/../.." && pwd); EB=${1:-}; export OUT=${OUT:-$I/build/m1012a}; export EMU_OWNER=${EMU_OWNER:-Управление}; export EMU_MINE=${EMU_MINE:-2}
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH; export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test71tour/PSP/GAME/SHARTEST15}
[ -f "$EB" ] || { echo "нет EBOOT $EB"; echo FAIL; exit 1; }
mkdir -p $OUT; rm -rf $OUT/out_a[1-5] $OUT/rr_a[1-5]; R=$I/tools/emu_run.sh
bash $R $D/menus_scenario.txt a1 "$EB" 300 >/dev/null 2>&1 & FLAGS=menuvec1012old bash $R $D/menus_scenario.txt a2 "$EB" 300 >/dev/null 2>&1 & wait
bash $R $D/menus_toon0_scenario.txt a3 "$EB" 300 >/dev/null 2>&1 & bash $R $D/main_scenario.txt a4 "$EB" 120 >/dev/null 2>&1 & wait
FLAGS=menuvec1012old bash $R $D/main_scenario.txt a5 "$EB" 120 >/dev/null 2>&1
rc=0
python3 -I $D/check.py $OUT/out_a1 $OUT/out_a2 $OUT/out_a3 $OUT/out_a4 $OUT/out_a5 || rc=1
echo "  красный контроль: разбор с кадрами «как TEST71» вместо новых должен упасть"
if python3 -I $D/check.py $OUT/out_a2 $OUT/out_a2 $OUT/out_a3 $OUT/out_a5 $OUT/out_a5 >/dev/null 2>&1; then echo "  КРАСНЫЙ НЕ КРАСНЫЙ: гейт зелёный на старом поведении"; rc=1; else echo "  ok   красный пойман"; fi
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
