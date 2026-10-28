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
  float x0 = atof(argv[2]), x1 = atof(argv[3]), z0 = atof(argv[4]), z1 = atof(argv[5]), st = atof(argv[6]), ytop = argc > 7 ? atof(argv[7]) : 200.f;
  printf("z\\x "); for (float x = x0; x <= x1; x += st) printf("%5.0f", x); printf("\n");
  for (float z = z0; z <= z1; z += st) { printf("%5.0f ", z);
    for (float x = x0; x <= x1; x += st) { float g, n[3]; if (colGround(&W, x, z, ytop, -200.f, &g, n)) printf("%5.1f", g); else printf("   --"); } printf("\n"); }
  return 0;
}
