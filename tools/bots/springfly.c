// №1223 (Кольцо-96): Гомер пешком на пружинах (подкидывающие точки JMP1) L4: полёт по нашей физике в 10 фазах вращения шестерни АЭС.
//   springfly LEVEL HOMER [--phases N] [--pad имя] [--obj N|-1] [--off] -> строки по полётам, RESULT: OK, если ни один полёт не пришёл ближе R к ящикам живых объектов
// Для каждой пружины и фазы: Гомер ждёт на земле у пружины (пока шестерня крутится), потом ставится в центр объёма пружины и летит 4 с.
// Пишется: вершина дуги (макс. высота, где), посадка, мин. расстояние до ближайшего ЖИВОГО ящика (объекты ANM1) и до ближайшего любого ящика коллизии.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
extern int homerBounceOff;   // viewer/homer.c: отрицательный контроль — пружины выключены
static BotWorld W;
static float boxDist(const ColBox *b, float x, float y, float z) {   // расстояние от точки до ориентированного ящика
  float d[3] = {x - b->c[0], y - b->c[1], z - b->c[2]}; const float *A[3] = {b->ax, b->ay, b->az}; float s = 0;
  for (int k = 0; k < 3; k++) { float p = d[0] * A[k][0] + d[1] * A[k][1] + d[2] * A[k][2]; float e = fabsf(p) - b->h[k]; if (e > 0) s += e * e; }
  return sqrtf(s);
}
int main(int argc, char **argv) {
  if (argc < 3 || bwData(argv[1], argv[2], NULL)) { fprintf(stderr, "usage: springfly LEVEL HOMER [--phases N] [--pad имя] [--radius R]\n"); return 2; }
  int nph = 10; const char *only = NULL; float R = 1.0f; int obj = 39;   // obj: объект ANM1, чьи ящики считаем «кольцом» (39 = шестерня АЭС L4; -1 — все живые)
  for (int i = 3; i + 1 < argc; i++) { if (!strcmp(argv[i], "--phases")) nph = atoi(argv[i + 1]); else if (!strcmp(argv[i], "--pad")) only = argv[i + 1]; else if (!strcmp(argv[i], "--radius")) R = (float)atof(argv[i + 1]); else if (!strcmp(argv[i], "--obj")) obj = atoi(argv[i + 1]); }
  for (int i = 3; i < argc; i++) if (!strcmp(argv[i], "--off")) homerBounceOff = 1;   // красный контроль: пружины не срабатывают -> «не подбросило», FAIL
  memset(&W, 0, sizeof W); if (bwNew(&W)) return 2;
#ifndef BW_HAVE_AO
  puts("нет animobj"); return 2;
#else
  const ColWorld *cw = bwCol(); int bad = 0, nfly = 0;
  static unsigned char live[1 << 16]; memset(live, 0, sizeof live); int nring = 0;
  for (unsigned q = 0; q < W.ao.nvol; q++) if (W.ao.vol[q].kind == 0 && W.ao.vol[q].col < sizeof live && (obj < 0 || W.ao.vol[q].obj == obj)) { live[W.ao.vol[q].col] = 1; nring++; }
  printf("ящиков %u, площадок-пружин %u, живых объёмов %u, из них проверяемых (объект %d) %d\n", cw->nbox, cw->npad, W.ao.nvol, obj, nring);
  for (uint32_t p = 0; p < cw->npad; p++) {
    const ColPad *P = &cw->pad[p]; if (only && strcmp(P->name, only)) continue;
    if (!only && strncmp(P->name, "spring", 6)) continue;
    for (int ph = 0; ph < nph; ph++) {
      // на землю у пружины (на 3 м от центра объёма — вне радиуса срабатывания), ждать фазу
      homerPlace(&W.homer, cw, P->c[0] + 3.f, P->c[1] + 1.f, P->c[2], 0.f);
      for (int f = 0; f < 90; f++) bwStep(&W, 0, 0, 0);
      for (int f = 0; f < ph * 36; f++) bwStep(&W, 0, 0, 0);   // 0,6 с на фазу: 10 фаз = 6 с = период шестерни №39
      homerPlace(&W.homer, cw, P->c[0], P->c[1] + .4f, P->c[2], 0.f);
      float x0 = W.homer.x, y0 = W.homer.y, z0 = W.homer.z, ymax = -1e9f, ax = 0, az = 0, dl = 1e9f, da = 1e9f; int bi = -1, bl = -1; float tl = 0; int bounced = 0;
      for (int f = 0; f < 240; f++) {
        bwStep(&W, 0, 0, 0);
        const Homer *H = &W.homer; if (H->bouncing || H->bounces) bounced = 1;
        if (H->y > ymax) { ymax = H->y; ax = H->x; az = H->z; }
        for (uint32_t b = 0; b < cw->nbox; b++) { float d = boxDist(&cw->box[b], H->x, H->y + .9f, H->z); if (live[b] ? d < dl : d < da) { if (live[b]) { dl = d; bl = (int)b; tl = f / 60.f; } else { da = d; bi = (int)b; } } }
        if (f > 20 && H->grounded && f > 60) break;
      }
      nfly++;
      printf("S %s ф%d: старт (%.1f %.1f %.1f) подброс %s вершина y=%.1f в (%.1f %.1f) цель (%.1f %.1f %.1f) посадка (%.1f %.1f %.1f) до ящиков объекта %.1f м (ящик %d, %.1f с) до любого другого %.1f м (%d)\n",
             P->name, ph, x0, y0, z0, bounced ? "да" : "НЕТ", ymax, ax, az, P->apex[0], P->apex[1], P->apex[2], W.homer.x, W.homer.y, W.homer.z, dl, bl, tl, da, bi);
      int reach = ymax >= P->apex[1] - .3f;   // долетел до вершины дуги (точки-цели локатора)
      if (!bounced || !reach || dl < R) bad++;
    }
  }
  printf("полётов %d, плохих %d (не подбросило, не долетело до цели или ближе %.1f м к ящикам объекта)\nRESULT %s\n", nfly, bad, R, bad ? "FAIL" : "OK");
  return bad != 0;
#endif
}
