#!/bin/bash
# 1310/1311 К1+К2: текст первого этапа (L1 m6: «HIT THE DELIVERY TRUCK AND COLLECT THE BONESTORM GAMES») игрок должен видеть не меньше 3,0 с: срок идёт с первого кадра, где мир виден
# (карточка не стоит и не уезжает, ирис открыт). Журнал: BUBBLE shown=<с> why=… (К2), при < 3,0 с WATCHDOG bubble actor=short.
# Стенд: «flag camfree1310 1» — пузырь не ждёт камеру старта (как в миссиях без неё: там текст раньше шёл под уезжающей карточкой; при камере старта ожидание камеры случайно прятало дефект).
# Зелёный: shown ≥ 3,0 и ни одного WATCHDOG bubble short; красный (flag bubvis1310old 1): shown < 3,0 и сторож сработал.   tests/load109/bub.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/load109; mkdir -p $OUT
run() { local N=$1 O=$2; { echo "scene SW_1_0"; echo "flag camfree1310 1"; echo "flag bubvis1310old $O"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 6"; echo "flag msnmainat 600"; echo "wait 14"; echo "press cross 0.2"; echo "wait 14"; echo "end"; } > $OUT/$N.scn
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $OUT/$N.scn > $OUT/$N.log 2>&1; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/; echo "DONE $N"; }
[ -n "${BUB_SKIP:-}" ] || { run bub1310_new 0; run bub1310_old 1; }
python3 - <<'PY'
import re
def an(n):
    L = open(f'build/load109/{n}/smoke.txt', errors='replace').read().splitlines()
    s = next((i for i, l in enumerate(L) if l.startswith('MSN mission m6 ')), -1)
    if s < 0: return None
    b = [l for l in L[s:] if l.startswith('BUBBLE shown=')]
    short = [l for l in L[s:] if l.startswith('WATCHDOG bubble actor=short')]
    return b, short
bad = 0
for n, want in (('bub1310_new', 1), ('bub1310_old', 0)):
    r = an(n)
    if r is None: print(n, 'FAIL: нет m6'); bad = 1; continue
    b, short = r
    first = b[0] if b else ''
    m = re.search(r'shown=([\d.]+)', first); v = float(m.group(1)) if m else -1
    print('%s: первый текст: %s | сторож short: %d' % (n, first[:90], len(short)))
    if v < 0: print('FAIL: нет строки BUBBLE'); bad = 1; continue
    if want and (v < 3.0 or short): print('FAIL зелёный'); bad = 1
    if not want and not (v < 3.0 and short): print('КРАСНЫЙ НЕ ПОЙМАН'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
