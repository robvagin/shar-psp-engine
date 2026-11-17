#!/bin/bash
# 108 О3 шаг 2: «дальняя ли клетка» кэшем по клетке вместо чтения куска. tests/city109/run.sh EBOOT_ТУРА [SET] -> PASS/FAIL
# Сцена: L1, езда 40 с. chk (flag city109chk 1): каждый кусок считается двумя способами — расхождений 0; красный (city109break): расхождений > 0; цена: city109 0/1, цикл dlCity мкс.
cd "$(dirname "$0")/../.."; EB=${1:?EBOOT}; export SET=${2:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=1 OUT=${OUT:-$PWD/build/city109}; mkdir -p "$OUT"
for n in chk red on off; do FLAGS="" nice -n 20 tools/emu_run.sh tests/city109/c_$n.txt $n "$EB" 200 > "$OUT/$n.log" 2>&1; done
python3 -I - "$OUT" <<'PY'
import re,sys,statistics as st
o=sys.argv[1]; ok=True
def last(n):
    T=open('%s/out_%s/smoke.txt'%(o,n),errors='replace').read().splitlines()
    d=[l for l in T if l.startswith('DCITY9')]; m=re.search(r'chk=(\d+) bad=(\d+)',d[-1]); lp=[int(re.search(r'loop_us=(\d+)',l).group(1)) for l in d if 'frames=60' in l]
    return int(m.group(1)),int(m.group(2)),st.median(lp)
cn,cb,_=last('chk'); rn,rb,_=last('red'); _,_,lon=last('on'); _,_,loff=last('off')
print('сверка: проверено %d кусков, расхождений %d; красный (city109break): расхождений %d; цикл dlCity мкс: новое %d, прежнее %d (Мак)'%(cn,cb,rb,lon,loff))
if cn<1000 or cb!=0: print('FAIL: сверка не прошла'); ok=False
if rb<100: print('КРАСНЫЙ НЕ ПОЙМАН'); ok=False
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
PY
