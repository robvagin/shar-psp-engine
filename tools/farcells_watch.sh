#!/bin/bash
# Гейт картинки дальних клеток: по каждому уровню стартует, как только в DIR_farcells появился l<N>/farcells.shr с неизменным размером 20 с; все уровни параллельно (по эмулятору на уровень).
# tools/farcells_watch.sh НАБОР_ЛЕСТНИЦЫ DIR_farcells [OUT]; итог по уровню — строка "L<N> PASS|FAIL" в OUT/RESULT.txt, потом ALL_DONE
set -u; cd "$(dirname "$0")/.."; SET="$1"; FD="$2"; OUT="${3:-build/farcells_$(basename "$FD")}"; mkdir -p "$OUT"; rm -f "$OUT/RESULT.txt"
PTS="${PTS:-$HOME/Downloads/shar-psp-t93/docs/state/viewpoints_93.json}"
one() { L=$1; F="$FD/l$L/farcells.shr"; last=-1
  while true; do [ -f "$F" ] && { sz=$(stat -f %z "$F"); if [ "$sz" = "$last" ]; then break; fi; last=$sz; sleep 20; continue; }; sleep 20; done
  echo "L$L старт $(date +%H:%M)" >> "$OUT/RESULT.txt"
  FARC_FOOT_ALL=1 .venv/bin/python tools/farcells_gate.py $L --full "$SET" --points "$PTS" --ingame "$F" --out "$OUT/L$L" > "$OUT/L$L.log" 2>&1
  echo "L$L $(grep -h 'ИТОГ' "$OUT/L$L/summary.txt" 2>/dev/null | tail -1 | sed 's/.*: //') (exit $?)" >> "$OUT/RESULT.txt"; }
for L in 1 2 3 4 5 6 7; do one $L & done; wait; echo ALL_DONE >> "$OUT/RESULT.txt"
