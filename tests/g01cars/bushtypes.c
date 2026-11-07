// №1201: типы, которым №1137 дал тело куста: размеры, число экземпляров, высоты. tests/g01cars/bushtypes.c level.shr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "props.h"
#include "stateprops.h"
__attribute__((weak)) unsigned homerSolidFrame = 0;
static Props props; static StateProps sp; static ColWorld W;
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); if (!f) return 99; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15L); if (fread(data, 1, size, f) != (size_t)size) return 99; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&W, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40)) || spInit(&sp, props.spr, props.sprSize)) return 98;
  for (unsigned t = 0; t < sp.ntype; t++) { const SpType *T = &sp.type[t]; if (!(T->cls == 2 && !T->kind && T->nCol == 1 && T->nPiece && sp.col[T->firstCol].h[0] > 0 && T->cls == 2)) continue;
    unsigned n = 0; float ymin = 1e9f, ymax = -1e9f; for (unsigned i = 0; i < sp.ninst; i++) if (sp.inst[i].type == t) { n++; if (sp.inst[i].y < ymin) ymin = sp.inst[i].y; if (sp.inst[i].y > ymax) ymax = sp.inst[i].y; }
    if (!n) continue; float hx = T->h[0], hz = T->h[2], hy = T->h[1]; float ratio = hx > hz ? hx / (hz > 1e-3f ? hz : 1e-3f) : hz / (hx > 1e-3f ? hx : 1e-3f);
    printf("type %3u atc %u mass %.0f inst %3u  h=(%.2f %.2f %.2f) ширина/глубина %.1f  высота %.1f  y %.1f..%.1f%s\n", t, T->atc, T->mass, n, hx, hy, hz, ratio, 2.f * hy, ymin, ymax, ratio > 2.f ? "  <-- плоский" : ""); }
  return 0;
}
