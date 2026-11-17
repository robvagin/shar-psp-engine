#!/bin/bash
# 1359 (Fable: код ЗА «//» в одной строке): таймер миссии не рисовался (msn_view.c uiTimerCb: uiHudSet/uiHudShow съедены комментарием 1324). Гейт ПО КАРТИНКЕ: L5 m2 «DRIVE TO THE HOSPITAL» (таймер 45 с),
# снимок tm2: в области таймера (x 15..115, y 12..50) жёлтые цифры (R>200, G>170, B<90) — новое их показывает, int108b до правки (EBOOT OLD) — нет.   tests/stena1359/gate.sh PACK_NEW PACK_OLD
set -u; cd "$(dirname "$0")/../.."; PN=$1; PO=$2
run() { SHIP_LEVELS="5 6 7" SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN=tests/stena1359/timer.scn SMOKE_MAXT=200 tools/smoke.sh "$1" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
cnt() { python3 - "$1/tm2.png" <<'PY'
import sys
from PIL import Image
a=Image.open(sys.argv[1]).convert('RGB'); n=0
for y in range(12,50):
    for x in range(15,115):
        r,g,b=a.getpixel((x,y))
        if r>200 and g>170 and b<90: n+=1
print(n)
PY
}
Dn=$(run "$PN"); Do=$(run "$PO"); cn=$(cnt "$Dn"); co=$(cnt "$Do")
echo "  жёлтых пикселей в области таймера: новое $cn, прежнее $co"
[ "$cn" -ge 150 ] && [ "$co" -lt 20 ] && echo PASS || { echo FAIL; exit 1; }
