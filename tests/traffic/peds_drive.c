// №1159 (урок ночи 03.11: «0 прохожих» в сцене езды по улице оказался настоящей ошибкой, а не случайностью). Игрок едет 10 м/с по полосам дорог уровня, три seed трафика;
// считаем секунды, в которые рядом есть хоть один прохожий (режим не «выключен»). Условие строгое по сумме трёх seed: ≥ MIN_SEC секунд (по уровню). Красный контроль
// (-DPEDSDRIVE_PLANT=1): прохожих нет вовсе (maxPeds = 0) — проверка обязана упасть.
//   tests/traffic/peds_drive.sh [level.shr traffic.shr]   (по умолчанию L1 из build/ship_stage или build/)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "traffic.h"
#ifndef MIN_SEC
#define MIN_SEC 120   // на Маке по 7 уровням 246…320 из 480; порог — половина худшего
#endif
static ColWorld world; static Traffic T; static Car player;
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) { unsigned char *m = aligned_alloc(64, (sz + 63) & ~63u); fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; } return m; }
int main(int argc, char **argv) {
  const char *lvl = argc > 1 ? argv[1] : "build/level.shr", *trf = argc > 2 ? argv[2] : "build/traffic.shr";
  FILE *f = fopen(lvl, "rb"); if (!f) { printf("no %s\n", lvl); return 2; }
  unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104 || memcmp(hdr, "SHR7", 4)) { printf("bad level\n"); return 2; }
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned end = (unsigned)ftell(f); if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); fclose(f);
  if (!cm || colInit(&world, cm, end - colOff)) { printf("COL1 failed\n"); return 2; }
  FILE *g = fopen(trf, "rb"); if (!g) { printf("no %s\n", trf); return 2; }
  fseek(g, 0, SEEK_END); unsigned tsize = (unsigned)ftell(g); unsigned char *buf = readAt(g, 0, tsize); fclose(g);
  if (trafficInitMem(&T, buf, tsize, 0)) { printf("traffic init failed\n"); return 2; }
  const float DT = 1.f / 30.f; int total = 0, seedSec[3] = {0, 0, 0};
  for (int sd = 0; sd < 3; sd++) {
    for (int k = 0; k < 4; k++) {   // четыре дороги по уровню, на каждой до 40 с езды (дальше — следующая дорога)
      uint32_t r = (uint32_t)((k * 2 + 1) * (T.hd->nRoad / 8)) % T.hd->nRoad; float s = 0.f;
      const TrPt *P = T.pt + T.road[r].firstPt; int n = T.road[r].nPt; if (n < 2) continue;
      carInit(&player, NULL, &world, P[0].x, P[0].y, P[0].z, 0.f);
      trafficReset(&T, &world); T.seed += (unsigned)(sd + 1) * 7919u + (unsigned)k * 104729u;
#ifdef PEDSDRIVE_PLANT
      T.maxPeds = 0;
#endif
      int secHit = 0, fr = 0;
      for (int i = 0; i < 40 * 30; i++, fr++) {
        s += 10.f * DT; if (s > P[n - 1].s - 1.f) { r = (r + 1) % T.hd->nRoad; P = T.pt + T.road[r].firstPt; n = T.road[r].nPt; s = 0.f; if (n < 2) break; }
        int q = 0; while (q + 2 < n && P[q + 1].s < s) q++;
        float u = (s - P[q].s) / fmaxf(1e-3f, P[q + 1].s - P[q].s), x = P[q].x + (P[q + 1].x - P[q].x) * u, z = P[q].z + (P[q + 1].z - P[q].z) * u, y = P[q].y + (P[q + 1].y - P[q].y) * u;
        float h = atan2f(P[q + 1].x - P[q].x, P[q + 1].z - P[q].z);
        carInit(&player, NULL, &world, x, y, z, h); carSetSpeed(&player, 10.f);
        float pp[3] = {x, y, z}, eye[3] = {x - sinf(h) * 7.f, y + 3.f, z - cosf(h) * 7.f}, fw[3] = {sinf(h), 0.f, cosf(h)};
        trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
        TrEvent e; while (trafficPopEvent(&T, &e)) {}
        if (fr % 30 == 29) { int any = 0; for (int p = 0; p < T.maxPeds; p++) any |= T.ped[p].mode != TRP_OFF; secHit += any; }
      }
      seedSec[sd] += secHit; total += secHit;
    }
  }
  printf("пешеходы при езде: секунд с прохожим по seed 1/2/3: %d / %d / %d, сумма %d (нужно ≥ %d из 480)\n", seedSec[0], seedSec[1], seedSec[2], total, MIN_SEC);
  int ok = total >= MIN_SEC; printf("%s\n", ok ? "PASS" : "FAIL"); return ok ? 0 : 1;
}
