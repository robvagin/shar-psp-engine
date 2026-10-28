#!/bin/bash
# №1020 приёмка: у монеты не больше двух блёсток, они появляются случайно, кадр оборота спрайта сдвинут по монете. accept60.sh NEW_EBOOT [red]
#   Сцена L1: камера 6,5 м от монеты, 12 с; журнал COIN1020 (раз в 0,5 с: монет с блёстками, блёсток, наибольшее число у монеты, кадров с блёстками, маска сдвигов кадра оборота).
#   «Было» (файл coin1020old): у ближней монеты три блёстки кольцом, всегда; сдвиг кадра один (маска 1). «Стало»: maxper <= 2; блёсток на монету не больше 70 % от «было»;
#   по 0,5-с выборкам число блёсток меняется (не меньше 3 разных значений) и есть выборки и без блёсток, и с ними; маска сдвигов у «стало» содержит не меньше 2 разных сдвигов.
#   «red» — красный контроль: «стало» тоже с файлом coin1020old -> три блёстки, одно значение, маска 1 -> FAIL. Последняя строка PASS / FAIL
D=$(cd "$(dirname "$0")" && pwd); I=$(cd "$D/../.." && pwd); RUN=${RUN:-$I/tools/emu_run.sh}; export OUT=${OUT:-/tmp/coin1020_out}; export EMU_OWNER=${EMU_OWNER:-Мультик}; export EMU_MINE=${EMU_MINE:-2}
EB=$1; RED=0; [ "${2:-}" = red ] && RED=1
[ -f "$EB" ] || { echo "нет EBOOT $EB"; echo FAIL; exit 1; }
mkdir -p $OUT; rm -rf $OUT/out_acc_new $OUT/out_acc_old
( [ $RED = 1 ] && export FLAGS=coin1020old; $RUN $D/scenario_accept.txt acc_new "$EB" 200 >/dev/null 2>&1 ) &
( FLAGS=coin1020old $RUN $D/scenario_accept.txt acc_old "$EB" 200 >/dev/null 2>&1 ) &
wait
python3 -I - "$OUT/out_acc_new" "$OUT/out_acc_old" <<'PYEOF'
import sys, re
dn, do = sys.argv[1], sys.argv[2]
rx = re.compile(r'COIN1020 t=\S+ on=(\d) frames=(\d+) coins=(\d+) glints=(\d+) maxper=(\d+) framesgl=(\d+) phmask=([0-9a-f]+)')
def rd(d):
    try: t = open(d + '/smoke.txt', encoding='utf8', errors='replace').read()
    except OSError: return None
    return [tuple(int(x, 16) if i == 6 else int(x) for i, x in enumerate(m.groups())) for m in rx.finditer(t)]
n, o = rd(dn), rd(do)
if not n or not o: print('нет журнала'); print('FAIL'); sys.exit(1)
n = [r for r in n if r[2] > 0]; o = [r for r in o if r[2] > 0]
ok_all = True
def chk(ok, s):
    global ok_all; ok_all &= bool(ok); print('  %s %s' % ('ok  ' if ok else 'FAIL', s))
chk(len(n) >= 8 and len(o) >= 8, 'выборок с монетами: стало %d, было %d (нужно >= 8)' % (len(n), len(o)))
chk(n and max(r[4] for r in n) <= 2, 'стало: наибольшее число блёсток у монеты %s (нужно <= 2)' % (max((r[4] for r in n), default=0)))
chk(o and max(r[4] for r in o) == 3, 'было: наибольшее число блёсток у монеты %s (нужно 3)' % (max((r[4] for r in o), default=0)))
pn = sum(r[3] for r in n) / max(1, sum(r[2] for r in n)); po = sum(r[3] for r in o) / max(1, sum(r[2] for r in o))
chk(pn <= .7 * po, 'блёсток на монету-кадр: стало %.2f, было %.2f (нужно стало <= 70 %% от было)' % (pn, po))
vals = sorted({round(r[3] / max(1, r[2]), 2) for r in n})
chk(len(vals) >= 3 and min(r[3] for r in n) == 0 and max(r[3] for r in n) >= 1, 'стало: по выборкам доля блёсток на монету меняется, значения %s, есть и пустые, и с блёстками' % vals[:6])
ph_n = bin(max((r[6] for r in n), default=0)).count('1'); ph_all = bin(sum({r[6] for r in n}) if False else 0).count('1')
m = 0
for r in n: m |= r[6]
chk(bin(m).count('1') >= 2, 'сдвигов кадра оборота у «стало»: %d разных (маска %x, нужно >= 2)' % (bin(m).count('1'), m))
print('PASS' if ok_all else 'FAIL'); sys.exit(0 if ok_all else 1)
PYEOF
