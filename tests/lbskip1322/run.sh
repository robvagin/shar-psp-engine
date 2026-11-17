#!/bin/bash
# 1322 (Роб, 15.11: «Continue ты выровнял, а Skip в сценах разговора нет; Skip с кнопкой по высоте выровняй»). На экране разговора (LETTERBOX, LetterBoxButtons.pag) значок ✕ у SKIP стоял на 3 px выше
# центра подписи (данные: значок y=237 h=17, подпись y=243 h=10). Значок опущен (uiLbSkip1322Dy=3), LBSKIP1322_OLD=1 / flag lbskip1322old 1 — как было.
# Стенд: L1, разговор Мардж (flag msnmainat -1), служебно «flag uitalkprompt1322 1» ставит подпись SKIP (2 — Start/Cancel для сравнения). Критерий по КАРТИНКЕ: центр значка и центр строки по высоте в нижней полосе (чёрная полоса, ненулевые пиксели).
# Зелёный: новое — |центр значка − центр подписи| ≤ 1 px у SKIP; красный (flag lbskip1322old 1): расхождение ≥ 2 px.   tests/lbskip1322/run.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; R=${R:-$HOME/Downloads/shar-psp-fin108}; OUT=${OUT:-$PWD/build/lbskip1322g}; mkdir -p $OUT
export SET=${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=2 SHIP_LEVELS="1 2 3 4 5 6 7"
for V in 0 1; do
  { echo "flag menu"; echo "flag lvset 1"; echo "scene talk_boot"; echo "wait 1.5"; echo "press cross"; echo "wait 9"; echo "scene F_LBSKIP1322"; echo "flag lbskip1322old $V"; echo "flag msnmainat -1"; echo "wait 2"
    echo "flag uitalkprompt1322 1"; echo "wait 0.6"; echo "shot skip"; echo "flag uitalkprompt1322 2"; echo "wait 0.6"; echo "shot start"; echo "end"; } > $OUT/s_$V.scn
  (cd $R && OUT=$OUT FLAGS="" nice -n 20 tools/emu_run.sh $OUT/s_$V.scn s$V "$EB" 400 > $OUT/s$V.log 2>&1) & done; wait
OUT=$OUT python3 - <<'PY'
import os
from PIL import Image
def box(im, x0, x1, th, y0=234, y1=262):   # th: порог яркости (значок тёмно-серый с синим крестом, подпись белая)
    ys = [y for y in range(y0, y1) for x in range(x0, x1) if max(im.getpixel((x, y))[:3]) > th]
    return (min(ys), max(ys)) if ys else None
def centres(v, n, icon, text):
    im = Image.open(f"{os.environ['OUT']}/out_s{v}/{n}.png").convert('RGB')
    a, b = box(im, *icon, 8), box(im, *text, 100)
    if not a or not b: return None
    return (a[0] + a[1]) / 2, (b[0] + b[1]) / 2
bad = 0
for v, want in ((0, 1), (1, 0)):
    c = centres(v, 'skip', (214, 231), (232, 304))
    if not c: print('FAIL: нет подписи SKIP в кадре (v=%d)' % v); bad = 1; continue
    d = abs(c[0] - c[1]); ref = centres(v, 'start', (106, 126), (128, 232))
    print('%s: SKIP значок %.1f, подпись %.1f, расхождение %.1f px (для сравнения Start Mission: %s)' % ('новое' if v == 0 else 'старое', c[0], c[1], d, '%.1f/%.1f' % ref if ref else '-'))
    if want and d > 1.0: print('FAIL зелёный'); bad = 1
    if not want and d < 2.0: print('КРАСНЫЙ НЕ ПОЙМАН'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
