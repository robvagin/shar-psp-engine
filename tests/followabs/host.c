// Круг 7 (g11, P11 Д2 / P12-12): объём за суставом — положение сустава целиком, без поворота экземпляра (как в оригинале). Хост на данных уровня + objnames.shr:
// для каждого экземпляра типа со следованием — расстояние между центром объёма (spVolumeCenter) и «запечённым» (поворот × центр + положение) в позе кадра 0.
// Новое: у типа с суставом, который в покое не в нуле (автомат 0,261 м, паук 0,92/−0,52), расстояние равно этому смещению у ВСЕХ экземпляров, как бы они ни были повёрнуты; старое (сдвиг от кадра 0): 0.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "stateprops.h"
unsigned homerSolidFrame = 0; void uiFlash(void) {}
static unsigned char *rd(const char *p, long *n) { FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); *n = ftell(f); rewind(f); unsigned char *d = aligned_alloc(16, (*n + 15) & ~15); if (fread(d, 1, *n, f) != (size_t)*n) return 0; fclose(f); return d; }
static Props props; static ColWorld world; static StateProps sp;
int main(int argc, char **argv) {
  int types = 0, nonzero = 0, inst = 0, same = 1; float firstD[3] = {0, 0, 0}; int haveFirst = 0;
  for (int a = 1; a + 1 < argc; a += 2) {
    long size, nn; unsigned char *data = rd(argv[a], &size), *names = rd(argv[a + 1], &nn); if (!data || !names) continue;
    unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
    memset(&props, 0, sizeof props); memset(&sp, 0, sizeof sp);
    colInit(&world, data + co, ce - co); propsInit(&props, data + po, *(unsigned *)(data + po + 40)); spInit(&sp, props.spr, props.sprSize);
    unsigned ny = *(unsigned *)(names + 16), oy = *(unsigned *)(names + 28);   // заголовок OBJN: число типов и смещение таблицы типов
    if (spFollowAdopt(&sp, names + oy, ny) < 0) continue;
    for (unsigned i = 0; i < sp.ninst; i++) {
      unsigned t = sp.inst[i].type; if (!sp.follow || t >= sp.nfollow || !sp.follow[t]) continue;
      const SpType *T = &sp.type[t]; float c[3], base[3]; if (!T->nCol) continue;
      sp.run[i].poseF = 0.f; sp.run[i].poseValid = 0;
      if (!spVolumeCenter(&sp, i, 0, c)) continue;
      const SpCol *C = &sp.col[T->firstCol]; const SpRun *r = &sp.run[i]; float cc[3] = {C->c[0], C->c[1], C->c[2]};
      for (int k = 0; k < 3; k++) base[k] = r->R[k] * cc[0] + r->R[3 + k] * cc[1] + r->R[6 + k] * cc[2];
      float d[3] = {c[0] - (base[0] + r->x), c[1] - (base[1] + r->y), c[2] - (base[2] + r->z)}, dn = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
      inst++; if (dn > .2f) nonzero++;
      if (dn > .2f && T->nCol == 2) { if (!haveFirst) { memcpy(firstD, d, sizeof d); haveFirst = 1; } else if (fabsf(d[0] - firstD[0]) + fabsf(d[1] - firstD[1]) + fabsf(d[2] - firstD[2]) > .01f) same = 0; }
    }
    for (unsigned t = 0; t < sp.nfollow; t++) types += sp.follow[t] != 0;
  }
  printf("types %d\ninstances %d\nnonzero %d\nsame_vector_for_vending %d\n", types, inst, nonzero, same);
  return 0;
}
