#!/bin/bash
# №1274: Гомер на платформе АЭС Toggle (L1, объект 32: ящик ~(-99,8; 4,0; -298,5), выключатель — сфера 0,5 м в (-99; 4; -297,2)). Эмулятор: Гомер пешком встаёт на платформу у выключателя, △ (flag aoact), платформа едет 4 м туда и обратно.
# Строки PLAT1274 (положение Гомера раз в 10 кадров). Зелёный: Гомер за 8 с проезжает по x не меньше 3 м вместе с платформой и y не падает ниже 3,5 (платформа на 4,0). Красный (flag ride1274old 1): Гомер остаётся (x не больше 1 м) или падает (y < 3,5). Г1: прыжок с платформы наследует её скорость (INHERIT1274), красный rideinh1274old 1.
#   tests/platform1274/emu.sh EBOOT
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; OUT=build/plat1274; mkdir -p $OUT
run() { local N=$1 OLD=$2 INH=${3:-0}; { echo "scene SW_1_0"; echo "flag ride1274old $OLD"; echo "flag rideinh1274old $INH"; echo "flag platlog 1"; echo "wait 20"; echo "foot -99.3 -297.5 0 4.3"; echo "wait 3"; echo "flag aoact"; echo "wait 3"; echo "press cross 0.15"; echo "wait 6"; echo "shot plat_end"; echo "wait 1"; echo "end"; } > $OUT/$N.scn
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 "$S" $N $OUT/$N.scn > $OUT/$N.log 2>&1; mkdir -p $OUT/$N; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt $OUT/$N/; echo "DONE $N"; }
run plat1274_new 0; run plat1274_old 1; run plat1274_inh 0 1
python3 - <<'PY'
import re
def info(n):
    xs, ys = [], []
    for l in open(f'build/plat1274/{n}/smoke.txt', errors='replace'):
        m = re.match(r'PLAT1274 t=([\d.]+) homer=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', l)
        if m: xs.append((float(m.group(2)), float(m.group(4)))); ys.append(float(m.group(3)))
    return xs, ys
bad = 0
for n, want in (('plat1274_new', 1), ('plat1274_old', 0)):
    xs, ys = info(n)
    if not xs: print(n, 'нет строк PLAT1274'); bad = 1; continue
    rng = max(((a[0]-b[0])**2 + (a[1]-b[1])**2) ** .5 for a in xs for b in xs); ymin = min(ys); ok = rng >= 3.0 and ymin >= 3.5
    print('%s: Гомер прошёл (макс. расстояние между точками) %.1f м, y от %.2f до %.2f, строк %d -> %s' % (n, rng, ymin, max(ys), len(xs), 'ЕДЕТ С ПЛАТФОРМОЙ' if ok else 'остался/упал'))
    if ok != bool(want): bad = 1
# Г1: прыжок с платформы наследует её горизонтальную скорость (строка INHERIT1274): новое есть, при rideinh1274old 1 нет
def inh(n):
    v = []
    for l in open(f'build/plat1274/{n}/smoke.txt', errors='replace'):
        m = re.match(r'INHERIT1274 v=(-?[\d.]+),(-?[\d.]+)', l)
        if m: v.append((float(m.group(1)) ** 2 + float(m.group(2)) ** 2) ** .5)
    return v
vn, vi = inh('plat1274_new'), inh('plat1274_inh')
print('Г1: прыжок с платформы, унаследовано (м/с): новое %s, при rideinh1274old 1 %s' % (['%.2f' % x for x in vn], ['%.2f' % x for x in vi]))
if not (vn and max(vn) >= .3): print('FAIL Г1: скорость опоры не унаследована'); bad = 1
if vi: print('КРАСНЫЙ Г1 НЕ ПОЙМАН'); bad = 1
print('PASS' if not bad else 'FAIL'); raise SystemExit(bad)
PY
