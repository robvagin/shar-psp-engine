#!/bin/bash
# 108: добавить пустой файл-выключатель в оба набора и пересчитать манифест и set.txt (как в tools/test108_set.sh).  tools/set108_addfile.sh имя [имя...]
set -eu; cd "$(dirname "$0")/.."
for s in test108 test108tour; do D=build/$s/PSP/GAME/SHARTEST15; O=build/$s
  for n in "$@"; do : > $D/$n; echo "файл $n добавлен в набор (решение дирижёра)" >> $O/ЗАМЕНЫ.txt; done
  ( cd $D && find . -type f ! -name set.txt | sort | while read f; do printf "%s  %s\n" "$(shasum -a 256 "$f" | cut -d' ' -f1)" "$f"; done ) > $O/MANIFEST_SHA256.txt
  rm -f $D/set.txt; tools/setrev.sh $D $O/MANIFEST_SHA256.txt 7 >/dev/null; echo "$s: $(wc -l < $O/MANIFEST_SHA256.txt) файлов, set.txt $(head -1 $D/set.txt)"
done
