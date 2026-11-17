#!/bin/bash
# Стенд окна Гомер 107b в эмуляторе (безголовый PPSSPP через scen.py; в фоне, без звука и окон): настоящий EBOOT, настоящий уровень 1.
#  S (склон, В1): Гомер на травяном склоне ~40° (L1, от (-86; -136) вверх по -Z, подъём 10 м), стик вперёд 4 с. Зелёный: набор высоты ≥ 3 м. Красный (flag homerslope107bold 1): ≈ 0,5 м.
#  R (крыша своей машины, Г5): flag homerroof 0 ставит Гомера на крышу своей стоящей машины, стик вперёд 1 с. Зелёный: ушёл от центра крыши ≥ 1,5 м. Красный (flag homeride107bold 1): < 1 м.
#  T (крыша машины трафика, Г4): flag homerroof 1 ставит Гомера на крышу ближайшей едущей машины трафика, без ввода 3 с. Зелёный: остался на крыше (dist < 2,6 м, grounded 1). Красный (homeride107bold 1): уехали друг от друга.
#   tests/homer107b/emu.sh EBOOT   (EMU_REANALYZE=1 — только разбор готовых журналов)
#  Г4: стенд берёт тот вариант (время ожидания 8/16/24/32 с после goto), где выбранная машина трафика действительно едет (v > 1 м/с) на всех трёх проверках; остановившаяся машина не свидетель
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/h107b/emu; mkdir -p $OUT
go() { local N=$1; SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_HOME=$ROOT/build/twins/ppsspp_home_h107b_$N SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $OUT/$N.scn > $OUT/$N.log 2>&1; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/; echo "DONE $N"; }
s() { local N=$1 O=$2; { echo "flag homerslope107bold $O"; echo "wait 10"; echo "foot -86 -136 3.1416"; echo "wait 8"; echo "stick 0 -1 4"; echo "wait 6"; echo "end"; } > $OUT/$N.scn; go $N; }
r() { local N=$1 O=$2; { echo "flag homeride107bold $O"; echo "wait 10"; echo "goto 220 172 1.5708"; echo "wait 6"; echo "flag homerroof 0"; echo "wait 2"; echo "flag homerroofchk"; echo "stick 0 -1 1"; echo "wait 2"; echo "flag homerroofchk"; echo "end"; } > $OUT/$N.scn; go $N; }
t() { local N=$1 O=$2 W=$3; { echo "flag homeride107bold $O"; echo "wait 10"; echo "goto 220 172 1.5708"; echo "wait $W"; echo "flag homerroof 1"; echo "wait 0.5"; echo "flag homerroofchk"; echo "wait 1.5"; echo "flag homerroofchk"; echo "wait 1.5"; echo "flag homerroofchk"; echo "end"; } > $OUT/$N.scn; go $N; }
[ -n "${EMU_REANALYZE:-}" ] || { ( s slopeN 0; s slopeO 1 ) & ( r roofN 0; r roofO 1 ) & ( t trafN1 0 8; t trafO1 1 8 ) & ( t trafN2 0 16; t trafO2 1 16 ) & ( t trafN3 0 24; t trafO3 1 24 ) & ( t trafN4 0 32; t trafO4 1 32 ) & wait; }
python3 - <<'PY'
import re, math
B = 'build/h107b/emu'
def rd(n): return open(f'{B}/{n}/smoke.txt', errors='replace').read().splitlines()
def homer(n):
    out = []
    for l in rd(n):
        m = re.match(r'HOMER t=([\d.]+) pos=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', l)
        if m: out.append(tuple(float(x) for x in m.groups()))
    return out
def chk(n):
    out = []
    for l in rd(n):
        m = re.match(r'ROOFCHK car=(-?\d+) dist=([\d.]+) dy=(-?[\d.]+) grounded=(\d+) onCar=(\d+) rideOk=(\d+) v=(-?[\d.]+)', l)
        if m: out.append((int(m.group(1)), float(m.group(2)), float(m.group(3)), int(m.group(4)), int(m.group(5)), int(m.group(6)), float(m.group(7))))
    return out
bad = 0
def slope(n):   # набор высоты после стика: y перед стиком (HOMER за 0,5 с до конца ожидания после foot) -> максимум y после
    L = rd(n); ft = None
    for l in L:
        m = re.match(r'FOOT (-?[\d.]+) (-?[\d.]+) (-?[\d.]+) (-?[\d.]+) t=([\d.]+)', l)
        if m and ft is None: ft = float(m.group(5))
    H = homer(n)
    if ft is None or not H: return None
    pre = [h for h in H if ft + 4.0 <= h[0] <= ft + 7.8]; post = [h for h in H if h[0] > ft + 8.0]
    if not pre or not post: return None
    return max(h[2] for h in post) - pre[-1][2]
for N in ('slopeN', 'slopeO'): print(N, 'набор', slope(N))
for N in ('roofN', 'roofO'):
    print(N, 'ROOFCHK', chk(N))
sg = {n: (slope(n) if slope(n) is not None else -99) for n in ('slopeN', 'slopeO')}
rc = {n: chk(n) for n in ('roofN', 'roofO')}
moving = None
for k in '1234':
    cn, co = chk('trafN' + k), chk('trafO' + k)
    print('traf вариант', k, 'новое', cn, 'старое', co)
    if moving is None and len(cn) >= 3 and len(co) >= 3 and all(c[6] > 1.0 for c in cn) and all(c[6] > 1.0 for c in co): moving = k
if moving is None: print('FAIL traf: ни в одном варианте машина не ехала (v > 1 м/с) все 3 проверки (стенд не свидетель)'); bad = 1; rc['trafN'] = rc['trafO'] = []
else: rc['trafN'] = chk('trafN' + moving); rc['trafO'] = chk('trafO' + moving); print('traf берём вариант', moving)
if sg['slopeN'] < 3.0: print('FAIL S зелёный: набор', sg['slopeN']); bad = 1
if sg['slopeO'] >= 1.5: print('КРАСНЫЙ S НЕ ПОЙМАН: набор', sg['slopeO']); bad = 1
else: print('красный S (homerslope107bold 1): набор %.2f м против %.2f зелёного' % (sg['slopeO'], sg['slopeN']))
for pre, ok_new, ok_old in (('roof', lambda c: len(c) >= 2 and c[-1][1] - c[-2][1] >= 1.2, lambda c: len(c) >= 2 and c[-1][1] - c[-2][1] < 1.0),
                            ('traf', lambda c: len(c) >= 3 and c[-1][1] < 2.6 and c[-1][3] == 1, lambda c: len(c) >= 3 and (c[-1][1] >= 3.5 or c[-1][3] == 0))):
    cn, co = rc[pre + 'N'], rc[pre + 'O']
    if not ok_new(cn): print('FAIL', pre, 'зелёный', cn); bad = 1
    if not ok_old(co): print('КРАСНЫЙ', pre, 'НЕ ПОЙМАН', co); bad = 1
    else: print('красный', pre, 'пойман, как должен')
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
