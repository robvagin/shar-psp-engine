#!/bin/bash
# Гейт выпуска: colbake.shr набора = живой разбор ТЕКУЩЕГО col.c, по всем 7 уровням. tools/colbake_gate.sh НАБОР [N ...]   -> строка на уровень, последняя PASS/FAIL (код 1 при расхождении)
#   НАБОР — папка с level.shr и colbake.shr (L1 в корне, L2..L7 в l2..l7/). Ищет устаревший бейк: ключ файла — хеш сырого COL1, версии логики разбора в нём нет (CB_REV, viewer/col.c).
#   Живой разбор строится tools/colbake.sh (хост, секунды). Красный на test96: L1, L2, L4, L5, L7 (набор t93/build/test96tour, 08.11).
set -u
cd "$(dirname "$0")/.."; export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}
SET=${1:?набор (папка с level.shr, colbake.shr, l2..l7/)}; shift; LV=${*:-1 2 3 4 5 6 7}
T=$(mktemp -d "${TMPDIR:-/tmp}/cbgate.XXXXXX"); bad=0
for n in $LV; do
  if [ "$n" = 1 ]; then L=$SET/level.shr; B=$SET/colbake.shr; else L=$SET/l$n/level.shr; B=$SET/l$n/colbake.shr; fi
  [ -f "$L" ] || { echo "L$n: нет level.shr — пропуск"; continue; }
  [ -f "$B" ] || { echo "L$n: КРАСНЫЙ нет colbake.shr (движок считает сам, это допустимо, но выпуск должен его печь)"; bad=1; continue; }
  tools/colbake.sh "$L" "$T/b$n.shr" > /dev/null 2>&1 || { echo "L$n: colbake.sh не отработал"; bad=1; continue; }
  if cmp -s "$T/b$n.shr" "$B"; then echo "L$n: OK  бейк набора = живой разбор ($(stat -f%z "$B") Б)"
  else echo "L$n: КРАСНЫЙ  бейк набора ($(stat -f%z "$B") Б) != живой разбор ($(stat -f%z "$T/b$n.shr") Б): перепечь tools/colbake.sh $L $B"; bad=1; fi
done
find "$T" -type f -delete; rmdir "$T"
[ $bad -eq 0 ] && echo PASS || { echo FAIL; exit 1; }
