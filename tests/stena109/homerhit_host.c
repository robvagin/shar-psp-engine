// С-г1 (109, Стена): Гомера пешком сбивает машина миссии, только когда едет на него. Хост-гейт; новое / прежнее (-DHOMERHIT109_OLD=1)
// Быстрая проверка на CPU Мака: новое поведение окна g04-homer (сверка с исходником, 02.11.2026) против старого.
// Один и тот же файл собирается дважды: без флагов (новое) и со всеми HOMER_*_OLD=1 (TEST81). Печатает числа; ключ --expect new|old проверяет ожидание.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "homer.h"
#include "car.h"

void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
void wdNote(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
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
static int old; int main(int argc, char **argv) {
  const char *lvl = argc > 1 ? argv[1] : "build/level.shr", *hom = argc > 2 ? argv[2] : "build/homer.shr"; if (argc > 3) mode = argv[3]; old = !strcmp(mode, "old");
  FILE *f = fopen(lvl, "rb"); if (!f) { printf("no %s\n", lvl); return 1; }
  unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104 || memcmp(hdr, "SHR7", 4)) { printf("bad level\n"); return 1; }
  const float *hd = (const float *)(hdr + 12); float spx = hd[6], spy = hd[7], spz = hd[8];
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned end = (unsigned)ftell(f); if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); if (!cm || colInit(&world, cm, end - colOff)) { printf("COL1 failed\n"); return 1; }
  SkinModel *m = skinLoad(hom); if (!m) { printf("no %s\n", hom); return 1; }
  static Homer H; if (homerInit(&H, m)) { printf("homerInit failed\n"); return 1; }

  // С-г1 (109): машина миссии едет по прямой на 6 м/с (cr.v); Гомер стоит в разных местах относительно кузова
  { static Car cr; carInit(&cr, NULL, &world, spx, spy, spz, 0.f); cr.v = 6.f; CarBox bx; carBox(&cr, &bx);
    struct { const char *what; float a, s, dy; int want; } T[] = {   // a — вперёд от центра кузова, s — вбок от оси, dy — выше начала машины
      {"наезд в лоб: Гомер перед бампером", bx.hl + .1f, 0.f, 0.f, 1},
      {"наезд углом: у переднего торца, на краю ширины", bx.hl + .1f, bx.hw - .1f, 0.f, 1},
      {"мимо в 0,5 м от борта у переднего торца", bx.hl, bx.hw + .5f, 0.f, 0},
      {"боком: борт посередине кузова, вплотную (0,32 м)", .3f, bx.hw + .32f, 0.f, 0},
      {"боком: борт у заднего торца", -bx.hl + .3f, bx.hw + .30f, 0.f, 0},
      {"стоит на крыше (впереди центра на 0,6 м)", .6f, 0.f, 1.2f, 0},
    };
    for (unsigned k = 0; k < sizeof T / sizeof T[0]; k++) {
      homerPlace(&H, &world, bx.x + bx.fx * T[k].a - bx.fz * T[k].s, spy + 1.f, bx.z + bx.fz * T[k].a + bx.fx * T[k].s, 0.f); for (int i = 0; i < 60; i++) step(&H, 0, 0, 0, 0, 0);
      H.x = bx.x + bx.fx * T[k].a - bx.fz * T[k].s; H.z = bx.z + bx.fz * T[k].a + bx.fx * T[k].s; H.y = cr.y + T[k].dy; H.carHitCool = 0.f; H.state = HS_FOOT; H.grounded = 1;
      HomerCar hk; homerCarFrom(&hk, &cr, .0f); hk.hostile = 1; hk.v = 6.f;
      int got = homerKnockedByCar(&H, &hk), want = old ? (k == 0 || k == 1 || k == 3 || k == 5) : T[k].want;   // прежнее: всё, что в кузове + радиус и впереди центра (в том числе боком и с крыши)
      if (got != want) fails++;
      printf("%-4s %-52s сбит=%d (%s ждём %d)\n", got == want ? "ok" : "FAIL", T[k].what, got, mode, want);
    } }
  printf("%s: %d провалов\n", mode, fails); return fails ? 1 : 0;
}
