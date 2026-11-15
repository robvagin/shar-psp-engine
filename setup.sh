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
  MINGW*|MSYS*|CYGWIN*) echo "This builder does not run in Windows itself (Git Bash, MSYS, Cygwin). On Windows it runs inside Ubuntu (WSL): in PowerShell run"
    echo "  irm https://raw.githubusercontent.com/robvagin/shar-psp-engine/main/app/get.ps1 | iex"; exit 1 ;;
  *) echo "Unsupported system: $(uname -s)-$(uname -m). Supported: macOS, Linux x86_64/arm64, Windows through WSL (Ubuntu)"; exit 1 ;;
esac
# причина остановки: одна строка в журнал сборки, на страницу-сборщик ("@@ error") и в build/app/setup_error.txt (app/core/build.sh не затирает её общей фразой)
die() { mkdir -p build/app; printf '%s\n' "$1" > build/app/setup_error.txt; printf '@@ {"error":"%s"}\n' "$(printf '%s' "$1" | tr '"\\\n' "'  ")"; echo "ERROR: $1" >&2; exit 1; }
rm -f build/app/setup_error.txt
[ -d pspdev ] || { echo "Качаю $F…"; curl -L "$B/$F" | tar xz; }
# №880 (сборка с чистого клона): компилятор PSPDEV для macOS связан с gmp, libmpc, mpfr, zstd, libusb из Homebrew — без них psp-gcc падает
# (dyld: libmpc.3.dylib); ffmpeg нужен с декодером IPU (ролики), пути — $FFMPEG или PATH
if [ "$(uname -s)" = Darwin ]; then brew install cmake sevenzip python@3.11 gmp libmpc mpfr zstd libusb ffmpeg libsndfile || true
else
  # Linux. Ubuntu/Debian (доработка noname8723, 06.11): пакеты для распаковки диска, сборки C-инструментов (build-essential: cc для tools/pvs, wind
  # и общих библиотек) и библиотеки готового PSPDEV. Fedora/RHEL (dnf) и Arch (pacman): тот же набор под их названиями (Build 107).
  # Без sudo (root, контейнер) — напрямую. 7zip называется «7zip» (Ubuntu 22.10+) или «p7zip-full»
  SUDO=""; [ "$(id -u)" = 0 ] || SUDO="sudo"
  if command -v apt-get >/dev/null 2>&1; then
    $SUDO apt-get update -y || true
    P7=7zip; apt-cache show 7zip >/dev/null 2>&1 || P7=p7zip-full
    $SUDO apt-get install -y build-essential cmake git curl ca-certificates "$P7" python3-pip python3-venv python3-dev ffmpeg libsndfile1-dev || true
    # psp-gcc/cc1 — gmp, mpc, mpfr, zstd; остальные утилиты PSPDEV — libusb, readline, ncurses/tinfo, udev, expat, lzma, libcap. По одной:
    # на Ubuntu 24.04 libreadline8 называется libreadline8t64, и один неизвестный пакет не должен ронять остальные
    for pkg in libgmp10 libmpc3 libmpfr6 libzstd1 libusb-1.0-0 libudev1 libncurses6 libtinfo6 libexpat1 liblzma5 libcap2 zlib1g libreadline8t64 libreadline8; do
      $SUDO apt-get install -y "$pkg" >/dev/null 2>&1 || true
    done
    HINT_DEV="sudo apt install python3-dev build-essential"; HINT_PY12="sudo apt install python3.12 python3.12-venv python3.12-dev"
  elif command -v dnf >/dev/null 2>&1; then
    # Fedora: python3-devel (заголовки Python) и gcc-c++ нужны сборке пакетов Python; ffmpeg из репозитория Fedora называется ffmpeg-free
    $SUDO dnf install -y gcc gcc-c++ make cmake git curl ca-certificates python3 python3-pip python3-devel libsndfile-devel || true
    $SUDO dnf install -y 7zip >/dev/null 2>&1 || $SUDO dnf install -y p7zip p7zip-plugins || true
    command -v ffmpeg >/dev/null 2>&1 || $SUDO dnf install -y ffmpeg-free || true
    for pkg in gmp libmpc mpfr libzstd libusb1 systemd-libs ncurses-libs readline expat xz-libs libcap zlib-ng-compat zlib; do
      $SUDO dnf install -y "$pkg" >/dev/null 2>&1 || true
    done
    HINT_DEV="sudo dnf install python3-devel gcc gcc-c++"; HINT_PY12="sudo dnf install python3.12 python3.12-devel"
  elif command -v pacman >/dev/null 2>&1; then
    $SUDO pacman -S --needed --noconfirm base-devel cmake git curl ca-certificates 7zip python python-pip ffmpeg libsndfile gmp libmpc mpfr zstd libusb readline ncurses expat xz libcap || true
    HINT_DEV="sudo pacman -S base-devel python"; HINT_PY12=""
  else echo "Linux without apt, dnf or pacman: install cmake, 7zip, ffmpeg, python3 with venv and dev headers, a C and C++ compiler and git yourself, then run this again"; HINT_DEV="install your distribution's python3 dev headers and a C++ compiler"; HINT_PY12=""; fi
  # у IPU-декодера ffmpeg — ролики PS2; в урезанной сборке (Fedora ffmpeg-free без нужных кодеков) роликов не будет
  command -v ffmpeg >/dev/null 2>&1 && { ffmpeg -hide_banner -decoders 2>/dev/null | grep -qw ipu || echo "WARNING: this ffmpeg has no IPU decoder, the PS2 movies will not convert. Fedora: install the full ffmpeg from RPM Fusion"; }
fi
# своё окружение Python ≥ 3.11 (.venv): его включают build_level.sh, ship.sh, build_all_levels.sh
# Build 107: у закреплённого в разборщике numpy 2.0.1 нет готовых колёс для Python 3.13/3.14 — pip собирал его из исходников и падал без заголовков Python
# (Fedora 41+). Поэтому берём 3.11 или 3.12, где всё закреплено как у автора; на 3.13+ пробуем поставить 3.12, иначе берём свежие колёса numpy/Pillow
pyok() { "$1" -c 'import sys; sys.exit(sys.version_info < (3, 11))' 2>/dev/null; }
pyminor() { "$1" -c 'import sys; print(sys.version_info[1])'; }
PY=""; for c in python3.11 python3.12 python3.13 python3.14 python3; do p=$(command -v "$c" 2>/dev/null) || continue; pyok "$p" && { PY="$p"; break; }; done
if [ -n "$PY" ] && [ "$(pyminor "$PY")" -ge 13 ] && [ "$(uname -s)" = Linux ] && [ -n "${HINT_PY12:-}" ]; then
  # запасной путь: если Python 3.12 ставится из репозитория (Fedora, Ubuntu), он надёжнее самого нового
  case "$HINT_PY12" in *dnf*) $SUDO dnf install -y python3.12 python3.12-devel >/dev/null 2>&1 || true ;; *apt*) $SUDO apt-get install -y python3.12 python3.12-venv python3.12-dev >/dev/null 2>&1 || true ;; esac
  p=$(command -v python3.12 2>/dev/null) && pyok "$p" && PY="$p"
fi
[ -n "$PY" ] || die "Python 3.11 or newer is needed. Ubuntu 22.04: sudo add-apt-repository ppa:deadsnakes/ppa && sudo apt install python3.11 python3.11-venv; Fedora: sudo dnf install python3.12; Ubuntu 24.04 already has 3.12"
PYV=$(pyminor "$PY"); echo "Python: $PY (3.$PYV)"
# .venv от другой версии Python (например, первый запуск взял 3.14, а 3.12 поставили потом) — пересоздать; в нём только пакеты, терять нечего
[ -x .venv/bin/python ] && [ "$(pyminor .venv/bin/python)" != "$PYV" ] && rm -rf .venv
[ -d .venv ] || "$PY" -m venv .venv || die "Could not create the Python environment. Ubuntu/Debian: sudo apt install python3-venv; Fedora: sudo dnf install python3-devel"
# 7zz зовёт разборщик диска; в p7zip (Ubuntu 22.04, Fedora без пакета 7zip) команда называется 7z/7za — кладём ссылку в .venv/bin
command -v 7zz >/dev/null 2>&1 || for b in 7z 7za 7zr; do command -v "$b" >/dev/null 2>&1 && { ln -sf "$(command -v "$b")" .venv/bin/7zz; break; }; done
command -v 7zz >/dev/null 2>&1 || [ -x .venv/bin/7zz ] || echo "WARNING: no 7zip found (7zz): the disc image cannot be unpacked. Install the 7zip package"
# №1162: разборщик диска (MIT) ставится сам, чтобы публичный путь не зависел от того, что лежало на машине автора
[ -d hit-and-run-web/tools ] || { mkdir -p hit-and-run-web && git clone --depth 1 https://github.com/Vheissu/hit-and-run-web.git hit-and-run-web.clone && cp -R hit-and-run-web.clone/. hit-and-run-web/ && rm -rf hit-and-run-web.clone; } || echo "hit-and-run-web не скачался: склонируй https://github.com/Vheissu/hit-and-run-web в папку hit-and-run-web"
[ -f hit-and-run-web/requirements.txt ] || die "hit-and-run-web did not download (no internet or no git?). Clone https://github.com/Vheissu/hit-and-run-web into the folder hit-and-run-web and run this again"
mkdir -p build/app; REQ=build/app/requirements.txt
if [ "$PYV" -ge 13 ]; then   # колёса есть у numpy 2.1+ (3.13) и 2.3+ (3.14), у Pillow 11.x; потолок — чтобы не уйти за версии, где убрали старые вызовы
  sed -E 's/^numpy==.*/numpy>=2.0.1,<2.4/; s/^[Pp]illow==.*/Pillow>=10.4.0,<12/' hit-and-run-web/requirements.txt > "$REQ"
else cp hit-and-run-web/requirements.txt "$REQ"; fi
HINT_DEV="${HINT_DEV:-xcode-select --install}"
pipfail() {   # что не так, человеческими словами; $1 — пакет, $2 = 1: пакет собирается из исходников (нужны заголовки Python и компилятор C++)
  if [ "${2:-0}" = 1 ] && ! .venv/bin/python -c 'import os, sys, sysconfig; sys.exit(0 if os.path.exists(os.path.join(sysconfig.get_paths()["include"], "Python.h")) else 1)'; then
    die "Python package $1 is built on your computer and needs the Python headers and a C++ compiler, which are missing. Run:  $HINT_DEV   and then run the same line again"
  fi
  if [ "$PYV" -ge 13 ] && [ -n "${HINT_PY12:-}" ]; then die "Python package $1 did not install on Python 3.$PYV (no internet, or no ready-made build for it yet). Check the connection, or install Python 3.12 ($HINT_PY12) and run the same line again; the log is build/app/build.log"; fi
  die "Python package $1 did not install (no internet, or no ready-made build for this Python). The reason is in the lines above and in build/app/build.log"
}
.venv/bin/python -m pip install --only-binary numpy,Pillow -r "$REQ" || pipfail "numpy/Pillow"
.venv/bin/python -m pip install fast_simplification segno || pipfail "fast_simplification/segno"
.venv/bin/python -m pip install meshoptimizer || pipfail "meshoptimizer" 1
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
