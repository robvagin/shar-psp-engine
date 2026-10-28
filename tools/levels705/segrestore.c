// №905 (Глюки): вернуть заборам верх из файла TEST74: segrestore NEW.shr OLD.shr X Z NX NZ [X Z NX NZ ...] — по точке находится звено OLD (на линии, та же нормаль),
// у всех звеньев NEW, лежащих на этой линии в её пределах, y1 := y1 старого. Размер и остальные байты не меняются.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "col.h"
unsigned homerSolidFrame = 0;
static unsigned char *ldb(const char *p, ColWorld *w, unsigned *n_) { FILE *f = fopen(p, "rb"); fseek(f, 0, SEEK_END); unsigned n = ftell(f); rewind(f); unsigned char *b = aligned_alloc(16, (n + 15) & ~15); if (fread(b, 1, n, f) != n) exit(2); fclose(f);
  unsigned co = *(unsigned *)(b + 92), po = *(unsigned *)(b + 96), pv = *(unsigned *)(b + 88), end = n; if (po > co && po < end) end = po; if (pv > co && pv < end) end = pv;
  if (colInit(w, b + co, end - co)) exit(2); *n_ = n; return b; }
int main(int argc, char **argv) {
  ColWorld a, o; unsigned na, no; unsigned char *ba = ldb(argv[1], &a, &na); ldb(argv[2], &o, &no); int total = 0;
  for (int k = 3; k + 3 < argc; k += 4) { float x = atof(argv[k]), z = atof(argv[k + 1]), nx = atof(argv[k + 2]), nz = atof(argv[k + 3]); int oj = -1;
    for (unsigned j = 0; j < o.nseg && oj < 0; j++) { const ColSeg *q = &o.seg[j]; if (q->n[0] * nx + q->n[1] * nz < .98f) continue; float dx = q->b[0] - q->a[0], dz = q->b[1] - q->a[1], L2 = dx * dx + dz * dz; if (L2 < 1e-6f) continue;
      float t = ((x - q->a[0]) * dx + (z - q->a[1]) * dz) / L2; if (t < -.02f || t > 1.02f) continue; float px = q->a[0] + t * dx - x, pz = q->a[1] + t * dz - z; if (px * px + pz * pz < .15f * .15f) oj = (int)j; }
    if (oj < 0) { printf("точка (%.1f,%.1f): звено в OLD не найдено\n", x, z); return 4; }
    const ColSeg *q = &o.seg[oj]; float dx = q->b[0] - q->a[0], dz = q->b[1] - q->a[1], L = hypotf(dx, dz), tx = dx / L, tz = dz / L, cnt = 0; int hit = 0;
    for (unsigned i = 0; i < a.nseg; i++) { const ColSeg *s = &a.seg[i]; if (s->n[0] * q->n[0] + s->n[1] * q->n[1] < .98f) continue;
      float da = (s->a[0] - q->a[0]) * q->n[0] + (s->a[1] - q->a[1]) * q->n[1], db = (s->b[0] - q->a[0]) * q->n[0] + (s->b[1] - q->a[1]) * q->n[1]; if (fabsf(da) > .03f || fabsf(db) > .03f) continue;
      float t0 = (s->a[0] - q->a[0]) * tx + (s->a[1] - q->a[1]) * tz, t1 = (s->b[0] - q->a[0]) * tx + (s->b[1] - q->a[1]) * tz; if (t0 > t1) { float u = t0; t0 = t1; t1 = u; }
      if (fminf(t1, L) - fmaxf(t0, 0.f) < .15f) continue; if (fabsf(s->y1 - q->y1) < 1e-3f) { cnt++; continue; }
      printf("  звено NEW %u (%.1f,%.1f)-(%.1f,%.1f): верх %.1f -> %.1f\n", i, s->a[0], s->a[1], s->b[0], s->b[1], s->y1, q->y1); *(float *)&s->y1 = q->y1; hit++; total++; }
    printf("старое звено %d (%.1f,%.1f)-(%.1f,%.1f) верх %.1f: исправлено новых %d, уже равных %.0f\n", oj, q->a[0], q->a[1], q->b[0], q->b[1], q->y1, hit, cnt); }
  FILE *fo = fopen(argv[1], "wb"); fwrite(ba, 1, na, fo); fclose(fo); printf("итого исправлено: %d\n", total); return 0; }
