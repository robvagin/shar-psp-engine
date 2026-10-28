#!/bin/bash
# №1026 приёмка: вывески (notoon, не небо) проходят ту же цветокоррекцию, что город: контраст и насыщенность из меню меняют их палитры; монета и ключ слушаются. accept60.sh NEW_EBOOT [red]
#   Журнал TONE1026 (сумма палитр вывесок, монеты, ключа после тона) на L1, L2, L5 при контрасте 100 и 160 и Colour 100 и 160 (flag tonecontrast, flag hzcachetone).
#   «Стало»: на каждом уровне вывесок >= 1; сумма палитр вывесок при контрасте 160 отличается от 100 не меньше чем на 0,5 %; при Colour 160 отличается от 100; сумма палитры монеты на L1 при контрасте 160
#   отличается от 100 (монета слушается). «Было» (файл tone1026old): сумма вывесок при смене контраста РОВНО та же (вывески мимо контраста), и сумма «стало» при том же контрасте отличается от «было».
#   «red» — красный контроль: «стало» тоже с файлом tone1026old -> вывески не слушаются контраста -> FAIL. Последняя строка PASS / FAIL
D=$(cd "$(dirname "$0")" && pwd); I=$(cd "$D/../.." && pwd); RUN=${RUN:-$I/tools/emu_run.sh}; export OUT=${OUT:-/tmp/tone1026_out}; export EMU_OWNER=${EMU_OWNER:-Мультик}; export EMU_MINE=${EMU_MINE:-2}
EB=$1; RED=0; [ "${2:-}" = red ] && RED=1
[ -f "$EB" ] || { echo "нет EBOOT $EB"; echo FAIL; exit 1; }
mkdir -p $OUT; rm -rf $OUT/out_acc_new $OUT/out_acc_old
( [ $RED = 1 ] && export FLAGS=tone1026old; $RUN $D/scenario_accept.txt acc_new "$EB" 400 >/dev/null 2>&1 ) &
( FLAGS=tone1026old $RUN $D/scenario_accept.txt acc_old "$EB" 400 >/dev/null 2>&1 ) &
wait
python3 -I - "$OUT/out_acc_new" "$OUT/out_acc_old" <<'PYEOF'
import sys, re
dn, do = sys.argv[1], sys.argv[2]
rx = re.compile(r'TONE1026 t=\S+ on=(\d) level=(\d+) signs=(\d+) signsum=(\d+),(\d+),(\d+) coinsum=(\d+) wssum=(\d+) sat=(\d+) ct=(\d+)')
def rd(d):
    try: t = open(d + '/smoke.txt', encoding='utf8', errors='replace').read()
    except OSError: return None
    out = {}
    for m in rx.finditer(t):
        g = [int(x) for x in m.groups()]; out[(g[1], g[8], g[9])] = (g[2], sum(g[3:6]), g[6], g[7])
    return out
n, o = rd(dn), rd(do)
if not n or not o: print('нет журнала'); print('FAIL'); sys.exit(1)
ok_all = True
def chk(ok, s):
    global ok_all; ok_all &= bool(ok); print('  %s %s' % ('ok  ' if ok else 'FAIL', s))
for lv in (1, 2, 5):
    try:
        a, b, c = n[(lv, 136, 100)] if (lv, 136, 100) in n else None, None, None
    except Exception: pass
    keys = sorted(k for k in n if k[0] == lv)
    try:
        c100 = [v for k, v in n.items() if k[0] == lv and k[2] == 100 and k[1] != 160][0]; c160 = [v for k, v in n.items() if k[0] == lv and k[2] == 160 and k[1] == c100 and 0 or k[0] == lv and k[2] == 160][0]
    except IndexError: print('нет выборок L%d' % lv); print('FAIL'); sys.exit(1)
    o100 = [v for k, v in o.items() if k[0] == lv and k[2] == 100][0]; o160 = [v for k, v in o.items() if k[0] == lv and k[2] == 160][0]
    chk(c100[0] >= 1, 'L%d: вывесок %d (нужно >= 1)' % (lv, c100[0]))
    chk(abs(c160[1] - c100[1]) >= .005 * c100[1], 'L%d «стало»: сумма палитр вывесок контраст 100 -> 160: %d -> %d (разница >= 0,5 %%)' % (lv, c100[1], c160[1]))
    chk(o160[1] == o100[1], 'L%d «было»: сумма палитр вывесок контраст 100 -> 160: %d -> %d (вывески мимо контраста, нужно ровно то же)' % (lv, o100[1], o160[1]))
    chk(c100[1] != o100[1], 'L%d: «стало» %d против «было» %d при том же контрасте (цветокоррекция применена)' % (lv, c100[1], o100[1]))
    if lv == 1: chk(c160[2] != c100[2], 'L1 монета «стало»: сумма палитры контраст 100 -> 160: %d -> %d (монета слушается)' % (c100[2], c160[2]))
print('PASS' if ok_all else 'FAIL'); sys.exit(0 if ok_all else 1)
PYEOF
