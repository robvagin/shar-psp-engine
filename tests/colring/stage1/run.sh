#!/bin/bash
# Коллизия-96 этап 1: тождество COL1-кольцом (игра не меняется). tests/colring/stage1/run.sh [LEVEL.shr ...] -> PASS/FAIL на каждый уровень
#  1) tools/col_ring_build_v1.py строит colring.shr; --verify восстанавливает COL1 побайтно (массивы по slot, cellStart, refs, списки страниц, crc); красный: испорченный байт -> FAIL
#  2) tests/colring/ring_host: эталон (прежний COL1) против col.c на копии с примитивами по местам и страницами под mprotect; вне кольца страницу дочитывает
#     обработчик (синхронная страховка). N запросов ground/ray/probe/push, отпечатки побитово; красный контроль (обработчик отдаёт мусор) обязан разойтись
#  3) в зоне до 200 м от фокуса дочитываний быть не должно (ящики, капсулы, заборы резидентны)
cd "$(dirname "$0")/../../.."; export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}
O=build/tmp/colring; mkdir -p $O; N=${N:-1000000}; R=${R:-250}
cc -std=gnu11 -O2 -w -DSKIN_HOST -I viewer tests/colring/stage1/ring_host.c viewer/col.c -lm -o $O/ring_host || { echo "сборка: FAIL"; exit 1; }
LV=("$@"); [ ${#LV[@]} -eq 0 ] && LV=($HOME/Downloads/shar-psp-integ/build/lo_set/l4/level.shr)
rc=0
for L in "${LV[@]}"; do
  n=$(basename "$(dirname "$L")"); F=$O/$n.colring; D=$O/$n.dig
  python3 tools/col_ring_build_v1.py "$L" "$F" >/dev/null || { echo "$n: сборка colring FAIL"; rc=1; continue; }
  python3 tools/col_ring_build_v1.py "$L" --verify "$F" | grep -q PASS || { echo "$n: verify FAIL"; rc=1; continue; }
  cp "$F" $O/bad.colring; printf '\xff' | dd of=$O/bad.colring bs=1 seek=40000 conv=notrunc 2>/dev/null
  python3 tools/col_ring_build_v1.py "$L" --verify $O/bad.colring | grep -q FAIL || { echo "$n: красный verify не покраснел"; rc=1; }
  nice -n 20 $O/ring_host a "$L" - $N 1 $R 1 $D >/dev/null
  out=$(nice -n 20 $O/ring_host b "$L" "$F" $N 1 $R 1 $D); echo "$out" | grep -E "RESULT|MEM итого" | sed "s/^/$n: /"
  m=$(echo "$out" | sed -n 's/.* mism=\([0-9]*\) .*/\1/p'); near=$(echo "$out" | sed -n 's/.*<150\/<200\/<250\/<300\/<500\/дальше: \([0-9]*\)\/\([0-9]*\)\/.*/\1 \2/p')
  [ "$m" = 0 ] || { echo "$n: FAIL расхождения $m"; rc=1; }
  set -- $near; [ "${1:-1}" = 0 ] && [ "${2:-1}" = 0 ] || { echo "$n: FAIL дочитывания ближе 200 м: $near"; rc=1; }
  red=$(nice -n 20 $O/ring_host r "$L" "$F" $N 1 $R 1 $D | tail -1); echo "$n: красный контроль: $red"; echo "$red" | grep -q "RED ok" || rc=1
  rm -f $D $O/bad.colring
done
[ $rc -eq 0 ] && echo PASS || { echo FAIL; exit 1; }
