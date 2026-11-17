#!/bin/bash
# 1309: L2 m1 этап «оторваться»: за 60 с погони Скиннер не упирается в стену дольше 1 с (удар: скорость падает на ≥ 7 м/с за 0,2 с с высоких, потом стоит < 1 м/с с игроком дальше 12 м). Красный: flag chasecurve1309old 1 — упирается.   skinner_gate.sh PACK_L2
set -u; cd "$(dirname "$0")/../.."; P=$1
run() { SHIP_LEVELS=2 SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=400 tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1; }
an() { python3 -I - "$1/smoke.txt" <<'PY'
import sys,re
rows=[]
for l in open(sys.argv[1],errors='replace'):
    if not l.startswith('MCLOG skinn'): continue
    m=re.search(r'pos=([-\d.]+),([-\d.]+) v=([-\d.]+) .* dp=([-\d.]+)',l)
    if m and ' mode=4 ' in l: rows.append((float(m.group(1)),float(m.group(2)),float(m.group(3)),float(m.group(4))))
imp=0; run=0; longest=0
for i,(x,z,v,dp) in enumerate(rows):
    if i and rows[i-1][2]>12 and v<rows[i-1][2]-7: imp+=1
    if v<1.0 and dp>15.0: run+=1; longest=max(longest,run)
    else: run=0
print(imp, round(longest*0.2,1), len(rows))
PY
}
Rn=$(run tests/stena1296/skinner_l2.txt); Ro=$(run tests/stena1296/skinner_l2_old.txt)
read ni ln nn <<< "$(an $Rn)"; read oi lo on <<< "$(an $Ro)"
echo "  новое: ударов $ni, самая долгая остановка $ln с (строк $nn);  прежнее: ударов $oi, остановка $lo с (строк $on)"
python3 -I - "$ni" "$ln" "$oi" "$lo" <<'PY' && echo PASS || { echo FAIL; exit 1; }
import sys; ni,ln,oi,lo=float(sys.argv[1]),float(sys.argv[2]),float(sys.argv[3]),float(sys.argv[4]); sys.exit(0 if ln<=1.0 and ni<=1 and (oi>=2 or lo>1.0) else 1)
PY
