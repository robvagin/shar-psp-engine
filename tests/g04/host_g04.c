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
static int stepSnds, stepSndsConc;   // круг 6: счёт вызовов sndFootstep (сильное определение вместо слабой заглушки homer.c)
void sndFootstep(int ground, float x, float y, float z) { (void)x; (void)y; (void)z; stepSnds++; if (ground == 0) stepSndsConc++; }
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
  chk("1.4/P02-3 бег на половине стика через 0,2 с, м/с (разгон 13 м/с² по записи PS2: 2,6)", hv(&H), 2.3f, 2.9f, 3.0f, 4.1f);
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
  chk("P02-7 прыжок с места: высота над землёй через 4 кадра, м (заводка 0,02 с по записи PS2; старое — на весь клип)", H.y - H.groundY, .2f, .45f, .05f, 2.f);
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
  // 10б. круг 6: звук шага на каждое касание ноги. Ходьба на половине стика 3 с (желаемая скорость 1,45 > 1): шагов больше 3 и меньше 12 (по кадру их было бы 180); по бетону (набор 0)
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0);
  stepSnds = stepSndsConc = 0; for (int i = 0; i < 180; i++) step(&H, 0, .5f, 0, 0, 0);
  chk("круг 6: звук шага, вызовов за 3 с ходьбы на половине стика", (float)stepSnds, 4.f, 11.f, 0.f, 0.f);
  chk("круг 6: звук шага по умолчанию — бетон (набор 0), доля", stepSnds ? (float)stepSndsConc / stepSnds : 0.f, 1.f, 1.f, 0.f, 0.f);
  // стоит на месте — шагов нет
  stepSnds = 0; for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0);
  chk("круг 6: звук шага, стоит 1 с", (float)stepSnds, 0.f, 0.f, 0.f, 0.f);
  // 10в. круг 6: оглушение осой (homerStun 0,5 с): стик не двигает, прыжок и удар проходят; после 0,5 с идёт как обычно. Старое: только мерцание, ходьба продолжается
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0);
  { float x0 = H.x, z0 = H.z; homerStun(&H, .5f); for (int i = 0; i < 24; i++) step(&H, 0, 1, 0, 0, 0);
    chk("круг 6: оглушён 0,4 с со стиком вперёд, пройдено, м", hypotf(H.x - x0, H.z - z0), 0.f, .05f, .3f, 3.f);
    for (int i = 0; i < 30; i++) step(&H, 0, 1, 0, 0, 0);
    chk("круг 6: после оглушения 0,5 с ещё 0,5 с стика, пройдено, м", hypotf(H.x - x0, H.z - z0), .3f, 3.f, .5f, 4.f); }
  // 10г. круг 6: в интерьере бег и прыжок отключены
  homerSetIndoor(&H, 1); homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0);
  for (int i = 0; i < 40; i++) step(&H, 0, 1, 1, 0, 0);
  chk("круг 6: в интерьере ○ + стик вперёд, скорость, м/с (ходьба 4)", hv(&H), 3.5f, 4.1f, 7.5f, 8.2f);
  step(&H, 0, 0, 0, 1, 0); for (int i = 0; i < 40; i++) step(&H, 0, 0, 0, 0, 0);
  { float hmax = 0.f; step(&H, 0, 0, 0, 1, 0); for (int i = 0; i < 50; i++) { step(&H, 0, 0, 0, 0, 0); if (H.y - H.groundY > hmax) hmax = H.y - H.groundY; }
    chk("круг 6: в интерьере ✕ — высота прыжка, м", hmax, 0.f, .02f, .3f, 3.f); }
  homerSetIndoor(&H, 0);
  // 10д. круг 6: нажатие кнопки (△ у переключателя) играет клип только пока стоит на земле
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0);
  chk("круг 6: homerPressButton на земле, играет (1)", (float)homerPressButton(&H, NAN), 1.f, 1.f, 0.f, 0.f);
  // 10е. круг 6: пинок на земле — сфера r 0,5 на 0,7 вперёд и 0,7 вверх от ног через 0,25 с (старое: r больше, 0,45 вверх, 8 кадров)
  { HomerEvent e; while (homerPopEvent(&H, &e)) {}
    homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0);
    while (homerPopEvent(&H, &e)) {}
    step(&H, 0, 0, 0, 0, 1); float tHit = -1.f, up = -9.f, rad = -1.f;
    for (int i = 0; i < 60 && tHit < 0.f; i++) { step(&H, 0, 0, 0, 0, 0); while (homerPopEvent(&H, &e)) if (e.type == HOMER_EV_HIT && tHit < 0.f) { tHit = (i + 2) * DT; up = e.y - H.y; rad = e.r; } }
    chk("круг 6: пинок — высота центра удара над ногами, м", up, .65f, .75f, .4f, .5f);
    chk("круг 6: пинок — радиус сферы, м", rad, .45f, .55f, .6f, 2.f);
    chk("круг 6: пинок — время удара от нажатия, с", tHit, .22f, .30f, .0f, .30f); }
  // 10ж. круг 6: приземление — сила пыли в событии: после одного прыжка 0,5, после двойного 0,7; после стомпа 1,0 и кольцо
  { HomerEvent e; float pw1 = -1.f, pw2 = -1.f, pwS = -1.f, ringS = -1.f;
    homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0); while (homerPopEvent(&H, &e)) {}
    step(&H, 0, 0, 0, 1, 0); for (int i = 0; i < 140; i++) { step(&H, 0, 0, 0, 0, 0); while (homerPopEvent(&H, &e)) if (e.type == HOMER_EV_LAND) pw1 = e.dx; }
    chk("круг 6: сила пыли после одного прыжка", pw1, .45f, .55f, -.01f, .01f);
    homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0); while (homerPopEvent(&H, &e)) {}
    step(&H, 0, 0, 0, 1, 0); { int n = 0; while (H.grounded && n++ < 60) step(&H, 0, 0, 0, 0, 0); } for (int i = 0; i < 20; i++) step(&H, 0, 0, 0, 0, 0);
    step(&H, 0, 0, 0, 1, 0); for (int i = 0; i < 140; i++) { step(&H, 0, 0, 0, 0, 0); while (homerPopEvent(&H, &e)) if (e.type == HOMER_EV_LAND) pw2 = e.dx; }
    chk("круг 6: сила пыли после двойного прыжка", pw2, .65f, .75f, -.01f, .01f);
    homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0); while (homerPopEvent(&H, &e)) {}
    step(&H, 0, 0, 0, 1, 0); { int n = 0; while (H.grounded && n++ < 60) step(&H, 0, 0, 0, 0, 0); } for (int i = 0; i < 20; i++) step(&H, 0, 0, 0, 0, 0);
    step(&H, 0, 0, 0, 1, 0); for (int i = 0; i < 6; i++) step(&H, 0, 0, 0, 0, 0); step(&H, 0, 0, 0, 0, 1);
    for (int i = 0; i < 200; i++) { step(&H, 0, 0, 0, 0, 0); while (homerPopEvent(&H, &e)) if (e.type == HOMER_EV_LAND) { pwS = e.dx; ringS = e.dz; } }
    chk("круг 6: сила пыли после стомпа", pwS, .95f, 1.05f, -.01f, .01f);
    chk("круг 6: кольцо после стомпа", ringS, .95f, 1.05f, -.01f, .01f); }
  // 10з. круг 6: герой уровня — имя для таблицы рта и размах рта 10 с речи (верхняя граница по таблице: Гомер 24,1°, Барт 29,9, Лиза 27,0, Мардж 24,1, Апу 28,1; нижняя −18° у всех)
  { const char *hero = getenv("G04_HERO"); if (!hero) hero = "homer";
    const char *nm = homerCharName(); chk("круг 6: имя героя для рта совпадает с героем пакета (1)", (float)!strcmp(nm, hero), 1.f, 1.f, hero[0] == 'h' ? 1.f : 0.f, hero[0] == 'h' ? 1.f : 0.f);
    static const struct { const char *n; float hi; } T[] = { {"homer", 24.1f}, {"bart", 29.9f}, {"lisa", 27.0f}, {"marge", 24.1f}, {"apu", 28.1f} }; float hiWant = 24.1f;
    for (int i = 0; i < 5; i++) if (!strcmp(T[i].n, hero)) hiWant = T[i].hi;
    SkinMouth mo; memset(&mo, 0, sizeof mo); float mx = -9.f, mn = 9.f;
    for (int i = 0; i < 600; i++) { float a = skinMouthStep(&mo, nm, DT) * 57.2958f; if (a > mx) mx = a; if (a < mn) mn = a; }
    chk("круг 6: рот — верхний угол за 10 с, градусов (по таблице героя; запас 3° на сдвиг границ)", mx, hiWant - 3.f, hiWant + .1f, 0.f, 90.f);
    chk("круг 6: рот — нижний угол за 10 с, градусов (−18 и не ниже)", mn, -18.1f, -10.f, -90.f, 0.f); }
  // 10и. круг 7: поворот лица на ходьбе. Шли вперёд, стик вбок: через 0,2 с остаток угла — 0,88 (30 Гц-тик, темп в кадр 1/3) 0,088 · 90° = 7,9°; экспонента 10/с даёт 12,2°; линейно по dt при 60 Гц — 10°
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0);
  for (int i = 0; i < 40; i++) step(&H, 0, 1, 0, 0, 0);
  { float h0_ = H.h; for (int i = 0; i < 12; i++) step(&H, 1, 0, 0, 0, 0); float rem = fabsf(fabsf(remainderf(H.h - h0_, 6.2831853f)) - 1.5707963f) * 57.2958f;
    chk("круг 7: ходьба, остаток поворота на 90° через 0,2 с, град", rem, 6.5f, 9.3f, 0.f, 13.5f); }
  // 10к. круг 7+: кайф за рулём (AllowRockOut). С флагом этапа: до 3 с покой водителя, после 3 с — ndr_in_car_victory_small по кругу; без флага покой всегда; старое: покой всегда
  for (int fl = 1; fl >= 0; fl--) {
    static Car car2; carInit(&car2, NULL, &world, spx, spy, spz, 0.f); HomerCar hc2; homerCarFrom(&hc2, &car2, .62f);
    homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); homerSetInCar(&H, &hc2); homerRockOut(&H, fl);
    for (int i = 0; i < 150; i++) { homerUpdate(&H, &world, &hc2, &(HomerInput){0}, DT); homerPose(&H); }
    int before = H.clip == H.cInCar;
    for (int i = 0; i < 60; i++) { homerUpdate(&H, &world, &hc2, &(HomerInput){0}, DT); homerPose(&H); }
    int after = H.cCarVic[0] >= 0 && H.clip == H.cCarVic[0];
    if (fl) { chk("круг 7+: кайф с флагом, 2,5 с в машине — ещё покой (1)", (float)before, 1.f, 1.f, 1.f, 1.f);
              chk("круг 7+: кайф с флагом, 3,5 с в машине — играет victory_small (1)", (float)after, 1.f, 1.f, 0.f, 0.f); }
    else chk("круг 7+: без флага этапа через 3,5 с — покой, не кайф (0)", (float)after, 0.f, 0.f, 0.f, 0.f); }
  // 10л. правка 1158: радость за рулём (малая, большая) на месте водителя. Корень позы во время клипа радости не уходит от места сиденья (раньше уезжал влево сквозь дверь);
  //      по клипу максимум за весь проигрыш, м. Ложный сосед: радость пешком — корень идёт по движению клипа, там привязки нет (проверяется отдельно ниже)
  for (int big = 0; big < 2; big++) {
    static Car car3; carInit(&car3, NULL, &world, spx, spy, spz, 0.f); HomerCar hc3; homerCarFrom(&hc3, &car3, .62f);
    homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); homerSetInCar(&H, &hc3); homerRockOut(&H, 0);
    for (int i = 0; i < 150; i++) { homerUpdate(&H, &world, &hc3, &(HomerInput){0}, DT); homerPose(&H); }
    int ok = homerCelebrate(&H, big); int root = (int)H.m->h->rootJoint; float mx = 0.f;
    for (int i = 0; i < 90; i++) { homerUpdate(&H, &world, &hc3, &(HomerInput){0}, DT); homerPose(&H);
      if (H.clip == H.cCarVic[big]) { float dx = H.xf[root].t[0] - H.seatRoot[0], dy = H.xf[root].t[1] - H.seatRoot[1], dz = H.xf[root].t[2] - H.seatRoot[2];
        float d = sqrtf(dx * dx + dy * dy + dz * dz); if (d > mx) mx = d; } }
    chk(big ? "правка 1158: большая радость за рулём запущена (1)" : "правка 1158: малая радость за рулём запущена (1)", (float)ok, 1.f, 1.f, 1.f, 1.f);
    chk(big ? "правка 1158: большая радость за рулём, уход корня от сиденья, м (0)" : "правка 1158: малая радость за рулём, уход корня от сиденья, м (0)", mx, 0.f, 0.01f, 0.05f, 9.f); }
  // 11. круг 6: прибавка воздуха «за тик». Прыжок с места, во время полёта стик вперёд на полную: через 0,1 с (6 кадров по 1/60) прибавка = 0,078 × 4 × (0,1 с × 30 тиков/с) = 0,94 м/с.
  //     Ложные соседи: тик 60 Гц даёт 1,87; старое (цель 4 м/с, разгон 4,68 м/с²) — 0,47
  homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0);
  step(&H, 0, 0, 0, 1, 0); for (int i = 0; i < 4; i++) step(&H, 0, 0, 0, 0, 0);
  { int n = 0; while (H.grounded && n++ < 60) step(&H, 0, 0, 0, 0, 0); }   // заводка прыжка = длина клипа героя: Гомер 0,27 с, Барт 0,23, Лиза 0,23, Мардж 0,30, Апу 0,48
  for (int i = 0; i < 6; i++) step(&H, 0, 1, 0, 0, 0);
  chk("круг 6: воздух, прибавка скорости за 0,1 с полного стика, м/с", hv(&H), 0.80f, 1.10f, 0.30f, 0.65f);
  // 12. круг 6: радиус тела — не диаметр и не прежние 0,30: упор в борт стоящей машины (коробка), Гомер идёт в неё боком
  { static Car cr; carInit(&cr, NULL, &world, spx, spy, spz, 0.f);
    CarBox bx; carBox(&cr, &bx); chk("круг 6: радиус тела, м (цилиндр 0,35; не 0,30 и не 0,70)", HOMER_RADIUS, 0.35f, 0.35f, 0.30f, 0.30f);
    chk("круг 6: центр тела сдвинут назад, м", HOMER_BODY_BACK, 0.05f, 0.05f, 0.f, 0.f);
    // поведение: борт едущей машины миссии на 0,32 м от кузова сбивает (радиус 0,35), на 0,40 — нет (не диаметр 0,70); прежнее 0,30 и «старое» выключает сбивание совсем
    for (int k = 0; k < 2; k++) { float t = (k ? bx.hw + .40f : bx.hw + .32f); cr.v = 5.f;
      homerPlace(&H, &world, bx.x + bx.fx * .3f - bx.fz * t, spy + 1.f, bx.z + bx.fz * .3f + bx.fx * t, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0);
      H.x = bx.x + bx.fx * .3f - bx.fz * t; H.z = bx.z + bx.fz * .3f + bx.fx * t; H.carHitCool = 0.f;
      HomerCar hk; homerCarFrom(&hk, &cr, .0f); hk.hostile = 1; hk.v = 5.f;
      chk(k ? "круг 6: машина миссии в 0,40 м от борта — сбивает ли (0)" : "круг 6: машина миссии в 0,32 м от борта — сбивает ли (1)", (float)homerKnockedByCar(&H, &hk), k ? 0.f : 1.f, k ? 0.f : 1.f, 0.f, 0.f); }
    // чужая (не миссионная) машина на 5 м/с не сбивает; миссионная на 1,5 м/с (порог 1 м/с, не 3) — сбивает
    { float t = bx.hw + .32f; HomerCar hk; homerCarFrom(&hk, &cr, .0f); hk.v = 5.f; hk.hostile = 0; H.carHitCool = 0.f; H.state = HS_FOOT;
      H.x = bx.x + bx.fx * .3f - bx.fz * t; H.z = bx.z + bx.fz * .3f + bx.fx * t;
      chk("круг 6: не миссионная машина 5 м/с — сбивает ли (0)", (float)homerKnockedByCar(&H, &hk), 0.f, 0.f, 0.f, 0.f);
      hk.hostile = 1; hk.v = 1.5f; H.carHitCool = 0.f; H.state = HS_FOOT; H.x = bx.x + bx.fx * .3f - bx.fz * t; H.z = bx.z + bx.fz * .3f + bx.fx * t;
      chk("круг 6: миссионная машина 1,5 м/с — сбивает ли (1)", (float)homerKnockedByCar(&H, &hk), 1.f, 1.f, 0.f, 0.f); } }
  // 13. круг 6: цена на PSP — из 6 машин миссии (две рядом и быстрые, одна рядом и медленная, три далеко) в разбор попадают только нужные
  { static Car cs[6]; float dxs[6] = {3.f, 5.f, 4.f, 60.f, 90.f, 40.f}, vs[6] = {8.f, -6.f, .5f, 9.f, 9.f, 9.f}; int worth = 0;
    homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (int i = 0; i < 5; i++) step(&H, 0, 0, 0, 0, 0);
    for (int i = 0; i < 6; i++) { carInit(&cs[i], NULL, &world, H.x + dxs[i], H.y, H.z, 0.f); cs[i].v = vs[i]; worth += homerCarWorth(&H, &cs[i]); }
    chk("круг 6: машин миссии, дошедших до разбора сбивания (из 6), штук", (float)worth, 2.f, 2.f, 6.f, 6.f); }
  // 14. GitHub #2 п.1 (запись PS2 04.11: gh2_stop_all / gh2_sprint_off / gh2_stick_off). Бег 1,5 с по ровной земле, дальше конец бега
  //     PS2: ○ отпущен при стике — 8,6 → 4,3 м/с за один тик; оба отпущены — 4,2 за тик и 0 за 0,44 с; стик отпущен при ○ — 10 м/с² (0 за 0,8 с)
  { int i, t0;
    homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (i = 0; i < 40; i++) step(&H, 0, 0, 0, 0, 0);
    for (i = 0; i < 48; i++) step(&H, 0, 1, 1, 0, 0);   // бег разгоняется ≈ 13 м/с² (запись PS2): до 8 м/с за 0,6 с
    float vRun = hv(&H); step(&H, 0, 1, 0, 0, 0);
    chk("GH2 п.1: бег 8 перед отпусканием ○, м/с", vRun, 7.5f, 8.3f, 7.5f, 8.3f);
    chk("GH2 п.1: ○ отпущен при стике, скорость через 1 тик, м/с (PS2 4,3)", hv(&H), 3.9f, 4.4f, 7.2f, 8.1f);
    for (i = 0; i < 30; i++) step(&H, 0, 1, 0, 0, 0);
    chk("GH2 п.1: ○ отпущен при стике, через 0,5 с остаётся шаг, м/с (PS2 3,9-4,3)", hv(&H), 3.7f, 4.3f, 3.7f, 4.3f);
    homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (i = 0; i < 40; i++) step(&H, 0, 0, 0, 0, 0);
    for (i = 0; i < 48; i++) step(&H, 0, 1, 1, 0, 0);   // бег разгоняется ≈ 13 м/с² (запись PS2): до 8 м/с за 0,6 с
    step(&H, 0, 0, 0, 0, 0);
    chk("GH2 п.1: оба отпущены, скорость через 1 тик, м/с (PS2 4,2)", hv(&H), 3.7f, 4.3f, 7.2f, 8.1f);
    for (t0 = 1; t0 < 120 && hv(&H) > .05f; t0++) step(&H, 0, 0, 0, 0, 0);
    chk("GH2 п.1: оба отпущены, остановка за, с (PS2 0,44; PC 0,48)", t0 * DT, .38f, .52f, .7f, .9f);
    homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (i = 0; i < 40; i++) step(&H, 0, 0, 0, 0, 0);
    for (i = 0; i < 48; i++) step(&H, 0, 1, 1, 0, 0);   // бег разгоняется ≈ 13 м/с² (запись PS2): до 8 м/с за 0,6 с
    for (t0 = 1; t0 < 200 && hv(&H) > .05f; t0++) step(&H, 0, 0, 1, 0, 0);
    chk("GH2 п.1: стик отпущен, ○ держат, остановка за, с (PS2 0,78; без изменений)", t0 * DT, .7f, .9f, .7f, .9f); }
  // 14б. GitHub #2: ○ отпущен при стике вбок — ход сразу по стику (PS2: рассогласование хода и стика 30° → 1° за тик)
  { int i; homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (i = 0; i < 40; i++) step(&H, 0, 0, 0, 0, 0);
    for (i = 0; i < 48; i++) step(&H, 0, 1, 1, 0, 0);
    float a0 = atan2f(H.vx, H.vz); step(&H, 1, 0, 0, 0, 0);
    chk("GH2 п.1: ○ отпущен, стик вбок: поворот хода за 1 тик, град (PS2 сразу к стику)", fabsf(remainderf(atan2f(H.vx, H.vz) - a0, 6.2831853f)) * 57.2958f, 70.f, 110.f, 0.f, 15.f); }
  // 14в. GitHub #2 п.2 и п.3 (запись PS2 gh2_turn30/90/180: один закон, доля 0,07-0,08 остатка угла за тик 0,04 с, без двух режимов; ход вдоль лица)
  //      Бег с места, стик вперёд, через 2 тика стик вбок (90°): границу задаёт кнопка ○, не скорость. За 0,5 с лицо: 90 × (1 − 0,92^12,5) = 58°
  { int i; homerPlace(&H, &world, spx, spy + 1.f, spz, 0.f); for (i = 0; i < 40; i++) step(&H, 0, 0, 0, 0, 0);
    for (i = 0; i < 2; i++) step(&H, 0, 1, 1, 0, 0);
    float hh0 = H.h; for (i = 0; i < 30; i++) step(&H, 1, 0, 1, 0, 0);
    chk("GH2 п.2: бег с места, стик на 90°, поворот лица за 0,5 с, град (PS2-закон 58)", fabsf(remainderf(H.h - hh0, 6.2831853f)) * 57.2958f, 50.f, 66.f, 80.f, 91.f);
    float ang = fabsf(remainderf(atan2f(H.vx, H.vz) - H.h, 6.2831853f)) * 57.2958f;
    chk("GH2 п.3: ход вдоль лица (совпало с оригиналом, красного нет), град", ang, 0.f, 3.f, 0.f, 91.f);
    chk("GH2 п.3: на дуге скорость на разгоне 13 м/с² за 0,53 с, м/с (старое: сразу 8)", hv(&H), 6.6f, 7.2f, 7.8f, 8.2f); }
  // 15. аудит 04.11, высокая посадка (решение дирижёра; оригинал: сиденье водителя выше 0,1 м над началом машины — клипы *_high_driver для посадки и выхода).
  //     Для каждого из двух видов машины: посадка с земли (HS_TO_DOOR → HS_GET_IN → HS_IN_CAR), поездка 1 с, выход (HS_GET_OUT → HS_FOOT).
  //     Считаем кадры с клипом *_high_driver, наибольший скачок корня за кадр (не должно быть разрывов цепочки клипов) и что Гомер дошёл до конца
  for (int hi = 0; hi < 2; hi++) {
    static Car car4; carInit(&car4, NULL, &world, spx, spy, spz, 0.f); HomerCar hc4; homerCarFrom(&hc4, &car4, .62f); hc4.high = hi;
    homerPlace(&H, &world, spx + 2.f, spy + 1.f, spz + 1.f, 0.f); for (int i = 0; i < 30; i++) step(&H, 0, 0, 0, 0, 0);
    int entered = homerEnterCar(&H, &hc4, NULL), highIn = 0, highOut = 0, gotIn = 0, gotOut = 0; float jump = 0.f, px = H.x, pz = H.z;
    for (int i = 0; i < 60 * 14 && !gotOut; i++) {
      if (H.state == HS_IN_CAR && !gotIn) gotIn = i;
      if (gotIn && !gotOut && i == gotIn + 60) homerExitCar(&H, &hc4, NULL);
      homerUpdate(&H, &world, &hc4, &(HomerInput){0}, DT); homerPose(&H);
      if (H.state == HS_GET_IN || H.state == HS_GET_OUT) {
        if (H.clip >= 0 && strstr(H.m->c[H.clip].name, "_high")) { if (H.state == HS_GET_IN) highIn++; else highOut++; }
        float d = hypotf(H.x - px, H.z - pz); if (d > jump) jump = d; }
      px = H.x; pz = H.z;
      if (gotIn && H.state == HS_FOOT && i > gotIn + 60) gotOut = 1;
    }
    chk(hi ? "аудит: высокое сиденье — посадка клипами *_high (кадров, > 20)" : "аудит: низкое сиденье — клипы *_high не играют (0)", (float)highIn, hi ? 20.f : 0.f, 400.f, hi ? 0.f : 0.f, hi ? 0.f : 0.f);
    chk(hi ? "аудит: высокое сиденье — выход клипами *_high (кадров, > 20)" : "аудит: низкое сиденье — выход обычный (0)", (float)highOut, hi ? 20.f : 0.f, 400.f, 0.f, 0.f);
    chk(hi ? "аудит: высокое сиденье — сел и вышел (1)" : "аудит: низкое сиденье — сел и вышел (1)", (float)(entered && gotIn && gotOut), 1.f, 1.f, 1.f, 1.f);
    chk(hi ? "аудит: высокое сиденье — наибольший скачок корня за кадр, м" : "аудит: низкое сиденье — наибольший скачок корня за кадр, м", jump, 0.f, .30f, 0.f, .30f);
  }
  printf("%s: %d провалов\n", mode, fails); return fails;
}
