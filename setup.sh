#!/usr/bin/env bash
# Ставит PSPDEV v20260601 в ./pspdev и нужные утилиты для распаковки PS2-диска.
set -e
cd "$(dirname "$0")"
V=v20260601; B=https://github.com/pspdev/pspdev/releases/download/$V
case "$(uname -s)-$(uname -m)" in
  Darwin-arm64)  F=pspdev-macos-latest-arm64.tar.gz ;;
  Darwin-x86_64) F=pspdev-macos-15-intel-x86_64.tar.gz ;;
  Linux-x86_64)  F=pspdev-ubuntu-latest-x86_64.tar.gz ;;
  Linux-aarch64) F=pspdev-ubuntu-24.04-arm-arm64.tar.gz ;;
  *) echo "Windows: используй https://github.com/dmang-dev/pspdev-win"; exit 1 ;;
esac
[ -d pspdev ] || { echo "Качаю $F…"; curl -L "$B/$F" | tar xz; }
# №880 (сборка с чистого клона): компилятор PSPDEV для macOS связан с gmp, libmpc, mpfr, zstd, libusb из Homebrew — без них psp-gcc падает
# (dyld: libmpc.3.dylib); ffmpeg нужен с декодером IPU (ролики), пути — $FFMPEG или PATH
if [ "$(uname -s)" = Darwin ]; then brew install cmake sevenzip python@3.11 gmp libmpc mpfr zstd libusb ffmpeg libsndfile || true
else
  # Linux (Ubuntu/Debian; доработка noname8723, 06.11): пакеты для распаковки диска, сборки C-инструментов (build-essential: cc для tools/pvs, wind
  # и общих библиотек) и библиотеки готового PSPDEV. Без sudo (root, контейнер) — напрямую. 7zip называется «7zip» (Ubuntu 22.10+) или «p7zip-full»
  SUDO=""; [ "$(id -u)" = 0 ] || SUDO="sudo"
  if command -v apt-get >/dev/null 2>&1; then
    $SUDO apt-get update -y || true
    P7=7zip; apt-cache show 7zip >/dev/null 2>&1 || P7=p7zip-full
    $SUDO apt-get install -y build-essential cmake git curl ca-certificates "$P7" python3-pip python3-venv ffmpeg libsndfile1-dev || true
    # psp-gcc/cc1 — gmp, mpc, mpfr, zstd; остальные утилиты PSPDEV — libusb, readline, ncurses/tinfo, udev, expat, lzma, libcap. По одной:
    # на Ubuntu 24.04 libreadline8 называется libreadline8t64, и один неизвестный пакет не должен ронять остальные
    for pkg in libgmp10 libmpc3 libmpfr6 libzstd1 libusb-1.0-0 libudev1 libncurses6 libtinfo6 libexpat1 liblzma5 libcap2 zlib1g libreadline8t64 libreadline8; do
      $SUDO apt-get install -y "$pkg" >/dev/null 2>&1 || true
    done
  else echo "Linux без apt: поставь вручную cmake, 7z, ffmpeg, python3-venv, build-essential, git"; fi
fi
# своё окружение Python ≥ 3.11 (.venv): его включают build_level.sh, ship.sh, build_all_levels.sh
PY=$(command -v python3.11 || command -v python3.12 || command -v python3.13 || command -v python3)
"$PY" -c 'import sys; sys.exit(sys.version_info < (3, 11))' || { echo "нужен Python 3.11 или новее (Ubuntu 22.04: sudo add-apt-repository ppa:deadsnakes/ppa && sudo apt install python3.11 python3.11-venv; в Ubuntu 24.04 уже 3.12)"; exit 1; }
[ -d .venv ] || "$PY" -m venv .venv
# №1162: разборщик диска (MIT) ставится сам, чтобы публичный путь не зависел от того, что лежало на машине автора
[ -d hit-and-run-web/tools ] || { mkdir -p hit-and-run-web && git clone --depth 1 https://github.com/Vheissu/hit-and-run-web.git hit-and-run-web.clone && cp -R hit-and-run-web.clone/. hit-and-run-web/ && rm -rf hit-and-run-web.clone; } || echo "hit-and-run-web не скачался: склонируй https://github.com/Vheissu/hit-and-run-web в папку hit-and-run-web"
.venv/bin/python -m pip install -r hit-and-run-web/requirements.txt fast_simplification meshoptimizer segno
# №1162: наша правка разборщика диска (прозрачность вершин PS2); ship.sh и build_level.sh накладывают её сами, здесь — чтобы была видна сразу
[ -d hit-and-run-web/tools ] && .venv/bin/python tools/hrw_patches/apply.py
# №1162: кодировщик ATRAC3 (открытый atracdenc, LGPL) для музыки значка игры в XMB; ту же версию собирает и автор (SND0.AT3 должен выйти тем же)
if [ ! -x build/tools/atracdenc/build/src/atracdenc ]; then
  mkdir -p build/tools
  { [ -d build/tools/atracdenc ] || git clone https://github.com/dcherednik/atracdenc build/tools/atracdenc; } \
    && git -C build/tools/atracdenc checkout -q cb170709f836cb43dc0312455188c791159d81dc \
    && git -C build/tools/atracdenc submodule update --init --recursive \
    && { [ "$(uname -s)" != Darwin ] || sed -i.orig "s/set(CMAKE_OSX_ARCHITECTURES \"arm64;x86_64\")/set(CMAKE_OSX_ARCHITECTURES \"$(uname -m)\")/" build/tools/atracdenc/CMakeLists.txt; } \
    && cmake -S build/tools/atracdenc -B build/tools/atracdenc/build -DCMAKE_BUILD_TYPE=Release -DATRACDENC_PCM_IO_BACKEND=libsndfile \
    && cmake --build build/tools/atracdenc/build --target atracdenc \
    || echo "atracdenc не собрался: музыка значка в XMB (SND0.AT3) будет пропущена"
fi
# donut (GPL-3.0) — рядом: tools/sound_export.py читает donut/dev/codegen/cmd.json
[ -d donut ] || git clone --depth 1 https://github.com/plowteam/donut.git donut || echo "donut не скачался: склонируй его в папку donut"

# тулчейн запускается? macOS без библиотек Homebrew — dyld, Linux без libgmp/libmpc/libmpfr/libzstd — «not found»
if [ -x pspdev/bin/psp-gcc ] && ! pspdev/bin/psp-gcc --version >/dev/null 2>&1; then
  echo "ВНИМАНИЕ: pspdev/bin/psp-gcc не запускается. Посмотри: ldd pspdev/libexec/gcc/psp/*/cc1 | grep 'not found' (macOS: otool -L)"
fi
L_ENV=""; [ "$(uname -s)" = Darwin ] || L_ENV="; source tools/linux_env.sh"
echo; echo "Готово. Перед работой:  export PSPDEV=$PWD/pspdev PATH=$PWD/pspdev/bin:\$PATH; source .venv/bin/activate$L_ENV"
