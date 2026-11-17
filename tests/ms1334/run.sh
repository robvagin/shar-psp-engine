#!/bin/bash
# 1334 (Роб: «в меню паузы в Mission Select галочки жёлтые, а в оригинале зелёные»). Оригинал: MissionSelect.pag, значки состояния строки — картинки dash / cross / check (диск, ingame.p3d), без тона (цвет белый);
# код оригинала: пройдена — index 2 «green check», попытки — 1 «red cross», нет — 0 «yellow dash». Цвет с диска (пипетка по самой картинке check.png): пик 78,244,9, оттенок ≈ 102°.
# У нас значки лежали на общей странице pextra (карточки VIEW CARDS, 255 цветов k-means на всё) и после квантования галочка становилась 123,170,66 (оттенок ≈ 87°, жёлто-салатовая).
# Правка данных: tools/menu_export.py — свои 3 картинки на своей странице (группа status), точная палитра. MSSTATUS1334_OLD=1 при экспорте — как было (красный контроль).
# Стенд: L1, строки 1–2 пройдены (flag uimsndone 6), экран Mission Select в паузе, снимок. Критерий по КАРТИНКЕ: оттенок пикселей галочки в 96–112° и R < 100 (новое); у старого оттенок < 94°.
#   tests/ms1334/run.sh EBOOT      (нужен python с PIL/numpy: PY=.../python; по умолчанию ~/Downloads/shar-psp-dal103/.venv/bin/python)
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; R=${R:-$HOME/Downloads/shar-psp-fin108}; OUT=${OUT:-$PWD/build/ms1334g}; PY=${PY:-$HOME/Downloads/shar-psp-dal103/.venv/bin/python}; mkdir -p $OUT/ovl_0 $OUT/ovl_1
$PY tools/menu_export.py --pack pextra --out $OUT/ovl_0/pextra.shr > $OUT/exp0.log 2>&1 || { echo "FAIL: pextra (новое) не собрался"; exit 1; }
MSSTATUS1334_OLD=1 $PY tools/menu_export.py --pack pextra --out $OUT/ovl_1/pextra.shr > $OUT/exp1.log 2>&1 || { echo "FAIL: pextra (старое) не собрался"; exit 1; }
export SET=${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=2 SHIP_LEVELS="1 2 3 4 5 6 7"
{ echo "scene start"; echo "flag msn"; echo "wait 3"; echo "flag uimsndone 6"; echo "flag msnsel 3"; echo "wait 2"; echo "press start"; echo "wait 0.8"; echo "press down"; echo "wait 0.3"; echo "press cross"; echo "wait 1.2"; echo "shot ms"; echo "end"; } > $OUT/s.scn
for V in 0 1; do (cd $R && OVL=$OUT/ovl_$V FLAGS="" OUT=$OUT nice -n 20 tools/emu_run.sh $OUT/s.scn s$V "$EB" 200 > $OUT/s$V.log 2>&1) & done; wait
OUT=$OUT $PY - <<'PY'
import os, colorsys
from PIL import Image
import numpy as np
O = os.environ['OUT']; bad = 0; hues = {}
for v, want in ((0, 1), (1, 0)):
    p = f'{O}/out_s{v}/ms.bmp'
    if not os.path.exists(p): print('FAIL: нет снимка v=%d' % v); bad = 1; continue
    im = Image.open(p).convert('RGB'); im.save(f'{O}/out_s{v}/ms.png'); a = np.asarray(im).astype(int)
    r = a[90:130, 392:424]; m = r[(r.max(2) > 100) & (r[..., 1] > r[..., 0]) & (r[..., 1] > r[..., 2])]   # яркие зеленоватые пиксели у значков строк 1–2
    if len(m) < 8: print('FAIL: пикселей галочки мало (%d), v=%d' % (len(m), v)); bad = 1; continue
    c = m.mean(0); h = colorsys.rgb_to_hsv(*(c / 255))[0] * 360; hues[v] = h
    print('%s: пикселей галочки %d, средний цвет %d,%d,%d, оттенок %.0f° (диск: 78,244,9, 102°)' % ('новое' if v == 0 else 'старое', len(m), *c.round(), h))
    if want and not (96 <= h <= 112 and c[0] < 100): print('FAIL зелёный'); bad = 1
    if not want and h >= 94: print('КРАСНЫЙ НЕ ПОЙМАН'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
