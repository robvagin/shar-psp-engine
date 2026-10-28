#include <stdio.h>
#include <stdlib.h>
#include "col.h"
static ColWorld W;
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *d = malloc((size + 15) & ~15); fread(d, 1, size, f); fclose(f);
  unsigned co = *(unsigned *)(d + 92), po = *(unsigned *)(d + 96), pv = *(unsigned *)(d + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&W, d + co, ce - co)) return 3;
  float x0 = atof(argv[2]), z0 = atof(argv[3]), x1 = atof(argv[4]), z1 = atof(argv[5]); int n = atoi(argv[6]); float yt = atof(argv[7]);
  for (int i = 0; i <= n; i++) { float t = (float)i / n, x = x0 + (x1 - x0) * t, z = z0 + (z1 - z0) * t, g, nn[3]; int r = colGround(&W, x, z, yt, -200.f, &g, nn);
    printf("%7.1f %7.1f  %s y %6.2f  n(%5.2f %5.2f %5.2f)\n", x, z, r ? "ok" : "--", g, r ? nn[0] : 0, r ? nn[1] : 0, r ? nn[2] : 0); }
  return 0;
}
