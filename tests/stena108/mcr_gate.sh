#!/bin/bash
# P0 шаг 1 (107b, Стена): отказы чтения модели машины миссии не ломают её навсегда. mcr_gate.sh PACK
#  mcrbad_new  (flag mcrreadfail 6): MCR load smith_v есть (своя модель встала), perm=1 нет, 'MCR bad-wait' (пауза) есть
#  mcrbad_old  (flag mcrbad108old 1): perm=1 и модели нет (старое поведение — красный контроль)
#  mcrfail_new (flag mcrfail 24): заглушка встала после 'ladder-exhausted', потом MCR load smith_v; число попыток лестницы не больше одной в кадр (шаги msncar1265 ≥150 мс друг от друга)
set -u; cd "$(dirname "$0")/../.."; P=$1; fail=0
run() { SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=300 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
chk() { local f="$1/smoke.txt" want="$2" pat="$3" what="$4" n; n=$(grep -cE "$pat" "$f"); if { [ "$want" = yes ] && [ "$n" -gt 0 ]; } || { [ "$want" = no ] && [ "$n" = 0 ]; }; then echo "  ok   $what"; else echo "  FAIL $what (строк $n)"; fail=1; fi; }
R=$(run tests/stena108/mcrbad_new.txt); echo "новое, 6 отказов чтения:"
chk "$R" yes "MCR load smith_v" "своя модель встала"; chk "$R" no "MCR bad .*perm=1" "битым навсегда не стала"; chk "$R" yes "MCR bad-wait|MCR bad .*n=[0-9]" "отказы считаются, пауза есть"
grep -E "MCR bad|MCR load smith|ladder-exhausted|mcr standin" "$R/smoke.txt" | cut -c1-120 | head -8 | sed 's/^/    /'
R=$(run tests/stena108/mcrbad_old.txt); echo "_OLD, 6 отказов чтения:"
chk "$R" yes "MCR bad .*perm=1" "КРАСНЫЙ: битый навсегда"; chk "$R" no "MCR load smith_v" "КРАСНЫЙ: модели нет до конца"
R=$(run tests/stena108/mcrfail_new.txt); echo "новое, 24 отказа подряд:"
chk "$R" yes "ladder-exhausted smith_v" "лестница исчерпана"; chk "$R" yes "mcr standin smith_v" "заглушка встала после исчерпания"; chk "$R" yes "MCR load smith_v" "потом своя модель"
python3 -I - "$R/smoke.txt" <<'PYEOF' || fail=1
import sys, re
ts = [float(m.group(1)) for l in open(sys.argv[1], errors='replace') for m in [re.search(r'msncar1265 smith_v(?: mem=-?\d+)? t=([\d.]+)', l)] if m]
gaps = [b - a for a, b in zip(ts, ts[1:]) if b - a < 0.5]
bad = [g for g in gaps if g < 0.05]
print('  %s шагов лестницы по smith_v: %d, в одно мгновение (< 0,05 с, журнал с шагом 0,1 с): %d' % ('ok  ' if ts and not bad else 'FAIL', len(ts), len(bad))); sys.exit(0 if ts and not bad else 1)
PYEOF
R=$(run tests/stena108/mcrsleep.txt); echo "сон (flag simsleep 4):"
chk "$R" yes "^PWR (suspend|resume)" "сон и пробуждение были"; chk "$R" yes "MCR failreset why=epoch" "эпоха сменилась — отказы заново"; chk "$R" yes "MCR load smith_v" "своя модель встала"; chk "$R" no "MCR bad .*perm=1" "битым навсегда не стала"
[ $fail = 0 ] && echo PASS || { echo FAIL; exit 1; }
