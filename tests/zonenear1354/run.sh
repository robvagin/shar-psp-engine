#!/bin/bash
# 1354 (Роб, набор 73015bd8: «L4 F10-4, лечу с трамплина, текстура листвы, которая должна быть вдалеке, прямо здесь, я лечу в ней»).
# Причина: правило «регион рядом» (1319/1319б, main_rooms.h zoneGatesFrame) брало регион «декораций издали» l4r1 (холм, дома, стена деревьев соседнего района,
# как l1r1 в 95б), которого оригинал с этой земли не держит; в полёте с трамплина jumpzone5 (L4 F10-8, летит на юг через F10-4) клетки l4r1 оказывались вокруг камеры: экран залит зелёным.
# Теперь «рядом» берёт только регионы маски REG1 клетки игрока. Выключатель: ZONENEARMASK1354_OLD / файл zonenearmask1354old / flag zonenearmask1354 0.
# Стенд (эмулятор без окна и звука): один и тот же заезд трамплина, сцены N (новое) и R (flag zonenearmask1354 0, красный контроль), по 10 снимков через 0,2 с.
# Заезды (газ ✕ с места, трамплин должен подбросить машину: размах высоты >= 5 м, иначе FAIL «не взлетел»): L4 и L1 jumpzone5 (старт -171 306, курс 3.20; то самое место Роба и «опять, как на L1»); L4 и L1 jumpzone4
# (старт -120 490, на юг, другой трамплин; правка там ничего не меняет). L3 и L7 в стенде не взяты: их трамплины сценарием не достижимы (L7: goto за пределы карты L7 игнорируется; L3: машина на jumpzone2 не взлетает). Метрика по КАРТИНКЕ: доля тёмно-зелёных пикселей
# в середине кадра (x 80..400, y 50..230; без шкалы и значков HUD) на самом залитом снимке.
# Критерий: l4 и l1 (jumpzone5): новое < 0.6, красный >= 0.85 / 0.7 (иначе «КРАСНЫЙ НЕ ПОЙМАН»); l4b и l1b (jumpzone4): новое < 0.6 и новое/красный отличаются не больше чем на 0.05.
#   tests/zonenear1354/run.sh EBOOT        (нужен python с PIL/numpy; SET — набор данных с уровнями l3, l4, l7)
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; OUT=${OUT:-$PWD/build/zonenear1354g}; PY=${PY:-python3}; rm -rf "$OUT"; mkdir -p "$OUT"
export SET=${SET:?SET=набор данных (папка с EBOOT.PBP, l4, l7, l3)} SMOKE_HEADLESS=1 EMU_MINE=${EMU_MINE:-2}
mk() {   # имя уровень "x z h"
  $PY - "$1" "$2" "$3" "$OUT/$1.scn" <<'PY'
import sys
n, lv, pos, out = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
L = ["scene C_BOOT", "flag cfgdefaults 1", "wait 6"]
if lv != 1: L += ["scene LOAD", f"flag golevel {lv}", "wait 45"]
L += ["flag stop", "flag player", "wait 3", "flag repair", "wait 4"]
for s, fl in (('N', 1), ('R', 0), ('N2', 1)):
    L += [f"scene F_{s}", f"flag zonenearmask1354 {fl}", "flag stop", f"goto {pos}", "wait 3", "press cross 14", "wait 1.5"]
    for i in range(10): L += [f"shot {s}{i:02d}", "wait 0.2"]
    L += ["press cross 0.01", "wait 1"]
L += ["flag stop", "end"]
open(out, "w").write("\n".join(L) + "\n")
PY
}
mk l4 4 "-171 306 3.20"; mk l1 1 "-171 306 3.20"; mk l4b 4 "-120 490 3.1416"; mk l1b 1 "-120 490 3.1416"
pids=""; for n in l4 l1 l4b l1b; do (OUT="$OUT" FLAGS="" nice -n 20 tools/emu_run.sh "$OUT/$n.scn" $n "$EB" 600 > "$OUT/$n.log" 2>&1) & pids="$pids $!"; done; wait $pids
OUT="$OUT" $PY - <<'PY'
import os
from PIL import Image
import numpy as np
O = os.environ['OUT']; bad = 0
def green(p):
    a = np.asarray(Image.open(p).convert('RGB')).astype(int)[50:230, 80:400]; r, g, b = a[..., 0], a[..., 1], a[..., 2]
    return float(((g > r + 20) & (g > b + 20) & (g < 150)).mean())
for n in ('l4', 'l1', 'l4b', 'l1b'):
    d = f'{O}/out_{n}/'
    try: v = {s: max(green(f'{d}{s}{i:02d}.png') for i in range(10)) for s in ('N', 'R', 'N2')}
    except OSError as e: print(n, 'FAIL: нет снимков', e); bad = 1; continue
    ys = [float(l.split()[5]) for l in open(d + 'smoke.txt') if l.startswith('SHOT N') and not l.startswith('SHOT N2')]
    if not ys or max(ys) - min(ys) < 5: print(n, 'FAIL: заезд не взлетел (размах высоты %.1f м)' % ((max(ys) - min(ys)) if ys else -1)); bad = 1
    print(f'{n}: самый зелёный снимок: новое {v["N"]:.2f} / {v["N2"]:.2f}, красный (flag zonenearmask1354 0) {v["R"]:.2f}')
    if n in ('l4', 'l1'):
        if max(v['N'], v['N2']) >= .6: print('FAIL зелёный: с правкой кадр залит листвой'); bad = 1
        if v['R'] < (.85 if n == 'l4' else .7): print('КРАСНЫЙ НЕ ПОЙМАН: без правки кадр не залит'); bad = 1
    else:
        if max(v['N'], v['N2']) >= .6 or abs(v['N'] - v['R']) > .05: print('FAIL: на этом трамплине правка что-то изменила или кадр залит'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
