// Правка 1230: что стоит рядом с точкой (L5 E3-5). dump_host LEVEL PLAYER X Z R -> заборы, ящики, цилиндры в радиусе R (xz)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "world.h"
#include "col.h"
int main(int argc, char **argv) {
  if (argc < 6 || bwData(argv[1], argv[2], 0)) return 2;
  float X = atof(argv[3]), Z = atof(argv[4]), R = atof(argv[5]);
  const ColWorld *w = bwCol();
  printf("nseg %u nbox %u ncyl %u ntri %u\n", w->nseg, w->nbox, w->ncyl, w->ntri);
  int n = 0;
  for (unsigned i = 0; i < w->nseg; i++) { const ColSeg *s = &w->seg[i];
    float mx = (s->a[0] + s->b[0]) / 2, mz = (s->a[1] + s->b[1]) / 2;
    if (fabsf(mx - X) > R || fabsf(mz - Z) > R) continue; n++;
    printf("SEG %u a %.2f %.2f b %.2f %.2f n %.3f %.3f y0 %.2f y1 %.2f len %.2f\n", i, s->a[0], s->a[1], s->b[0], s->b[1], s->n[0], s->n[1], s->y0, s->y1,
           hypotf(s->b[0] - s->a[0], s->b[1] - s->a[1])); }
  printf("segs in R: %d\n", n);
  for (unsigned i = 0; i < w->nbox; i++) { const ColBox *b = &w->box[i]; if (fabsf(b->c[0] - X) > R || fabsf(b->c[2] - Z) > R) continue;
    printf("BOX %u c %.2f %.2f %.2f h %.2f %.2f %.2f ax %.2f %.2f %.2f az %.2f %.2f %.2f\n", i, b->c[0], b->c[1], b->c[2], b->h[0], b->h[1], b->h[2], b->ax[0], b->ax[1], b->ax[2], b->az[0], b->az[1], b->az[2]); }
  for (unsigned i = 0; i < w->ncyl; i++) { const ColCyl *c = &w->cyl[i]; if (fabsf(c->c[0] - X) > R || fabsf(c->c[2] - Z) > R) continue;
    printf("CYL %u c %.2f %.2f %.2f axis %.2f %.2f %.2f r %.2f half %.2f flat %u\n", i, c->c[0], c->c[1], c->c[2], c->axis[0], c->axis[1], c->axis[2], c->r, c->halfLen, c->flat); }
  return 0;
}
