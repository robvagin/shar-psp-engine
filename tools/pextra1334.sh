#!/bin/bash
# 1334 (данные): pextra.shr всех уровней и языков пересобирается экспортом с отдельной страницей значков состояния Mission Select (tools/menu_export.py, группа status).
#   tools/pextra1334.sh OUT     OUT/pextra.shr, OUT/l2..l7/, OUT/lang/{F,G,S}/ (то же дерево, что в наборе). MSSTATUS1334_OLD=1 — прежняя общая страница (красный контроль / откат данными)
set -eu; cd "$(dirname "$0")/.."; OUT=${1:?OUT}; rm -rf "$OUT"; mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
one() { L=$1; G=$2; [ "$G" = E ] && G=""; d=$OUT; [ -z "$G" ] || d=$d/lang/$G; [ "$L" = 1 ] || d=$d/l$L; mkdir -p "$d"; S=$(mktemp -d)
  UI_SCRATCH=$S LEVEL=$L LANG_BIBLE=${G:-E} nice .venv/bin/python tools/menu_export.py --pack pextra --out "$d/pextra.shr" > "$OUT/log_${G:-E}_$L.txt" 2>&1 || { echo "pextra L$L ${G:-E}: не собрался"; exit 1; }; rm -rf "$S"; }
export -f one; export OUT
for G in E F G S; do for L in 1 2 3 4 5 6 7; do echo "$L $G"; done; done | xargs -P 6 -L 1 bash -c 'one $0 $1' || { echo "pextra1334: сбой"; exit 1; }
echo "pextra1334: $(find "$OUT" -name pextra.shr | wc -l) файлов (ожидается 28)"
