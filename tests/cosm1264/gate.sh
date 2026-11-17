#!/bin/bash
# Гейт 1264: косметика целей A1.8 / A1.11 / A1.12. tests/cosm1264/gate.sh [EBOOT]
#  1) хост (Мак): чистые функции viewer/mission/msn_cosm1264.h, новое и красное «old» дают разные ответы;
#  2) эмулятор (нужен EBOOT): A1.11 на L5 m2, этап timer SetDurationTime(0.5): новое — миссия закрывается ≤ 0,2 с после старта этапа, flag msncosm1264old 1 — ≥ 0,4 с.
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools; ok=1
cc -std=c11 -O1 -Iviewer tests/cosm1264/host.c -lm -o /tmp/cosm_host && /tmp/cosm_host | tail -1 | grep -q 'RESULT PASS' && echo "хост: PASS" || { echo "хост: FAIL"; ok=0; }
EB=${1:-}
if [ -n "$EB" ]; then S=$HOME/.claude/skills/shar-level-polish/scripts/scen.py; ROOT=${SCEN_ROOT:-$HOME/Downloads/shar-psp}; mkdir -p build/cosm1264
  for O in 0 1; do N=cosm1264_t${O}
    { echo "scene SW_5_0"; echo "flag camdet 1"; echo "flag golevel 5"; [ $O = 1 ] && echo "flag msncosm1264old 1"; echo "wait 20"; echo "flag player 1"; echo "flag msn"; echo "wait 6"; echo "flag msnmainat 105"; echo "wait 3"; echo "press cross"; echo "wait 2"; echo "press cross"; echo "wait 4"; echo "flag msnpass"; echo "wait 6"; echo "end"; } > build/cosm1264/$N.scn
    SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SCEN_EBOOT=$EB nice -n 20 python3 $S $N build/cosm1264/$N.scn > build/cosm1264/$N.log 2>&1; cp -f $ROOT/build/twins/scen/full/$N/smoke.txt build/cosm1264/$N.txt
    d=$(python3 - build/cosm1264/$N.txt <<'PY'
import re,sys
L=open(sys.argv[1],errors='replace').read().splitlines()
t0=next(float(re.search(r't=([\d.]+)',l).group(1)) for l in L if l.startswith('MSN stage m2.1.5 obj=timer'))
t1=next(float(re.search(r't=([\d.]+)',l).group(1)) for l in L if l.startswith('MSN complete m2'))
print('%.1f'%(t1-t0))
PY
)
    echo "A1.11 old=$O: от старта этапа timer до конца миссии $d с"
    python3 -c "import sys; d=float('$d'); sys.exit(0 if ($O==0 and d<=0.2) or ($O==1 and d>=0.4) else 1)" || ok=0
  done
fi
[ $ok = 1 ] && echo "ГЕЙТ ЗЕЛЁНЫЙ" || { echo "ГЕЙТ КРАСНЫЙ"; exit 1; }
