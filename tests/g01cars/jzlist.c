// Н8–10/№1200: список зон прыжка уровня (имя, центр, размер, код места) — найти трамплин по сектору. tests/g01cars/jzlist.sh [level.shr]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "props.h"
#include "stateprops.h"
#include "placecode.h"
__attribute__((weak)) unsigned homerSolidFrame = 0;
static Props props; static StateProps sp; static ColWorld W;
int main(int argc, char **argv) {
  FILE *f = fopen(argc > 1 ? argv[1] : "build/level.shr", "rb"); if (!f) return 99; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15L); if (fread(data, 1, size, f) != (size_t)size) return 99; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&W, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40)) || spInit(&sp, props.spr, props.sprSize)) return 98;
  printf("зон прыжка %u\n", W.nzone);
  for (unsigned i = 0; i < W.nzone; i++) { const ColZone *z = &W.zone[i]; char pc[16]; placeCode(z->c[0], z->c[2], pc, sizeof pc);
    printf("%-16s c=(%8.1f %6.1f %8.1f) h=(%5.1f %5.1f %5.1f) ax=(%5.2f %5.2f %5.2f) shape=%u  %s\n", z->name, z->c[0], z->c[1], z->c[2], z->h[0], z->h[1], z->h[2], z->ax[0], z->ax[1], z->ax[2], z->shape, pc); }
  return 0;
}
