#!/bin/bash
# perf.sh ПАКЕТ  — пара на безголовом эмуляторе, сцена F_GO7_L2: PH4SUB npc (мс на кадр, Мак) новое против красного (flag npcprop1301bold 1 = проверка у каждого шага, как в 108).
# Зелёный: новое ≤ 0,2 мс и красное ≥ 1,0 мс (красный контроль ловит прежнюю цену). ПАКЕТ — папка набора (EBOOT с 1301b, данные уровня 2).
cd "$(dirname "$0")/../.."; P=${1:?пакет}
med() { grep -a '^PH4SUB' $1/smoke.txt | sed -n 's/.*tick: npc=\([0-9.]*\).*/\1/p' | tail -n +10 | sort -n | awk '{a[NR]=$1} END{print a[int((NR+1)/2)]}'; }
run() { SHIP_LEVELS=2 SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=200 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
N=$(run tests/stena1301b/l2npc.txt); R=$(run tests/stena1301b/l2npc_red.txt)
n=$(med $N); r=$(med $R); echo "новое npc=$n мс ($N), красное npc=$r мс ($R)"
awk -v n="$n" -v r="$r" 'BEGIN{ if (n+0<=0.2 && r+0>=1.0) {print "PASS"; exit 0} else {print "FAIL"; exit 1} }'
