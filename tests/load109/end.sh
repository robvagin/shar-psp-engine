#!/bin/bash
# 109 (е): записи миссии msnart снимаются на КОНЦЕ миссии (cleanup без повтора), не на старте следующей. Эмулятор, L1 m3: старт с этапа 0, карточка ✕, перезапуск (flag msnretry — art-end НЕ должен быть), затем смена миссии на m4 (flag msnmainat 400: cleanup m3 = её конец).
# Зелёный: перезапуск (flag msnretry) art-end НЕ даёт; при смене миссии art-end n > 0 (записи m3 сняты). Красный (flag loadend109old 1): art-end нет совсем.   tests/load109/end.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/load109; mkdir -p $OUT
run() { local N=$1 O=$2; { echo "scene SW_1_0"; echo "flag camdet 1"; echo "flag loadend109old $O"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag msnmainat 300"; echo "wait 3"; echo "press cross"; echo "wait 4"
    echo "flag msnretry"; echo "wait 6"; echo "flag msnmainat 400"; echo "wait 6"; echo "end"; } > $OUT/$N.scn
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $OUT/$N.scn > $OUT/$N.log 2>&1; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/; echo "DONE $N"; }
run end109_new 0; run end109_old 1
python3 - <<'PY'
import re
def info(n):
    L = open(f'build/load109/{n}/smoke.txt', errors='replace').read().splitlines(); retry = -1; ends = []
    for i, l in enumerate(L):
        if l.startswith('MSN pause-restart'): retry = i
        m = re.match(r'BBOP art-end mission_n=(\d+)', l)
        if m: ends.append((i, int(m.group(1))))
    return retry, ends
bad = 0
for n, want in (('end109_new', 1), ('end109_old', 0)):
    retry, ends = info(n); before = [e for e in ends if retry >= 0 and e[0] < retry and e[1] > 0 and e[0] > retry - 400]; after = [e for e in ends if e[0] > retry and e[1] > 0]
    print('%s: перезапуск на строке %d, art-end %s' % (n, retry, ends))
    if want == 1 and (retry < 0 or not after): print('FAIL зелёный: при смене миссии записи не сняты'); bad = 1
    if want == 1 and any(retry - 300 < e[0] < retry for e in ends if e[1] > 0): print('FAIL зелёный: перезапуск снял записи'); bad = 1
    if want == 0 and ends: print('КРАСНЫЙ НЕ ПОЙМАН'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
