#!/bin/bash
# Аудит 04.11: камера пешком у своей машины за спиной героя не прижимается к голове (L5 этап m3, Апу рядом с машиной; было 1,12 м).
#   tests/camera/footcarpin.sh НАБОР          зелёный: после 6 с на старте этапа глаз не ближе 3,0 м к голове героя (L4 m3, L5 m3, L6 m3)
#   tests/camera/footcarpin.sh --old НАБОР    красный контроль: тот же L5 с «flag homercarpinold 1» (как в TEST89) ОБЯЗАН дать меньше 3,0 м
# НАБОР — папка набора с EBOOT этой сборки; уровни 2–7 из SHIP_LEVELS
set -u; cd "$(dirname "$0")/../.."
OLD=0; [ "${1:-}" = --old ] && { OLD=1; shift; }
SRC="${1:?набор}"; export SHIP_LEVELS="2 3 4 5 6 7"; TMP=$(mktemp -d); rc=0; MIN=3.0
run() {   # уровень этап_k флаг_старый
  { echo "scene FCP_$1_$2"; echo "flag camdet 1"; echo "flag camlog"; echo "flag startcam 1"; [ -n "$3" ] && echo "flag homercarpinold 1"; echo "flag golevel $1"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 8"; echo "flag msnsel $2"; echo "wait 6"; echo "shot a"; echo "wait 1"; echo "end"; } > "$TMP/s.txt"
  SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_SCN="$TMP/s.txt" SMOKE_MAXT=300 tools/smoke.sh "$SRC" > "$TMP/l.log" 2>&1
  D=$(sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p' "$TMP/l.log"); [ -n "$D" ] || { echo "L$1 k$2: нет отчёта smoke"; rc=1; return; }
  python3 - "$D" "$1" "$2" "$MIN" <<'PY' || rc=1
import re, sys, math
d, L, k, mn = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4]); last = None
for l in open(d + '/smoke.txt', errors='replace'):
    m = re.match(r'CAMF t=([\d.]+) st=\d+ fov=[\d.]+ eye=(\S+) look=(\S+) homer=(\S+)', l)
    if m: last = m.groups()
e = list(map(float, last[1].split(','))); h = list(map(float, last[3].split(',')))
dd = math.dist(e, [h[0], h[1] + 1.5, h[2]]); ok = dd >= mn
print(f'L{L} k{k}: глаз от головы героя {dd:.2f} м ({"OK" if ok else "БЛИЗКО"})'); sys.exit(0 if ok else 1)
PY
}
if [ $OLD = 1 ]; then run 5 2 old; [ $rc = 1 ] && { echo "красный контроль: упал, как и должен"; exit 0; }; echo "красный контроль НЕ упал"; exit 1; fi
run 5 2 ""; run 6 2 ""; run 4 2 ""
exit $rc
