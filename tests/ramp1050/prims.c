#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "col.h"
static ColWorld W;
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *d = malloc((size + 15) & ~15); fread(d, 1, size, f); fclose(f);
  unsigned co = *(unsigned *)(d + 92), po = *(unsigned *)(d + 96), pv = *(unsigned *)(d + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&W, d + co, ce - co)) return 3;
  float x0 = atof(argv[2]), x1 = atof(argv[3]), z0 = atof(argv[4]), z1 = atof(argv[5]);
  printf("tri %u box %u cyl %u seg %u\n", W.ntri, W.nbox, W.ncyl, W.nseg);
  for (unsigned i = 0; i < W.nbox; i++) { const ColBox *b = &W.box[i]; float rx = fabsf(b->ax[0]) * b->h[0] + fabsf(b->ay[0]) * b->h[1] + fabsf(b->az[0]) * b->h[2], rz = fabsf(b->ax[2]) * b->h[0] + fabsf(b->ay[2]) * b->h[1] + fabsf(b->az[2]) * b->h[2];
    if (b->c[0] + rx < x0 || b->c[0] - rx > x1 || b->c[2] + rz < z0 || b->c[2] - rz > z1) continue;
    printf("BOX %u c(%.1f %.1f %.1f) h(%.2f %.2f %.2f) ax(%.2f %.2f %.2f) ay(%.2f %.2f %.2f) az(%.2f %.2f %.2f)\n", i, b->c[0], b->c[1], b->c[2], b->h[0], b->h[1], b->h[2], b->ax[0], b->ax[1], b->ax[2], b->ay[0], b->ay[1], b->ay[2], b->az[0], b->az[1], b->az[2]); }
  int nt = 0; for (unsigned i = 0; i < W.ntri; i++) { const ColTri *t = &W.tri[i]; float cx = (t->a[0] + t->b[0] + t->c[0]) / 3, cz = (t->a[2] + t->b[2] + t->c[2]) / 3;
    if (cx < x0 || cx > x1 || cz < z0 || cz > z1) continue; if ((t->a[1] > 27.5f || t->b[1] > 27.5f || t->c[1] > 27.5f || t->n[1] < .8f) && nt++ < 60) printf("TRI %u c(%.1f %.1f %.1f) n(%.2f %.2f %.2f) ya %.1f yb %.1f yc %.1f\n", i, cx, (t->a[1] + t->b[1] + t->c[1]) / 3, cz, t->n[0], t->n[1], t->n[2], t->a[1], t->b[1], t->c[1]); }
  printf("треугольников в окне: %d\n", nt);
  for (unsigned i = 0; i < W.nseg; i++) { const ColSeg *g = &W.seg[i]; float mx = (g->a[0] + g->b[0]) / 2, mz = (g->a[1] + g->b[1]) / 2; if (mx < x0 - 2 || mx > x1 + 2 || mz < z0 - 2 || mz > z1 + 2) continue; printf("SEG %u a(%.1f %.1f) b(%.1f %.1f) n(%.2f %.2f) y0 %.1f y1 %.1f\n", i, g->a[0], g->a[1], g->b[0], g->b[1], g->n[0], g->n[1], g->y0, g->y1); }
  for (unsigned i = 0; i < W.ncyl; i++) { const ColCyl *c = &W.cyl[i]; if (c->c[0] < x0 - 3 || c->c[0] > x1 + 3 || c->c[2] < z0 - 3 || c->c[2] > z1 + 3) continue; printf("CYL %u c(%.1f %.1f %.1f) r %.2f halfLen %.2f flat %u\n", i, c->c[0], c->c[1], c->c[2], c->r, c->halfLen, c->flat); }
  return 0;
}
