#!/bin/bash
# 108: значок задания (msnicons) не читается — «art+ …/images/msnic» раз в секунду, «art=» нет (ящик PSP 13.11, L1 m5). Эмулятор, L1, старт m5; «flag msnartiostuck 1» — модель залипшего асинхронного дескриптора msnart.shr.
# Зелёный (по умолчанию): art+ msnic не больше 3 раз, значок встал (ico …/1 или art= по значку), строки art-io в журнале. Красный (flag msnartio108old 1): art+ msnic не меньше 5 раз за 30 с, «art=» по значку нет.
#   tests/hot108/msnic.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/msnic; mkdir -p $OUT
run() { local N=$1 OLD=$2; { echo "scene SW_1_0"; echo "flag camdet 1"; echo "flag startcam 1"; echo "flag msnartio108old $OLD"; echo "flag msnartiostuck 1"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag msnmainat 500"; echo "wait 30"; echo "end"; } > $OUT/$N.scn
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $OUT/$N.scn > $OUT/$N.log 2>&1; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/; echo "DONE $N"; }
run msnic108_new 0; run msnic108_old 1
python3 - <<'PY'
import re
def info(n):
    plus = eq = io = mem = 0; rdy = 0
    for l in open(f'build/msnic/{n}/smoke.txt', errors='replace'):
        if l.startswith('BBOP art+') and 'msnic' in l: plus += 1
        if l.startswith('BBOP art=') and 'msnic' in l: eq += 1
        if l.startswith('BBOP art-io'): io += 1
        if l.startswith('BBOP art-mem'): mem += 1
        m = re.search(r' ico (\S+)/(-?\d+)/(\d+)/L', l)
        if l.startswith('BB ') and m and m.group(1) not in ('-',) and m.group(3) == '1': rdy += 1
    return plus, eq, io, mem, rdy
n = info('msnic108_new'); o = info('msnic108_old'); bad = 0
print('new: art+ msnic %d, art= msnic %d, art-io %d, art-mem %d, строк «значок готов» %d' % n); print('old: art+ msnic %d, art= msnic %d, art-io %d, art-mem %d, строк «значок готов» %d' % o)
if not (n[0] <= 3 and n[1] >= 1): print('FAIL зелёный: значок читается слишком долго или не прочитан'); bad = 1
if not (o[0] >= 5 and o[1] == 0): print('КРАСНЫЙ НЕ ПОЙМАН: прежнее прошло'); bad = 1
else: print('красный msnartio108old 1: упал, как должен')
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
