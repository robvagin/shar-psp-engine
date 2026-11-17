#!/bin/bash
# build/test108n и build/test108ntour: дописать строки в rollback.txt (имя_OLD, по одной) и пересчитать манифест и set.txt.  tools/set108n_addrollback.sh [--game-only|--tour-only] ИМЯ_OLD [ИМЯ_OLD ...]
set -eu; cd "$(dirname "$0")/.."
W="both"; case "${1:-}" in --game-only) W=game; shift;; --tour-only) W=tour; shift;; esac
for s in test108n test108ntour; do
  [ "$W" = both ] || { [ "$W" = game ] && [ "$s" = test108ntour ] && continue; [ "$W" = tour ] && [ "$s" = test108n ] && continue; }
  D=build/$s/PSP/GAME/SHARTEST15; O=build/$s
  for n in "$@"; do grep -qx "$n" $D/rollback.txt 2>/dev/null || echo "$n" >> $D/rollback.txt; echo "rollback.txt: $n (решение дирижёра)" >> $O/ЗАМЕНЫ.txt; done
  ( cd $D && find . -type f ! -name set.txt | sort | while read f; do printf "%s  %s\n" "$(shasum -a 256 "$f" | cut -d' ' -f1)" "$f"; done ) > $O/MANIFEST_SHA256.txt
  rm -f $D/set.txt; tools/setrev.sh $D $O/MANIFEST_SHA256.txt 7 >/dev/null; echo "$s: rollback.txt [$(tr '\n' ' ' < $D/rollback.txt)], $(wc -l < $O/MANIFEST_SHA256.txt) файлов, set.txt $(head -1 $D/set.txt)"
done
