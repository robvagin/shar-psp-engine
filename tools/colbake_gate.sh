#!/bin/bash
# №bake-3Б: гейт печёного разбора COL1 в эмуляторе (тот же бинарник, что на PSP). На уровень два прогона:
#   «use»   — движок берёт colbake.shr: ждём COLBAKE used=1;
#   «check» — считает живое и сверяет с файлом (colBakeVerify): ждём verify=0.
# Красный контроль: tools/colbake_gate.sh SET -n — на уровне 1 вместо файла подставляется копия с испорченным байтом: ждём used=0 (хеш записи отверг, откат на живое), verify=-9.
#   tools/colbake_gate.sh SET [-n] [УРОВНИ]    SET с colbake.shr рядом с уровнями (L1 — в корне SET, L<N> — в l<N>/); хост-печь tools/colbake.sh
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; SET="$1"; shift; NEG=0; [ "${1:-}" = -n ] && { NEG=1; shift; }; LV="${*:-1 2 3 4 5 6 7}"; SCN="$(mktemp -d)"; FAIL=0
EXTRA="colbake.shr l2/colbake.shr l3/colbake.shr l4/colbake.shr l5/colbake.shr l6/colbake.shr l7/colbake.shr"
one() { # level flags tag set
  printf 'flag level %s\nscene c%s\nwait 14\nend\n' "$1" "$1" > "$SCN/s$1$3.txt"
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_EXTRA="$EXTRA" SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_FLAGS="$2" SMOKE_SCN="$SCN/s$1$3.txt" SMOKE_MAXT="${SMOKE_MAXT:-600}" "$ROOT/tools/smoke.sh" "${4:-$SET}" >"$SCN/run$1$3.log" 2>&1
  d=$(sed -n 's/^отчёт: \(.*\)\/summary.txt.*/\1/p' "$SCN/run$1$3.log" | tail -1); case "$d" in /*) ;; *) d="$ROOT/$d" ;; esac; grep -a '^COLBAKE ' "$d/smoke.txt" | head -1
}
fld() { echo "$1" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }
for n in $LV; do
  u=$(one $n "" u); c=$(one $n colbakecheck c)
  if [ "$(fld "$u" used)" = 1 ] && [ "$(fld "$c" verify)" = 0 ]; then echo "L$n PASS  use: $u | check: $c"; else echo "L$n FAIL  use: $u | check: $c"; FAIL=1; fi
done
if [ $NEG = 1 ]; then
  BAD="$(mktemp -d)"; for f in $(ls -A "$SET"); do ln -s "$SET/$f" "$BAD/$f"; done; rm "$BAD/colbake.shr"; cp "$SET/colbake.shr" "$BAD/colbake.shr"; python3 -c "
import sys; p=sys.argv[1]; b=bytearray(open(p,'rb').read()); b[len(b)//2]^=0x5a; open(p,'wb').write(b)" "$BAD/colbake.shr"
  a=$(one 1 "" a "$BAD"); [ "$(fld "$a" used)" = 0 ] && echo "КОНТРОЛЬ испорченный файл OK (used=0, откат на живое): $a" || { echo "КОНТРОЛЬ НЕ СРАБОТАЛ: $a"; FAIL=1; }
fi
exit $FAIL
