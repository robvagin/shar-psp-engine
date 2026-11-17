#!/bin/bash
# Restart из паузы миссии L1 (этап 302) ПО КАРТИНКЕ (замена хрупкого гейта 1349 по журналу): снимки каждые 0,5 с после выбора Restart. Новое: через 2 с после выбора средняя яркость кадра > 40 (чёрного нет);
# красное (flag fadedir1349old 1 и fadeclk1362bold 1: прежний чёрный ~5 с; одного fadedir1349old мало — 1362Б убирает ту же причину): кадры c1..c7 чёрные (< 5).   tests/stena1362b/restart_gate.sh ПАКЕТ_L1
set -u; cd "$(dirname "$0")/../.."; P=${1:?пакет}
run() { SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=200 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
lum() { python3 - "$1" <<'PY'
import sys
from PIL import Image, ImageStat
d=sys.argv[1]; v=[ImageStat.Stat(Image.open(f'{d}/{n}.png').convert('L')).mean[0] for n in ('c1','c2','c3','c4','c5','c6','c7')]
print('%.1f %.1f' % (min(v), max(v)))
PY
}
N=$(run tests/stena1362b/restart_pic.scn); R=$(run tests/stena1362b/restart_pic_red.scn)
read nmin nmax <<< "$(lum $N)"; read rmin rmax <<< "$(lum $R)"
echo "новое: яркость c1..c7 от $nmin до $nmax ($N); красное: от $rmin до $rmax ($R)"
ok=$(awk -v a="$nmin" 'BEGIN{print (a>40)?1:0}'); red=$(awk -v b="$rmax" 'BEGIN{print (b<5)?1:0}')
[ $ok = 1 ] && [ $red = 1 ] && { echo PASS; exit 0; }; [ $red = 0 ] && echo "КРАСНЫЙ НЕ ПОЙМАН"; echo FAIL; exit 1
