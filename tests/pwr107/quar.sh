#!/bin/bash
# 107б P0 (замечание дирижёра): брошенный буфер чтения msnart (сон, тайм-аут) не копится от сна к сну. Эмулятор L1: «flag msnartstall107 1» держит чтение msnart в полёте, «flag simsleep 1» ×20 подряд (раз в 2 с), каждый сон
# находит чтение в полёте и бросает его буфер. Зелёный: leak = 0 и held = 0 в конце (карантин освободился через 10 с), куча в конце не меньше, чем после первого сна, минус 64 КБ. Красный (flag quar107old 1): leak > 0, куча падает.
#   tests/pwr107/quar.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/pwr107q; mkdir -p $OUT
run() { local N=$1 O=$2; { echo "scene SW_1_0"; echo "flag quar107old $O"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag msnartstall107 1"; echo "flag msnmainat 500"; echo "wait 1"
    for i in $(seq 1 20); do echo "flag simsleep 1"; echo "wait 2"; done; echo "flag msnartstall107 0"; echo "wait 14"; echo "flag quarlog107"; echo "wait 0.5"; echo "end"; } > $OUT/$N.scn
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $OUT/$N.scn > $OUT/$N.log 2>&1; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/; echo "DONE $N"; }
run quar_new 0; run quar_old 1
python3 - <<'PY'
import re
def info(n):
    L = open(f'build/pwr107q/{n}/smoke.txt', errors='replace').read().splitlines(); sleeps = 0; heapAfter1 = None; heapEnd = None; leak = held = 0; pw = 0
    for l in L:
        if l.startswith('PWR sim sleep'):
            sleeps += 1
        if l.startswith('BBOP art-pwr'): pw += 1
        m = re.match(r'QUAR107 leak=(\d+) held=(\d+)', l)
        if m: leak, held = int(m.group(1)), int(m.group(2))
        m = re.match(r'BB .* heap +([\d.]+) ', l)
        if m:
            h = float(m.group(1)); heapEnd = h
            if sleeps >= 1 and heapAfter1 is None: heapAfter1 = h
    return sleeps, pw, leak, held, heapAfter1, heapEnd
bad = 0
for n, want in (('quar_new', 0), ('quar_old', 1)):
    s, pw, leak, held, h1, he = info(n); print('%s: снов %d, art-pwr %d, leak %d Б, held %d Б, куча после 1-го сна %s МБ, в конце %s МБ' % (n, s, pw, leak, held, h1, he))
    if s < 20 or pw < 20: print('FAIL: снов меньше 20'); bad = 1
    if want == 0:
        if leak != 0 or held != 0: print('FAIL зелёный: утечка или карантин не пуст'); bad = 1
        if h1 is None or he is None or he < h1 - 0.0625: print('FAIL зелёный: куча упала больше 64 КБ'); bad = 1
    else:
        if leak <= 0: print('КРАСНЫЙ НЕ ПОЙМАН: утечки нет'); bad = 1
        else: print('красный quar107old 1: утечка %d Б, как должна' % leak)
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
