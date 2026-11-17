#!/bin/bash
# 1339 (Роб, 108 на PSP: «нельзя отменить перезапись сейва: жму No на "перезаписать", потом "do you want to cancel save" Yes, а оно опять открывает запись на тот же слот»). Ящик Роба (ИГРА108_2026-11-15_3): после SVIO res=cancel сразу
# SVSEL sel=0 r=0 и снова окно, три раза подряд. Причина: кадр собирает нажатия по последним 16 отсчётам пульта (padbuf.h), окно PSP закрылось только что, ✕ «Да» ещё в отсчётах — главный цикл видит его как новое нажатие на слоте.
# Оригинал (guiscreensavegame.cpp): No на «перезаписать» — возврат на экран выбора слота, то есть назад к выбору слота, записи нет. Правка: после окна утилиты пульт берётся с нынешнего состояния (padResync). SVDLGPAD1339_OLD=1 / flag dlgpad1339old — как было.
# Стенд без железа: flag svdlgfake (окно «показано», ответ — отмена), flag svdlgphantom (✕ в последнем отсчёте пульта сразу после окна, как кольцо отсчётов на PSP). Сценарий: пауза, SAVE GAME, пустой слот 1 — запись (занят), снова SAVE GAME,
# слот 1 (занят) -> окно -> отмена. Критерий по ЖУРНАЛУ: отмен ровно одна, больше окно не открывается, записей всего одна (файл сохранения после отмены не менялся); старое (flag dlgpad1339old): окно открывается снова и снова (≥ 3 отмен).
#   tests/savegame/pad1339/run.sh EBOOT
set -u; cd "$(dirname "$0")/../../.."; export DEVELOPER_DIR=/Library/Developer/CommandLineTools
EB=${1:?EBOOT}; R=${R:-$HOME/Downloads/shar-psp-fin108}; OUT=${OUT:-$PWD/build/pad1339g}; rm -rf "$OUT"; mkdir -p "$OUT"
export SET=${SET:-$HOME/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15} SMOKE_HEADLESS=1 EMU_MINE=2
{ echo "flag menu"; echo "flag saves"; echo "wait 1.5"; echo "press cross"; echo "wait 4"; echo "flag svmark"; echo "press start"; echo "wait 1"; echo "press up"; echo "wait 0.4"; echo "press up"; echo "wait 0.4"; echo "press cross"; echo "wait 1"
  echo "press cross"; echo "wait 3"; echo "press cross"; echo "wait 2"; echo "flag svdialog"; echo "flag svdlgfake"; echo "flag svdlgphantom"; echo "shot a_before"; echo "press cross"; echo "wait 1"; echo "press cross"; echo "wait 6"; echo "shot a_after"; echo "end"; } > "$OUT/s.scn"
for V in 0 1; do F=""; [ $V = 1 ] && F="dlgpad1339old=1"; (cd "$R" && OUT="$OUT" FLAGS="$F" nice -n 20 tools/emu_run.sh "$OUT/s.scn" s$V "$EB" 200 > "$OUT/s$V.log" 2>&1) & done; wait
bad=0
for V in 0 1; do
  L=$OUT/out_s$V/smoke.txt; [ -f "$L" ] || { echo "FAIL: нет журнала v=$V"; bad=1; continue; }
  c=$(grep -ac "^SVIO op=save slot=0 res=cancel" "$L"); w=$(grep -ac "^SVIO op=save slot=0 res=0000" "$L"); o=$(grep -ac "^SVOPEN mode=save" "$L")
  echo "$([ $V = 0 ] && echo новое || echo старое): отмен окна $c, записей $w, открытий меню сохранения $o"
  if [ $V = 0 ]; then [ "$c" = 1 ] && [ "$w" = 1 ] || { echo "FAIL зелёный (нужно: 1 отмена, 1 запись)"; bad=1; }
  else [ "$c" -ge 3 ] || { echo "КРАСНЫЙ НЕ ПОЙМАН"; bad=1; }; fi
done
[ $bad = 0 ] && echo "pad1339: PASS" || echo "pad1339: FAIL"; exit $bad
