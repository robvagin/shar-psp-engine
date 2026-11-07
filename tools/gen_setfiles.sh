#!/bin/bash
# Список обязательных файлов набора для viewer/setcheck.h (TEST93, C2) -> viewer/setfiles.inc.
# Источник — tools/ship_files.sh (то, что движок упоминает и чему есть правило выпуска) с SHIP_LEVELS="2 3 4 5 6 7", ОТФИЛЬТРОВАННЫЙ до того, без чего уровень
# не загружается (ядро: уровень, трафик, интерфейс; остальное движок обходит молча или читает после). Остальное — необязательно (движок молча обходится), в список не входит:
# ложная тревога хуже молчания (тур test92 собирается без level_toon.shr, winspr-l2/3/6 нет и в полном наборе).
# Правило CMake (viewer/CMakeLists.txt) перегенерирует файл, когда изменились tools/ship_rules.txt или этот скрипт. Запуск: tools/gen_setfiles.sh [выход]
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; OUT="${1:-$ROOT/viewer/setfiles.inc}"
CORE='^(l[2-7]/)?(level\.shr|traffic\.shr|ui\.shr)$'   # ключевые три на уровень: проход на PSP (карта памяти) ≤ 300 мс, 27 sceIoGetstat; полный перечень из 8 файлов стоил 56 мс в эмуляторе (≈ 360 мс на PSP)
TMP="$OUT.tmp.$$"
if [ -n "${SETFILES_LIST:-}" ]; then cat "$SETFILES_LIST"; else SHIP_LEVELS="2 3 4 5 6 7" "$ROOT/tools/ship_files.sh"; fi | grep -E "$CORE" | sort -u > "$TMP.list"
n=$(wc -l < "$TMP.list" | tr -d ' ')
[ "$n" -ge 7 ] || { echo "gen_setfiles: в списке только $n имён, ожидалось 21 (3 на уровень × 7) — не пишу"; rm -f "$TMP.list"; exit 1; }
{ echo "// Сгенерировано tools/gen_setfiles.sh — не править руками. Обязательные файлы набора, $n имён (уровень 1 — в корне, 2–7 — в l<N>/)."
  echo "static const char *const setFiles[] = {"
  sed 's/.*/  "&",/' "$TMP.list"
  echo "};"
  echo "#define SETFILES_N $n"; } > "$TMP"
rm -f "$TMP.list"
if [ -f "$OUT" ] && cmp -s "$TMP" "$OUT"; then rm -f "$TMP"; touch "$OUT"; else mv "$TMP" "$OUT"; fi   # touch: правило CMake не гоняет скрипт при каждой сборке
echo "gen_setfiles: $OUT ($n имён)"
