#!/bin/bash
# Правило ship_rules для flat912.bin одного уровня: tools/flat_list.py по level.shr и notoon.txt уровня.
#   tools/flat_rule.sh LEVEL_SHR OUT [PRIOR]   нет notoon.txt рядом с level.shr (его печёт notoon_rule.sh, на машине без texsrc его может не быть) — прежний flat912.bin
#   (PRIOR) остаётся как есть («оставлен»); нет и его — flat_list.py без списка (как корневое правило раньше), а не падение
set -eu
SHR="${1:?level.shr}"; OUT="${2:?выход}"; PRIOR="${3:-}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; D="$(cd "$(dirname "$SHR")" && pwd)"; NT="$D/notoon.txt"
if [ -s "$NT" ]; then python3 "$ROOT/tools/flat_list.py" "$SHR" "$NT" "$OUT"
elif [ -n "$PRIOR" ] && [ -s "$PRIOR" ]; then cp "$PRIOR" "$OUT"; echo "flat_rule: нет $NT, flat912.bin оставлен как есть ($PRIOR)"
else echo "flat_rule: нет $NT и нет прежнего flat912.bin, считаю без списка notoon"; python3 "$ROOT/tools/flat_list.py" "$SHR" "$OUT"; fi
