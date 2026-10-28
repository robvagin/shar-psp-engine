#!/bin/bash
# №723 приёмка в игре (эмулятор, сборка тура): игрок за рулём стоит у дома (место PS2 state_l1_incar, (215; 170)), шкала 100 одним `flag hnr 100`, 50 с, строки HNRV раз в секунду.
# PS2 (ps2-hnr-meter, tw_v8_stand, b3_chase, v111): погоня без паузы 3 с, спад 6,0/с до 5 → защёлка снята через 15,8–16,0 с, до нуля 15,9–17,6 с; полиция не ближе 27 м, возвращается к месту появления (ближе 30 → снова далеко).
# Зелёный: защёлка снята через 14,8…17,0 с, ближайшая полиция ≥ 26,5 м, пересадка полиции (подряд сближение ≤ 30 м и затем > 45 м) ≥ 1. Красные: flag hnrpause723 0 (пауза 3 с: ~19 с) и flag police723 0 (полиция вплотную < 26 м) — оба FAIL.
# usage: tests/police723/stand_gate.sh EBOOT   (SET — набор тура, по умолчанию TEST74); нужен tools/emu_run.sh. Последняя строка PASS/FAIL
set -u; cd "$(dirname "$0")/../.."; E=${1:?EBOOT}; SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test74tour/PSP/GAME/SHARTEST15}; OUT=${OUT:-build/p723/stand}; mkdir -p $OUT; export SET EMU_OWNER=${EMU_OWNER:-Глюки} EMU_MINE=${EMU_MINE:-3} OUT; rc=0
mk() { { printf 'scene start\nflag cfgdefaults 1\nwait 6\nscene L1_load\nflag golevel 1\nwait 25\nflag player\nwait 1\ngoto 215.05 170.25 3.1416\nwait 2\nfoot 217.5 171.5 0\nwait 1.5\npress triangle 0.3\nwait 3\nscene STAND\n%s\nflag hnr 100\n' "$2"; for i in $(seq 50); do echo 'wait 1'; done; echo end; } > $OUT/$1.txt; }
mk g ""; mk r_pause "flag hnrpause723 0"; mk r_police "flag police723 0"
for v in g r_pause r_police; do bash tools/emu_run.sh $OUT/$v.txt $v $E 600 >/dev/null 2>&1 & done; wait
an() { python3 -I - "$OUT/out_$1/smoke.txt" <<'PY'
import re,sys
rows=[]; sc=None
for l in open(sys.argv[1],errors='replace'):
    m=re.match(r'SCENE (\S+)',l)
    if m: sc=m.group(1); continue
    if sc!='STAND': continue
    m=re.search(r'HNRV t=(\S+) heat=(\S+) latch=(\d) police=(\d+) near=(\S+)',l)
    if m: rows.append((float(m.group(1)),float(m.group(2)),int(m.group(3)),int(m.group(4)),float(m.group(5))))
t0=next((t for t,h,l,p,n in rows if l==1),None); te=next((t for t,h,l,p,n in rows if t0 is not None and t>t0 and l==0),None)
nears=[n for t,h,l,p,n in rows if p>0 and n>0 and (t0 is None or t0<=t<=(te or 1e9))]
close=False; resp=0
for n in nears:
    if n<=30: close=True
    elif close and n>45: resp+=1; close=False
print('%.1f %.1f %d'%((te-t0) if (te and t0) else -1, min(nears) if nears else -1, resp))
PY
}
read rel nr resp < <(an g); echo "  зелёный: защёлка снята через $rel с (PS2 15,8–16,0), ближайшая полиция $nr м (PS2 ≥ 27), пересадок после сближения $resp"
awk -v a="$rel" -v n="$nr" -v r="$resp" 'BEGIN{exit !(a>=14.8 && a<=17.0 && n>=26.5 && r>=1)}' || { echo "    !! вне допуска"; rc=1; }
read rel nr resp < <(an r_pause); echo "  красный (пауза 3 с): защёлка снята через $rel с"; awk -v a="$rel" 'BEGIN{exit !(a>17.0)}' || { echo "    !! красный не красный"; rc=1; }
read rel nr resp < <(an r_police); echo "  красный (flag police723 0): ближайшая полиция $nr м"; awk -v n="$nr" 'BEGIN{exit !(n<26)}' || { echo "    !! красный не красный"; rc=1; }
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
