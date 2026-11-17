// 1275 (107b, Стена): сколько в данных уровня машин в слотах трафика, мест стоящих машин, зон-загрузчиков, пешеходных моделей/групп (traffic.shr уровней). Сборка: cc -std=gnu99 -O2 -DSKIN_HOST -Iviewer tests/stena108/trfcount.c viewer/skin.c viewer/traffic.c viewer/car.c viewer/col.c viewer/watchdog.c -lm -o /tmp/trfcount
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "traffic.h"
int (*sceneReadyHook)(const ColWorld *w, float x, float z, int *failed);
int main(int argc, char **argv) {
  for (int a = 1; a < argc; a++) {
    FILE *f = fopen(argv[a], "rb"); if (!f) continue; fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET); unsigned char *b = aligned_alloc(64, ((size_t)sz + 63) & ~(size_t)63);
    if (fread(b, 1, sz, f) != (size_t)sz) { fclose(f); continue; } fclose(f);
    static Traffic T; memset(&T, 0, sizeof T); if (trafficInitMem(&T, b, (unsigned)sz, 0)) { printf("%s: не traffic.shr\n", argv[a]); continue; }
    unsigned nz = 0; for (unsigned i = 0; i < T.hd->nZone; i++) nz += T.zone[i].group == 0xffffffffu ? 0 : 0;
    printf("%s: слотов трафика %u (maxCars по умолчанию %d), мест стоящих %u, зон %u, групп пешеходов %u, пешеходных моделей %d, пешеходов не больше %d, моделей машин %u\n", strrchr(argv[a], '/') ? strrchr(argv[a], '/') + 1 : argv[a], T.hd->nSlot, T.maxCars, T.hd->nPark, T.hd->nZone, T.hd->nGroup, T.npeds, T.maxPeds, T.hd->nModel);
    (void)nz;
  }
  return 0;
}
