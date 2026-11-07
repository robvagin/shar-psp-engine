#!/bin/bash
# Какие данные нужны движку рядом с EBOOT: имена из tools/ship_rules.txt, которые упомянуты ("имя") в исходниках
# движка (файлы из add_executable в viewer/CMakeLists.txt + их заголовки).
#   tools/ship_files.sh          -> имена, по одному в строке
#   tools/ship_files.sh --rules  -> строки правил целиком
#   tools/ship_files.sh --pairs  -> «имя|где лежит» (tools/smoke.sh: набор из build/, если в папке прогона его нет)
# Уровни 2–7 (29.09): строка правила с {N} — по строке на каждый уровень из SHIP_LEVELS (по умолчанию «2 3 4 5 6 7»), {NN} —
# номер двумя цифрами. Имя набора уровня l<N>/... едет, если движок знает папку уровня ("l%d/%s", viewer/main.c levelFile)
# и упоминает имя без папки (как у уровня 1: "level.shr", "collect-l%d.shr", "level%02d.msn", "%smusic.sap").
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRCS=$(sed -n 's/.*add_executable([^ ]* \(.*\))/\1/p' "$ROOT/viewer/CMakeLists.txt")
FILES=""; for s in $SRCS; do FILES="$FILES $ROOT/viewer/$s ${ROOT}/viewer/${s%.c}.h"; done
# main.c assembly modules contain the same original file references.
if [ "${MAIN_FILES_OLD:-0}" != 1 ]; then FILES="$FILES $ROOT/viewer/main_*.h"; fi
# №1162: имя файла, которое упоминает заголовок без одноимённого .c (bootscreen.h: bootvo.bin; carfill930.h: carflat930.bin; spritelight.h: phonespr.bin,
# wrenchspr.bin, wrglow.bin; coinspr.h: coinspr.bin), раньше не находилось, и публичный путь (tools/ship.sh) не собирал эти файлы; в нашем наборе они
# были, их клали наложением. Теперь просматриваются все заголовки viewer/. SHIPFILES1162_OLD=1 — прежний перечень
if [ "${SHIPFILES1162_OLD:-0}" != 1 ]; then FILES="$FILES $ROOT/viewer/*.h"; fi
LEVELS="${SHIP_LEVELS-}"   # Роб 01.10: уровни 2–7 не собирать, пока он не принял L1 (вернуть: SHIP_LEVELS="2 3 4 5 6 7")
LVDIR=0; cat $FILES 2>/dev/null | grep -q '"l%d/%s"' && LVDIR=1
expand() {   # строки с {N} -> по уровню
  while IFS= read -r line; do
    case "$line" in
      *'{N}'*) for n in $LEVELS; do nn=$(printf '%02d' "$n"); l=${line//\{NN\}/$nn}; printf '%s\n' "${l//\{N\}/$n}"; done ;;
      *) printf '%s\n' "$line" ;;
    esac
  done
}
grep -v '^\s*#' "$ROOT/tools/ship_rules.txt" | grep '|' | expand | while IFS= read -r line; do
  name=$(echo "${line%%|*}" | tr -d ' ')
  rest=$name
  case "$name" in l[0-9]/*) [ $LVDIR = 1 ] || continue; rest=${name#l[0-9]/} ;; esac   # набор уровня: папка l<N>/ — levelFile
  base=${rest##*/}   # звук строит путь из префикса каталога: "%ssfx.sab"
  # номер уровня в имени движок подставляет сам (viewer/main.c levelFile: "collect-l%d.shr", msn_view.c "level%02d.msn"):
  # цифры имени правила совпадают и с %d / %02d в исходнике
  pat=$(printf '%s' "$rest" | sed -e 's/\./\\./g' -e 's/[0-9][0-9]*/([0-9]+|%0?[0-9]*d)/g')   # имена: [a-z0-9._/-]
  # перепись миссий 04.10: имя целиком с папкой уровня ("l%d/racefence-l%d.bin" в viewer/msn_view.c — движок строит путь сам,
  # без levelFile) и файл папки по шаблону движка ("movies/%s.smv": ролик гэга movies/tele.smv, ролики целей fmvN)
  full=$(printf '%s' "$name" | sed -e 's/\./\\./g' -e 's/[0-9][0-9]*/([0-9]+|%0?[0-9]*d)/g')
  tmpl="@@none@@"; case "$rest" in */*) tmpl=$(printf '%s' "${rest%/*}/%s.${rest##*.}" | sed -e 's/\./\\./g') ;; esac
  if cat $FILES 2>/dev/null | grep -qE "\"$pat\"|%s$base\"|\"$full\"|\"$tmpl\""; then
    case "${1:-}" in
      --rules) echo "$line" ;;
      --pairs) path=$(echo "$line" | cut -d'|' -f2 | tr -d ' '); echo "$name|$path" ;;
      *) echo "$name" ;;
    esac
  fi
done
