#!/bin/bash
# №1161: строки Jump Cameras и Adjust Camera страницы CAMERA паузы включены на всех четырёх языках (эмулятор, безголовый, L1).
#   tests/camera/camrowlang.sh EBOOT НАБОР      НАБОР — папка игры (ui, level.shr …) с lang/F|G|S/ (pmenu.shr, rowkeys.txt, ui.shr …). Код выхода 0 = на всех языках rows=0x,1,1,0
#   Красный контроль: EBOOT, собранный с -DCAMROWLANG_OLD=1, — на F, G, S rows другие, скрипт возвращает 1
set -u; cd "$(dirname "$0")/../.."; export DEVELOPER_DIR="${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}"
EB=$1; SET=$2; T=build/camrowlang_set.$$; rm -rf $T; mkdir -p $T
for f in "$SET"/*; do b=$(basename "$f"); [ "$b" = EBOOT.PBP ] || ln -s "$(cd "$(dirname "$f")" && pwd -P)/$b" "$T/$b"; done; cp "$EB" $T/EBOOT.PBP
bad=0
for L in E F G S; do
  fl=""; ex=""; if [ $L != E ]; then fl="lang_$(echo $L | tr A-Z a-z)"; ex="lang/$L/ui.shr lang/$L/pmenu.shr lang/$L/rowkeys.txt lang/$L/prompts.txt lang/$L/menu.shr lang/$L/pextra.shr"; fi
  SMOKE_EXTRA="$ex" SMOKE_FLAGS="$fl" SMOKE_HEADLESS=1 SMOKE_FILEDATA="meaudio=3" SMOKE_SCN=$PWD/tests/camera/camrowlang_scenario.txt SMOKE_MAXT=120 tools/smoke.sh $PWD/$T > /dev/null 2>&1
  d=$(ls -dt build/smoke/* | head -1)
  rows=$(grep -a "^OPT pack=pmenu scr=3" $d/smoke.txt | head -1 | sed -n 's/.* rows=\([^ ]*\) .*/\1/p')
  if [ "$rows" = "0x,1,1,0" ]; then echo "OK   $L rows=$rows"; else echo "FAIL $L rows=${rows:-нет строки OPT}"; bad=1; fi
done
rm -rf $T; exit $bad
