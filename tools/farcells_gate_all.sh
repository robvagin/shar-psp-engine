#!/bin/bash
# Гейт картинки «дальних клеток» на 7 уровнях через переключение в игре, 2 эмулятора (цепочки L1→L3→L5→L7 и L2→L4→L6).
# tools/farcells_gate_all.sh НАБОР_ЛЕСТНИЦЫ DIR_farcells (например ~/Downloads/shar-psp-mod96/build/farcells/m53) [OUT=build/farcells_<имя DIR>]
# Точки через foot + footfsm 1 на всех уровнях (FARC_FOOT_ALL=1); итог по уровню — OUT/L<N>/summary.txt, сводка — OUT/ALL.txt
set -u; cd "$(dirname "$0")/.."; SET="$1"; FD="$2"; OUT="${3:-build/farcells_$(basename "$FD")}"; mkdir -p "$OUT"
PTS="${PTS:-$HOME/Downloads/shar-psp-t93/docs/state/viewpoints_93.json}"
chain() { for L in "$@"; do FARC_FOOT_ALL=1 .venv/bin/python tools/farcells_gate.py $L --full "$SET" --points "$PTS" --ingame "$FD/l$L/farcells.shr" --out "$OUT/L$L" > "$OUT/L$L.log" 2>&1; echo "L$L exit $?" >> "$OUT/ALL.txt"; done; }
rm -f "$OUT/ALL.txt"
chain 1 3 5 7 & chain 2 4 6 & wait
echo ALL_DONE >> "$OUT/ALL.txt"
