// Гейт правки 139(2) на Маке: правило зон стоящих машин (viewer/traffic.c, TRAFFIC_ZONE_PARK=1) на настоящих данных
// (build/level.shr COL1 + build/traffic.shr TRF1 с воротами загрузчиков). Игрок проходит через ворота всех уличных
// загрузчиков (ближайший следующий), камера смотрит НАЗАД (на район, который ворота выгружают — худший случай: задний
// ход, взгляд назад); в каждых воротах — оборот камеры на месте (4 с).
// Проверки каждого кадра:
//   (1) ни одна стоящая не исчезает и не появляется в кадре (кадр теста: 90 м, конус 45° — уже, чем inView движка);
//   (2) пропущено только место вне зоны: спрятанное место в загруженных файлах допустимо, только пока его точка может быть
//       в кадре (возврат ждёт, когда камера отвернётся); за спиной камеры или дальше 100 м оно обязано вернуться сразу;
// после оборота в воротах:
//   (3) места ближе 60 м к воротам в загруженных файлах — живы;
// за весь проход: (4) правило что-то делает — минимум живых < всех мест.
//   ./host level.shr traffic.shr [swap]   swap — подброска: маски ворот наоборот («;» и «:» перепутаны)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "traffic.h"
int (*sceneReadyHook)(const ColWorld *w, float x, float z, int *failed);   // заглушка: в игре определён в homer.c (traffic.c его читает)

#if !TRAFFIC_ZONE_PARK
#error "собирать с -DTRAFFIC_ZONE_PARK=1"
#endif

static ColWorld world; static Traffic T;
static const float DT = 1.f / 30.f;
static int vanishInFrame, appearInFrame, goneInZone, deferred, nearDead, minLive = 1 << 30, activated, badLazy;
static int surelyOut(const float eye[3], const float fwd[3], float x, float y, float z) {   // точно вне кадра
  float dx = x - eye[0], dy = y + 1.f - eye[1], dz = z - eye[2], d = sqrtf(dx * dx + dy * dy + dz * dz);
  return d > 100.f || (d > 8.f && dx * fwd[0] + dy * fwd[1] + dz * fwd[2] < 0.f);
}

static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) {
  unsigned char *m = aligned_alloc(64, (sz + 63) & ~63u);
  fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; }
  return m;
}
static int inFrame(const float eye[3], const float fwd[3], const Car *k) {
  float dx = k->x - eye[0], dy = k->y + 1.f - eye[1], dz = k->z - eye[2], d = sqrtf(dx * dx + dy * dy + dz * dz);
  if (d > 90.f || d < 1e-3f) return d <= 90.f;
  return (dx * fwd[0] + dy * fwd[1] + dz * fwd[2]) / d > .7071f;
}
static void frame(const float p[3], float yaw) {
  float fwd[3] = {sinf(yaw), -.08f, cosf(yaw)}, eye[3] = {p[0] - fwd[0] * 6.f, p[1] + 2.f, p[2] - fwd[2] * 6.f};
  static int was[256], wasSeen[256]; static int init;
  int n = T.npark < 256 ? T.npark : 256;
  if (!init) { for (int i = 0; i < n; i++) { was[i] = !T.park[i].gone; wasSeen[i] = 0; } init = 1; }
  for (int i = 0; i < n; i++) wasSeen[i] = inFrame(eye, fwd, &T.park[i].car);
  Car keep[256]; for (int i = 0; i < n; i++) keep[i] = T.park[i].car;
  trafficUpdate(&T, &world, 0, 0, p, eye, fwd, DT);
  TrEvent e; while (trafficPopEvent(&T, &e)) {}
  int live = 0;
  for (int i = 0; i < n; i++) {
    int now = !T.park[i].gone; live += now;
    if (now && !was[i]) activated++;   // Коллизия-96: место включилось — должно быть посажено на землю (lazy снят)
    { float ex_ = T.parkd[i].x - p[0], ez_ = T.parkd[i].z - p[2]; if (now && T.park[i].lazy && ex_ * ex_ + ez_ * ez_ < 170.f * 170.f) badLazy++; }   // ближе 170 м включённое место обязано стоять на земле
    if (was[i] && !now && wasSeen[i]) { vanishInFrame++; if (vanishInFrame < 4) printf("  vanish in frame: %.12s\n", T.parkd[i].name); }
    if (!was[i] && now && inFrame(eye, fwd, &T.park[i].car)) { appearInFrame++; if (appearInFrame < 4) printf("  appear in frame: %.12s\n", T.parkd[i].name); }
    if (!now && (!T.parkd[i].zmask || (T.parkd[i].zmask & T.zoneMask))) {
      if (surelyOut(eye, fwd, T.parkd[i].x, T.parkd[i].y, T.parkd[i].z)) goneInZone++; else deferred++; }
    was[i] = now; (void)keep;
  }
  if (live < minLive) minLive = live;
}

int main(int argc, char **argv) {
  const char *lvl = argc > 1 ? argv[1] : "build/level.shr", *trf = argc > 2 ? argv[2] : "build/traffic.shr";
  int swap = argc > 3 && !strcmp(argv[3], "swap");
  FILE *f = fopen(lvl, "rb"); if (!f) { printf("no %s\n", lvl); return 2; }
  unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104 || memcmp(hdr, "SHR7", 4)) { printf("bad level\n"); return 2; }
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned end = (unsigned)ftell(f);
  if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); fclose(f);
  if (!cm || colInit(&world, cm, end - colOff)) { printf("COL1 failed\n"); return 2; }
  FILE *g = fopen(trf, "rb"); if (!g) { printf("no %s\n", trf); return 2; }
  fseek(g, 0, SEEK_END); unsigned tsize = (unsigned)ftell(g); unsigned char *buf = readAt(g, 0, tsize); fclose(g);
  TrHeader *h = (TrHeader *)buf; uint32_t trueMask[64] = {0};   // маски ворот по данным (до подброски) — для проверки (3)
  for (unsigned i = 0; i < h->nLoader && i < 64; i++) trueMask[i] = ((TrZone *)(buf + h->offLoader))[i].group;
  if (swap && h->nLoader) {   // подброска: неверное прочтение строки загрузчика — загружено ровно то, что выгружено
    uint32_t all = 0; TrPark *k = (TrPark *)(buf + h->offPark); TrZone *L = (TrZone *)(buf + h->offLoader);
    for (unsigned i = 0; i < h->nPark; i++) all |= k[i].zmask;
    for (unsigned i = 0; i < h->nLoader; i++) all |= L[i].group;
    for (unsigned i = 0; i < h->nLoader; i++) L[i].group = ~L[i].group & all;
  }
  int r = trafficInitMem(&T, buf, tsize, 0); if (r) { printf("trafficInitMem %d\n", r); return 2; }
  extern long colStat[4]; long g0_ = colStat[1];
  trafficReset(&T, &world);
  long resetGround = colStat[1] - g0_;
  printf("%s: %d мест, %d ворот загрузчиков%s\n", trf, T.npark, T.nLoader, swap ? " (ПОДБРОСКА swap)" : "");
  if (!T.nLoader) { printf("FAIL нет ворот загрузчиков в traffic.shr\n"); return 1; }
  int ng = T.nLoader, done[64] = {0}, cur = 0; float p[3] = {T.loader[0].c[0], T.loader[0].c[1], T.loader[0].c[2]}, yaw = 0.f;
  for (int step = 0; step < ng && step < 64; step++) {
    int best = -1; float bd = 1e30f;   // ближайшие ещё не пройденные ворота
    for (int i = 0; i < ng && i < 64; i++) if (!done[i]) { float dx = T.loader[i].c[0] - p[0], dz = T.loader[i].c[2] - p[2], d = dx * dx + dz * dz; if (d < bd) { bd = d; best = i; } }
    cur = best; done[cur] = 1; const TrZone *Z = &T.loader[cur];
    float dx = Z->c[0] - p[0], dz = Z->c[2] - p[2], L = sqrtf(dx * dx + dz * dz); int nf = (int)(L / (15.f * DT)) + 1;
    if (L > .1f) yaw = atan2f(dx, dz) + 3.1415927f;   // подъезд к воротам с камерой назад: в кадре — район, который выгружается
    float a[3] = {p[0], p[1], p[2]};
    for (int s = 1; s <= nf; s++) { float u = (float)s / nf; float q[3] = {a[0] + dx * u, a[1] + (Z->c[1] - a[1]) * u, a[2] + dz * u}; frame(q, yaw); p[0] = q[0]; p[1] = q[1]; p[2] = q[2]; }
    for (int s = 0; s < 120; s++) frame(p, yaw + s * (6.2831853f / 120.f));   // оборот камеры в воротах
    int dead = 0, near = 0, live = 0;
    for (int i = 0; i < T.npark; i++) {
      live += !T.park[i].gone;
      float ex = T.parkd[i].x - p[0], ez = T.parkd[i].z - p[2];
      if (ex * ex + ez * ez > 60.f * 60.f || !(T.parkd[i].zmask & trueMask[cur])) continue;
      near++; if (T.park[i].gone) dead++;
    }
    nearDead += dead;
    printf("  ворота %2d (%6.0f %6.0f) маска %04x: живых %2d/%d, рядом в зоне %d, из них спрятано %d\n", cur, p[0], p[2], (unsigned)Z->group, live, T.npark, near, dead);
  }
  int fails = 0;
#define CHECK(c, what) do { int ok_ = (c); printf("  %-4s %s\n", ok_ ? "ok" : "FAIL", what); fails += !ok_; } while (0)
  printf("ждали кадра (место в зоне, камера на нём) %d место-кадров\n", deferred);
  printf("исчезло в кадре %d, появилось в кадре %d, спрятано в загруженной зоне %d, рядом с воротами спрятано %d, минимум живых %d/%d\n",
         vanishInFrame, appearInFrame, goneInZone, nearDead, minLive, T.npark);
  CHECK(vanishInFrame == 0 && appearInFrame == 0, "(1) в кадре ничего не исчезает и не появляется");
  CHECK(goneInZone == 0, "(2) спрятаны только места вне загруженных файлов");
  CHECK(nearDead == 0, "(3) места у ворот в загруженных файлах живы");
  CHECK(minLive < T.npark, "(4) правило работает: живых меньше всех мест");
  CHECK(badLazy == 0 && activated > 0, "(6) включённое место ближе 170 м посажено на землю, и включения были");
  printf("опросов земли при сбросе трафика: %ld (мест %d), включений в прогоне %d, включённых ближе 170 м без посадки (кадров) %d\n", resetGround, T.npark, activated, badLazy);
  CHECK(resetGround == 0, "(5) сброс трафика не опрашивает землю (места сажаются при включении зоны, а не все сразу за сотни метров от старта)");
  printf("RESULT %s\n", fails ? "FAIL" : "PASS");
  trafficFree(&T); return fails ? 1 : 0;
}
