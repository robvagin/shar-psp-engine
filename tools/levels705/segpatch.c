// №905 (Глюки): вернуть куску забора прежний верх (+1e6, «стена») прямо в файле: segpatch LEVEL.shr X Z NX NZ [X Z NX NZ ...]
// кусок ищется по середине (0,3 м) и нормали; y1 := 1e6 (как в TEST74). Размер файла и остальные байты те же. Причина — в README: съезд с горки на 22 м/с проезжал новый конечный верх
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "col.h"
unsigned homerSolidFrame = 0;
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); unsigned n = ftell(f); rewind(f); unsigned char *b = aligned_alloc(16, (n + 15) & ~15); if (fread(b, 1, n, f) != n) return 2; fclose(f);
  unsigned co = *(unsigned *)(b + 92), po = *(unsigned *)(b + 96), pv = *(unsigned *)(b + 88), end = n; if (po > co && po < end) end = po; if (pv > co && pv < end) end = pv;
  ColWorld w; if (colInit(&w, b + co, end - co)) return 2;
  if ((const unsigned char *)w.seg < b || (const unsigned char *)(w.seg + w.nseg) > b + n) { puts("seg не внутри буфера"); return 3; }
  int done = 0;
  for (int k = 2; k + 3 < argc + 0; k += 4) { float x = atof(argv[k]), z = atof(argv[k + 1]), nx = atof(argv[k + 2]), nz = atof(argv[k + 3]); int hit = 0;
    for (unsigned i = 0; i < w.nseg; i++) { const ColSeg *s = &w.seg[i]; float mx = (s->a[0] + s->b[0]) / 2, mz = (s->a[1] + s->b[1]) / 2;
      if (fabsf(mx - x) < .3f && fabsf(mz - z) < .3f && s->n[0] * nx + s->n[1] * nz > .98f) { float *y1 = (float *)&s->y1; printf("кусок %u (%.1f,%.1f): верх %.1f -> 1000000\n", i, mx, mz, s->y1); *y1 = 1000000.f; hit++; done++; } }
    if (hit != 1) { printf("ожидался один кусок у (%.1f,%.1f), найдено %d\n", x, z, hit); return 4; } }
  FILE *o = fopen(argv[1], "wb"); fwrite(b, 1, n, o); fclose(o); printf("исправлено кусков: %d\n", done); return 0; }
