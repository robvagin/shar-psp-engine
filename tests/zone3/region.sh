#!/bin/bash
# Build 95 шаг 9в: tests/zone3/region.sh - хост-модель региона областей (viewer/zone3.h). Зелёное: new (L1->L7, куча 40 и 31 МБ) и busy PASS; красное: old и busyold обязаны дать FAIL.
H=$(cd "$(dirname "$0")" && pwd); export DEVELOPER_DIR=/Library/Developer/CommandLineTools
O=${OUT:-/tmp}/z3region.$$; cc -O1 -DZ3_HOST -o $O $H/region.c || exit 2
rc=0; for m in 40 31; do
  $O new $m | tail -1 | grep -q PASS && echo "new $m МБ: PASS" || { echo "new $m МБ: FAIL"; $O new $m; rc=1; }
  $O old $m | tail -1 | grep -q PASS && { echo "old $m МБ: красный контроль НЕ покраснел"; rc=1; } || echo "old $m МБ: красный (ожидается)"
done
$O busy | tail -1 | grep -q PASS && echo "busy: PASS" || { echo "busy: FAIL"; $O busy; rc=1; }
$O busyold | tail -1 | grep -q PASS && { echo "busyold: красный контроль НЕ покраснел"; rc=1; } || echo "busyold: красный (ожидается)"
$O tight | tail -1 | grep -q PASS && echo "tight: PASS" || { echo "tight: FAIL"; $O tight; rc=1; }
$O shift | tail -1 | grep -q PASS && echo "shift: PASS" || { echo "shift: FAIL"; $O shift; rc=1; }
cc -O1 -DZ3_HOST -DZONE3_SHIFT_OLD=1 -o $O.so $H/region.c || exit 2
$O.so shift | tail -1 | grep -q PASS && { echo "shiftold: красный контроль НЕ покраснел"; rc=1; } || echo "shiftold: красный (ожидается)"
rm -f $O.so
$O tightold | tail -1 | grep -q PASS && { echo "tightold: красный контроль НЕ покраснел"; rc=1; } || echo "tightold: красный (ожидается)"
rm -f $O; [ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
