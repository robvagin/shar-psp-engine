// 1350: машина миссии и погоня Hit & Run с одним именем модели (cPolice: L4 m1, m7, L5 m6). Проверки поиска и постановки по имени (viewer/msncar.c):
//   1) погоня уже стоит (police = 1), ставится машина миссии cPolice — погоня остаётся, машин две, mcFind отдаёт машину миссии;
//   2) mcRemove убирает машину миссии, а не погоню (удаление по mcFind).
// Красный: -DHNRPOLNAME1350_OLD=1 — машина миссии занимает место погони (машин одна).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msncar.h"
static ColWorld world;
static unsigned char *readFile(const char *p, unsigned *n) { FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET); unsigned char *b = aligned_alloc(64, ((size_t)s + 63) & ~(size_t)63); if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) return 0; fclose(f); *n = (unsigned)s; return b; }
void bbNoteDiag(const char *k, const char *t) { (void)k; (void)t; }
int main(int argc, char **argv) {
  unsigned n; unsigned char *lvl = readFile(argv[1], &n); if (!lvl || memcmp(lvl, "SHR7", 4)) return 2;
  unsigned pv = *(unsigned *)(lvl + 88), co = *(unsigned *)(lvl + 92), po = *(unsigned *)(lvl + 96), ce = n; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&world, lvl + co, ce - co)) return 2;
  MsnCars m; mcInit(&m); float p1[3] = {214.f, 3.5f, 172.f}, p2[3] = {230.f, 3.5f, 172.f};
  McCar *pol = mcSpawn(&m, &world, 0, "cPolice", p1, 0.f, 1, "chase", "", ""); if (pol) pol->police = 1;
  McCar *ms = mcSpawn(&m, &world, 0, "cPolice", p2, 0.f, 1, "evade", "", "");
  int on = 0, police = 0; for (int i = 0; i < m.n; i++) if (m.c[i].on) { on++; police += m.c[i].police; }
  McCar *f = mcFind(&m, "cPolice");
  int ok1 = on == 2 && police == 1 && f && !f->police && f == ms;
  mcRemove(&m, "cPolice"); on = 0; police = 0; for (int i = 0; i < m.n; i++) if (m.c[i].on) { on++; police += m.c[i].police; }
  int ok2 = on == 1 && police == 1;
  printf("машины после постановки: %s; снята машина миссии, а не погоня: %s\n", ok1 ? "две, найдена миссии" : "НЕТ (одна/не та)", ok2 ? "да" : "НЕТ");
  printf("%s\n", ok1 && ok2 ? "RESULT: PASS" : "RESULT: FAIL"); return ok1 && ok2 ? 0 : 1; }
