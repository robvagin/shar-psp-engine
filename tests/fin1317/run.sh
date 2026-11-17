#!/bin/bash
# 1317 (Роб, L2 m4 гонка: «финишная лента не перпендикулярна дороге, а вдоль полосы; поставить поперёк, как в оригинале»). Лента finish_line (fline.p3d, AddCollectible гонки) стоит на локаторе с его поворотом
# (у m4_finish — матрица локатора: 90°), мы рисовали без поворота. Данные: tools/missions/compile.py теперь кладёт поворот локатора по матрице (MsnLoc.flags бит1), метка рисуется msnartDrawRot.
# Стенд: L2, миссия m4 (flag msnmainat 306), «flag uifinmark1317 1» ставит метку на m4_finish; камера у ленты по оси дороги (x), снимки r2/r3. Критерий по КАРТИНКЕ: в кадрах r2+r3 площадь шашечной ленты поперёк дороги.
# Зелёный: новое — лента видна поперёк (≥ 40 пар чёрно-белых пикселей строки); красный (flag finline1317old 1): лента ребром, ≤ 12.   tests/fin1317/run.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; R=${R:-$HOME/Downloads/shar-psp-fin108}; OUT=${OUT:-$PWD/build/fin1317g}; mkdir -p $OUT/ovl/l2 $OUT/msn
python3 tools/missions/compile.py --level 2 --out $OUT/msn -q > $OUT/compile.log 2>&1 || { echo "FAIL: не собралась миссия L2"; exit 1; }
cp $OUT/msn/level02.msn $OUT/ovl/l2/level02.msn
export SET=${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=2 SHIP_LEVELS="1 2 3 4 5 6 7"
for V in 0 1; do
  { echo "flag menu"; echo "flag lvset 2"; echo "scene lv_boot"; echo "wait 1.5"; echo "press cross"; echo "waitfor LEVEL ev=load n=2 300"; echo "wait 10"; echo "flag player 1"; echo "flag msn"; echo "wait 6"; echo "flag msnmainat 306"; echo "wait 14"; echo "press cross 0.3"; echo "wait 16"; echo "flag finline1317old $V"
    echo "foot 64.0 -339.8 1.5708 28.8"; echo "wait 10"; echo "flag uifinmark1317 1"; echo "wait 2"; echo "shot r2"
    echo "foot 102.0 -339.8 -1.5708 28.8"; echo "wait 10"; echo "flag uifinmark1317 1"; echo "wait 2"; echo "shot r3"; echo "end"; } > $OUT/s_$V.scn
  (cd $R && OVL=$OUT/ovl FLAGS="" OUT=$OUT nice -n 20 tools/emu_run.sh $OUT/s_$V.scn s$V "$EB" 600 > $OUT/s$V.log 2>&1; touch $OUT/done_s$V) & done; wait
OUT=$OUT python3 - <<'PY'
import os
from PIL import Image
def score(v, n):
    im = Image.open(f"{os.environ['OUT']}/out_s{v}/{n}.png").convert('RGB'); w, h = im.size; best = 0
    for y in range(50, 130):
        cnt = 0
        for x in range(90, w - 6):
            r, g, b = im.getpixel((x, y))
            if abs(r - g) < 30 and abs(g - b) < 30 and (r < 60 or r > 190):
                r2, g2, b2 = im.getpixel((x + 4, y))
                if abs(r2 - g2) < 30 and abs(g2 - b2) < 30 and abs(r2 - r) > 110: cnt += 1
        best = max(best, cnt)
    return best
bad = 0
for v, want in ((0, 1), (1, 0)):
    s = score(v, 'r2') + score(v, 'r3'); print('%s: шашечная лента поперёк дороги, пар пикселей %d' % ('новое' if v == 0 else 'старое', s))
    if want and s < 40: print('FAIL зелёный'); bad = 1
    if not want and s > 12: print('КРАСНЫЙ НЕ ПОЙМАН'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
