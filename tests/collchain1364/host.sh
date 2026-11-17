#!/bin/bash
# 1364б: tests/collchain1364/host.sh [папка набора с collect-l1.shr и lN/collect-lN.shr]. Модели карты: card (PSP, 16 МБ/с: чтение 64 КБ ≈ 4 мс) и fast (мгновенный ответ: кеш, эмулятор).
# Новое: сонов 0; на карте PSP кадров до оседания не больше, чем у OLD, опросов ≤ 2 на кадр; на мгновенной карте район оседает быстрее. Красный контроль: прежняя версия 1364 со сном (d954071c) краснеет на «сонов 0».
set -eu
cd "$(dirname "$0")/../.."
S="${1:-$HOME/Downloads/shar-psp-int107b/build/test108/PSP/GAME/SHARTEST15}"
D="${TMPDIR:-/tmp}/collchain1364.$$"; mkdir -p "$D"
SRC="viewer/coll_stream.c viewer/gag_skin.c viewer/collect.c viewer/stateprops.c viewer/skin.c viewer/homer.c viewer/col.c viewer/car.c viewer/coins.c viewer/ticket.c viewer/props.c"
build() { cc -std=c11 -O1 -g -DSKIN_HOST $2 -Iviewer tests/collchain1364/host.c $3 -lm -Wl,-U,_uiFlash -o "$D/$1" 2>"$D/$1.err" || { cat "$D/$1.err"; exit 2; }; }
build new "" "$SRC"; build old "-DCOLLCHAIN1364_OLD=1" "$SRC"
if git cat-file -e d954071c:viewer/coll_stream.c 2>/dev/null; then git show d954071c:viewer/coll_stream.c > "$D/coll_stream_spin.c"; build red "" "$(echo $SRC | sed "s#viewer/coll_stream.c#$D/coll_stream_spin.c#")"; fi
for l in 1 2 3 4 5 6 7; do p="$S/l$l/collect-l$l.shr"; [ "$l" = 1 ] && p="$S/collect-l1.shr"; [ -f "$p" ] || continue
  for v in new old red; do [ -x "$D/$v" ] || continue; "$D/$v" "$p" > "$D/${v}_card$l.txt"; CARD_FAST=1 "$D/$v" "$p" > "$D/${v}_fast$l.txt"; done
  cat "$D/old_card$l.txt" "$D/new_card$l.txt" "$D/old_fast$l.txt" "$D/new_fast$l.txt"; done
python3 tests/collchain1364/check.py "$D"; r=$?; rm -rf "${D:?}"; exit $r
