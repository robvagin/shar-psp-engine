#!/bin/bash
# Т1 (107b, Стена): трафик не тормозит перед обычным пешеходом (как у оригинала). trfped_gate.sh PACK
# новое: машина трафика 0 проходит место пешехода (x дальше пешехода на 3 м и больше), _OLD: стоит перед ним (красный контроль)
set -u; cd "$(dirname "$0")/../.."; P=$1; fail=0
run() { SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=300 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
for k in new old; do R=$(run tests/stena108/trfped_$k.txt)
  python3 -I - "$R/smoke.txt" $k <<'PYEOF' || fail=1
import sys, re
L = open(sys.argv[1], errors='replace').read().splitlines(); k = sys.argv[2]
pr = [l for l in L if l.startswith('TRFPROBE')]; xs = [(float(re.search(r'car0=(-?[\d.]+)', l).group(1)), float(re.search(r'ped0=(-?[\d.]+)', l).group(1)), float(re.search(r' v=(-?[\d.]+)', l).group(1))) for l in pr]
if not xs: print('  FAIL нет строк TRFPROBE'); sys.exit(1)
cx, px, v = xs[-1]; passed = any(a > b + 3.0 for a, b, _ in xs); stopped = all(a < b - 0.5 for a, b, _ in xs) and all(w < 0.5 for _, _, w in xs[1:])
ok = passed if k == 'new' else stopped
print('  %s %s: машина x=%.1f, пешеход x=%.1f, v=%.1f -> %s' % ('ok  ' if ok else 'FAIL', 'новое' if k == 'new' else '_OLD (красный)', cx, px, v, 'проехала мимо' if passed else 'стоит перед пешеходом' if stopped else 'в пути'))
sys.exit(0 if ok else 1)
PYEOF
done
R=$(run tests/stena108/trfnpc.txt); echo "персонаж миссии на полосе (пустышка):"
python3 -I - "$R/smoke.txt" <<'PYEOF' || fail=1
import sys, re
L = [l for l in open(sys.argv[1], errors='replace') if l.startswith('TRFPROBE')]
xs = [(float(re.search(r'car0=(-?[\d.]+)', l).group(1)), float(re.search(r' v=(-?[\d.]+)', l).group(1))) for l in L]
ok = len(xs) >= 3 and 236.0 < xs[1][0] < 242.5 and xs[1][1] < 0.5   # через 4 с стоит в 0,5-7 м перед персонажем (дальше машину убирает/пересоздаёт трафик)
print('  %s трафик встаёт перед персонажем миссии (x=243): машина x=%s' % ('ok  ' if ok else 'FAIL', ', '.join('%.1f' % a for a, _ in xs))); sys.exit(0 if ok else 1)
PYEOF
[ $fail = 0 ] && echo PASS || { echo FAIL; exit 1; }
