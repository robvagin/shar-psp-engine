#!/bin/bash
# run.sh СЦЕНАРИЙ [ПАКЕТ]  — безголовый прогон L2, печатает путь результата
cd "$(dirname "$0")/../.."
P=${2:-/private/tmp/claude-501/-Users-USER-Downloads-shar-psp-dal103/428f0a00-a5c0-42e7-8ef3-da9952a94163/scratchpad/p96}
SHIP_LEVELS=2 SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN="$1" SMOKE_MAXT=${MAXT:-500} tools/smoke.sh "$P" >/dev/null 2>&1; ls -td build/smoke/2* | head -1
