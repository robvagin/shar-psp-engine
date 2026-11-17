#!/bin/bash
# Уворот трафика по оригиналу (107b, Стена). trfdodge_gate.sh PACK_NEW [PACK_OFF]  (PACK_OFF — сборка с -DTRF_SV_DODGE_OLD=1: уворот выключен, как в 105; красный контроль)
# Числа оригинала: игрок быстрее 11,111 м/с, трафик быстрее 8,333 м/с, лоб в лоб (стороны согласованы), линия игрока ±3 м, дальность взгляда трафика v×1,5 с.
set -u; cd "$(dirname "$0")/../.."; P=$1; Q=${2:-}; fail=0
run() { SMOKE_HEADLESS=1 SMOKE_SCN="$2" SMOKE_MAXT=300 tools/smoke.sh "$1" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
chk() { local f="$1/smoke.txt" want="$2" pat="$3" what="$4" n; n=$(grep -cE "$pat" "$f"); if { [ "$want" = yes ] && [ "$n" -gt 0 ]; } || { [ "$want" = no ] && [ "$n" = 0 ]; }; then echo "  ok   $what"; else echo "  FAIL $what (строк $n)"; fail=1; fi; }
R=$(run $P tests/stena108/trfdodge_oncoming.txt); echo "встречная, игрок разгоняется:"; chk "$R" yes "^TRFDODGE pv=" "уворот был (TRFDODGE)"
grep -m1 "^TRFDODGE" "$R/smoke.txt" | cut -c1-140 | sed 's/^/    /'
python3 -I - "$R/smoke.txt" <<'PYEOF' || fail=1
import sys, re
for l in open(sys.argv[1], errors='replace'):
    m = re.match(r'TRFDODGE pv=([\d.]+) cv=([\d.]+) ap=([\d.]+) lp=(-?[\d.]+)', l)
    if m:
        pv, cv, ap, lp = map(float, m.groups()); ok = pv > 11.111 and cv > 8.333 and ap > 0 and abs(lp) <= 3.0 and ap < cv * 1.5
        print('  %s пороги оригинала: игрок %.2f > 11,111, трафик %.2f > 8,333, впереди %.1f < %.1f м, поперёк %.1f <= 3,0' % ('ok  ' if ok else 'FAIL', pv, cv, ap, cv * 1.5, abs(lp))); sys.exit(0 if ok else 1)
print('  FAIL нет строки TRFDODGE'); sys.exit(1)
PYEOF
python3 -I - "$R/smoke.txt" <<'PYEOF'
import sys, re
pr = [(float(m.group(1)), float(m.group(2)), float(m.group(3)), int(m.group(4))) for l in open(sys.argv[1], errors='replace') for m in [re.match(r'TRFPROBE t=([\d.]+) car0=(-?[\d.]+),(-?[\d.]+) v=(-?[\d.]+) mode=(\d)', l)] if m for m in [type('M', (), {'group': lambda self, i, mm=m: [None, mm.group(1), mm.group(3), mm.group(4), mm.group(5)][i]})()]]
print('    замер ухода (машина 0: время, z, скорость, режим): ' + '; '.join('t=%.1f z=%.1f v=%.1f m=%d' % (a, b, c, d) for a, b, c, d in pr))
PYEOF
python3 -I - "$R/smoke.txt" new <<'PYEOF' || fail=1
import sys, re
def traj(path):
    t0 = None; rows = {}
    for l in open(path, errors='replace'):
        m = re.match(r'TRFDODGE .* trf#(\d+) t=([\d.]+)', l)
        if m and t0 is None: idx, t0 = int(m.group(1)), float(m.group(2))
        m = re.match(r'TRFPROBEALL t=([\d.]+) i=(\d+) x=(-?[\d.]+) z=(-?[\d.]+) v=(-?[\d.]+) mode=(\d) dodged=(\d)', l)
        if m: rows.setdefault(int(m.group(2)), []).append((float(m.group(1)), float(m.group(5)), int(m.group(6)), int(m.group(7))))
    return (idx, t0, rows) if t0 is not None else (None, None, rows)
idx, t0, rows = traj(sys.argv[1]); k = sys.argv[2]
if t0 is None: print('  FAIL нет уворота'); sys.exit(1)
tr = [r for r in rows.get(idx, []) if r[0] >= t0]
print('    стенд, машина %d после уворота (t, v, режим, dodged): %s' % (idx, '; '.join('%.0f:%.1f/%d/%d' % r for r in tr[:12])))
late = [r for r in tr if r[0] >= t0 + 1.5 and r[2] != 0]
stopped = bool(late) and late[0][3] == 0 and late[0][1] < 1.0   # первая проба после 1,5 с: машина остановилась (v<1), этап уворота кончился (dodged=0), дальше обычные правила
ok = stopped if k == 'new' else not stopped
print('  %s %s: к 2-10 с после уворота машина %s' % ('ok  ' if ok else 'FAIL', 'новое' if k == 'new' else 'прежний вариант (красный)', 'остановилась, дальше обычные правила' if stopped else 'всё ещё в манёвре')); sys.exit(0 if ok else 1)
PYEOF
R2=$(run $P tests/stena108/trfdodge_hold.txt); python3 -I - "$R2/smoke.txt" hold <<'PYEOF2' || fail=1
import sys, re
t0 = None; rows = {}
for l in open(sys.argv[1], errors='replace'):
    m = re.match(r'TRFDODGE .* trf#(\d+) t=([\d.]+)', l)
    if m and t0 is None: idx, t0 = int(m.group(1)), float(m.group(2))
    m = re.match(r'TRFPROBEALL t=([\d.]+) i=(\d+) x=(-?[\d.]+) z=(-?[\d.]+) v=(-?[\d.]+) mode=(\d) dodged=(\d)', l)
    if m: rows.setdefault(int(m.group(2)), []).append((float(m.group(1)), float(m.group(5)), int(m.group(6)), int(m.group(7))))
if t0 is None: print('  FAIL нет уворота в красном'); sys.exit(1)
late = [r for r in rows.get(idx, []) if r[0] >= t0 + 1.5 and r[2] != 0]
moving = bool(late) and late[0][3] == 1   # прежний вариант: через 1,5 с после уворота этап манёвра не кончен (dodged=1, ручник, газ и руль держатся)
print('  %s прежний вариант (ручник, газ и руль без срока): к 2-10 с манёвр не кончается: dodged=1 без срока (красный) — %s' % ('ok  ' if moving else 'FAIL', 'так' if moving else 'нет')); sys.exit(0 if moving else 1)
PYEOF2
R=$(run $P tests/stena108/trfdodge_same.txt); echo "впереди в том же направлении:"; chk "$R" no "^TRFDODGE pv=" "уворота нет (не лоб в лоб)"
R=$(run $P tests/stena108/trfdodge_slow.txt); echo "встречная, игрок стоит:"; chk "$R" no "^TRFDODGE pv=" "уворота нет (игрок медленнее 40 км/ч)"
if [ -n "$Q" ]; then R=$(run $Q tests/stena108/trfdodge_oncoming.txt); echo "красный: уворот выключен как в 105:"; chk "$R" no "^TRFDODGE pv=" "КРАСНЫЙ: уворота нет"; fi
[ $fail = 0 ] && echo PASS || { echo FAIL; exit 1; }
