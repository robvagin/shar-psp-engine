#!/bin/bash
# 1362Б (FADECLK1362B, Fable: корень класса 1304/1316/1326/1349 — часы затемнения под карточкой шли по игровому dt = 0): L1 этап 302, карточка открыта, flag fadeout 1.
# Новое: FADE1316 black за ~1 с настенного времени; красное (flag fadeclk1362bold 1): затемнение не наступает.   tests/stena1362b/gate.sh ПАКЕТ_L1  (один эмулятор за раз)
set -u; cd "$(dirname "$0")/../.."; P=${1:?пакет}
run() { SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=200 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
N=$(run tests/stena1362b/card_fade.scn); R=$(run tests/stena1362b/card_fade_red.scn)
nb=$(grep -a -c '^FADE1316 black' $N/smoke.txt); rb=$(grep -a -c '^FADE1316 black' $R/smoke.txt)
echo "новое: чёрный наступил $nb раз ($N); красное: $rb раз ($R)"
[ "$nb" -ge 1 ] && [ "$rb" = 0 ] && { echo PASS; exit 0; }; [ "$rb" != 0 ] && echo "КРАСНЫЙ НЕ ПОЙМАН"; echo FAIL; exit 1
