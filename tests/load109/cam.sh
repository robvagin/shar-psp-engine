#!/bin/bash
# 1315 К5: «камера занята» (наезд, полосы разговора, дуга) держала пузырь этапа без предела: текст «сядь в машину» всплывал, когда игрок уже в машине.
# Стенд: «flag camstuck1315 1» — признак камеры застрял навсегда (как гипотеза Fable после разговора в помещении), L1 m6.
# Зелёный: текст этапа показан (BUBBLE start) через ≈2 с, перед ним WATCHDOG bubble cam-late why=duga cap=2000; красный (flag bubcam1315old 1): текст так и не показан.   tests/load109/cam.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/load109; mkdir -p $OUT
run() { local N=$1 O=$2; { echo "scene SW_1_0"; echo "flag bubcam1315old $O"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 6"; echo "flag camstuck1315 1"; echo "flag msnmainat 600"; echo "wait 14"; echo "press cross 0.2"; echo "wait 14"; echo "end"; } > $OUT/$N.scn
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $OUT/$N.scn > $OUT/$N.log 2>&1; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/; echo "DONE $N"; }
[ -n "${CAM_SKIP:-}" ] || { run cam1315_new 0; run cam1315_old 1; }
python3 - <<'PY'
def an(n):
    L = open(f'build/load109/{n}/smoke.txt', errors='replace').read().splitlines()
    s = next((i for i, l in enumerate(L) if l.startswith('MSN mission m6 ')), -1)
    if s < 0: return None
    late = [l for l in L[s:] if l.startswith('WATCHDOG bubble cam-late')]
    shown = [l for l in L[s:] if l.startswith('BUBBLE start')]
    return late, shown
bad = 0
for n, want in (('cam1315_new', 1), ('cam1315_old', 0)):
    r = an(n)
    if r is None: print(n, 'FAIL: нет m6'); bad = 1; continue
    late, shown = r
    print('%s: cam-late %d %s, текст показан %d раз' % (n, len(late), late[0][:80] if late else '', len(shown)))
    if want and (len(late) < 1 or 'why=duga' not in late[0] or not shown): print('FAIL зелёный'); bad = 1
    if not want and (late or shown): print('КРАСНЫЙ НЕ ПОЙМАН'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
