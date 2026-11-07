// №1159: цена кадра трафика и пешеходов на Маке (viewer/traffic.c на настоящих данных уровня). Игрок едет по кругу в нескольких местах,
// считается среднее и худшее время одного trafficUpdate (мкс) и число живых машин / пешеходов. Сравнение двух сборок traffic.c (TEST83 и текущая)
// даёт отношение; абсолют Мака не доказывает PSP (×5,8 по процессору, кэш данных 16 КБ), зато лишний обход виден как прибавка.
//   tests/trfcost/bench.sh [level.shr] [traffic.shr] [homer.shr]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "traffic.h"
#include "homer.h"
static ColWorld world; static Traffic T; static Car player;
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) { unsigned char *m = aligned_alloc(64, (sz + 63) & ~63u); fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; } return m; }
static float ground(float x, float z, float y) { float g; return colGround(&world, x, z, y + 2.f, y - 50.f, &g, 0) ? g : y; }
static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3; }
int main(int argc, char **argv) {
  const char *lvl = argc > 1 ? argv[1] : "build/level.shr", *trf = argc > 2 ? argv[2] : "build/traffic.shr";
  FILE *f = fopen(lvl, "rb"); if (!f) { printf("no %s\n", lvl); return 2; }
  unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104 || memcmp(hdr, "SHR7", 4)) { printf("bad level\n"); return 2; }
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned fsize = (unsigned)ftell(f), end = fsize; if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); fclose(f);
  if (!cm || colInit(&world, cm, end - colOff)) { printf("COL1 failed\n"); return 2; }
  FILE *g = fopen(trf, "rb"); if (!g) { printf("no %s\n", trf); return 2; }
  fseek(g, 0, SEEK_END); unsigned tsize = (unsigned)ftell(g); unsigned char *buf = readAt(g, 0, tsize); fclose(g);
  if (trafficInitMem(&T, buf, tsize, 0)) { printf("traffic init failed\n"); return 2; }
  float at[5][2]; for (int k = 0; k < 5; k++) { const TrPt *q = &T.pt[(unsigned)((k * 2 + 1) * (T.hd->nPt / 10))]; at[k][0] = q->x; at[k][1] = q->z; }   // пять точек полос по всему уровню
  const float DT = 1.f / 30.f; double sum = 0, worst = 0; long frames = 0, cars = 0, peds = 0, standing = 0;
  for (int a = 0; a < 5; a++) {
    carInit(&player, NULL, &world, at[a][0], ground(at[a][0], at[a][1], 50.f), at[a][1], 0.f);
    trafficReset(&T, &world); if (getenv("SEED")) T.seed += (unsigned)atoi(getenv("SEED")) * 7919u;   // SEED=n — другой поток случайных чисел
    for (int fr = 0; fr < 2400; fr++) {
      CarInput ci = {0, 0, 0, fr < 1200 ? 1 : 0}; carUpdate(&player, &world, &ci, DT);
      float pp[3] = {player.x, player.y, player.z}, eye[3] = {player.x, player.y + 3.f, player.z - 7.f}, fw[3] = {0, 0, 1};
      if (getenv("FREEZE") && fr == 180) for (int i = 0; i < T.maxCars; i++) { TrCar *c = &T.car[i]; if (c->mode == TRM_DRIVE) { c->mode = TRM_PHYS; c->noRejoin = 1; c->stillT = 0.f; carSetSpeed(&c->car, 0.f); c->v = 0.f; } }   // FREEZE=1: все машины съехали и встали (случай №1159)
      double t0 = now(); trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT); double dt = now() - t0;
      TrEvent e; while (trafficPopEvent(&T, &e)) {}
      if (fr > (getenv("FREEZE") ? 240 : 60)) { sum += dt; frames++; if (dt > worst) worst = dt; }
      for (int i = 0; i < T.maxCars; i++) cars += T.car[i].mode != TRM_OFF;
      for (int i = 0; i < T.maxPeds; i++) peds += T.ped[i].mode != TRP_OFF;
    }
  }
  printf("кадров %ld  среднее %.2f мкс  худший %.1f мкс  машин/кадр %.2f  пешеходов/кадр %.2f  стоящих мест %d\n", frames, sum / frames, worst, (double)cars / (frames + 300), (double)peds / (frames + 300), T.npark);
  (void)standing; return 0;
}
