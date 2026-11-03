#!/bin/bash
# сверка g01-cars №1159: кэш матриц куска (carvis.c) — то же бит в бит, меньше расчётов. carvis.c целиком на Маке не собрать (pspgu), поэтому берём из него нужный кусок текста
cd "$(dirname "$0")/../.."; export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk} PATH=~/Downloads/shar-psp/build/xcshim/bin:$PATH
mkdir -p build/g1159c
python3 - <<'PY'
import re
s=open('viewer/carvis.c',encoding='utf-8').read()
a=s.index('static void jointXf'); b=s.index('int carVisJointWorld')
sw=re.search(r'#ifndef SV_CARCOST1159_OLD.*?#endif\n',s,re.S).group(0)
open('build/g1159c/cvpart.c','w',encoding='utf-8').write('#include "carvis.h"\n#include <math.h>\n#include <string.h>\n'+sw+'\n#ifndef clampf\nstatic float clampf(float x,float a,float b){return x<a?a:x>b?b:x;}\n#endif\n'+s[a:b])
PY
for k in new old; do F=""; [ $k = old ] && F="-DSV_CARCOST1159_OLD=1"
  cc -std=c11 -O2 -w -I viewer $F -DcarPieceMatrix=cvPM -c build/g1159c/cvpart.c -o build/g1159c/cv_$k.o || exit 1
  cc -std=c11 -O2 -w -I viewer tests/g01cars/cvcache.c build/g1159c/cv_$k.o viewer/car.c viewer/col.c -lm -o build/g1159c/t_$k 2>&1 | head -5
  echo "== $k"; build/g1159c/t_$k
done
