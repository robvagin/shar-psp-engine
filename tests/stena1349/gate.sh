#!/bin/bash
# 1349: Restart из паузы миссии (L1, этап 302): чёрный экран (FADE1316 black → clear) не дольше 1 с настенного времени, WATCHDOG fade-stuck нет. Красное (flag fadedir1349old 1 и fadeclk1362bold 1: после 1362Б одного fadedir1349old мало, 1362Б убирает ту же причину чёрного): ~5 с и fade-stuck — гейт обязан упасть.
#   tests/stena1349/gate.sh ПАКЕТ
cd "$(dirname "$0")/../.."; P=${1:?пакет}
run() { SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=200 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
dur() { python3 - "$1" <<'PY'
import sys,re
b=None; d=0.0
for l in open(sys.argv[1]+'/smoke.txt',errors='replace'):
    m=re.match(r'FADE1316 (black|clear) wall=([0-9.]+)',l)
    if m:
        w=float(m.group(2))
        if m.group(1)=='black': b=w
        elif b is not None: d=max(d,w-b); b=None
print('%.1f'%d)
PY
}
N=$(run tests/stena1349/restart.scn); R=$(run tests/stena1349/restart_red.scn)
nd=$(dur $N); rd=$(dur $R); nw=$(grep -a -c '^WATCHDOG fade-stuck' $N/smoke.txt); rw=$(grep -a -c '^WATCHDOG fade-stuck' $R/smoke.txt)
echo "новое: чёрный $nd с, fade-stuck $nw ($N); красное: чёрный $rd с, fade-stuck $rw ($R)"
ok=$(awk -v d="$nd" -v w="$nw" 'BEGIN{print (d<=1.0 && w==0) ? 1 : 0}'); red=$(awk -v d="$rd" -v w="$rw" 'BEGIN{print (d>=3.0 && w>=1) ? 1 : 0}')
[ $ok = 1 ] && [ $red = 1 ] && { echo PASS; exit 0; }; [ $red = 0 ] && echo "КРАСНЫЙ НЕ ПОЙМАН"; echo FAIL; exit 1
