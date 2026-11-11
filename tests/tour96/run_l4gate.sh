#!/bin/bash
# Гейт шага 11 (FABLE_АУДИТ §3.4 п.2) целиком: tests/tour96/run_l4gate.sh НАБОР_96 [a|b|c ...]  (по умолчанию a b c)
#   a: сцена l4_free250.txt (около 2 мин), b: заезд 20 мин L4 с заходами, c: тот же заезд при принудительных 250 м (по 25-30 мин). Ballast 5, nice 20 вызывающим, один эмулятор.
#   После каждого прогона: ключевые строки и вывод проверки остаются в build/smoke/<прогон>/, smoke.txt и bmp удаляются (правило Роба по диску 08.11).
set -u
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; SET=${1:?набор 96 (папка с EBOOT.PBP)}; shift; cd "$W"
MODES=${*:-a b c}; RC=0
export SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 DEVELOPER_DIR=/Library/Developer/CommandLineTools PATH=$W/.venv/bin:$PATH
for m in $MODES; do
  case $m in a) SCN=$H/l4_free250.txt; T=900;; b) python3 -I "$H/gen_l4gate.py" > build/l4gate_b.txt; SCN=$W/build/l4gate_b.txt; T=3000;; c) python3 -I "$H/gen_l4gate.py" 250 > build/l4gate_c.txt; SCN=$W/build/l4gate_c.txt; T=3000;; *) continue;; esac
  echo "=== режим $m $(date +%H:%M:%S)"
  SMOKE_SCN=$SCN SMOKE_MAXT=$T tools/smoke.sh "$SET" > build/l4gate_$m.out 2>&1
  D=$(grep -o "build/smoke/[0-9-]*" build/l4gate_$m.out | head -1); [ -n "$D" ] || { echo "нет отчёта смоука: build/l4gate_$m.out"; RC=2; continue; }
  python3 -I "$H/check_l4free.py" $m "$D/smoke.txt" | tee "$D/gate_l4_$m.txt"; r=${PIPESTATUS[0]}; [ $r = 0 ] || RC=$r
  grep -a -E "^(SCENE|PH6MEM|MEMFAILSZ|WATCHDOG (mem|zone3|cellthrash)|LEVEL ev|XITION|LVA level)" "$D/smoke.txt" > "$D/keylines.txt"
  rm -f -- "$W/$D/smoke.txt" "$W/$D/headless.log"; rm -f -- "$W/$D"/*.bmp
done
exit $RC
