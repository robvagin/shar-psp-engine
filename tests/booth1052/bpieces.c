// №1052 (Управление): куски стейтпропа у будки — текстура, флаги, размер по вершинам: найти основание-восьмиугольник. bpieces.c level.shr X Z [радиус]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "stateprops.h"
unsigned homerSolidFrame = 0;
static Props props; static ColWorld world; static StateProps sp;
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (!data || fread(data, 1, size, f) != (size_t)size) return 2; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&world, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40))) return 2;
  if (!props.spr || spInit(&sp, props.spr, props.sprSize)) { printf("нет стейтпропов\n"); return 0; }
  float X = (float)atof(argv[2]), Z = (float)atof(argv[3]), R = argc > 4 ? (float)atof(argv[4]) : 6.f;
  for (unsigned i = 0; i < sp.ninst; i++) { const SpInst *I = &sp.inst[i]; if (hypotf(I->x - X, I->z - Z) > R) continue; const SpType *T = &sp.type[I->type];
    printf("inst %u type %u pos %.2f %.2f %.2f cls %u mass %.0f nPiece %u kind %u\n", i, I->type, I->x, I->y, I->z, T->cls, T->mass, T->nPiece, T->kind);
    for (unsigned k = 0; k < T->nPiece; k++) { const SpPiece *p = &sp.piece[T->firstPiece + k]; const short *v = (const short *)((const char *)sp.vertBase + p->vOff);
      float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f}; for (unsigned q = 0; q < p->nv; q++) { const short *w = (const short *)((const char *)v + 16 * q + 8); float c[3] = {w[0] * p->sc / 32768.f + p->ox, w[1] * p->sc / 32768.f + p->oy, w[2] * p->sc / 32768.f + p->oz}; for (int a = 0; a < 3; a++) { if (c[a] < lo[a]) lo[a] = c[a]; if (c[a] > hi[a]) hi[a] = c[a]; } }
      printf("  piece %u tex %d flags %08x joint %u slot %u nv %u ni %u  bbox x %.2f..%.2f y %.2f..%.2f z %.2f..%.2f\n", k, p->tex, p->flags, (p->flags >> 8) & 255, (p->flags >> 16) & 255, p->nv, p->ni, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]); } }
  return 0;
}
