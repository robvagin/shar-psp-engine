#!/bin/bash
# 1340 (Роб, 108 на PSP: «у собираемых предметов миссии нет блестяшки при подборе, блинк-блинк, как у гаечного ключа»): предмет миссии без команды SetCollectibleEffect, но с рисунком,
# при подборе играет эффект миссии по умолчанию mission_col (оригинал: эффект по умолчанию при подборе; три квада свечения + частицы mission_col_psShape, 0,7 с).
#   tests/missions/cfxdefault1340.sh ПАКЕТ [RED]   — RED=1: flag cfxdefault1340old 1 (прежнее: без эффекта), гейт обязан упасть
# Случаи (ничего не говорят про эффект): доставка «собери предметы» L2 m5 (14 предметов); сброс L3 m5 («собери улики»: Снейк роняет предмет, гонимся — «msngo» к машине, потом goto на место выпадения, предмет 0).
# Порядок: сначала RED=1 (кадры в build/cfxdefault1340_red), потом обычный прогон — он сравнивает пиксели с красным. Запуск: flag msnmainat <миссия*100+этап>, ✕ Continue,
# «flag msngo» — игрок у ближайшего предмета, подбор по близости. Ждём «MSN cfx mission_col item=N … parts>=3» и квады (MSN cfx-sprites>0); снимки e_a/e_b сразу после подбора —
# голубого света больше, чем в красном прогоне.
cd "$(dirname "$0")/../.."; P=${1:?пакет}; RED=${RED:-0}; T=${TMPDIR:-/tmp}/cfx1340.$$; mkdir -p $T
O=build/cfxdefault1340; [ $RED = 1 ] && O=build/cfxdefault1340_red; mkdir -p $O
CASES="2:400:l2m5:go 3:400:l3m5:dump"
fail=0; red=0
for c in $CASES; do IFS=: read L M N K <<<"$c"
  S=$T/$N.scn; { echo "scene g1340"; echo "flag golevel $L"; echo "wait 30"; echo "flag msn"; echo "wait 2"; [ $RED = 1 ] && echo "flag cfxdefault1340old 1"
    echo "flag msnmainat $M"; echo "wait 4"; echo "press cross"; echo "wait 6"; echo "flag msngo"; [ $K = dump ] && { echo "wait 3.2"; echo "goto -230.6 168.7 1.5"; }; echo "wait 0.1"; echo "shot e_a"; echo "wait 0.1"; echo "shot e_b"; echo "wait 0.15"; echo "shot e_c"; echo "wait 1"; echo end; } > $S
  SHIP_LEVELS="2 3 4 5 6 7" SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN=$S SMOKE_MAXT=300 tools/smoke.sh "$P" >/dev/null 2>&1; D=$(ls -td build/smoke/2* | head -1)
  n=$(grep -a -c "^MSN cfx mission_col item=[0-9]* at=[^ ]* parts=[3-9]" $D/smoke.txt); sp=$(grep -a "^MSN cfx-sprites" $D/smoke.txt | tail -1 | sed 's/.*sprites \([0-9]*\).*/\1/')
  for f in e_a e_b e_c; do cp -c $D/$f.png $O/${N}_$f.png 2>/dev/null; done
  if [ $RED = 1 ]; then if [ "$n" != 0 ]; then echo "$N: эффект mission_col есть при выключателе (красный не пойман) n=$n"; red=1; else echo "$N: красный пойман, эффекта нет"; fi
  else if [ "$n" -ge 1 ] && [ "${sp:-0}" -gt 0 ]; then echo "$N: подбор с эффектом mission_col (строк $n, квадов $sp) — ок"; else echo "$N: эффекта нет (строк $n, квадов ${sp:-0})"; fail=1; fi; fi
done
rm -rf $T
if [ $RED = 1 ]; then [ $red = 0 ] && { echo "красный пойман на всех"; exit 0; } || { echo FAIL; exit 1; }; fi
if [ $fail = 0 ]; then python3 - "$O" <<'PY' || fail=1
import sys, numpy as np
from PIL import Image
from pathlib import Path
o=Path(sys.argv[1]); r=Path(str(o)+'_red')
for n in ('l2m5','l3m5'):
    best=0
    for f in ('e_a','e_b','e_c'):
        a=o/f'{n}_{f}.png'; b=r/f'{n}_{f}.png'
        if not (a.exists() and b.exists()): continue
        A=np.asarray(Image.open(a).convert('RGB'),np.int16); B=np.asarray(Image.open(b).convert('RGB'),np.int16)
        best=max(best,int(((A[...,2]-B[...,2])>30).sum()))
    print(f'{n}: пикселей голубее красного прогона: {best}')
    if best<150: print(f'{n}: кадр с блеском не отличается от красного'); sys.exit(1)
PY
fi
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
