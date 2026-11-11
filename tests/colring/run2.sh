#!/bin/bash
# Коллизия-96 этап 2 (Мак): настоящее кольцо (viewer/colring.c + col.c при COLRING_OLD=0) против прежнего COL1. tests/colring/run2.sh [LEVEL.shr ...] -> PASS/FAIL
#  0) COLRING_OLD=1 (умолчание): col.c этапа 2 и col.c до правки (94a70248) дают побитово одинаковые отпечатки на 1 млн запросов — прежнее поведение не тронуто
#  1) tools/col_ring_build.py (+ colbake.shr рядом с level.shr) строит colring.shr; --verify восстанавливает COL1 побайтно, хеши; mcHash сверяется с настоящим mcProbeHash
#  2) tests/colring/ring2_host: a — прежний COL1, b — кольцо; N запросов ground/ray/probe/push побитово; фокус идёт 0,7 м/кадр, телепорт раз в 600 кадров с colRingEnsure
#  3) в радиусе 200 м от фокуса дочитываний нет; ioErr=0; пул хватает (poolShort=0)
#  4) красные: r — синхронная страховка выключена («пусто» вне кольца) и c — испорченный байт резидентного блока: оба обязаны разойтись
cd "$(dirname "$0")/../.."; export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}
O=build/tmp/colring; mkdir -p $O/orig; N=${N:-1000000}; R=${R:-250}; BASE=94a70248
LS=$HOME/Downloads/shar-psp-integ/build/lo_set
LV=("$@"); [ ${#LV[@]} -eq 0 ] && LV=($LS/l4/level.shr)
rc=0
cc -std=gnu11 -O2 -w -DSKIN_HOST -DCOLRING_OLD=0 -DCOLRING_TEST_HOOKS -I viewer tests/colring/ring2_host.c viewer/col.c viewer/colring.c -lm -o $O/ring2_host || { echo "сборка: FAIL"; exit 1; }
git show $BASE:viewer/col.c > $O/orig/col.c; git show $BASE:viewer/col.h > $O/orig/col.h
cc -std=gnu11 -O2 -w -DSKIN_HOST -I $O/orig -I viewer tests/colring/stage1/ring_host.c $O/orig/col.c -lm -o $O/rh_orig || { echo "сборка orig: FAIL"; exit 1; }
cc -std=gnu11 -O2 -w -DSKIN_HOST -I viewer tests/colring/stage1/ring_host.c viewer/col.c -lm -o $O/rh_new || { echo "сборка new: FAIL"; exit 1; }
for L in "${LV[@]}"; do
  d=$(dirname "$L"); n=$(basename "$d"); F=$O/$n.colring; D=$O/$n.dig
  B=$O/$n.bake; tools/colbake.sh "$L" "$B" >/dev/null 2>&1 || { echo "$n: colbake.sh не отработал"; rc=1; continue; }   # свежий разбор текущего col.c (CB_REV=2: старые бейки в наборах не принимаются)
  $O/rh_orig a "$L" - $N 7 250 0 $D.o >/dev/null; $O/rh_new a "$L" - $N 7 250 0 $D.n >/dev/null
  cmp -s $D.o $D.n && echo "$n: COLRING_OLD=1 побитово равен прежнему col.c" || { echo "$n: FAIL COLRING_OLD=1 отличается от прежнего"; rc=1; }
  rm -f $D.o $D.n
  python3 tools/col_ring_build.py "$L" "$F" --bake "$B" | sed "s/^/$n: /" | cut -c1-260
  python3 tools/col_ring_build.py "$L" --verify "$F" | grep -q PASS || { echo "$n: verify FAIL"; rc=1; continue; }
  $O/ring2_host a "$L" - $N 1 $R $D "$B" >/dev/null || { echo "$n: эталон FAIL"; rc=1; continue; }
  out=$($O/ring2_host b "$L" "$F" $N 1 $R $D); echo "$out" | grep -E "^(RESULT|MEM)" | sed "s/^/$n: /" | cut -c1-330
  echo "$out" | tail -1 | grep -q PASS || { echo "$n: FAIL кольцо расходится с прежним"; rc=1; }
  near=$(echo "$out" | sed -n 's/.*<150\/<200\/<250\/<300\/<500\/далее: \([0-9]*\)\/\([0-9]*\)\/.*/\1 \2/p'); set -- $near
  [ "${1:-1}" = 0 ] && [ "${2:-1}" = 0 ] || { echo "$n: FAIL дочитывания ближе 200 м: $near"; rc=1; }
  echo "$out" | grep -q "ioErr=0 poolShort=0" || { echo "$n: FAIL ioErr/poolShort"; rc=1; }
  # стресс: 12 % запросов в случайных местах карты (30 за кадр): отпечатки обязаны совпасть и тут, пул может переполниться (куча), это не ошибка
  RING_FAR=120 $O/ring2_host a "$L" - $((N/4)) 3 $R $D "$B" >/dev/null; RING_FAR=120 $O/ring2_host b "$L" "$F" $((N/4)) 3 $R $D | tail -1 | grep -q PASS && echo "$n: стресс 12% дальних: совпало" || { echo "$n: FAIL стресс"; rc=1; }
  $O/ring2_host a "$L" - $N 1 $R $D "$B" >/dev/null
  for m in r c; do red=$($O/ring2_host $m "$L" "$F" $N 1 $R $D | tail -1); echo "$n: красный $m: $red"; echo "$red" | grep -q "RED ok" || rc=1; done
  rm -f $D $F $B
done
[ $rc -eq 0 ] && echo PASS || { echo FAIL; exit 1; }
