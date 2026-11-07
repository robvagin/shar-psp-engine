#!/bin/bash
# ТЕСТ90 (g01-cars): кэш общей части матриц кусков машины — бит в бит и меньше расчётов; красный контроль: -DCARMTX_OLD=1 (расчёт на каждый кусок) обязан падать на «меньше расчётов»
cd "$(dirname "$0")/../.."; export SDKROOT=${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX15.sdk} PATH=~/Downloads/shar-psp/build/xcshim/bin:$PATH
mkdir -p build/g1190
python3 - <<'PY'
import re
s=open('viewer/carvis.c',encoding='utf-8').read()
a=s.index('static void jointXf'); b=s.index('int carVisJointWorld')
sw=re.search(r'#ifndef SV_CARCOST1159_OLD.*?#endif\n',s,re.S).group(0)
open('build/g1190/cvpart.c','w',encoding='utf-8').write('#include "carvis.h"\n#include <math.h>\n#include <string.h>\n'+sw+'\n#ifndef clampf\nstatic float clampf(float x,float a,float b){return x<a?a:x>b?b:x;}\n#endif\n'+s[a:b])
PY
rc=0
for k in new old; do F=""; [ $k = old ] && F="-DCARMTX_OLD=1"
  cc -std=c11 -O2 -w -I viewer $F -c build/g1190/cvpart.c -o build/g1190/cv_$k.o || exit 1
  cc -std=c11 -O2 -w -I viewer tests/g01cars/carmtx.c build/g1190/cv_$k.o viewer/car.c viewer/col.c -lm -o build/g1190/t_$k 2>&1 | head -5
  echo "== $k"; build/g1190/t_$k; r=$?
  if [ $k = new ]; then [ $r = 0 ] && echo "PASS: новое — бит в бит, меньше расчётов" || { echo "FAIL: новое"; rc=1; }
  else [ $r != 0 ] && echo "красный контроль пойман (CARMTX_OLD=1 падает)" || { echo "FAIL: красный не пойман"; rc=1; }; fi
done; exit $rc
