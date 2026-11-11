#!/bin/bash
# Гейт «сырых» выделений (аудит Fable §7 п.13, спецификация Г.1 П4): новое место memalign/malloc/calloc/realloc с размером >= 16 КБ (или неизвестным) вне зонных аллокаторов
# (zone*.h, zoneslot*, cellpool.h, memwatch.c, функции lva*/zone*) = красный. Базовая линия: tools/rawalloc_baseline.txt (ключ файл:функция:вызов:размер, без номеров строк).
#   tools/rawalloc_gate.sh              проверка: новые места -> FAIL (код 1); места, ушедшие из линии, печатаются (обновить линию: --update)
#   tools/rawalloc_gate.sh --update     записать текущий список как базовую линию (после того как места осознанно ушли в зону или добавлены)
#   tools/rawalloc_gate.sh --selftest   красный и зелёный контроль на временной копии: новое сырое выделение обязано дать FAIL, такое же в zone*.h и маленькое (4 КБ) — PASS
# Шаг 7 плана (всё через zoneAlloc): число мест должно падать, линию тогда сжимают --update в том же коммите.
set -u
export LC_ALL=C   # comm требует один порядок сортировки у обоих файлов
W=$(cd "$(dirname "$0")/.." && pwd); BASE="$W/tools/rawalloc_baseline.txt"; S="$W/tools/rawalloc_scan.py"
gate() {   # $1 корень; $2 файл линии
  local cur; cur=$(mktemp); python3 -I "$S" "$1" > "$cur"
  local new gone; new=$(comm -13 "$2" "$cur"); gone=$(comm -23 "$2" "$cur")
  echo "rawalloc: мест $(wc -l < "$cur" | tr -d ' ') (базовая линия $(wc -l < "$2" | tr -d ' '))"
  [ -z "$gone" ] || echo "ушли из линии ($(echo "$gone" | wc -l | tr -d ' ')), можно сжать: tools/rawalloc_gate.sh --update"
  rm -f "$cur"
  if [ -n "$new" ]; then echo "НОВЫЕ сырые выделения (вынести в zoneAlloc/lva* или осознанно добавить в линию):"; echo "$new" | sed 's/^/  /'; return 1; fi
  return 0
}
case "${1:-}" in
  --update) python3 -I "$S" "$W" > "$BASE"; echo "rawalloc: линия обновлена, мест $(wc -l < "$BASE" | tr -d ' ')"; exit 0 ;;
  --selftest)
    T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; mkdir -p "$T/viewer"; cp "$W"/viewer/*.c "$W"/viewer/*.h "$T/viewer/" 2>/dev/null
    bad=0
    printf 'static void *zzRaw(unsigned n) { return memalign(64, n); }\n' > "$T/viewer/zz_selftest_new.h"
    gate "$T" "$BASE" > "$T/red.txt" 2>&1 && { echo "красный контроль НЕ покраснел (новое сырое memalign прошло)"; bad=1; } || echo "красный: новое сырое выделение поймано"
    rm -f "$T/viewer/zz_selftest_new.h"
    printf 'static void *zzOk(unsigned n) { return memalign(64, n); }\n' > "$T/viewer/zone_selftest.h"
    printf 'static void *zzSmall(void) { return malloc(4096); }\n' > "$T/viewer/zz_selftest_small.h"
    gate "$T" "$BASE" > "$T/green.txt" 2>&1 || { echo "зелёный контроль красный (zone*.h или малый размер не должны считаться)"; cat "$T/green.txt"; bad=1; }
    [ $bad -eq 0 ] && { echo PASS; exit 0; } || { echo FAIL; exit 1; } ;;
  "") gate "$W" "$BASE" && echo PASS || { echo FAIL; exit 1; } ;;
  *) echo "usage: rawalloc_gate.sh [--update|--selftest]"; exit 2 ;;
esac
