#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "col.h"
// №705: звенья заборов COL1 в окне x0..x1, z0..z1
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); unsigned n = ftell(f); rewind(f); unsigned char *b = malloc(n); if (fread(b, 1, n, f) != n) return 2; fclose(f);
  unsigned co = *(unsigned *)(b + 92), po = *(unsigned *)(b + 96), pv = *(unsigned *)(b + 88), end = n; if (po > co && po < end) end = po; if (pv > co && pv < end) end = pv;
  ColWorld w; if (colInit(&w, b + co, end - co)) return 2;
  float x0 = atof(argv[2]), x1 = atof(argv[3]), z0 = atof(argv[4]), z1 = atof(argv[5]);
  for (unsigned i = 0; i < w.nseg; i++) { const ColSeg *s = &w.seg[i];
    float mx0 = fminf(s->a[0], s->b[0]), mx1 = fmaxf(s->a[0], s->b[0]), mz0 = fminf(s->a[1], s->b[1]), mz1 = fmaxf(s->a[1], s->b[1]);
    if (mx1 >= x0 && mx0 <= x1 && mz1 >= z0 && mz0 <= z1) printf("seg %u a=(%.1f,%.1f) b=(%.1f,%.1f) n=(%.2f,%.2f) y0=%.1f y1=%.1f\n", i, s->a[0], s->a[1], s->b[0], s->b[1], s->n[0], s->n[1], s->y0, s->y1); }
  return 0; }
