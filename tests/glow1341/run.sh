#!/bin/bash
# 1341 (Роб, 108 на PSP: «светяшка на каждом входе в дом и в помещения классная по краям, но в центре слишком яркая; в оригинале видно только края, края у нас идентичные»). Свечение двери (viewer/glow1261.h, g61Burst):
# заливка шла от границы ядра (r .30) с альфой .20 до .68 к r .78, середина густо-лиловая; у PS2 (build/ps2_glow_1261, дом Симпсонов) середина почти прозрачна, видна дверь. Теперь ядро r .52 с альфой 0, от r .78 наружу как было.
# Стенд: L1, дом Симпсонов с 13 м (место и размер кадра как tests/glow1261/scn_phase.txt), максимум пульса. Кадры: new, old (flag glow1341old 1), off (flag glow1261old 1 — без свечения, фон).
# Критерий по КАРТИНКЕ (разность с кадром без свечения): средняя заметность в середине (эллипс r<.35 от центра ореола) относительно края (p95 заметности старого кадра): новое ≤ 30 %, старое ≥ 45 %;
# края (r .8–1.1) нового и старого одинаковы в пределах 12 %.   tests/glow1341/run.sh EBOOT      (нужен python с PIL/numpy)
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; R=${R:-$HOME/Downloads/shar-psp-fin108}; OUT=${OUT:-$PWD/build/glow1341g}; PY=${PY:-python3}; rm -rf "$OUT"; mkdir -p "$OUT"
export SET=${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=2
printf '%s\n' "scene F_L1" "flag cfgdefaults 1" "flag repair" "wait 6" "scene F_GLOW" "flag camdet 1" "flag stop" "goto 232.40 170.70 -0.268" "wait 4" "flag glowphase 0" "wait 0.3" "shot new" "flag glow1341old 1" "wait 0.3" "shot old" "flag glow1341old 0" "flag glow1261old 1" "wait 0.3" "shot off" "end" > "$OUT/s.scn"
(cd "$R" && OUT="$OUT" FLAGS="" nice -n 20 tools/emu_run.sh "$OUT/s.scn" g "$EB" 150 > "$OUT/g.log" 2>&1)
OUT="$OUT" $PY - <<'PY'
import os
from PIL import Image
import numpy as np
O = os.environ['OUT'] + '/out_g/'
def L(n): return np.asarray(Image.open(O + n + '.bmp').convert('RGB')).astype(int)[:, :480]
try: new, old, off = L('new'), L('old'), L('off')
except OSError as e: print('FAIL: нет снимков', e); raise SystemExit(1)
Y, X = np.mgrid[0:272, 0:480]; cx, cy, hx, hy = 237, 86, 48, 35; rn = np.sqrt(((X - cx) / hx) ** 2 + ((Y - cy) / hy) ** 2); win = (X >= 150) & (X < 360) & (Y >= 30) & (Y < 112)
dold = np.abs(old - off).max(2).astype(float); dnew = np.abs(new - off).max(2).astype(float)
if (dold > 15).sum() < 800: print('FAIL: свечения в кадре нет (%d px)' % (dold > 15).sum()); raise SystemExit(1)
edge = np.percentile(dold[win & (dold > 15)], 95); c = win & (rn < .35); rim = win & (rn >= .8) & (rn < 1.1)
rc_new, rc_old = dnew[c].mean() / edge, dold[c].mean() / edge; rim_new, rim_old = dnew[rim].mean(), dold[rim].mean(); dr = abs(rim_new - rim_old) / max(rim_old, 1e-6)
print('середина / край: новое %.2f, старое %.2f; края (r .8–1.1): новое %.1f, старое %.1f (расхождение %.0f %%)' % (rc_new, rc_old, rim_new, rim_old, 100 * dr))
bad = 0
if rc_new > .30: print('FAIL зелёный: середина новой ярче 30 %'); bad = 1
if rc_old < .45: print('КРАСНЫЙ НЕ ПОЙМАН: у старого середина < 45 %'); bad = 1
if dr > .12: print('FAIL: края изменились'); bad = 1
a, b = Image.open(O + 'old.bmp').convert('RGB').crop((170, 35, 310, 125)), Image.open(O + 'new.bmp').convert('RGB').crop((170, 35, 310, 125))
s = Image.new('RGB', (140 * 2 * 3 + 10, 90 * 3), (40, 0, 0)); s.paste(a.resize((420, 270), Image.NEAREST), (0, 0)); s.paste(b.resize((420, 270), Image.NEAREST), (430, 0)); s.save(O + 'glow1341_before_after.png')
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
