#!/bin/bash
# Сборка SHAR для PSP из образа своего диска, по шагам, с прогрессом для страницы-сборщика (app/server.py).
#   app/core/build.sh ОБРАЗ.iso
# Строки «@@ {json}» — прогресс: step, total, title, pct (0–100 внутри всей сборки). Остальное — журнал как есть.
# Повторный запуск продолжает с места остановки: setup и распаковка пропускаются, если уже сделаны, tools/ship.sh берёт готовые
# данные из кеша по хешам (№1162). Результат — build/ (EBOOT.PBP и данные, манифест build/ship_manifest.txt).
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
ISO="${1:?образ диска .iso}"
TOTAL=5
say() { printf '@@ {"step":%d,"total":%d,"title":"%s","pct":%s}\n' "$1" "$TOTAL" "$2" "$3"; }
fail() { printf '@@ {"error":"%s"}\n' "$1"; exit 1; }

# macOS: компилятору нужен путь к системным заголовкам (SDK). Обычно его даёт xcrun; Xcode с непринятой лицензией или cc не из /usr/bin — берём SDK инструментов разработчика
if [ "$(uname -s)" = Darwin ] && [ -z "${SDKROOT:-}" ]; then
  SDKROOT="$(xcrun --show-sdk-path 2>/dev/null || true)"; [ -d "$SDKROOT" ] || SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk; export SDKROOT
fi
# 1. инструменты (PSPDEV, Python-окружение, разборщик диска, donut); уже есть — пропуск
say 1 "Setting up the tools" 1
if [ ! -x pspdev/bin/psp-gcc ] || [ ! -x .venv/bin/python ] || [ ! -d hit-and-run-web/tools ] || [ ! -d donut ]; then
  ./setup.sh || fail "setup.sh stopped, see the log"
fi
export PSPDEV="$ROOT/pspdev" PATH="$ROOT/pspdev/bin:$PATH"
. .venv/bin/activate

# 2. движок: та сборка, что идёт на PSP-2000 (README «The exact build»); ship.sh дальше только достраивает
say 2 "Building the engine" 4
if [ ! -f viewer/build/Makefile ]; then
  (cd viewer && psp-cmake -S . -B build -DSHAR_RELEASE=ON -DSHAR_RELEASE_KEEP_DIAG=ON -DSHAR_RELEASE_KEEP_HUD=ON \
    -DCMAKE_C_FLAGS="-DJITTER_STRIPS_OLD=1 -DJITTER_CAR_OLD=1 -DFE_DEMO_PS2_OLD=1 -DZF932_OLD=1 -DCARFILL930_OLD=1 -DGAME_C7_TALKHIDE_OLD=1") \
    || fail "the engine did not configure"
fi

# 3. диск: распаковка в hit-and-run-web/source (уже распакован этот же образ — пропуск)
say 3 "Unpacking the disc" 7
STAMP=build/app/disc.stamp; mkdir -p build/app
ID="$(python3 app/core/iso_id.py "$ISO")"; echo "$ID"
case "$ID" in *'"ok": true'*) ;; *) fail "this is not The Simpsons: Hit & Run";; esac
SIG="$(printf '%s %s' "$(wc -c < "$ISO" | tr -d ' ')" "$ID")"
if [ ! -f hit-and-run-web/source/disc/SYSTEM.CNF ] || [ "$(cat "$STAMP" 2>/dev/null)" != "$SIG" ]; then
  python3 hit-and-run-web/tools/extract.py --iso "$ISO" || fail "the disc did not unpack"
  printf '%s' "$SIG" > "$STAMP"
fi

# 4. уровни, ролики, звук, интерфейс: tools/ship.sh; прогресс — по номеру правила данных («@rule i/N имя», SHIP_PROGRESS=1)
export SHIP_LEVELS="2 3 4 5 6 7" SHIP_PROGRESS=1
say 4 "Building the levels" 12
tools/ship.sh 2>&1 | while IFS= read -r l; do
  case "$l" in
    "@rule "*) r=${l#@rule }; i=${r%%/*}; r=${r#*/}; n=${r%% *}; name=${r#* }
      p=$((14 + i * 80 / n)); case "$name" in l[2-4]/*) t="Building levels 2 to 4";; l[5-7]/*) t="Building levels 5 to 7";; movies/*|*.rmv|*.pmf) t="Converting the movies";; *) t="Building level 1";; esac
      say 4 "$t" $p; continue ;;
    "· EBOOT собран"*) say 4 "Building level 1" 13 ;;
    "· установлено в build/"*) say 5 "Checking the build" 98 ;;
    "ship: "*) printf '@@ {"error":"%s"}\n' "$(printf '%s' "${l#ship: }" | tr '"\\' "''")" ;;
  esac
  echo "$l"
done
[ "${PIPESTATUS[0]}" = 0 ] || exit 1
[ -f build/ship_manifest.txt ] || fail "the build finished without a manifest"
say 5 "Ready" 100
