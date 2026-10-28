// №705 (Глюки): отрезки COL1, у которых верх (y1) отличается между экспортом «новый» и «прежний». diffsegs new.shr old.shr
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "col.h"
static ColWorld load(const char *p) {
  FILE *f = fopen(p, "rb"); fseek(f, 0, SEEK_END); unsigned n = ftell(f); rewind(f); unsigned char *b = malloc(n); if (fread(b, 1, n, f) != n) exit(2); fclose(f);
  unsigned co = *(unsigned *)(b + 92), po = *(unsigned *)(b + 96), pv = *(unsigned *)(b + 88), end = n; if (po > co && po < end) end = po; if (pv > co && pv < end) end = pv;
  ColWorld w; if (colInit(&w, b + co, end - co)) exit(2); return w; }
int main(int argc, char **argv) {
  ColWorld a = load(argv[1]), b = load(argv[2]);
  printf("nseg новый %u прежний %u\n", a.nseg, b.nseg);
  // правило режет забор на куски разной высоты: сопоставляем по середине куска «нового» с отрезком «прежнего» на той же линии (расстояние < 0.06 м, та же нормаль, середина внутри)
  for (unsigned i = 0; i < a.nseg; i++) { const ColSeg *s = &a.seg[i]; float mx = (s->a[0] + s->b[0]) / 2, mz = (s->a[1] + s->b[1]) / 2, best = -1e9f; int hit = 0, bj = -1;
    for (unsigned j = 0; j < b.nseg; j++) { const ColSeg *o = &b.seg[j]; if (o->n[0] * s->n[0] + o->n[1] * s->n[1] < .98f) continue;
      float dx = o->b[0] - o->a[0], dz = o->b[1] - o->a[1], L2 = dx * dx + dz * dz; if (L2 < 1e-6f) continue;
      float t = ((mx - o->a[0]) * dx + (mz - o->a[1]) * dz) / L2; if (t < -.001f || t > 1.001f) continue;
      float px = o->a[0] + t * dx - mx, pz = o->a[1] + t * dz - mz; if (px * px + pz * pz > .06f * .06f) continue;
      if (fabsf(o->y0 - s->y0) > 2.f) continue; hit = 1; if (o->y1 > best) { best = o->y1; bj = j; } }
    if (!hit) { printf("seg %u a=(%.1f,%.1f) b=(%.1f,%.1f) НЕТ пары в прежнем\n", i, s->a[0], s->a[1], s->b[0], s->b[1]); continue; }
    if (best - s->y1 > .05f) printf("seg %u a=(%.1f,%.1f) b=(%.1f,%.1f) n=(%.2f,%.2f) y0=%.1f y1: %.1f -> %.1f (%.1f)\n", i, s->a[0], s->a[1], s->b[0], s->b[1], s->n[0], s->n[1], s->y0, best, s->y1, best - s->y1);
    else if (s->y1 - best > .05f) printf("seg %u a=(%.1f,%.1f) b=(%.1f,%.1f) ВЫШЕ чем прежде: %.1f -> %.1f\n", i, s->a[0], s->a[1], s->b[0], s->b[1], best, s->y1); }
  return 0; }
