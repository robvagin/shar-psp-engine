#!/bin/bash
# 1303 (3): ✕, нажатый, пока карточка на LOADING, не запоминается и не закрывает карточку при готовности (у оригинала нажатия до кнопки не считаются).
# Стенд: карточка m3 держится на LOADING 6 с (flag mvcarsdelay109 6), ✕ на 2-й секунде, ещё ✕ через 3 с после готовности.
# Зелёный: после готовности до второго ✕ нет «MSN briefing-continue», при втором ✕ ровно один; красный (loadq1303old 1): continue идёт сразу при готовности (до второго ✕).   tests/load109/xq.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/load109; mkdir -p $OUT
run() { local N=$1 O=$2; { echo "scene SW_1_0"; echo "flag camdet 1"; echo "flag loadq1303old $O"; echo "flag mvcarsdelay109 6"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 6"; echo "flag msnmainat 300"; echo "wait 2"; echo "press cross 0.2"; echo "wait 9"; echo "press cross 0.2"; echo "wait 6"; echo "end"; } > $OUT/$N.scn
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $OUT/$N.scn > $OUT/$N.log 2>&1; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/; echo "DONE $N"; }
[ -n "${XQ_SKIP:-}" ] || { run xq1303_new 0; run xq1303_old 1; }
python3 - <<'PY'
import re
def rd(n): return open(f'build/load109/{n}/smoke.txt', errors='replace').read().splitlines()
def an(n):
    L = rd(n); s = next((i for i, l in enumerate(L) if l.startswith('MSN mission m3 ')), -1)
    ld = [l for l in L[s:] if l.startswith('MSN briefing-loaded')] if s >= 0 else []
    q = int(re.search(r'queued=(\d+)', ld[0]).group(1)) if ld else -1
    c = sum(1 for l in L[s:] if l.startswith('MSN briefing-continue')) if s >= 0 else 0
    ig = sum(1 for l in L[s:] if l.startswith('MSN briefing-x-ignored')) if s >= 0 else 0
    return q, c, ig
bad = 0
for n, want in (('xq1303_new', 1), ('xq1303_old', 0)):
    q, c, ig = an(n)
    print('%s: при готовности ✕ в памяти (queued) = %d, ✕ проигнорировано на LOADING = %d, continue всего = %d' % (n, q, ig, c))
    if q < 0: print('FAIL: карточка не вышла'); bad = 1; continue
    if want and (q != 0 or ig < 1 or c != 1): print('FAIL зелёный'); bad = 1
    if not want and q != 1: print('КРАСНЫЙ НЕ ПОЙМАН'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
