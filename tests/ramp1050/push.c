#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "col.h"
static ColWorld W;
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *d = malloc((size + 15) & ~15); fread(d, 1, size, f); fclose(f);
  unsigned co = *(unsigned *)(d + 92), po = *(unsigned *)(d + 96), pv = *(unsigned *)(d + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&W, d + co, ce - co)) return 3;
  colRamp1050 = atoi(argv[2]);
  float z = atof(argv[3]), y = atof(argv[4]);
  for (float x = -82.f; x >= -90.f; x -= 0.5f) {
    CarBox b; memset(&b, 0, sizeof b); b.x = x; b.z = z; b.fx = -1.f; b.fz = 0.f; b.hw = 1.05f; b.hl = 2.2f; b.y = y; b.lo = .35f; b.hi = 1.6f; b.fsupOn = 1; b.fsup = y;
    float mx, mz, nx, nz; int n = colPushCar(&W, &b, &mx, &mz, &nx, &nz);
    printf("x %.1f: n %d mtv(%.2f %.2f) maxDepth %.2f", x, n, mx, mz, b.maxDepth);
    for (int k = 0; k < n; k++) printf(" | t%d i%u n(%.2f %.2f) d %.2f", b.type[k], b.index[k], b.nx[k], b.nz[k], b.depth[k]);
    printf("\n"); }
  return 0;
}
