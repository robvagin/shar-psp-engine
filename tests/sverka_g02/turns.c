// №1092 (Роб, TEST83, уровень 5): трафик в поворотах «носится». Бот на CPU Мака: игрок следует за машиной трафика, 5 машин ездят и рождаются как в игре; на каждом кадре каждой
// едущей по рельсам машины меряем скорость, поперечное ускорение (скорость × скорость поворота курса) и продольное; при въезде в перекрёсток — скорость входа.
// Запуск: tests/sverka_g02/turns.sh [level.shr] [traffic.shr] [минут]. Печатает строку TURNS для сравнения «до / после» (-D…_OLD=1).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../../viewer/traffic.c"
void uiFlash(void) {}
static ColWorld world; static Traffic T; static Car player;
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) { unsigned char *m = aligned_alloc(64, (sz + 63) & ~63u); fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; } return m; }
static int cmpf(const void *a, const void *b) { float x = *(const float *)a, y = *(const float *)b; return x < y ? -1 : x > y; }
int main(int argc, char **argv) {
  const char *lvl = argc > 1 ? argv[1] : "build/levels/l5/level.shr", *trf = argc > 2 ? argv[2] : "build/levels/l5/traffic.shr"; float minutes = argc > 3 ? (float)atof(argv[3]) : 20.f;
  FILE *f = fopen(lvl, "rb"); if (!f) return 2; unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104) return 2;
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned end = (unsigned)ftell(f); if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); fclose(f); if (!cm || colInit(&world, cm, end - colOff)) return 2;
  FILE *g = fopen(trf, "rb"); if (!g) return 2; fseek(g, 0, SEEK_END); unsigned ts = (unsigned)ftell(g); unsigned char *buf = readAt(g, 0, ts); fclose(g);
  if (!buf || trafficInitMem(&T, buf, ts, 0)) return 2;
  const float DT = 1.f / 60.f; T.maxCars = T.hd->nSlot < TR_MAXCAR ? (int)T.hd->nSlot : TR_MAXCAR; T.maxPeds = 0; T.npark = 0; T.seed = 0x2545F491u;
  trafficReset(&T, &world); T.maxCars = T.hd->nSlot < TR_MAXCAR ? (int)T.hd->nSlot : TR_MAXCAR; T.npark = 0; T.maxPeds = 0;
  int nf = (int)(minutes * 60.f * 60.f); static float lat[2000000], latJ[1000000]; int nj_ = 0; static float entry[100000]; static float lon[2000000]; int nl = 0, ne = 0, nlon = 0, hard = 0, fast = 0; float prevH[TR_MAXCAR], prevV[TR_MAXCAR], prevX[TR_MAXCAR], prevZ[TR_MAXCAR]; static float hist[TR_MAXCAR][16]; static int hn[TR_MAXCAR]; int prevElem[TR_MAXCAR], prevMode[TR_MAXCAR];
  for (int i = 0; i < TR_MAXCAR; i++) { prevMode[i] = TRM_OFF; prevElem[i] = 0; }
  float px = 0, pz = 0, ph = 0; for (uint32_t r = 0; r < T.hd->nRoad; r++) if (roadOk(&T, (int)r) && T.road[r].length > 40.f) { float q[3]; lanePos(&T, (int)r, 0, 10.f, q); px = q[0]; pz = q[2]; break; }
  carInit(&player, NULL, &world, px, 0.f, pz, 0.f); { float g; if (colGround(&world, px, pz, 300.f, -300.f, &g, 0)) player.y = g; }
  double maxLat = 0; int follow = -1;
  for (int fr = 0; fr < nf; fr++) {
    if (follow < 0 || T.car[follow].mode != TRM_DRIVE) { follow = -1; for (int i = 0; i < T.maxCars; i++) if (T.car[i].mode == TRM_DRIVE) { follow = i; break; } }
    if (follow >= 0) { const Car *k = &T.car[follow].car; px = k->x; pz = k->z; ph = k->h; player.x = px + cosf(ph) * 30.f; player.z = pz - sinf(ph) * 30.f; player.y = k->y; player.h = ph; }   // игрок в 30 м сбоку: не мешает, зона вокруг машины живёт
    float pp[3] = {player.x, player.y, player.z}, eye[3] = {px - sinf(ph) * 12.f, player.y + 4.f, pz - cosf(ph) * 12.f}, fw[3] = {sinf(ph), 0, cosf(ph)};
    T.nextra = 0; trafficUpdate(&T, &world, &player, 0, pp, eye, fw, DT);
    if (getenv("TDBG") && fr % 600 == 0) { int n = 0; for (int i = 0; i < T.maxCars; i++) n += T.car[i].mode == TRM_DRIVE; fprintf(stderr, "dbg fr=%d drive=%d follow=%d slots=%d nRoad=%u \n", fr, n, follow, T.maxCars, T.hd->nRoad); }
    for (int i = 0; i < T.maxCars; i++) { TrCar *c = &T.car[i];
      if (getenv("TSTUCK") && c->mode == TRM_DRIVE && fr % 1800 == 0 && fr > 7200 && c->v < .1f) fprintf(stderr, "stuck fr=%d car=%d road=%d lane=%d elem=%d s=%.1f L=%.1f next=%d relJ=%d retryT=%.1f waitT=%.1f vt=%.2f bzVmax=%.1f junc=%d beh=%d honkT=%.1f dbgrem=%.2f jamT=%.1f\n", fr, i, c->road, c->lane, c->elem, c->s, c->elem ? c->bzLen : laneLen(&T, c->road, c->lane), c->next, c->relJ, c->retryT, c->waitT, c->vt, c->bzVmax, c->junc, c->junc >= 0 ? (int)T.junc[c->junc].behaviour : -1, c->honkT, 0.f, c->jamT);
      if (c->mode == TRM_DRIVE && prevMode[i] == TRM_DRIVE && c->v > .5f && hypotf(c->car.x - prevX[i], c->car.z - prevZ[i]) < c->v * DT * 2.f + .3f) {   // без прыжков (машина убрана и родилась в том же слоте)
        hist[i][hn[i] & 15] = c->car.h; hn[i]++; float dh = hn[i] > 16 ? fabsf(wrapPi(c->car.h - hist[i][hn[i] & 15])) / (15.f * DT) : 0.f, a = c->v * dh; double dv = (c->v - prevV[i]) / DT;   // курс за 0,25 с: вершины ломаной полосы не дают пиков double dv = (c->v - prevV[i]) / DT;
        if (c->elem == 1 && nj_ < 1000000) latJ[nj_++] = a; if (nl < 2000000) lat[nl++] = a; if (a > maxLat) maxLat = a; hard += a > 4.f; fast += a > 8.f; if (nlon < 2000000) lon[nlon++] = (float)dv;
        if (c->elem == 1 && prevElem[i] == 0 && ne < 100000) entry[ne++] = c->v; }
      if (c->mode != TRM_DRIVE || prevMode[i] != TRM_DRIVE) hn[i] = 0; prevH[i] = c->car.h; prevV[i] = c->v; prevX[i] = c->car.x; prevZ[i] = c->car.z; prevElem[i] = c->elem; prevMode[i] = c->mode; }
  }
  qsort(latJ, nj_, sizeof(float), cmpf); { int h4 = 0; for (int i = 0; i < nj_; i++) h4 += latJ[i] > 4.5f; printf("В ПЕРЕКРЁСТКЕ кадров %d: поперечное м/с² p50 %.2f p95 %.2f p99 %.2f макс %.2f, кадров >4,5: %d (%.0f %%)\n", nj_, nj_ ? latJ[nj_ / 2] : 0, nj_ ? latJ[(int)(nj_ * .95)] : 0, nj_ ? latJ[(int)(nj_ * .99)] : 0, nj_ ? latJ[nj_ - 1] : 0, h4, nj_ ? 100.f * h4 / nj_ : 0); }
  qsort(lat, nl, sizeof(float), cmpf); qsort(entry, ne, sizeof(float), cmpf); qsort(lon, nlon, sizeof(float), cmpf);
  printf("TURNS кадров езды %d | поперечное м/с²: медиана %.2f p95 %.2f p99 %.2f макс %.2f, кадров >4: %d, >8: %d | входов в перекрёсток %d: скорость входа медиана %.1f p95 %.1f макс %.1f | продольное: торможение p1 %.1f мин %.1f, разгон p99 %.1f\n",
    nl, nl ? lat[nl / 2] : 0, nl ? lat[(int)(nl * .95)] : 0, nl ? lat[(int)(nl * .99)] : 0, maxLat, hard, fast, ne, ne ? entry[ne / 2] : 0, ne ? entry[(int)(ne * .95)] : 0, ne ? entry[ne - 1] : 0,
    nlon ? lon[(int)(nlon * .01)] : 0, nlon ? lon[0] : 0, nlon ? lon[(int)(nlon * .99)] : 0);
  return 0;
}
