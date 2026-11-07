#!/bin/bash
# №1185: «машины на линии» без эмулятора: следы всех четырёх водителей полигона (race_host, настоящие модули движка) против линии секторов bonus/bNN.sec.
#   tests/bonus1167/sector_cars.sh [ПАПКА_ТРАСС] [ПАПКА_SEC]    ЗЕЛЁНЫЙ: медиана расстояния до линии <= 4 м и 95 % <= 10 м на каждой трассе (ширина дороги с полосами ≈ 7 м; 1 м ≈ 1 px кадра 480×272).
# КРАСНЫЙ: та же линия, сдвинутая на 15 м, обязана упасть.
cd "$(dirname "$0")/../.."; export DEVELOPER_DIR="${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}"
S="${1:-$HOME/Downloads/shar-psp-integ/build/test87tour/PSP/GAME/SHARTEST15/bonus}"; D="${2:-build/secdata}"; R=build/bonus/bin/race_host; mkdir -p build/secred
[ -x $R ] || bash tools/bonus/build_host.sh $R >/dev/null 2>&1
python3 -I - "$D" build/secred <<'PY'
import struct, sys
for t in range(1, 8):
    d = open(f'{sys.argv[1]}/b0{t}.sec', 'rb').read(); n = struct.unpack('<I', d[4:8])[0]; pts = [list(struct.unpack('<3f', d[8 + 12 * i:20 + 12 * i])) for i in range(n)]
    for p in pts: p[0] += 15.0
    open(f'{sys.argv[2]}/b0{t}.sec', 'wb').write(d[:8] + b''.join(struct.pack('<3f', *p) for p in pts))
PY
chk() { local bad=0; for t in 1 2 3 4 5 6 7; do l=$($R "$S/b0$t.shr" "$S/b0$t.bns" "$S/msncars-b.shr" --laps 1 --secs 120 --sec-in "$1/b0$t.sec" 2>&1 | grep "^SECTORS"); echo "$l" | sed 's/^SECTORS /  /'
  python3 -I -c "
import re,sys
m=re.search(r'median=([\d.]+) p90=([\d.]+) p95=([\d.]+) max=([\d.]+)', '''$l''')
sys.exit(0 if m and float(m.group(1))<=4 and float(m.group(3))<=10 else 1)" || bad=$((bad+1)); done; return $bad; }
echo "== зелёный"; chk "$D"; g=$?
echo "== красный (линия сдвинута на 15 м)"; chk build/secred >/dev/null; r=$?
[ $g -eq 0 ] && [ $r -ge 5 ] && { echo "ИТОГ: PASS (зелёный: все 7 трасс; красный упал на $r из 7)"; exit 0; }
echo "ИТОГ: FAIL (зелёный упал на $g, красный упал на $r из 7)"; exit 1
