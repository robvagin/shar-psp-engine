// Гейт 1264 (Бабл 107, A2.9/Б.6): уличная гонка выключает пешеходов и стоящие машины на всё время миссии и возвращает их на уборке.
// Хост: viewer/traffic.c на build/level.shr + build/traffic.shr (L1). Последовательность игры: Load пропов -> trafficStreetRace(1); Unload стоит в той же инициализации и признак НЕ снимает
// (снимает уборка миссии -> trafficStreetRace(0)). Красный: -DTRF_RACEHOLD1264_OLD=1 — прежнее (признак снимался сразу после установки, стоящие не трогались).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "traffic.h"
int (*sceneReadyHook)(const ColWorld *w, float x, float z, int *failed);
static ColWorld world; static Traffic T; static const float DT = 1.f / 30.f;
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) { unsigned char *m = aligned_alloc(64, (sz + 63) & ~63u); fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; } return m; }
static void frames(const float p[3], int n, float yaw0) {
  for (int s = 0; s < n; s++) { float yaw = yaw0 + s * 0.05f; float fwd[3] = {sinf(yaw), -.08f, cosf(yaw)}, eye[3] = {p[0] - fwd[0] * 6.f, p[1] + 2.f, p[2] - fwd[2] * 6.f};
    trafficUpdate(&T, &world, 0, 0, p, eye, fwd, DT); TrEvent e; while (trafficPopEvent(&T, &e)) {} }
}
static int liveParked(void) { int n = 0; for (int i = 0; i < T.npark; i++) n += !T.park[i].gone; return n; }
static int livePeds(void) { int n = 0; for (int i = 0; i < T.maxPeds; i++) n += T.ped[i].mode != TRP_OFF; return n; }
int main(int argc, char **argv) {
  const char *lvl = argc > 1 ? argv[1] : "build/level.shr", *trf = argc > 2 ? argv[2] : "build/traffic.shr";
  FILE *f = fopen(lvl, "rb"); if (!f) { printf("no %s\n", lvl); return 2; }
  unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104 || memcmp(hdr, "SHR7", 4)) { printf("bad level\n"); return 2; }
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned end = (unsigned)ftell(f);
  if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); fclose(f);
  if (!cm || colInit(&world, cm, end - colOff)) { printf("COL1 failed\n"); return 2; }
  FILE *g = fopen(trf, "rb"); if (!g) { printf("no %s\n", trf); return 2; }
  fseek(g, 0, SEEK_END); unsigned tsize = (unsigned)ftell(g); unsigned char *buf = readAt(g, 0, tsize); fclose(g);
  int r = trafficInitMem(&T, buf, tsize, 0); if (r) { printf("trafficInitMem %d\n", r); return 2; }
  trafficReset(&T, &world);
  /* игрок ходит по воротам загрузчиков, пока не наберётся хоть сколько-то живых стоящих и пешеходов */
  float p[3] = {0, 0, 0}; int bestPark = 0, bestPeds = 0, bestG = 0;
  for (int gI = 0; gI < T.nLoader && gI < 40; gI++) {
    float q[3] = {T.loader[gI].c[0], T.loader[gI].c[1], T.loader[gI].c[2]}; frames(q, 200, 0.f);
    int lp = liveParked(), pd = livePeds();
    if (lp + pd > bestPark + bestPeds) { bestPark = lp; bestPeds = pd; bestG = gI; }
  }
  { float q[3] = {T.loader[bestG].c[0], T.loader[bestG].c[1], T.loader[bestG].c[2]}; memcpy(p, q, sizeof p); frames(p, 300, 0.f); }
  int pk0 = liveParked(), pd0 = livePeds();
  printf("ворота %d: до гонки живых стоящих %d, пешеходов %d\n", bestG, pk0, pd0);
  int fails = 0;
#define CHECK(c, what) do { int ok_ = (c); printf("  %-4s %s\n", ok_ ? "ok" : "FAIL", what); fails += !ok_; } while (0)
  CHECK(pk0 > 0, "(0) стенд: до гонки стоящие машины есть");
  /* старт гонки: Load пропов; Unload в той же инициализации */
  trafficStreetRace(&T, 1);
#if TRF_RACEHOLD1264_OLD
  trafficStreetRace(&T, 0);   // прежний propsRaceC2: Unload сразу снимал признак
#endif
  int maxPk = 0, maxPd = 0;
  for (int k = 0; k < 20; k++) { frames(p, 30, 0.f); int a = liveParked(), b = livePeds(); if (a > maxPk) maxPk = a; if (b > maxPd) maxPd = b; }
  printf("за 20 с гонки: макс. живых стоящих %d, макс. пешеходов %d\n", maxPk, maxPd);
  CHECK(maxPk == 0, "(1) на гонке стоящих машин нет");
  CHECK(maxPd == 0, "(2) на гонке пешеходов нет");
  /* уборка миссии */
  trafficStreetRace(&T, 0);
  int backPk = 0;
  for (int k = 0; k < 10; k++) { frames(p, 30, 1.f + k); int a = liveParked(); if (a > backPk) backPk = a; }
  printf("после уборки: макс. живых стоящих %d (до гонки было %d)\n", backPk, pk0);
  CHECK(backPk > 0, "(3) после уборки стоящие возвращаются");
  printf("RESULT %s\n", fails ? "FAIL" : "PASS");
  trafficFree(&T); return fails ? 1 : 0;
}
