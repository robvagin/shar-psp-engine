// Правка 1265 (Стена 107): локаторы миссий пакета рядом с точкой (X Z R): имя, координаты, расстояние
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "world.h"
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 7 || bwData(argv[1], argv[2], argv[3])) return 2;
  float X = (float)atof(argv[4]), Z = (float)atof(argv[5]), R = (float)atof(argv[6]); bwNew(&W); const MsnFile *f = &W.msn.f;
  for (uint32_t i = 0; i < f->nlocs; i++) { const MsnLoc *l = &f->locs[i]; float d = hypotf(l->pos[0] - X, l->pos[2] - Z); if (d <= R) printf("%s\t%.1f %.1f %.1f\td=%.0f flags=%x\n", msn_str(f, l->name), l->pos[0], l->pos[1], l->pos[2], d, l->flags); }
  return 0; }
