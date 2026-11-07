#!/bin/bash
# Правка Роба 1200 (тангаж на склонах и трамплинах): сравнение прежней подвески (CAR_PITCH1200_OLD=1, по умолчанию) и кандидата (CAR_PITCH1200_OLD=0) против PS2.
#   tests/g01cars/pitch1200.sh [level.shr]
# Печатает для каждого варианта: 14 прыжков twin (Σ|Δподъём|, Σ|Δудар|), тангаж и vy на кромке (tests/jumps/pitchcmp.py), jz5 (5 записей PS2 с толчком, tests/jumps/jz5.c) и крышу на 96..112 км/ч.
# Код выхода: 0 — кандидат не вне допуска ни на одном из 14 прыжков twin и не хуже по Σ|Δподъём| больше 10 %; цель (jz5 100 км/ч садится на крышу) печатается отдельной строкой.
# ИТОГ 05.11.2026: кандидат цель НЕ достигает (docs/handoff/ТАНГАЖ_1200_2026-11-05.md), поэтому по умолчанию выключен.
cd "$(dirname "$0")/../.."; mkdir -p build/susp
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}
LV=${1:-${LV:-$HOME/Downloads/shar-psp-integ/build/test91tour/PSP/GAME/SHARTEST15/level.shr}}
HOST="viewer/car.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/coins.c"
rc=0
for k in old new; do
  F=""; [ $k = new ] && F="-DCAR_PITCH1200_OLD=0"
  O=build/susp/p1200_$k; rm -rf $O; mkdir -p $O
  cc -std=c11 -O2 -w -DSKIN_HOST -I viewer $F tests/jumps/twin.c $HOST -lm -o $O/twin || exit 2
  $O/twin "$LV" --trace $O > $O/twin.txt 2>&1; tr=$?
  cc -std=c11 -O2 -w -DSKIN_HOST -I viewer $F tests/jumps/jz5.c $HOST -lm -o $O/jz5 || exit 2
  $O/jz5 "$LV" > $O/jz5.txt 2>&1; $O/jz5 "$LV" --scan > $O/scan.txt 2>&1
  echo "=== $([ $k = old ] && echo 'прежняя подвеска (CAR_PITCH1200_OLD=1)' || echo 'кандидат (CAR_PITCH1200_OLD=0)')"
  python3 - "$O/twin.txt" <<'PY'
import re, sys
n = sh = sr = sv = 0; fails = 0
for l in open(sys.argv[1], errors='replace'):
    if '|' not in l or l.startswith(' ') or l.startswith('заезд'): continue
    p = l.split('|')
    try:
        nums = [re.findall(r'-?\d+\.\d+', x) for x in p[1:]]
        v = (float(nums[0][0]), float(nums[0][1])); r = (float(nums[1][0]), float(nums[1][1])); h = (float(nums[2][0]), float(nums[2][1]))
    except Exception: continue
    n += 1; sv += abs(v[1] - v[0]); sr += abs(r[1] - r[0]); sh += abs(h[1] - h[0]); fails += 'FAIL' in l
print('  twin: %d прыжков, вне допуска %d, Σ|Δкромка| %.1f км/ч, Σ|Δподъём| %.2f м, Σ|Δудар| %.1f м' % (n, fails, sv, sr, sh))
PY
  echo "  $(python3 tests/jumps/pitchcmp.py $O)"
  grep -E '^[1-5]_' $O/jz5.txt | sed -E 's/^([^|]*)\| кромка +([0-9.]+) км\/ч vy +(-?[0-9.]+) \(PS2 +(-?[0-9.]+)\) тангаж +(-?[0-9.]+)° \(PS2 +(-?[0-9.]+)°\).*(КРЫША|ФАСАД\/УЛИЦА|ПЕРЕЛЕТ|ДОМ_ДРУГОЕ|НЕ_СЕЛА) +\(PS2 ([А-ЯЁ\/_]+).*/  jz5 \1: кромка \2 км\/ч vy \3 (PS2 \4) тангаж \5° (PS2 \6°) -> \7 (PS2 \8)/'
  for lat in "+0" "+6" "-6"; do echo "  scan lat $lat: $(grep "scan lat $lat " $O/scan.txt | sed -E 's/scan lat [+-][0-9]+ +([0-9]+) км.*(КРЫША|ФАСАД\/УЛИЦА|ПЕРЕЛЕТ|ДОМ_ДРУГОЕ|НЕ_СЕЛА).*/\1:\2/' | tr '\n' ' ')"; done
  roof100=$(grep -c "scan lat .* 100 км.*КРЫША" $O/scan.txt); echo "  цель: jz5 на 100 км/ч садится на КРЫШУ в $roof100 из 3 вариантов сдвига (PS2: КРЫША)"
  if [ $k = new ]; then
    bad=$(grep -c FAIL $O/twin.txt); [ "$bad" = 0 ] || { echo "FAIL: кандидат вне допуска twin ($bad)"; rc=1; }
  fi
done
exit $rc
