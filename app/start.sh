#!/bin/bash
# SHAR for PSP: страница-сборщик в браузере. Из корня копии репозитория: bash app/start.sh
# Сервер (app/server.py) — только стандартная библиотека Python 3.8+; инструменты сборки ставит сам шаг 1 сборки (setup.sh)
cd "$(dirname "$0")/.."
# Мак с Xcode, чья лицензия не принята: git и компилятор не запускаются (06.11, проверка чистой копии) — сказать сразу и понятно
if [ "$(uname -s)" = Darwin ] && git --version 2>&1 | grep -qi 'license'; then
  echo "Xcode is installed on this Mac but its license was never accepted, so git and the compiler do not run."
  echo "Run this once, then the same line again:   sudo xcodebuild -license accept"
  exit 1
fi
PY=$(command -v python3.12 || command -v python3.11 || command -v python3.13 || command -v python3 || true)
[ -n "$PY" ] || { echo "Python 3 is needed: macOS — run 'xcode-select --install', Ubuntu — 'sudo apt install python3'"; exit 1; }
exec "$PY" app/server.py "$@"
