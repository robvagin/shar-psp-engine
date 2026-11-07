#!/bin/bash
# 1Б гейт пула клеток (viewer/cellpool.h): верность на Маке + красные контроли (испорченная копия заголовка обязана провалить проверку).
#   tests/cellpool1b/accept.sh        -> последняя строка PASS / FAIL
H=$(cd "$(dirname "$0")" && pwd); W=$H/../..; T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; rc=0
export DEVELOPER_DIR=${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}
build() { ${CC:-/usr/bin/cc} -O1 -DCP_HOST -DCP_HEADER="\"$2\"" -o "$1" "$H/host.c" -lm 2>"$T/cc.log" || { echo "FAIL: не собралось ($2)"; head -5 "$T/cc.log"; return 1; }; }
build "$T/good" "$W/viewer/cellpool.h" || exit 1
if "$T/good" 7 quick > "$T/good.txt" 2>&1; then echo "ok: прежний заголовок проходит"; else echo "FAIL: исправный заголовок не прошёл"; cat "$T/good.txt"; rc=1; fi
mut() {   # $1 имя, $2 что заменить, $3 чем
  python3 - "$W/viewer/cellpool.h" "$T/$1.h" "$2" "$3" <<'P' || { echo "FAIL: мутация $1 не применилась"; rc=1; return; }
import sys; s=open(sys.argv[1]).read(); assert s.count(sys.argv[3])==1, 'нет места'; open(sys.argv[2],'w').write(s.replace(sys.argv[3], sys.argv[4]))
P
  build "$T/$1" "$T/$1.h" || { rc=1; return; }
  if "$T/$1" 7 quick > "$T/$1.txt" 2>&1; then echo "FAIL: красный контроль $1 прошёл (гейт ничего не ловит)"; rc=1; else echo "ok: красный контроль $1 краснеет ($(grep -m1 FAIL "$T/$1.txt" | cut -c1-90))"; fi
}
mut leak 's->freeMask |= 1u << bit; s->nfree++; cpCellsLive--;' 's->freeMask |= 1u << bit; cpCellsLive--;'                    # ячейка вернулась, но плита не считает её свободной: плиты не возвращаются
mut overlap 'best->freeMask &= ~(1u << bit); best->nfree--;' 'best->nfree--;'                                                                           # занятая ячейка остаётся свободной: две клетки в одном месте
mut shortcell 'r = best->base + (size_t)bit * best->cell;' 'r = best->base + (size_t)bit * (best->cell - 64u);'                                           # ячейки наезжают друг на друга
[ $rc = 0 ] && echo PASS || echo FAIL; exit $rc
