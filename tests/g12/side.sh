#!/bin/sh
# Круг 4: тип поверхности из файла рядом (surf-l<N>.shr) поверх level.shr TEST81 == тип из блока SRF1 свежего level.shr. tests/g12/side.sh T81_level.shr surf-lN.shr FRESH_level.shr [OTHER_surf.shr]
# Зелёный: на сетке 10 м построчно одинаковые ответы; красные контроли: без файла рядом (всё «дорога») и чужой файл (отвергнут) обязаны отличаться.
set -eu
B="$1"; S="$2"; F="$3"; O="${4:-}"; D="$(cd "$(dirname "$0")/../.." && pwd)"; T="${TMPDIR:-/tmp}/g12_side.$$"; mkdir -p "$T"
export SDKROOT="${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}"
PTS=$(awk 'BEGIN{for(x=-700;x<=700;x+=10)for(z=-700;z<=700;z+=10)printf "%d %d ",x,z}')
cc -std=c11 -O2 -Wall -I "$D/viewer" "$D/tests/g12/surface_test.c" "$D/viewer/surface.c" "$D/viewer/col.c" -lm -o "$T/t"
"$T/t" "$F" $PTS > "$T/fresh.txt"
SURF_SIDE="$S" "$T/t" "$B" $PTS > "$T/side.txt" 2> "$T/side.err"; cat "$T/side.err"
"$T/t" "$B" $PTS > "$T/none.txt"
# строки "(x,z) y=.. тип T помещение I": сравниваем тип и помещение (y одинаков, т.к. COL1 тот же)
f() { sed 's/ y=[^ ]*//' "$1"; }
f "$T/fresh.txt" > "$T/a"; f "$T/side.txt" > "$T/b"; f "$T/none.txt" > "$T/c"
if cmp -s "$T/a" "$T/b"; then echo "ЗЕЛЁНЫЙ: поверхность из файла рядом совпала с SRF1 на $(grep -c тип "$T/a") точках; $(tail -1 "$T/b")"; else echo "FAIL: расходятся"; diff "$T/a" "$T/b" | head -5; exit 1; fi
if cmp -s "$T/a" "$T/c"; then echo "FAIL: без файла рядом ответы те же — гейт ничего не доказывает"; exit 1; else echo "красный контроль 1: без файла рядом ответы другие (везде дорога), как должно"; fi
if [ -n "$O" ]; then SURF_SIDE="$O" "$T/t" "$B" $PTS 2>"$T/o.err" | sed 's/ y=[^ ]*//' > "$T/d"; grep -q ОТВЕРГНУТ "$T/o.err" && cmp -s "$T/c" "$T/d" && echo "красный контроль 2: чужой файл отвергнут (ответы как без файла)" || { echo "FAIL: чужой файл принят"; exit 1; }; fi
rm -rf "$T"
