#!/bin/bash
# Гейт g08-sound: новое поведение проходит, прежнее (все *_OLD=1) краснеет. Мак, CPU, без эмулятора. Нужен пакет звука: build/audio/ (или PACK=...)
set -eu
cd "$(dirname "$0")/../.."
export SDKROOT="${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}"
PACK="${PACK:-build/audio/}"; O=build/g08sound; mkdir -p "$O"
cc -std=c11 -O2 -w -DAUDIO_HOST -I viewer tests/g08sound/host.c viewer/sound.c viewer/audio.c -lm -o "$O/green"
cc -std=c11 -O2 -w -DAUDIO_HOST -DSV_MODEMIX_OLD=1 -DENGINE_SV_OLD=1 -I viewer tests/g08sound/host.c viewer/sound.c viewer/audio.c -lm -o "$O/red"
"$O/green" "$PACK"
if "$O/red" "$PACK" > "$O/red.txt"; then echo "FAIL: красный контроль не пойман"; exit 1; fi
grep -q '^FAIL' "$O/red.txt" || { echo "FAIL: красный не дошёл до проверки"; exit 1; }
echo "PASS: g08sound зелёный, красный (SV_MODEMIX_OLD, ENGINE_SV_OLD) краснеет"
