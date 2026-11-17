#!/bin/bash
# 109 (а): карточка миссии готова (MSN briefing-loaded), только когда очередь msnart пуста и модели машин фазы в памяти. Стенд: «flag mvcarsdelay109 6» — крючок моделей машин фазы отвечает «готово» через 6 с после открытия карточки (настоящий крючок — Стена).
# Зелёный: в строке «MSN briefing-loaded» wait не меньше 5,8 с. Красный (flag loadready109old 1): меньше 5 с.   tests/load109/ready.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/load109; mkdir -p $OUT
run() { local N=$1 O=$2; { echo "scene SW_1_0"; echo "flag camdet 1"; echo "flag loadready109old $O"; echo "flag mvcarsdelay109 6"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag msnmainat 300"; echo "wait 14"; echo "end"; } > $OUT/$N.scn
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $OUT/$N.scn > $OUT/$N.log 2>&1; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/; echo "DONE $N"; }
run ready109_new 0; run ready109_old 1
python3 - <<'PY'
import re
def wait(n):
    for l in open(f'build/load109/{n}/smoke.txt', errors='replace'):
        m = re.match(r'MSN briefing-loaded .* wait=([\d.]+) ', l)
        if m: return float(m.group(1))
    return None
bad = 0
for n, want in (('ready109_new', 1), ('ready109_old', 0)):
    w = wait(n); print(n, 'карточка ждала готовности', w, 'с')
    if w is None: print('FAIL: нет строки briefing-loaded'); bad = 1; continue
    if want and w < 5.8: print('FAIL зелёный: не дождалась крючка'); bad = 1
    if not want and w >= 5.0: print('КРАСНЫЙ НЕ ПОЙМАН'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
