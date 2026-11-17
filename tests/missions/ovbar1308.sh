#!/bin/bash
# 1308 (Роб: на L2 в погоне шкала «не отстать» перекрывает таймер): шкалы близости и урона входят в стопку оверлеев миссии и идут в ней первыми (как у оригинала), таймер ниже.
#   tests/missions/ovbar1308.sh DIR_С_ТУР_EBOOT   -> PASS/FAIL. Эмулятор headless. flag uiovforce1308 N — оверлеи по маске (бит = метка: 6 таймер, 10 урон, 11 близость), flag uiov — рамки видимых оверлеев.
# Зелёный: близость+таймер и урон+таймер — рамки не пересекаются; одиночный таймер стоит там же, где в прежнем порядке. Красный (flag ovbar1308old 1): близость+таймер пересекаются.
cd "$(dirname "$0")/../.."; DIR=${1:?DIR с EBOOT}
SCN=$(mktemp /tmp/ov1308.XXXXXX)
cat > $SCN <<'SC'
scene hud
wait 3
flag msn
wait 2
flag uiovforce1308 2112
wait 2
flag uiov
flag uiovforce1308 1088
wait 2
flag uiov
flag uiovforce1308 64
wait 2
flag uiov
flag ovbar1308old 1
flag uiovforce1308 2112
wait 2
flag uiov
flag uiovforce1308 64
wait 2
flag uiov
end
SC
OUT=$(SMOKE_HEADLESS=1 SMOKE_SCN="$SCN" tools/smoke.sh "$DIR" 2>&1); rm -f $SCN
REP=$(echo "$OUT" | sed -n 's/^отчёт: \(.*\)\/summary.txt.*/\1/p' | tail -1)
[ -f "$REP/smoke.txt" ] || { echo "$OUT" | tail -3; echo "FAIL: нет отчёта"; exit 1; }
python3 -I - "$REP/smoke.txt" <<'PY'
import re,sys
blocks=[]; cur=[]
for l in open(sys.argv[1],errors='replace'):
    m=re.match(r'UIOV tag=(\d+) (-?\d+) (-?\d+) (-?\d+) (-?\d+)',l)
    if m: cur.append(tuple(map(int,m.groups())))
    elif l.startswith('UIOV n='): blocks.append(cur); cur=[]
print('блоков UIOV:',len(blocks))
if len(blocks)<5: print('FAIL: меньше 5 блоков'); sys.exit(1)
def box(b,t): return next((x[1:] for x in b if x[0]==t),None)
def ov(a,b): return a[0]<b[2] and b[0]<a[2] and a[1]<b[3] and b[1]<a[3]
ok=True
A,B,C,D,E=blocks[:5]
for name,b,bar in (('близость+таймер',A,11),('урон+таймер',B,10)):
    bb,tb=box(b,bar),box(b,6)
    print(name,'шкала',bb,'таймер',tb)
    if not bb or not tb: print('FAIL: нет рамки'); ok=False
    elif ov(bb,tb): print('FAIL: НАЛОЖЕНИЕ'); ok=False
tc,te=box(C,6),box(E,6); print('одиночный таймер: новое',tc,'прежнее',te)
if not tc or tc!=te: print('FAIL: одиночный таймер сдвинулся'); ok=False
bd,td=box(D,11),box(D,6); print('красное (ovbar1308old): шкала',bd,'таймер',td)
if not (bd and td and ov(bd,td)): print('КРАСНЫЙ НЕ ПОЙМАН (пересечения нет)'); ok=False
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
PY
