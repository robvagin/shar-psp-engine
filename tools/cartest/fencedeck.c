// Семейство «забор без высоты под настилом»: отрезки забора COL1 с бесконечным верхом (y1 > 1e5), под которыми по какой-то точке отрезка есть настил/дорога выше основания на 6 м и больше
// (шоссе L5 над землёй): такой забор стенкой «до неба» перегораживает машину на настиле (ночные боты 05.11: L5 gr1 забор 869/870 на шоссе). Печать: уровень, отрезок, y0 y1, длина, точек с настилом выше.
//   fencedeck LEVEL.shr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
static unsigned char *readAll(const char *p, size_t *n) { FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); long s = ftell(f); rewind(f); unsigned char *b = aligned_alloc(16, ((size_t)s + 15) & ~15u); if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) return 0; fclose(f); *n = (size_t)s; return b; }
int main(int argc, char **argv) {
  size_t n; unsigned char *b = readAll(argv[1], &n); if (!b) return 2; unsigned co = *(unsigned *)(b + 92), pvs = *(unsigned *)(b + 88), po = *(unsigned *)(b + 96), ce = (unsigned)n;
  if (po > co && po < ce) ce = po; if (pvs > co && pvs < ce) ce = pvs; static ColWorld W; if (colInit(&W, b + co, ce - co)) return 2;
  int inf = 0, deck = 0;
  for (unsigned i = 0; i < W.nseg; i++) { const ColSeg *G = &W.seg[i]; if (G->y1 < 1e5f) continue; inf++;
    float dx = G->b[0] - G->a[0], dz = G->b[1] - G->a[1], L = hypotf(dx, dz); int pts = 0, tot = 0; float maxD = 0;
    for (float t = 0; t <= L; t += 1.f) { float x = G->a[0] + dx * t / (L > 0 ? L : 1), z = G->a[1] + dz * t / (L > 0 ? L : 1), g1, g0, nn[3]; tot++;
      if (!colGround(&W, x, z, 200.f, -1e6f, &g1, nn)) continue; if (!colGround(&W, x, z, g1 - 6.f, -1e6f, &g0, nn)) continue; if (g1 - g0 > maxD) maxD = g1 - g0; pts++; }
    if (pts >= 2) { deck++; printf("seg %u\t(%.1f,%.1f)-(%.1f,%.1f)\tдлина %.1f\ty0 %.0f y1 %.0f\tточек с настилом выше %d из %d, перепад до %.1f м\n", i, G->a[0], G->a[1], G->b[0], G->b[1], L, G->y0 < -1e5f ? -1e6 : G->y0, 1e6, pts, tot, maxD); } }
  fprintf(stderr, "отрезков %u, с бесконечным верхом %d, из них над ними настил (≥ 2 точек) %d\n", W.nseg, inf, deck); return 0;
}
