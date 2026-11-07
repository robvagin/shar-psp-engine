#!/bin/bash
# №1172: срыв машин в боковой занос на скорости (fleet1172.c: 89 машин × 3 скорости × 2 стороны, полный руль 2 с, 30 к/с). Новое ≤ 30 срывов из 534 (TEST82 — 24, TEST87 — 139);
# красные контроли: SV_GRIPLOSS1172_OLD=1, SV_YAWGROUND1172_OLD=1 и оба вместе (как TEST87) — каждое обязано дать больше 30
export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk} PATH=~/Downloads/shar-psp/build/xcshim/bin:$PATH
cd "$(dirname "$0")/../.." || exit 1
O=build/g1172; mkdir -p $O
NAMES=$(python3 -c "import re;print(' '.join(sorted(set(re.findall(r'^\\s*\\{\"([a-z0-9_]+)\"',open('viewer/carparams.h').read().split('carCons[]')[1],re.M)))))")
run() { cc -std=c99 -O2 -w -I viewer -DSV_FORCE_PLAYER=1 -DSV_AIRWHEEL_OLD=0 $1 tests/g01cars/fleet1172.c viewer/col.c viewer/car.c -lm -o $O/fleet_t || exit 1; $O/fleet_t $NAMES | grep -oE 'срыв в занос: [0-9]+' | grep -oE '[0-9]+$'; }
fail=0; n=$(run ""); if [ "$n" -le 30 ]; then echo "OK   новое: срывов $n из 534 (≤ 30)"; else echo "FAIL новое: срывов $n из 534 (> 30)"; fail=1; fi
for f in "-DSV_GRIPLOSS1172_OLD=1" "-DSV_YAWGROUND1172_OLD=1" "-DSV_GRIPLOSS1172_OLD=1 -DSV_YAWGROUND1172_OLD=1"; do n=$(run "$f"); if [ "$n" -gt 30 ]; then echo "OK   $f даёт $n (красный)"; else echo "FAIL $f даёт $n (должно быть > 30)"; fail=1; fi; done
exit $fail
