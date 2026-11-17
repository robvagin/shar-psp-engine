#!/bin/bash
# 1355: tests/bot1355/run.sh УРОВЕНЬ [ПАРАЛЛЕЛЬНО]: генерирует сценарии (gen.py), гоняет каждую миссию уровня в безголовом эмуляторе (ballast 5), печатает таблицу (table.py).
cd "$(dirname "$0")/../.."; L=${1:-4}; P=${2:-3}; O=${OUT:-/tmp/bot1355_L$L}; mkdir -p $O/scn
export SET=${SET:-$HOME/Downloads/shar-psp-int107b/build/test108tour/PSP/GAME/SHARTEST15} OUT=$O EMU_MINE=$P EB=${EB:-viewer/tbot/EBOOT.PBP}
python3 -I tests/bot1355/gen.py $O/scn --level $L >/dev/null
ls $O/scn/*.txt | xargs -P $P -I{} bash -c 'n=$(basename {} .txt); SMOKE_HEADLESS=1 FLAGS="ballast=5" tools/emu_run.sh {} $n $EB 900 >/dev/null 2>&1'
python3 -I tests/bot1355/table.py $O
