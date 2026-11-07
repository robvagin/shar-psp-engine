#!/bin/bash
# №1200/№1201: чувствительность twin (14 прыжков против PS2) и jz5 к параметрам; usage: rampsweep.sh "-DFLAGS" -> метрика twin + jz5
cd "$(dirname "$0")/../.."; mkdir -p build/g1201
export PATH=$HOME/Downloads/shar-psp/build/xcshim/bin:$PATH SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
LV=$HOME/Downloads/shar-psp-integ/build/test91tour/PSP/GAME/SHARTEST15/level.shr
cc -std=c11 -O2 -w -DSKIN_HOST -I viewer $1 tests/jumps/twin.c viewer/car.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -o build/g1201/tw_x || exit 1
build/g1201/tw_x "$LV" > build/g1201/tw_out.txt 2>&1
python3 - <<'PY'
import re
rows=[l for l in open('build/g1201/tw_out.txt',errors='replace') if '|' in l and not l.startswith(' ') and not l.startswith('заезд')]
sr=sh=sv=0;n=0;det=[]
for l in rows:
    p=l.split('|')
    nums=[re.findall(r'-?\d+\.\d+',x) for x in p[1:]]
    try:
        v=(float(nums[0][0]),float(nums[0][1])); r=(float(nums[1][0]),float(nums[1][1])); h=(float(nums[2][0]),float(nums[2][1]))
    except Exception: continue
    n+=1; sv+=abs(v[1]-v[0]); sr+=abs(r[1]-r[0]); sh+=abs(h[1]-h[0]); det.append((p[0].split()[0],round(r[1]-r[0],2),round(h[1]-h[0],1)))
print('n',n,'Σ|Δкромка км/ч| %.1f Σ|Δподъём м| %.2f Σ|Δудар м| %.1f'%(sv,sr,sh))
print(' '.join('%s:%+.2f/%+.0f'%d for d in det))
PY
cc -std=c11 -O2 -w -DSKIN_HOST -I viewer $1 tests/airwalls/host.c viewer/car.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -o build/g1201/aw_x 2>&1 | head -2
build/g1201/aw_x "$LV" scan -165 306 180 --y 5.6 --kmh 96:112:4 --lat 0:0:1 --secs 10 2>&1 | grep -E '^lat' | sed 's/air hits.*ground hits/gh/' | awk '{printf "kmh %s maxY %s %s | ", $7, $11, $0 ~ /end/ ? substr($0, index($0,"end")) : ""} END{print ""}' | cut -c1-420
