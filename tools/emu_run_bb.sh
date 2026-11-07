#!/bin/bash
# Тур в эмуляторе PPSSPP на Маке (терминал рендера): визуальная проверка, когда PSP недоступна. Не железо: цифры времени не годятся.
#   emu_run.sh EBOOT.PBP СЦЕНАРИЙ.txt МЕТКА МАКС_С [файлы-флаги…]
#   -> ~/Downloads/shar-psp-render-w/build/emu_runs/МЕТКА/{smoke.txt, *.png}
# Набор данных — TEST30 (клоны APFS, место не тратят), EBOOT подменяется. Очередь эмулятора и снимки окна — tools/ppsspp_run.sh (без звука)
. "$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)/linux_env.sh"   # Linux: обёртки stat/cp/sed под синтаксис macOS (tools/linux_env.sh)
EB=$1; SCN=$2; LBL=$3; MAXT=${4:?макс. секунд}; shift 4
W=$HOME/Downloads/shar-psp-render-w; SET=${EMU_SET:-$HOME/Downloads/shar-psp-integ/build/test52tour/PSP/GAME/SHARTEST15}   # EMU_SET — другой набор (TEST31: shar-psp-integ/build/test31tour/PSP/GAME/SHARTEST15)
RUN=$W/build/emu/rr_$LBL; OUT=$W/build/emu_runs/$LBL
rm -rf "$RUN" "$OUT"; mkdir -p "$RUN" "$OUT"
cp -c -R "$SET"/. "$RUN"/ || exit 1
rm -f "$RUN"/smoke.txt "$RUN"/shot_*.ok "$RUN"/scenario.txt
cp "$EB" "$RUN/EBOOT.PBP"; cp "$SCN" "$RUN/scenario.txt"; touch "$RUN/smokelog"
for f in "$@"; do touch "$RUN/$f"; done
PPSSPP_SHOTS="$RUN/smoke.txt" PPSSPP_SHOTDIR="$OUT" "$W/tools/ppsspp_run.sh" "$RUN/EBOOT.PBP" "$MAXT"
cp "$RUN/smoke.txt" "$OUT/" 2>/dev/null
echo "$(date '+%H:%M:%S') эмулятор $LBL: END $(grep -a -c '^END' "$OUT/smoke.txt" 2>/dev/null), снимков $(ls "$OUT"/*.png 2>/dev/null | wc -l | tr -d ' ')"
cp "$RUN/blackbox.txt" "$RUN/stall.txt" "$OUT/" 2>/dev/null; rm -rf "$RUN"
