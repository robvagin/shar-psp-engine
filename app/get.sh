#!/bin/bash
# Одна строка для Мака и Linux (в Терминале):
#   curl -fsSL https://raw.githubusercontent.com/robvagin/shar-psp-engine/main/app/get.sh | bash
# Скачивает открытый код в ~/SHAR-PSP (повторный запуск обновляет его и продолжает сборку с места остановки) и открывает
# страницу-сборщик в браузере. Ничего из игры не скачивает: игра собирается только из образа своего диска.
set -e
DIR="${SHAR_DIR:-$HOME/SHAR-PSP}"; REPO=https://github.com/robvagin/shar-psp-engine
echo "Hit & Run PSP Builder: getting it into $DIR"
if [ "$(uname -s)" = Darwin ] && ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew is needed for the build tools; its official installer will ask for your password"
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" </dev/tty
  [ -x /opt/homebrew/bin/brew ] && eval "$(/opt/homebrew/bin/brew shellenv)"; [ -x /usr/local/bin/brew ] && eval "$(/usr/local/bin/brew shellenv)"
fi
# Мак с Xcode, чья лицензия не принята: git и компилятор не запускаются (06.11, проверка чистой копии) — сказать сразу и понятно
if [ "$(uname -s)" = Darwin ] && git --version 2>&1 | grep -qi 'license'; then
  echo "Xcode is installed on this Mac but its license was never accepted, so git and the compiler do not run."
  echo "Run this once, then the same line again:   sudo xcodebuild -license accept"
  exit 1
fi
if [ -d "$DIR/.git" ]; then git -C "$DIR" pull --ff-only || echo "update skipped, using the copy you have"
elif command -v git >/dev/null 2>&1; then git clone --depth 1 "$REPO" "$DIR"
else mkdir -p "$DIR" && curl -fsSL "$REPO/archive/refs/heads/main.tar.gz" | tar xz --strip-components 1 -C "$DIR"; fi
exec bash "$DIR/app/start.sh" </dev/tty
