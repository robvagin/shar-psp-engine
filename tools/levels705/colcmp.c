// colcmp A.shr B.shr: COL1 двух файлов поэлементно: всё, кроме заборов (seg), должно совпасть побайтно; у заборов — что изменилось
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
unsigned homerSolidFrame = 0;
static ColWorld ld(const char *p) { FILE *f = fopen(p, "rb"); fseek(f, 0, SEEK_END); unsigned n = ftell(f); rewind(f); unsigned char *b = malloc(n); if (fread(b, 1, n, f) != n) exit(2); fclose(f);
  unsigned co = *(unsigned *)(b + 92), po = *(unsigned *)(b + 96), pv = *(unsigned *)(b + 88), end = n; if (po > co && po < end) end = po; if (pv > co && pv < end) end = pv;
  ColWorld w; if (colInit(&w, b + co, end - co)) exit(2); return w; }
#define EQ(name, cnt, arr, sz) printf("  %-6s %6u / %6u %s\n", name, a.cnt, b.cnt, (a.cnt == b.cnt && !memcmp(a.arr, b.arr, (size_t)a.cnt * sz)) ? "равны" : "РАЗНЫЕ")
int main(int argc, char **argv) {
  ColWorld a = ld(argv[1]), b = ld(argv[2]);
  EQ("tri", ntri, tri, sizeof(ColTri)); EQ("box", nbox, box, sizeof(ColBox)); EQ("cyl", ncyl, cyl, sizeof(ColCyl));
  EQ("zone", nzone, zone, sizeof(ColZone)); EQ("pad", npad, pad, sizeof(ColPad)); EQ("wrench", nwrench, wrench, sizeof(ColWrench));
  EQ("phone", nphone, phone, sizeof(ColWrench)); EQ("death", ndeath, death, sizeof(ColPad)); EQ("phonex", nphonex, phonex, sizeof(ColPhoneX));
  printf("  seg    %6u / %6u (заборы)\n", a.nseg, b.nseg);
  return 0; }
