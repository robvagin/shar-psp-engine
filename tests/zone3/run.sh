#!/bin/bash
# П5: tests/zone3/run.sh [seed...] — хост-проверка zone3.h на Маке. Зелёное: p1 и new PASS; красное: old обязан дать FAIL (отказы входа в «куче без областей»).
H=$(cd "$(dirname "$0")" && pwd); export DEVELOPER_DIR=/Library/Developer/CommandLineTools
O=${OUT:-/tmp}/z3host.$$; mkdir -p $(dirname $O); cc -O1 -DZ3_HOST -o $O $H/host.c -lm || exit 2
rc=0; for s in ${@:-1 2 3 4 5}; do
  $O $s p1 | tail -1 | grep -q PASS || { echo "seed $s p1: FAIL"; rc=1; }
  $O $s new > $O.new.txt; tail -1 $O.new.txt | grep -q PASS && echo "seed $s new: PASS $(grep '^new:' $O.new.txt)" || { echo "seed $s new: FAIL"; cat $O.new.txt; rc=1; }
  $O $s old > $O.old.txt; tail -1 $O.old.txt | grep -q PASS && { echo "seed $s old: красный контроль НЕ покраснел"; rc=1; } || echo "seed $s old: красный (как и ожидается) $(grep '^old:' $O.old.txt)"
done
rm -f $O $O.*.txt; [ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
