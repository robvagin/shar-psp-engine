#!/bin/bash
# 107b (Стена): строки ящика STUCK/NEAR/TRFWAIT/MCRSTATE/HNR/PAUSE называют держателя. stuck_gate.sh PACK_NEW   (PACK_NEW — набор с EBOOT этой ветки)
# PASS: сцена «газ в стоящую машину трафика» даёт в smoke.txt STUCK (с «hit» или «body»), в нём [trf#, строку TRFWAIT и PAUSE; без газа STUCK нет; flag stucklog108old 1 — строк нет. Игра не меняется (только чтение): снимок/журнал физики — gate_phys.sh
set -u; cd "$(dirname "$0")/../.."; P=$1; fail=0
run() { SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=300 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
chk() { local f="$1/smoke.txt" want="$2" pat="$3" what="$4" n; n=$(grep -cE "$pat" "$f"); if { [ "$want" = yes ] && [ "$n" -gt 0 ]; } || { [ "$want" = no ] && [ "$n" = 0 ]; }; then echo "  ok   $what"; else echo "  FAIL $what (строк $n)"; fail=1; fi; }
R=$(run tests/stena108/stuck_scn.txt); R0=$R; echo "газ в стоящую машину:"
chk "$R" yes "^STUCK .*(hit|body)" "STUCK с контактом"; chk "$R" yes "^STUCK .*\[(HEAD )?trf#" "в near названа машина трафика"; chk "$R" yes "^TRFWAIT " "TRFWAIT"; chk "$R" yes "^MCRSTATE |^HNR " "HNR или MCRSTATE"; chk "$R" yes "^PAUSE " "PAUSE"
grep -m2 -E "^STUCK |^TRFWAIT " "$R/smoke.txt" | cut -c1-260 | sed 's/^/    /'
R=$(run tests/stena108/stuck_scn_none.txt); echo "без газа (красный контроль):"
chk "$R" no "^STUCK " "STUCK нет"
RO=$(run tests/stena108/stuck_scn_old.txt); echo "flag stucklog108old 1 (строк нет, игра та же):"
chk "$RO" no "^(STUCK|TRFWAIT|PAUSE L|NEAR|MSNEND) " "строк ящика нет"
# физика та же: положение машины по секундам (BB x y z) в двух прогонах — разница не больше 1,5 м (часы эмулятора дают малый разброс)
python3 -I - "$R0" "$RO" <<'PYEOF' || fail=1
import sys, re
def pos(d):
    out = {}
    for l in open(d + '/smoke.txt', errors='replace'):
        m = re.match(r'BB\s+(\d+)\s+\S+\s+L\d+\s+\S+\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)', l)
        if m: out[int(m.group(1))] = (int(m.group(2)), int(m.group(4)))
    return out
a, b = pos(sys.argv[1]), pos(sys.argv[2]); ks = sorted(set(a) & set(b))
bad = [k for k in ks if abs(a[k][0] - b[k][0]) > 2 or abs(a[k][1] - b[k][1]) > 2]
print('  %s положения машины по секундам: сравнено %d, разошлось %d' % ('ok  ' if ks and not bad else 'FAIL', len(ks), len(bad))); sys.exit(0 if ks and not bad else 1)
PYEOF
[ $fail = 0 ] && echo PASS || { echo FAIL; exit 1; }
