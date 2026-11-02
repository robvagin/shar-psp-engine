// Быстрая проверка на CPU Мака: новое поведение окна g04-homer (сверка с исходником, 02.11.2026) против старого.
// Один и тот же файл собирается дважды: без флагов (новое) и со всеми HOMER_*_OLD=1 (TEST81). Печатает числа; ключ --expect new|old проверяет ожидание.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "homer.h"
#include "car.h"
static ColWorld world; static int fails; static const char *mode = "new";
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) { unsigned char *m = aligned_alloc(16, (sz + 15) & ~15u); fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; } return m; }
static void chk(const char *what, float got, float newLo, float newHi, float oldLo, float oldHi) {
  int isNew = !strcmp(mode, "new"); float lo = isNew ? newLo : oldLo, hi = isNew ? newHi : oldHi; int ok = got >= lo && got <= hi; if (!ok) fails++;
  printf("%-4s %-52s %8.3f  (%s %.2f..%.2f)\n", ok ? "ok" : "FAIL", what, got, mode, lo, hi);
}
static const float DT = 1.f / 60.f;
static void step(Homer *H, float mx, float my, int dash, int jump, int attack) {
  HomerInput in; memset(&in, 0, sizeof in); in.mx = mx; in.my = my; in.dash = (unsigned char)dash; in.jump = (unsigned char)jump; in.attack = (unsigned char)attack; in.camYaw = 0.f;
  homerUpdate(H, &world, NULL, &in, DT); homerPose(H);
}
static float hv(Homer *H) { return sqrtf(H->vx * H->vx + H->vz * H->vz); }
int main(int argc, char **argv) {
  const char *lvl = argc > 1 ? argv[1] : "build/level.shr", *hom = argc > 2 ? argv[2] : "build/homer.shr"; if (argc > 3) mode = argv[3];
  FILE *f = fopen(lvl, "rb"); if (!f) { printf("no %s\n", lvl); return 1; }
  unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104 || memcmp(hdr, "SHR7", 4)) { printf("bad level\n"); return 1; }
  const float *hd = (const float *)(hdr + 12); float spx = hd[6], spy = hd[7], spz = hd[8];
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned end = (unsigned)ftell(f); if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); if (!cm || colInit(&world, cm, end - colOff)) { printf("COL1 failed\n"); return 1; }
  SkinModel *m = skinLoad(hom); if (!m) { printf("no %s\n", hom); return 1; }
  static Homer H; if (homerInit(&H, m)) { printf("homerInit failed\n"); return 1; }
  // 1. ходьба на половине стика: установившаяся скорость
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0);
  for (int i = 0; i < 90; i++) step(&H, 0, .5f, 0, 0, 0);
  chk("1.1 ходьба на половине стика, м/с (новое 1,45; старое 2,0)", hv(&H), 1.35f, 1.55f, 1.9f, 2.1f);
  // 2. бег: скорость сразу 8 при половине стика
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0);
  for (int i = 0; i < 12; i++) step(&H, 0, .5f, 1, 0, 0);
  chk("1.4/P02-3 бег на половине стика через 0,2 с, м/с", hv(&H), 7.5f, 8.2f, 3.0f, 4.1f);
  // 3. разворот на бегу: стик назад на полную через 0,5 с
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 30; i++) step(&H, 0, 1, 1, 0, 0);
  float h0 = H.h; for (int i = 0; i < 30; i++) step(&H, 0, -1, 1, 0, 0); float dh = fabsf(remainderf(H.h - h0, 6.2831853f)) * 57.2958f;
  chk("1.3 поворот лица на бегу за 0,5 с, град (дуга)", dh, 90.f, 135.f, 170.f, 181.f);
  // 4. стик назад меньше 0,6 после остановки: не разворачивает
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 30; i++) step(&H, 0, 0, 0, 0, 0);
  h0 = H.h; for (int i = 0; i < 60; i++) step(&H, 0, -.4f, 0, 0, 0); dh = fabsf(remainderf(H.h - h0, 6.2831853f)) * 57.2958f;
  chk("P02-1 стик назад 0,4: поворот лица за 1 с, град", dh, 0.f, 5.f, 150.f, 181.f);
  // 5. воздух: прыжок на бегу — горизонталь режется до 4
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 40; i++) step(&H, 0, 1, 1, 0, 0);
  step(&H, 0, 1, 1, 1, 0); for (int i = 0; i < 12; i++) step(&H, 0, 1, 1, 0, 0);
  chk("2.3 прыжок с разбега: горизонталь в воздухе через 0,2 с, м/с", hv(&H), 3.5f, 4.5f, 6.5f, 8.2f);
  // 6. воздух без стика: скорость не гасится
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 40; i++) step(&H, 0, 1, 0, 0, 0);
  step(&H, 0, 0, 0, 1, 0); for (int i = 0; i < 20; i++) step(&H, 0, 0, 0, 0, 0);
  chk("P02-9 в воздухе без стика, м/с (новое 4,0)", hv(&H), 3.8f, 4.1f, 0.1f, 3.4f);
  // 6б. круг 3: воздух без бега, стик держится — потолок отрыва, скорость не растёт
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 40; i++) step(&H, 0, 1, 0, 0, 0);
  step(&H, 0, 1, 0, 1, 0); float vmax = 0; for (int i = 0; i < 40; i++) { step(&H, 0, 1, 0, 0, 0); if (!H.grounded && hv(&H) > vmax) vmax = hv(&H); }
  chk("круг 3: воздух со стиком без бега, максимум горизонтали, м/с", vmax, 3.5f, 4.05f, 3.5f, 9.f);
  // 7. пинок: сразу стоп
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 40; i++) step(&H, 0, 1, 0, 0, 0);
  step(&H, 0, 0, 0, 0, 1); for (int i = 0; i < 2; i++) step(&H, 0, 0, 0, 0, 0);
  chk("P02-17 пинок на ходу: скорость через 2 кадра, м/с", hv(&H), 0.f, .01f, 1.5f, 4.1f);
  // 8. прыжок с места: заводка
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0);
  step(&H, 0, 0, 0, 1, 0); for (int i = 0; i < 3; i++) step(&H, 0, 0, 0, 0, 0);
  chk("P02-7 прыжок с места: высота над землёй через 4 кадра, м", H.y - H.groundY, 0.f, .02f, .05f, 2.f);
  // 9. стомп: вертикаль
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 40; i++) step(&H, 0, 1, 0, 0, 0);
  step(&H, 0, 1, 0, 1, 0); for (int i = 0; i < 20; i++) step(&H, 0, 1, 0, 0, 0); step(&H, 0, 1, 0, 1, 0); for (int i = 0; i < 6; i++) step(&H, 0, 1, 0, 0, 0);
  float vyBefore = H.vy; step(&H, 0, 0, 0, 0, 1); step(&H, 0, 0, 0, 0, 0);
  chk("P02-12 стомп: вертикаль на замахе (ввод удара) — состояние STOMP", (float)(H.state == HS_STOMP), 1.f, 1.f, 1.f, 1.f);
  (void)vyBefore;
  // 10. круг 5: взрыв машины — Гомер в машине, ждёт, уворачивается клипом (новое) или летит (старое); куда и как далеко
  { static Car car; carInit(&car, NULL, &world, spx, spy, spz, 0.f); HomerCar hc; homerCarFrom(&hc, &car, .62f);
    homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); homerSetInCar(&H, &hc);
    for (int i = 0; i < 30; i++) { homerUpdate(&H, &world, &hc, &(HomerInput){0}, DT); homerPose(&H); }
    homerEject(&H, &world, &hc, car.x + 3.f, car.z, 1.f);
    float tFoot = -1, maxd = 0; int ph4 = 0;
    for (int i = 0; i < 360; i++) { homerUpdate(&H, &world, &hc, &(HomerInput){0}, DT); homerPose(&H); if (H.ejPh == 4) ph4 = 1;
      float d = hypotf(H.x - car.x, H.z - car.z); if (d > maxd) maxd = d; if (tFoot < 0 && H.state == HS_FOOT) tFoot = i * DT; }
    chk("круг 5: взрыв — клип уворота играл (ejPh 4)", (float)ph4, 1.f, 1.f, 0.f, 0.f);
    chk("круг 5: взрыв — дальше всего от центра машины, м", maxd, 3.f, 9.f, 2.f, 9.f);
    chk("круг 5: взрыв — на ногах через, с", tFoot, 0.5f, 3.5f, 0.5f, 4.f); }
  printf("%s: %d провалов\n", mode, fails); return fails;
}
