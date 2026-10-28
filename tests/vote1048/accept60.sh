#!/bin/bash
# №1048 приёмка: фургон Квимби в трафике L5 говорит «Vote Quimby» (клип votetruc_horn), когда ближе 40 м; раз в 9 с, пока рядом; громкость по расстоянию (как у любого позиционного звука порта). accept60.sh NEW_EBOOT [red]
#   Журнал VOTE1048: «стало» — не меньше двух запусков, дальность не больше 40 м, handle != 0 (звук запущен), gain в (0, 1]; повтор не чаще 6 с по часам журнала (9 с игрового времени). «red» (flag vote1048 0 с начала) — запусков нет -> FAIL. Последняя строка PASS / FAIL
D=$(cd "$(dirname "$0")" && pwd); I=$(cd "$D/../.." && pwd); RUN=${RUN:-$I/tools/emu_run.sh}; export OUT=${OUT:-/tmp/vote1048_out}; export EMU_OWNER=${EMU_OWNER:-Мультик}; export EMU_MINE=${EMU_MINE:-2}
EB=$1; RED=0; [ "${2:-}" = red ] && RED=1
[ -f "$EB" ] || { echo "нет EBOOT $EB"; echo FAIL; exit 1; }
mkdir -p $OUT; rm -rf $OUT/out_acc; [ $RED = 1 ] && export FLAGS=vote1048old
$RUN $D/scenario.txt acc "$EB" 200 >/dev/null 2>&1
python3 -I - "$OUT/out_acc/smoke.txt" <<'PYEOF'
import sys, re
try: t = open(sys.argv[1], encoding='utf8', errors='replace').read()
except OSError: print('нет журнала'); print('FAIL'); sys.exit(1)
ev = [(float(m.group(1)), float(m.group(3)), float(m.group(4)), int(m.group(5))) for m in re.finditer(r'VOTE1048 t=([\d.]+) car=(\d+) dist=([\d.]+) gain=([\d.]+) handle=(-?\d+)', t)]
ok_all = True
def chk(ok, s):
    global ok_all; ok_all &= bool(ok); print('  %s %s' % ('ok  ' if ok else 'FAIL', s))
chk(len(ev) >= 2, 'запусков голоса: %d (нужно >= 2)' % len(ev))
chk(ev and all(e[1] <= 40.0 for e in ev), 'дальность запусков до %.1f м (нужно <= 40)' % max((e[1] for e in ev), default=0))
chk(ev and all(e[3] != 0 for e in ev), 'звук запущен (handle != 0): %s' % [e[3] for e in ev][:4])
chk(ev and all(0 < e[2] <= 1 for e in ev), 'громкость по расстоянию: %s' % [(round(e[1]), e[2]) for e in ev][:4])
chk(all(b[0] - a[0] >= 6.0 for a, b in zip(ev, ev[1:])), 'повтор не чаще 6 с по часам журнала (9 с игрового времени)')
print('PASS' if ok_all else 'FAIL'); sys.exit(0 if ok_all else 1)
PYEOF
