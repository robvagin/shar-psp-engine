// №1052 (Управление): пропы PRP1 у точки: тип, центр, куски (текстура, флаги). pnear.c level.shr X Z [радиус]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "props.h"
unsigned homerSolidFrame = 0;
static Props props; static ColWorld world;
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (!data || fread(data, 1, size, f) != (size_t)size) return 2; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&world, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40))) return 2;
  float X = (float)atof(argv[2]), Z = (float)atof(argv[3]), R = argc > 4 ? (float)atof(argv[4]) : 5.f;
  for (unsigned i = 0; i < props.ninst; i++) { const PropState *st = &props.st[i]; if (hypotf(st->home[0] - X, st->home[2] - Z) > R) continue; const PropType *T = &props.types[props.def[i].type];
    printf("prop %u type %u home %.2f %.2f %.2f half %.2f %.2f %.2f mass %.0f nPiece %u\n", i, props.def[i].type, st->home[0], st->home[1], st->home[2], st->hx, st->hy, st->hz, T->mass, T->nPiece);
    for (unsigned k = 0; k < T->nPiece; k++) { const PropPiece *p = &props.pieces[T->firstPiece + k]; printf("  piece %u tex %d flags %08x nv %u ni %u sc %.3f off %.2f %.2f %.2f\n", k, p->tex, p->flags, p->nv, p->ni, p->sc, p->ox, p->oy, p->oz); } }
  return 0;
}
