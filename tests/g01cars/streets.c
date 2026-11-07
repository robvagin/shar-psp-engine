// сверка g01-cars №1126: замер езды машины игрока по улицам уровня (автопилот теста), без эмулятора
//   streets LEVEL PLAYER MSN SECS SEED DTMODE      DTMODE: 0 — 1/60 ровно; 1 — 1/30 ровно; 2 — 1/30 ± 4 мс (шум таймера, без прилипания к сетке);
//                                                           3 — 1/45 ровно; 4 — 1/30 с редкими провалами до 1/15
// печатает: переворотов, крен/тангаж на поворотах, скачки скорости и неровность пути по кадрам
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "smkpilot.h"
float bwDtNow = 1.f / 60.f;
static BotWorld W;
static unsigned rs = 1; static float rnd(void) { rs = rs * 1664525u + 1013904223u; return (rs >> 8) / 16777216.f; }
static int cmpf(const void *a, const void *b) { float x = *(const float *)a, y = *(const float *)b; return x < y ? -1 : x > y; }
static float pct(float *v, int n, float p) { if (n <= 0) return 0.f; qsort(v, n, sizeof *v, cmpf); return v[(int)(p * (n - 1))]; }
int main(int argc, char **argv) {
  if (argc < 7 || bwData(argv[1], argv[2], argv[3])) { fprintf(stderr, "usage: streets LEVEL PLAYER MSN SECS SEED DTMODE\n"); return 2; }
  float secs = (float)atof(argv[4]); unsigned seed = (unsigned)atoi(argv[5]); int mode = atoi(argv[6]); rs = seed * 2654435761u + 7;
  bwNew(&W); printf("машина: %s\n", bwCarName()); Traffic *t = (Traffic *)bwTraffic(); if (!t || !t->hd) { printf("нет трафика\n"); return 2; }
  SmkPilot P; smkPilotInit(&P, seed, 16.f); P.props = &W.props; P.sp = &W.sp;
  if (getenv("PILOT_AT")) { float sx, sz, y0 = 50.f, sh; sscanf(getenv("PILOT_AT"), "%f,%f,%f,%f", &sx, &sz, &y0, &sh); float gy; colGround(bwCol(), sx, sz, y0, -1e6f, &gy, 0); bwPlace(&W, sx, gy + .3f, sz, sh); }
  bwDtNow = 1.f / 60.f; for (int f = 0; f < 60; f++) bwStep(&W, 0, 0.f, 0.f);
  P.lx = W.car.x; P.lz = W.car.z;
  int N = (int)(secs * 60); float *rollT = malloc(N * sizeof(float)), *pitchT = malloc(N * sizeof(float)), *jit = malloc(N * sizeof(float)), *dv = malloc(N * sizeof(float));
  int twoW = 0; int nt = 0, nj = 0, flips = 0, inFlip = 0, maxFlipRun = 0, flipRun = 0, airFr = 0, frames = 0, jumpsDv = 0; double tot = 0.0, simT = 0.0; float vPrev = 0.f, px = W.car.x, pz = W.car.z, hPrev = W.car.h, maxRoll = 0.f, maxPitch = 0.f; int starts = 0, lastEv = -999;
  FILE *posesF = getenv("STREETS_POSES") ? fopen(getenv("STREETS_POSES"), "w") : 0;   // позиции хода по улицам для corner.c
  for (int f = 0; f < N; f++) {
    float dt;
    switch (mode) { case 1: dt = 1.f / 30.f; break; case 2: dt = 1.f / 30.f + (rnd() - .5f) * .008f; break; case 3: dt = 1.f / 45.f; break; case 4: dt = rnd() < .05f ? 1.f / 15.f : 1.f / 30.f; break; default: dt = 1.f / 60.f; }
    bwDtNow = dt; simT += dt;
    CarInput in; smkPilotStep(&P, t, &W.car, dt, &in);
    { static float fv = -1.f; if (fv < 0.f) { const char *e = getenv("STREETS_FASTV"); fv = e ? (float)atof(e) : 0.f; } if (fv > 0.f) { in.brake = 0.f; in.throttle = W.car.v < fv ? 1.f : 0.f; } }   // правка 435: как «flag drivefast V» окна Кадры: газ до V м/с без сброса на поворотах и без торможения
    unsigned b = 0; if (in.throttle > .5f) b |= GP_R; if (in.brake > .1f) b |= GP_L; if (in.handbrake > .5f) b |= GP_SQUARE;
    bwStep(&W, b, in.steer, 0.f);
    const Car *c = &W.car; frames++;
    float qw = c->q[0], qx = c->q[1], qz = c->q[3];   // кватернион (w, x, y, z): ось Y кузова в мире
    float upy = 1.f - 2.f * (qx * qx + qz * qz); float tilt = acosf(fmaxf(-1.f, fminf(1.f, upy))) * 57.29578f; (void)qw;
    if (tilt > 70.f) { flipRun++; if (flipRun > maxFlipRun) maxFlipRun = flipRun; if (!inFlip) { flips++; inFlip = 1; if (getenv("STREETS_FLIPLOG")) printf("  переворот t=%.2f с: v %.1f м/с руль %.2f газ %.1f тормоз %.1f крен %.0f° тангаж %.0f° колёс %d на земле %d поз (%.1f,%.1f,%.1f) курс %.2f\n", simT, c->v, in.steer, in.throttle, in.brake, c->roll * 57.29578f, c->pitch * 57.29578f, c->wheels, c->grounded, c->x, c->y, c->z, c->h); } } else { inFlip = 0; flipRun = 0; }
    if (!c->grounded) airFr++;
    { static float t0 = -1, t1 = -1; if (t0 < 0) { const char *e = getenv("STREETS_TRACE"); if (e) sscanf(e, "%f,%f", &t0, &t1); else t0 = t1 = 0; } if (t1 > 0 && simT >= t0 && simT <= t1) printf("  кадр t=%.2f v %.1f руль %.2f газ %.1f крен %.0f тангаж %.0f колёс %d нзем %d y %.2f курс %.2f конт %d тип %d нормаль %.2f,%.2f wall %.1f тип %d мат %d x %.1f z %.1f | wedge %.2f/%d/%d perch %.2f/%d glide %.2f st32 %d/%d dl %d belly %.2f embed %.2f flipT %.2f air %.2f nogr %.2f revWhy %d pen %.2f\n", simT, c->v, in.steer, in.throttle, c->roll * 57.29578f, c->pitch * 57.29578f, c->wheels, c->grounded, c->y, c->h, c->contacts, c->ctype[0], c->cnx[0], c->cnz[0], c->wallV, c->wallType, c->wallMat, c->x, c->z, c->wedgeT, c->wedgeTried, c->wedgeEsc, c->perchT, c->perchEsc, c->glideT, c->stuck32, c->escapes32, c->deadendLifts, c->bellyT, c->embedT, c->flipT, c->airT, c->noGroundT, c->revWhy, c->pen); }
    if (c->wheels <= 2 && fabsf(c->roll) > .785f) twoW++;   // правка 434: кадров на двух колёсах с креном больше 45°
    float d = hypotf(c->x - px, c->z - pz); float v = d / dt; px = c->x; pz = c->z;
    float yaw = fabsf(c->h - hPrev); if (yaw > 3.14159f) yaw = 6.28318f - yaw; float yr = yaw / dt; hPrev = c->h;
    if (posesF && f > 300 && f % 240 == 0 && v > 9.f && yr < .12f && c->grounded && fabsf(c->roll) < .08f) fprintf(posesF, "%.2f %.2f %.2f %.4f\n", c->x, c->y + .1f, c->z, c->h);
    if (f > 120 && v > 5.f && vPrev > 5.f) {
      if (fabsf(v - vPrev) > 1.5f) jumpsDv++;
      dv[nj] = fabsf(v - vPrev); jit[nj] = fabsf(d / (.5f * (v + vPrev) * dt) - 1.f); nj++;
    }
    if (f > 120 && v > 5.f && yr > .5f && c->grounded) { rollT[nt] = fabsf(c->roll) * 57.29578f; pitchT[nt] = fabsf(c->pitch) * 57.29578f; nt++; }
    if (getenv("STREETS_EV") && c->grounded && v > 3.f && fabsf(c->roll) > .5f && (f - lastEv) > 120) { lastEv = f; printf("  событие t=%.1f с: крен %.1f° тангаж %.1f° v %.1f м/с руль %.2f газ %.1f тормоз %.1f поз (%.1f,%.1f,%.1f) колёс на земле ?\n", simT, c->roll * 57.29578f, c->pitch * 57.29578f, v, in.steer, in.throttle, in.brake, c->x, c->y, c->z); }
    if (c->grounded && v > 3.f) { if (fabsf(c->roll) > maxRoll) maxRoll = fabsf(c->roll); if (fabsf(c->pitch) > maxPitch) maxPitch = fabsf(c->pitch); }
    if (v < .5f) starts++;
    vPrev = v; tot += d;
  }
  printf("кадров %d (%.0f с), путь %.0f м, застреваний бота %d, стояла кадров %d\n", frames, simT, tot, P.stuckN, starts);
  printf("перевороты (наклон > 70°): %d, самый долгий %.2f с, кадров в воздухе %d (%.1f%%)\n", flips, maxFlipRun * (mode == 1 ? 1.f / 30 : mode == 3 ? 1.f / 45 : 1.f / 60), airFr, 100.f * airFr / frames);
  printf("на двух колёсах (крен > 45°): %d кадров\n", twoW);
  printf("на земле при скорости > 3: макс крен %.1f°, макс тангаж %.1f°\n", maxRoll * 57.29578f, maxPitch * 57.29578f);
  printf("на поворотах (кадров %d): крен p50 %.1f p95 %.1f; тангаж p50 %.1f p95 %.1f\n", nt, pct(rollT, nt, .5f), pct(rollT, nt, .95f), pct(pitchT, nt, .5f), pct(pitchT, nt, .95f));
  printf("рывки: скачок скорости кадр к кадру > 1,5 м/с: %d раз; |Δv| p50 %.3f p99 %.3f; неровность пути p50 %.4f p99 %.4f\n", jumpsDv, pct(dv, nj, .5f), pct(dv, nj, .99f), pct(jit, nj, .5f), pct(jit, nj, .99f));
  if (posesF) fclose(posesF);
  return 0;
}
