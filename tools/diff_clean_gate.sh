#!/bin/bash
# Гейт «чистота диффа» свода (план упаковки Fable §5.3): в изменениях против базы нет объектных файлов, модулей, EBOOT, образов, ссылок на папки сборки и ресурсов игры.
#   tools/diff_clean_gate.sh [БАЗА=rel/test107]   -> OK / FAIL (код 1)
set -u; cd "$(dirname "$0")/.."; BASE=${1:-rel/test107}; BAD=0
FILES=$(git diff --name-only --diff-filter=AMR "$BASE"...HEAD)
check() { local n; n=$(echo "$FILES" | grep -E "$1" | head -5 | tr '\n' ' '); [ -z "$n" ] || { echo "FAIL $2: $n"; BAD=1; }; }
check '\.(o|obj|a|elf|prx|PBP|iso|cso|bin\.o)$' "объектные файлы, модули, EBOOT"
check '^(build[^/]*|viewer/build[^/]*|tests/[^/]+/build[^/]*)/' "папки сборки"
check '\.(shr|smv|sab|sap|p3d|rcf|rmv)$' "ресурсы игры"
# символические ссылки в изменённом
for f in $FILES; do [ -L "$f" ] && { echo "FAIL символическая ссылка: $f"; BAD=1; }; done
# режим 120000 в индексе
git diff --raw "$BASE"...HEAD | awk '$2 ~ /120000|^:120000/ || $1 ~ /120000/ {print "FAIL ссылка в дереве:", $NF}' | head -3
[ $BAD = 0 ] && echo "OK: дифф против $BASE чист ($(echo "$FILES" | wc -l | tr -d ' ') файлов)" || exit 1
