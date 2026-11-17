#!/bin/bash
# 109 (в + крючок Бабла): моя проверка машин фазы (phaseCarsAllReady, подключена к mvPhaseCarsReady) отвечает «не готово», пока модели машин миссии не в памяти, потом «готово»; _OLD (flag msnphasecars109old 1) отвечает «готово» сразу.
# Карточка Бабла ждёт этот ответ до 25 с (его гейт tests/load109/ready.sh, тот же крючок). cardwait_gate.sh PACK
set -u; cd "$(dirname "$0")/../.."; P=$1; fail=0
run() { SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=300 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
Rn=$(run tests/stena109/cardwait_new.txt); Ro=$(run tests/stena109/cardwait_old.txt)
echo "  новое:  $(grep '^PHASECARS109' $Rn/smoke.txt | sed 's/PHASECARS109 //' | tr '\n' ';')"; echo "  _OLD:   $(grep '^PHASECARS109' $Ro/smoke.txt | sed 's/PHASECARS109 //' | tr '\n' ';')"
python3 -I - "$Rn/smoke.txt" "$Ro/smoke.txt" <<'PYEOF' || fail=1
import sys, re
def rd(p): return [(float(m.group(1)), int(m.group(2))) for l in open(p, errors='replace') for m in [re.match(r'PHASECARS109 t=([\d.]+) ready=(-?\d+)', l)] if m]
n, o = rd(sys.argv[1]), rd(sys.argv[2])
ok = len(n) == 3 and n[0][1] == 0 and n[-1][1] == 1 and len(o) == 3 and all(r == -1 for _, r in o)   # _OLD: крючка нет (-1) — карточка Бабла условия не имеет, готова сразу
print('  %s новое: не готово -> готово (%s); _OLD: крючка нет, карточка ждёт только прежнее (%s)' % ('ok  ' if ok else 'FAIL', [r for _, r in n], [r for _, r in o])); sys.exit(0 if ok else 1)
PYEOF
[ $fail = 0 ] && echo PASS || { echo FAIL; exit 1; }
