// Н8–10/№1200/№1201: стейтпропы рядом с точкой — тип, класс, ящик кусков и тело (объём), в том числе тело из bushBodies (№1137). tests/g01cars/spnear.sh X Y Z R [level.shr]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "props.h"
#include "stateprops.h"
__attribute__((weak)) unsigned homerSolidFrame = 0;
static Props props; static StateProps sp; static ColWorld W;
int main(int argc, char **argv) {
  if (argc < 5) return 2; float px = atof(argv[1]), py = atof(argv[2]), pz = atof(argv[3]), R = atof(argv[4]);
  FILE *f = fopen(argc > 5 ? argv[5] : "build/level.shr", "rb"); if (!f) return 99; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15L); if (fread(data, 1, size, f) != (size_t)size) return 99; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&W, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40)) || spInit(&sp, props.spr, props.sprSize)) return 98;
  printf("стейтпропов-экземпляров %u, типов %u\n", sp.ninst, sp.ntype);
  for (unsigned i = 0; i < sp.ninst; i++) { const SpInst *s = &sp.inst[i]; float dx = s->x - px, dy = s->y - py, dz = s->z - pz; if (dx * dx + dz * dz > R * R || fabsf(dy) > 40.f) continue;
    const SpType *T = &sp.type[s->type];
    printf("inst %4u type %3u cls %u kind %u pieces %u nCol %u  at (%7.1f %5.1f %7.1f)  box c=(%.1f %.1f %.1f) h=(%.1f %.1f %.1f)", i, s->type, T->cls, T->kind, T->nPiece, T->nCol, s->x, s->y, s->z, T->c[0], T->c[1], T->c[2], T->h[0], T->h[1], T->h[2]);
    if (T->nCol) { const SpCol *C = &sp.col[T->firstCol]; printf("  col0: kind %u c=(%.1f %.1f %.1f) h=(%.1f %.1f %.1f)", C->kind, C->c[0], C->c[1], C->c[2], C->h[0], C->h[1], C->h[2]); }
    printf("\n"); }
  return 0;
}
