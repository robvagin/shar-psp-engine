#!/bin/bash
# №968 приёмка (Роб 25.10: «всё должно падать как в оригинале, что столбы, что деревья»): падение высокого предмета в порте = замер PS2.
# 1) фонарь L1 (экземпляр 144, настоящие car/col/props на данных набора): время до 85° от первого наклона при 8,4 / 12 / 16,8 / 22,8 м/с без газа — в пределах ±0,6 с от PS2 (tests/fall968/ps2_ref.tsv: 3,52 / 3,56 / 2,08 / 1,28), итог 90° ±8, центр не уходит дальше 12 м
# красный: POLE968OLD=1 (физика коробки №946: при 12 м/с 1,2 с вместо 3,6)
# 2) соседние гейты не хуже: tests/props903 (деревья по стволу, без «задето мимо»), и, если дан EBOOT тура (аргумент 1), tests/poles946 (перепись и игра на L7)
# Использование: [SET=<набор тура>] tests/fall968/accept60.sh [EBOOT тура с -DZF932_OLD=1]; последняя строка PASS/FAIL
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; E=${1:-}; OUT=${OUT:-$W/build/f968}; mkdir -p $OUT; rc=0
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test64tour/PSP/GAME/SHARTEST15}; export SET
cc -std=c11 -O2 -w -DSKIN_HOST -I $W/viewer -o $OUT/fcensus $H/fall_census.c $W/viewer/col.c $W/viewer/props.c $W/viewer/stateprops.c $W/viewer/car.c $W/viewer/coins.c -lm || { echo "fcensus не собрался"; echo FAIL; exit 1; }
chk() {   # $1 — "red" или "": сверка с PS2
  python3 - "$1" <<PY
import re, subprocess, sys, os
red = sys.argv[1] in ('red', 'red1276')
ref = {}
for l in open('$H/ps2_ref.tsv'):
    if l.startswith('#') or not l.strip(): continue
    f = l.split(); ref[float(f[0])] = (float(f[1]), float(f[2]), float(f[3]))
env = dict(os.environ, GAS968='0'); 
if sys.argv[1] == 'red': env['POLE968OLD'] = '1'
if sys.argv[1] == 'red1276': env['POLE1276OLD'] = '1'   # 107б П6: толчок 1046 валит за 0,5 с против 3,5 по записи PS2
out = subprocess.run(['$OUT/fcensus', '$SET/level.shr', 'F', '144', '8.4', '12', '16.8', '22.8'], env=env, capture_output=True, text=True).stdout
ok = True
for l in out.splitlines():
    m = re.match(r'F v (\S+) res (\S+): 10deg \S+ 30deg \S+ 60deg \S+ 85deg (\S+) s \| final (\S+) deg \| centre moved (\S+) m', l)
    if not m: continue
    v = float(m.group(1)); t85 = float(m.group(3)); fin = float(m.group(4)); mv = float(m.group(5)); r = ref[v]
    good = t85 > 0 and abs(t85 - r[0]) <= 0.6 and abs(fin - 90) <= 8 and mv <= 12
    ok &= good
    print(f'  v {v:5.1f} м/с: до 85° порт {t85:4.2f} с, PS2 {r[0]:4.2f} с; итог {fin:.0f}°; центр {mv:.1f} м  ->  ' + ('ок' if good else 'мимо'))
sys.exit(0 if ok != red else 1)
PY
}
echo "== фонарь L1: порт против PS2 (без газа)"; chk "" || rc=1
echo "== красный (POLE968OLD=1, физика коробки №946): должен разойтись с PS2"; chk red || { echo "КРАСНЫЙ КОНТРОЛЬ НЕ КРАСНЫЙ"; rc=1; }
echo "== красный (POLE1276_OLD, толчок бампера 1046): должен разойтись с PS2"; chk red1276 || { echo "КРАСНЫЙ 1276 НЕ КРАСНЫЙ"; rc=1; }
echo "== tests/props903"; bash $W/tests/props903/gate.sh 2>&1 | tail -2 | sed 's/^/  /'; [ "$(bash $W/tests/props903/gate.sh 2>&1 | tail -1)" = PASS ] || rc=1
if [ -n "$E" ]; then echo "== tests/poles946"; OUT=$OUT/p946 bash $W/tests/poles946/accept60.sh "$E" 2>&1 | tail -9 | sed 's/^/  /'; [ "$(OUT=$OUT/p946 bash $W/tests/poles946/accept60.sh "$E" 2>&1 | tail -1)" = PASS ] || rc=1; fi
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
