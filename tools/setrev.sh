#!/bin/bash
# Штамп набора (TEST93, C1): пишет <папка>/set.txt — номер выпуска данных (viewer/datarev.h SHAR_DATA_REV), первые 8 знаков sha256 манифеста,
# дата, число уровней в наборе. Движок при старте сверяет номер (viewer/setcheck.h). Зовут tools/ship.sh и test93_set.sh.
#   tools/setrev.sh <папка набора> [файл манифеста] [уровни]
# Файл манифеста по умолчанию <папка>/manifest.txt (нет — manifest=none). Уровни: число 1..7 (по умолчанию — по набору: старший l<N>/level.shr в папке).
# Формат set.txt, ASCII: rev=93 / manifest=ab12cd34 / date=2026-11-07 / levels=7
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
D="${1:?папка набора}"; M="${2:-$D/manifest.txt}"; LV="${3:-}"
rev=$(sed -n 's/^#define SHAR_DATA_REV[[:space:]]*\([0-9][0-9]*\).*/\1/p' "$ROOT/viewer/datarev.h" | head -1)
[ -n "$rev" ] || { echo "setrev: нет SHAR_DATA_REV в viewer/datarev.h"; exit 1; }
if [ -f "$M" ]; then
  if command -v shasum >/dev/null 2>&1; then man=$(shasum -a 256 "$M" | cut -c1-8); else man=$(sha256sum "$M" | cut -c1-8); fi
else man=none; fi
if [ -z "$LV" ]; then   # по самому набору: старший уровень, у которого лежит l<N>/level.shr (пропуск посередине движок потом назовёт недостающим)
  LV=1; for n in 2 3 4 5 6 7; do [ -f "$D/l$n/level.shr" ] && LV=$n; done
fi
mkdir -p "$D"
printf 'rev=%s\nmanifest=%s\ndate=%s\nlevels=%s\n' "$rev" "$man" "$(date '+%Y-%m-%d')" "$LV" > "$D/set.txt"
echo "setrev: $D/set.txt rev=$rev manifest=$man levels=$LV"
