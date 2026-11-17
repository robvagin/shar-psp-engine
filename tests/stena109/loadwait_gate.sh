#!/bin/bash
# 109 (в/г, Стена): этап поездки стартует, когда машины следующего этапа в памяти (очередь пуста), мир не стоит; не дольше 6 с. loadwait_gate.sh PACK
# новое: после msnpass этап 1 стартует позже минимум на 1 с, чем у _OLD (flag msnloadwait109old 1), строка MSN loadwait есть, модель skinn_v прочитана до старта; _OLD: этап стартует сразу, строки нет
set -u; cd "$(dirname "$0")/../.."; P=$1; fail=0
run() { SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=300 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
tstage() { grep -m1 -E '^BB +[0-9]+ .* msn m1\.1\.1 ' "$1/smoke.txt" | awk '{print $2}'; }   # номер строки ящика (секунда) первого кадра этапа 1
Rn=$(run tests/stena109/loadwait_new.txt); Ro=$(run tests/stena109/loadwait_old.txt); tn=$(tstage $Rn); to=$(tstage $Ro)
echo "  старт этапа 1 (секунда ящика): новое t=${tn:-нет}, _OLD t=${to:-нет}"
grep -m1 "^MSN loadwait" $Rn/smoke.txt | sed 's/^/    /'; grep -m2 "MSN phasecars" $Rn/smoke.txt | sed 's/^/    /'
grep -q "^MSN loadwait" $Rn/smoke.txt && echo "  ok   новое: этап ждал очередь (MSN loadwait)" || { echo "  FAIL нет MSN loadwait"; fail=1; }
grep -q "^MSN loadwait" $Ro/smoke.txt && { echo "  FAIL у _OLD есть loadwait"; fail=1; } || echo "  ok   КРАСНЫЙ: у _OLD этап не ждёт"
python3 -I - "${tn:-0}" "${to:-0}" <<'PYEOF' || fail=1
import sys
tn, to = float(sys.argv[1]), float(sys.argv[2]); ok = tn > 0 and to > 0 and tn - to >= 1.0 and tn - to <= 8.0
print('  %s новое стартует позже _OLD на %.1f с (нужно от 1 до 8: отказы чтения пережидаются, не дольше 6 с)' % ('ok  ' if ok else 'FAIL', tn - to)); sys.exit(0 if ok else 1)
PYEOF
grep -q "MCR load skinn_v" $Rn/smoke.txt && echo "  ok   skinn_v прочитан" || { echo "  FAIL skinn_v не прочитан"; fail=1; }
[ $fail = 0 ] && echo PASS || { echo FAIL; exit 1; }
