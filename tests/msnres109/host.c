// Хост-тест читателя msnres.c (запекание З9): печатает по каждой миссии число записей, пик и суммы; код выхода 0 — файл принят, 1 — отказ.
// сборка и запуск: tests/msnres109/run.sh
#include <stdio.h>
#include <stdlib.h>
#include "../../viewer/msnres.h"
int main(int argc, char **argv) {
  if (argc < 3) return 2;
  int level = atoi(argv[2]);
  if (msnResOpen(argv[1], level) != 0) { printf("MSNRES open level=%d rc=-1\n", level); return 1; }
  unsigned tot = 0;
  static const char *ids[] = {"L", "m0", "m1", "m2", "m3", "m4", "m5", "m6", "m7", "m8", "sr1", "sr2", "sr3", "gr1", "bm1", "ismovie"};
  for (unsigned k = 0; k < sizeof ids / sizeof *ids; k++) { int m = msnResMission(ids[k]); if (m < 0) continue;
    unsigned n = msnResCount(m), miss = 0; for (unsigned i = 0; i < n; i++) { const MsnResEnt *e = msnResAt(m, i); if (e->flags & MSNRES_F_MISSING) miss++; tot += e->bytes; }
    printf("%s n=%u peak=%u art=%u car=%u chr=%u missing=%u\n", ids[k], n, msnResPeak(m), msnResBytes(m, MSNRES_ART), msnResBytes(m, MSNRES_CAR), msnResBytes(m, MSNRES_CHR), miss); }
  printf("MSNRES ok level=%d bytes=%u\n", level, tot); msnResClose(); return 0;
}
