#!/bin/bash
# №1223: «рисунок шестерни АЭС отсекается там, где её коллизия жива?» Сетка глаз 10..90 м × 1/8/15 м вокруг шестерни L4 (взгляд на неё), игрок у кольца.
# Движок (смоук, flag colwire) пишет AOLOG gate=маска ворот отсечения: 1 кандидат, 2 spDrawable, 4 сфера/пиксели, 8 охрана камеры, 16 куски нарисованы; 31 = рисуется.
#   tests/airbox1223/gate_grid.sh SET_DIR   (набор с EBOOT и данными L4)  -> RESULT OK, если во всех точках, кроме первого лога, gate=31
set -e; cd "$(dirname "$0")/../.."; SET=${1:?набор}; source .venv/bin/activate
SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_BALLAST=5 SMOKE_SCN=tests/airbox1223/gate_grid.txt SMOKE_MAXT=500 nice -n 20 tools/smoke.sh "$SET" > /dev/null 2>&1 || true
F=$(ls -d build/smoke/*/ | tail -1)smoke.txt
python3 - "$F" <<'P'
import sys,re,collections
rows=[]
for l in open(sys.argv[1],'rb'):
    l=l.decode('latin1')
    if 'AOLOG' in l and 'nVol' not in l:
        m=re.search(r'obj=(\d+) inst=\(-176\.4 6\.5 -301\.9\).*gate=(\d+) eye=\(([^)]*)\)',l)
        if m: rows.append((int(m.group(2)),m.group(3)))
rows=rows[1:]   # первый лог выбирает экземпляр, маска в нём ещё не писалась
c=collections.Counter(g for g,_ in rows); bad=[r for r in rows if r[0]!=31]
print('точек',len(rows),dict(c)); [print('дыра: глаз',e,'маска',g) for g,e in bad[:10]]
print('RESULT','OK' if rows and not bad else 'FAIL'); sys.exit(1 if (not rows or bad) else 0)
P
