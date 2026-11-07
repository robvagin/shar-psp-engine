#!/bin/bash
# №1159 п.2: список будок не держит память между уровнями. Хост: цепочка из 11 «загрузок» (уровни 1..7, потом 1 снова) с тысячами предметов; считаем живые байты списка.
# Зелёный: пока уровень загружен — не больше 64 Б; после выгрузки (ptBoothFree, как levelUnload) — 0 Б; состав списка тот же (будки найдены). Красный (обязан упасть): -DPROPBOOTHMEM1159_OLD=1 —
# 4 байта на экземпляр и после выгрузки память остаётся.   tests/boothmem1159/accept.sh
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; OUT=build/boothmem1159; mkdir -p "$OUT"
cat > "$OUT/t.c" <<'EOT'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static long live, peakLoaded;
static void *cm(size_t n) { void *p = malloc(n + 16); if (!p) return 0; *(size_t *)p = n; live += (long)n; return (char *)p + 16; }
static void cf(void *p) { if (!p) return; char *q = (char *)p - 16; live -= (long)*(size_t *)q; free(q); }
#define malloc cm
#define free cf
#include "propbooth1159.h"
#undef malloc
#undef free
int main(void) {
  static SpType ty[2]; static SpPiece pc[14]; static SpInst in_[6000];
  ty[0].nPiece = 1; ty[1].nPiece = 7; ty[1].kind = 0; ty[1].firstPiece = 7; pc[7 + 1].nv = 18; pc[7 + 6].nv = 42;
  StateProps sp; memset(&sp, 0, sizeof sp); sp.type = ty; sp.piece = pc; sp.inst = in_;
  long maxLoaded = 0, afterUnload = 0; unsigned booths = 0, expect = 0;
  for (int load = 0; load < 11; load++) {
    unsigned n = 1500 + 400 * (load % 7); sp.ninst = n;
    for (unsigned i = 0; i < n; i++) in_[i].type = (i % 997 == 5 || i == n - 1) ? 1 : 0;
    expect = 0; for (unsigned i = 0; i < n; i++) expect += in_[i].type == 1;
    booths = ptBoothEnsure(&sp); if (booths != expect) { printf("BADLIST load %d %u != %u\n", load, booths, expect); return 1; }
    if (live > maxLoaded) maxLoaded = live;
    ptBoothFree(); if (live > afterUnload) afterUnload = live;
  }
  printf("loaded_max %ld\nafter_unload_max %ld\nlist_ok 1\n", maxLoaded, afterUnload); return 0;
}
EOT
b() { cc -std=c11 -O1 -w -I viewer "$@" "$OUT/t.c" -o "$OUT/$NAME" && "$OUT/$NAME" > "$OUT/$NAME.txt"; }
NAME=green; b; NAME=old; b -DPROPBOOTHMEM1159_OLD=1
g() { grep "^$2 " "$OUT/$1.txt" | cut -d' ' -f2; }
echo "зелёный: пока уровень загружен до $(g green loaded_max) Б, после выгрузки $(g green after_unload_max) Б; красный: $(g old loaded_max) Б и $(g old after_unload_max) Б"
fail=0
[ -n "$(g green loaded_max)" ] && [ "$(g green loaded_max)" -le 64 ] && [ "$(g green after_unload_max)" = 0 ] || { echo "FAIL зелёный"; fail=1; }
[ -n "$(g old loaded_max)" ] && [ "$(g old after_unload_max)" -gt 0 ] && echo "красный упал, как должен" || { echo "КРАСНЫЙ НЕ ПОЙМАН"; fail=1; }
grep -q 'ptBoothFree();' viewer/main_levels.h || { echo "FAIL: levelUnload не зовёт ptBoothFree"; fail=1; }
[ $fail -eq 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
