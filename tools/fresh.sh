#!/bin/bash
# Свежесть входов гейта (04.11, окно gates; арх-разбор К3 §2 п.8: ≈ 75 гейтов читают готовые build/* без проверки свежести — 9 ложных красных g04 от build/homer.shr 9 октября).
# Рядом с готовым файлом лежит ФАЙЛ.inputs.sha256: sha256 самого файла и каждого входа (исходники экспорта, скрипт-экспортёр). Гейт спрашивает, не устарел ли файл:
#   tools/fresh.sh stamp ФАЙЛ ВХОД...   записать штамп (после того как ФАЙЛ построен из этих входов)
#   tools/fresh.sh check ФАЙЛ [ВХОД...] проверить; без списка входов берёт список из штампа
# Коды: 0 свежий · 5 УСТАРЕЛ (вход изменился или сам файл подменён после штампа) · 6 штампа нет (возраст неизвестен) · 7 файла нет.
# Устаревший вход — это «НЕ СВИДЕТЕЛЬ», а не FAIL и не PASS: гейт пересобирает файл (если умеет) или выходит кодом 77 с этой строкой.
# Использование в гейте (образец tests/homer/host.sh):  tools/fresh.sh check build/homer.shr tools/homer_export.py || пересобрать && tools/fresh.sh stamp build/homer.shr tools/homer_export.py
set -u
cmd=${1:?stamp|check}; f=${2:?ФАЙЛ}; shift 2
st="$f.inputs.sha256"
h() { shasum -a 256 "$1" | cut -d' ' -f1; }
case "$cmd" in
  stamp)
    [ -f "$f" ] || { echo "fresh: нет файла $f"; exit 7; }
    { echo "# штамп свежести $(date '+%Y-%m-%d %H:%M'): $f построен из этих входов"; echo "$(h "$f")  @self"; for i in "$@"; do [ -f "$i" ] || { echo "fresh: нет входа $i" >&2; exit 7; }; echo "$(h "$i")  $i"; done; } > "$st"
    echo "fresh: штамп $st ($# входов)";;
  check)
    [ -f "$f" ] || { echo "fresh: нет файла $f"; exit 7; }
    [ -f "$st" ] || { echo "fresh: НЕ СВИДЕТЕЛЬ — у $f нет штампа $st (возраст неизвестен)"; exit 6; }
    bad=0
    while read -r sum name; do
      case "$sum" in '#'*|'') continue;; esac
      if [ "$name" = @self ]; then [ "$(h "$f")" = "$sum" ] || { echo "fresh: УСТАРЕЛ — сам $f изменён после штампа"; bad=1; }; continue; fi
      [ -f "$name" ] || { echo "fresh: УСТАРЕЛ — входа $name больше нет"; bad=1; continue; }
      [ "$(h "$name")" = "$sum" ] || { echo "fresh: УСТАРЕЛ — вход $name изменился с момента штампа"; bad=1; }
    done < "$st"
    for i in "$@"; do grep -q "  $i\$" "$st" || { echo "fresh: УСТАРЕЛ — вход $i в штампе не значится (список входов изменился)"; bad=1; }; done
    [ $bad = 0 ] && { echo "fresh: свежий ($f)"; exit 0; }; exit 5;;
  *) echo "fresh: stamp|check"; exit 64;;
esac
