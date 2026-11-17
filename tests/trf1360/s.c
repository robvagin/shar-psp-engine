// 1345: ударенная машина трафика после удара в борт на 50 км/ч (стенд без езды: толчок поперёк на 14 м/с с разворотом, дальше настоящие physStep / svRejoinCheck / driveStep на данных уровня).
//   trf1345_host LEVEL.shr TRAFFIC.shr vis|hid     vis — машина в кадре весь прогон (игрок смотрит на неё), hid — кадр отвёрнут (машина вне кадра)
// Меряется: наибольший перенос за кадр (больше 2 м — «чпонг»), когда машина снова ДЕЙСТВУЮЩАЯ едущая (TRM_DRIVE) после остановки, как далеко от оси полосы в этот миг.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "traffic.h"
#ifndef LAT
#define LAT 10.f
#endif
#ifndef SPIN
#define SPIN 4.f
#endif
void bbNoteDiag(const char *kind, const char *text) { if (getenv("D1345")) printf("   [%s] %s\n", kind, text); }
static ColWorld world; static Traffic T; static Car player;
static const float DT = 1.f / 30.f;
static float laneLenX(const Traffic *t, const TrCar *c) { const TrRoad *r = &t->road[c->road]; return t->pt[r->firstPt + (uint32_t)c->lane * r->nPt + r->nPt - 1].s; }
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) { unsigned char *m = aligned_alloc(64, (sz + 63) & ~63u); fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; } return m; }
static float ground(float x, float z, float y) { float g; return colGround(&world, x, z, y + 2.f, y - 50.f, &g, 0) ? g : y; }
int main(int argc, char **argv) {
  if (argc < 4) return 2; int vis = !strcmp(argv[3], "vis");
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2; unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104 || memcmp(hdr, "SHR7", 4)) return 2;
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned end = (unsigned)ftell(f); if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); fclose(f); if (!cm || colInit(&world, cm, end - colOff)) return 2;
  FILE *g = fopen(argv[2], "rb"); if (!g) return 2; fseek(g, 0, SEEK_END); unsigned ts = (unsigned)ftell(g); unsigned char *buf = readAt(g, 0, ts); fclose(g);
  if (!buf || trafficInitMem(&T, buf, ts, 0)) return 2;
  trafficReset(&T, &world);
  int skipN = getenv("R1345") ? atoi(getenv("R1345")) : 0; int ci0 = -1; for (int fr = 0; fr < 6000 && ci0 < 0; fr++) {   // ждём, пока появится едущая машина с запасом дороги; игрок стоит где угодно (кадр ставим по ней)
    float pp0[3] = {220.f, 3.f, 172.f}, ey0[3] = {220.f, 6.f, 160.f}, fw0[3] = {0.f, 0.f, 1.f};
    player.x = 220.f; player.z = 172.f; player.y = ground(220.f, 172.f, 10.f); CarInput cz = {0, 0, 0, 1}; (void)cz;
    trafficUpdate(&T, &world, &player, 1, pp0, ey0, fw0, DT); TrEvent e0; while (trafficPopEvent(&T, &e0)) {}
    for (int i = 0; i < T.maxCars; i++) if (T.car[i].mode == TRM_DRIVE && T.car[i].elem == 0 && T.car[i].v > 4.f && T.car[i].s > 20.f && T.car[i].s + 80.f < laneLenX(&T, &T.car[i]) ) { if (skipN-- > 0) { T.car[i].mode = TRM_OFF; continue; } ci0 = i; break; } }
  if (ci0 < 0) { fprintf(stderr, "нет машины\n"); return 2; }
  if (ci0 != 0) T.car[0] = T.car[ci0];
  for (int i = 1; i < TR_MAXCAR; i++) T.car[i].mode = TRM_OFF;
  T.maxCars = 1; ci0 = 0;   // одна машина на весь прогон: другие не спавнятся и не задевают
  TrCar *c = &T.car[0]; Car *k = &c->car;
  float fx = sinf(k->h), fz = cosf(k->h);
  float lx0 = 0, lz0 = 0; int shoveF = 60, stopF = -1, backF = -1; float stopOff = -1.f, stopHead = -1.f, maxTurn = 0.f, maxJump = 0.f, offAtBack = -1.f, px0 = k->x, pz0 = k->z; int teleView = 0; float maxRetV = 0.f; int retFr = 0, slipFr = 0, hbFr = 0; unsigned tele0 = trfRejoin1345Tele; int prevMode = c->mode;
  for (int fr = 0; fr < 30 * 45; fr++) {
    if (fr == shoveF) { float rx = fz, rz = -fx; k->vel[0] = rx * LAT + fx * 3.f; k->vel[2] = rz * LAT + fz * 3.f; k->v = 0.f; k->w[1] = SPIN; c->mode = TRM_PHYS; c->stillT = 0.f; c->ramStrong = 0; c->outLaneT = 0.f; c->hitV = 3.f; }
    float ex_ = k->x - fx * 25.f, ez_ = k->z - fz * 25.f; player.x = ex_; player.z = ez_; float pp[3] = {ex_, k->y, ez_}, eye[3] = {ex_, k->y + 3.f, ez_}, fw[3] = {vis ? fx : -fx, 0.f, vis ? fz : -fz};
    diagFrame108++; if (vis) c->drawnF108 = diagFrame108;
    float bx = k->x, bz = k->z, bh = k->h;
    trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
    TrEvent e; while (trafficPopEvent(&T, &e)) {}
    if (c->mode == TRM_OFF) { printf("машина убрана на кадре %d (%.1f с)\n", fr, fr * DT); break; }
    float jump = hypotf(k->x - bx, k->z - bz), dh = fabsf(atan2f(sinf(k->h - bh), cosf(k->h - bh)));   // перенос и разворот за кадр
    if (fr > shoveF + 2 && jump > maxJump) maxJump = jump; if (fr > shoveF + 2 && dh > maxTurn) maxTurn = dh; if (fr > shoveF + 2 && (jump > 1.f || dh > .5f) && vis) teleView++;
    if (fr > shoveF && stopF < 0 && c->mode == TRM_PHYS && c->stillT > .3f) { stopF = fr; float q0[3], q1[3]; trafficRoutePoint(&T, c, 0.f, q0); trafficRoutePoint(&T, c, 4.f, q1); stopOff = hypotf(k->x - q0[0], k->z - q0[2]); stopHead = fabsf(atan2f(sinf(k->h - atan2f(q1[0] - q0[0], q1[2] - q0[2])), cosf(k->h - atan2f(q1[0] - q0[0], q1[2] - q0[2])))); }
    if (fr > shoveF && c->retOn) { retFr++; if (k->slip) slipFr++; if (k->lastHb) hbFr++; if (fabsf(k->v) > maxRetV) maxRetV = fabsf(k->v); }
    if (fr > shoveF && prevMode == TRM_PHYS && c->mode == TRM_DRIVE && backF < 0) {
      backF = fr; float p[3]; trafficRoutePoint(&T, c, 0.f, p); offAtBack = hypotf(k->x - p[0], k->z - p[2]); }
    if (getenv("D1345") && fr > shoveF && fr % 30 == 0) { float q0[3], q1[3]; trafficRoutePoint(&T, c, 0.f, q0); printf("   wait=%.1f offT=%.1f t=%.0f mode=%d retOn=%d retT=%.1f try=%d outLaneT=%.1f v=%.1f h=%.2f pos=%.1f,%.1f\n", c->retWait, c->offT, (fr - shoveF) * DT, c->mode, c->retOn, c->retT, c->retTry, c->outLaneT, k->v, k->h, k->x, k->z); }
    prevMode = c->mode; (void)lx0; (void)lz0; (void)px0; (void)pz0;
  }
  float tBack = backF >= 0 ? (backF - shoveF) * DT : -1.f;
  printf("[стоит криво: %.1f м от оси, курс %.0f° к полосе] ", stopOff, stopHead * 57.2958f); printf("trf1345 %s: наибольший перенос за кадр %.2f м, разворот за кадр %.2f рад, «чпонгов» (перенос > 1 м или разворот > 0,5 рад за кадр) в кадре %d, вернулась на полосу через %.1f с после удара (остановилась на %.1f с), от оси в этот миг %.2f м, переносов вне кадра %u, режим в конце %d noRejoin=%d outLaneT=%.1f stillT=%.1f retOn=%d elem=%d tilt_up=%.2f\n",
         vis ? "в кадре" : "вне кадра", maxJump, maxTurn, teleView, tBack, stopF >= 0 ? (stopF - shoveF) * DT : -1.f, offAtBack, trfRejoin1345Tele - tele0, c->mode, c->noRejoin, c->outLaneT, c->stillT, c->retOn, c->elem, 1.f - 2.f * (k->q[1] * k->q[1] + k->q[3] * k->q[3]));
  printf("trf1360 %s: возврат на полосу идёт %.1f с, наибольшая скорость возврата %.2f м/с, кадров с ручником %d из %d, кадров в заносе %d, вернулась через %.1f с\n", vis ? "в кадре" : "вне кадра", retFr * DT, maxRetV, hbFr, retFr, slipFr, tBack);
  return 0; }
