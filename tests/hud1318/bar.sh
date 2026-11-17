#!/bin/bash
# 1318 (Роб: шкалы «красная — разбить, жёлтая — угнать, зелёная — прогресс»: что-то не так). Оригинал (исходник HUD + запись PS2 L1 m3/m5): шкала близости стоит ПОЛНОЙ, пока игрок рядом с целью, и пустеет с удалением
# (значение = 1 − расстояние/предел), полоса красная у «оторвись» (losetail), зелёная у «не отставай» (follow). У нас было: заполнение = расстояние/предел (наоборот) и серая полоса.
# Стенд: «flag uiproxdemo1318 N» (N<100 — «не отставай», 100..199 — «оторвись», расстояние N mod 100 % от предела: 20 → рядом).
# Зелёный (новое): при расстоянии 20 % полоса заполнена ≈ 80 %, красная у «оторвись» и зелёная у «не отставай». Красный (flag proxbar1318old 1): заполнена ≈ 20 %, цвета нет.   tests/hud1318/bar.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; R=${R:-$HOME/Downloads/shar-psp-fin108}; OUT=${OUT:-$PWD/build/hud1318}; mkdir -p $OUT
export SET=${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=2 SHIP_LEVELS="1 2 3 4 5 6 7"
for V in 0 1; do printf 'scene hud\nwait 3\nflag msn\nwait 2\nflag proxbar1318old %s\nflag uiproxdemo1318 120\nwait 2\nshot lose\nflag uiproxdemo1318 20\nwait 2\nshot follow\nend\n' $V > $OUT/bar_$V.scn
  (cd $R && FLAGS="" OUT=$OUT nice -n 20 tools/emu_run.sh $OUT/bar_$V.scn b$V "$EB" 200 > $OUT/b$V.log 2>&1; touch $OUT/done_b$V) & done; wait
OUT=$OUT python3 - <<'PY'
import os
from PIL import Image
def meas(v, n):
    im = Image.open(f"{os.environ['OUT']}/out_b{v}/{n}.png").convert('RGB'); red = grn = fill = tot = 0
    for y in (35, 36, 37):
        for x in range(84, 147):
            r, g, b = im.getpixel((x, y)); tot += 1
            if max(r, g, b) > 70: fill += 1
            if r > g + 40 and r > b + 40: red += 1
            if g > r + 40 and g > b + 40: grn += 1
    return fill / tot, red / tot, grn / tot
bad = 0
for v, want in ((0, 1), (1, 0)):
    for n in ('lose', 'follow'):
        f, r, g = meas(v, n)
        print('%s %s: заполнено %.0f %%, красных %.0f %%, зелёных %.0f %%' % ('новое' if v == 0 else 'старое', n, f * 100, r * 100, g * 100))
        if want:
            if not (0.65 <= f <= 0.95): print('FAIL заполнение'); bad = 1
            if n == 'lose' and (r < 0.5 or g > 0.05): print('FAIL цвет lose'); bad = 1
            if n == 'follow' and (g < 0.5 or r > 0.05): print('FAIL цвет follow'); bad = 1
        else:
            if f > 0.4 or r > 0.05 or g > 0.05: print('КРАСНЫЙ НЕ ПОЙМАН'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
