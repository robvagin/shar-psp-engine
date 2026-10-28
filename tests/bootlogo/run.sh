#!/bin/bash
# Гейт заставки GONE Makers на Маке (CPU, без эмулятора): правки Роба 125 (шрифт PSPSDK msx 8×8, 1 бит, сетка 60×34)
# и 127 (пиксельные знаки причастных, tools/bootmarks.py + viewer/bootmarks.h). Сначала КРАСНЫЙ — испорченный вход
# обязан упасть, потом ЗЕЛЁНЫЙ на настоящем. Итог: build/bootlogo/*.txt, код 0 только если всё так.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
OUT=build/bootlogo; mkdir -p "$OUT"; PY="$ROOT/.venv/bin/python"
red=0; nred=0; green=0; ngreen=0
r() { local tag="$1"; shift; nred=$((nred + 1))
  if "$@" > "$OUT/red_$tag.txt" 2>&1; then echo "КРАСНЫЙ НЕ ПОЙМАН ($tag): $*"
  else echo "красный ($tag): упал, как должен — $(grep -m1 -E 'RESULT|FAIL' "$OUT/red_$tag.txt" | cut -c1-90)"; red=$((red + 1)); fi; }
g() { local tag="$1"; shift; ngreen=$((ngreen + 1))
  if "$@" > "$OUT/green_$tag.txt" 2>&1; then echo "зелёный ($tag): $(grep -m1 -E 'RESULT|^OK' "$OUT/green_$tag.txt" | cut -c1-90)"; green=$((green + 1))
  else echo "НЕ ЗЕЛЁНЫЙ ($tag):"; tail -5 "$OUT/green_$tag.txt"; fi; }

# 125: копия шрифта = msx в libpspdebug.a; испорченный байт копии — красный
sed 's/^  0x00,/  0x01,/' viewer/bootmsx.h > "$OUT/bootmsx_bad.h"
r font_byte "$PY" tools/bootfont.py --check --out "$OUT/bootmsx_bad.h"
g font_copy "$PY" tools/bootfont.py --check
# 125: шаг клетки 6 (прежний) — знаки налезают, сетка не 60×34
r cw6 bash tests/bootlogo/host.sh -DBUI_CW=6
# 203: прежняя клетка 8 на любой знак (межбуквенный «очень большой») и знак слева — красные
r mono bash tests/bootlogo/host.sh -DBUI_MONO=1
r left bash tests/bootlogo/host.sh -DBUI_LOGO_LEFT
# 127: знаки — испорченный файл, размер вне слота, 5 серых, знак без исходника; слот на знаке GONE; перерисовка всех знаков
"$PY" tools/bootmarks.py > "$OUT/bootmarks_build.txt" 2>&1 || { echo "FAIL: сборка build/bootmarks.bin"; tail -3 "$OUT/bootmarks_build.txt"; exit 1; }
BOOTMARKS_PACKED_OLD=1 "$PY" tools/bootmarks.py --out "$OUT/bootmarks-old-bmk2.bin" > "$OUT/bootmarks_old_build.txt" 2>&1 || exit 1
"$PY" tests/bootlogo/badmarks.py build/bootmarks.bin "$OUT" > /dev/null
r mk_magic "$PY" tools/bootmarks.py --verify "$OUT/bad_magic.bin"
r mk_145   "$PY" tools/bootmarks.py --verify "$OUT/bad_145.bin"
r mk_color "$PY" tools/bootmarks.py --verify "$OUT/bad_color.bin"
r mk_bpp   "$PY" tools/bootmarks.py --verify "$OUT/bad_bpp.bin"
r mk_extra "$PY" tools/bootmarks.py --check "$OUT/bad_extra.bin"
g mk_verify "$PY" tools/bootmarks.py --verify
g mk_check  "$PY" tools/bootmarks.py --check
r mk1010_old env ELBARTO1010_OLD=1 "$PY" tools/bootmarks.py --check   # №1010: файл с родным тегом L6 ≠ сборке без него -> FAIL
r mk1010b_old env ELBARTO1010B_OLD=1 "$PY" tools/bootmarks.py --check   # №1010б: серый растянутый тег ≠ сборке «1:1 красным» -> FAIL
r packed_size_old "$PY" tests/bootlogo/compact_size.py "$OUT/bootmarks-old-bmk2.bin" "$OUT/bootmarks-old-bmk2.bin"
g packed_size "$PY" tests/bootlogo/compact_size.py build/bootmarks.bin "$OUT/bootmarks-old-bmk2.bin"
g short_bmk3 bash tests/bootlogo/short_bmk3.sh
r bmk3_old sh -c 'cc -std=c99 -O1 -I viewer -DBOOTMARKS_BMK3_OLD tests/bootlogo/host.c -o build/bootlogo-host-old && BOOTMARKS_FILE=build/bootmarks.bin build/bootlogo-host-old'
cc -std=c99 -O1 -I viewer tests/bootlogo/compact.c -o build/bootlogo-compact
build/bootlogo-compact "$OUT/bootmarks-old-bmk2.bin" build/bootmarks.bin > "$OUT/compact_exact.txt" 2>&1 || { cat "$OUT/compact_exact.txt"; exit 1; }
cat "$OUT/compact_exact.txt"
r small bash tests/bootlogo/host.sh -DBOOTANIM_SCALE=1 -DBOOTANIM_DOT_W=1 -DBOOTANIM_DOT_H=1   # правка 189: «больше GONE Makers» — знак 1:1 мелкий
r plant bash tests/bootlogo/host.sh -DBSQ_MARK_PLANT
r redraw bash tests/bootlogo/host.sh -DBOOTMARK_ALWAYS_REDRAW
g host bash tests/bootlogo/host.sh
# 257: «просто PSP (как на корпусе) и El Barto; вспомогательные — красиво, как спонсорские, аккуратно снизу логотипа» — данные и раскладка
r mk_257old env BMK_257_OLD=1 BMK_264_OLD=1 "$PY" tests/bootlogo/marks257.py
g mk_257 env BMK_264_OLD=1 "$PY" tests/bootlogo/marks257.py
# 264: «основные логотипы, системно; PSP убрать; в конце тег El Barto баллончиком» — набор и тег (режим 6, красный спрей с диска)
r mk_264old env BMK_264_OLD=1 "$PY" tests/bootlogo/marks264.py
g mk_264 "$PY" tests/bootlogo/marks264.py
r fill257_old sh -c "cc -std=c99 -O1 -Wall -I viewer -DBMK_257_OLD tests/bootlogo/fill257.c -o build/fill257_old && build/fill257_old"
g fill257 sh -c "cc -std=c99 -O1 -Wall -I viewer tests/bootlogo/fill257.c -o build/fill257 && build/fill257"

if [ $red -eq $nred ] && [ $green -eq $ngreen ]; then echo "PASS: красный $red/$nred пойман, зелёный $green/$ngreen"; exit 0; fi
echo "FAIL: красный $red/$nred, зелёный $green/$ngreen"; exit 1
