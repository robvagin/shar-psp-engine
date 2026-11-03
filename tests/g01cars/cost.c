// сверка g01-cars №1159: цена физики машин на кадр в счётчиках обходов (время Мака в ×5,8 не доказательство)
//   cost LEVEL PLAYER MSN SECS SEED DTMODE   — как streets, но печатает за кадр: вызовов carUpdate (все машины мира: игрок, трафик, миссии), подшагов, обращений к миру по видам
//   сборка и прогон: tests/g01cars/cost.sh (счётчики вставляет cost_prep.py в копию car.c)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "smkpilot.h"
#include "costnames.h"
float bwDtNow = 1.f / 60.f;
unsigned long cntQ[64], cntSub, cntCalls, cntWho[8], cntNs[8], cntSubWho[8];
static BotWorld W;
static unsigned rs = 1; static float rnd(void) { rs = rs * 1664525u + 1013904223u; return (rs >> 8) / 16777216.f; }
int main(int argc, char **argv) {
  if (argc < 7 || bwData(argv[1], argv[2], argv[3])) { fprintf(stderr, "usage: cost LEVEL PLAYER MSN SECS SEED DTMODE\n"); return 2; }
  float secs = (float)atof(argv[4]); unsigned seed = (unsigned)atoi(argv[5]); int mode = atoi(argv[6]);
  bwNew(&W); Traffic *t = (Traffic *)bwTraffic(); if (!t || !t->hd) { printf("нет трафика\n"); return 2; }
  SmkPilot P; smkPilotInit(&P, seed, 16.f); P.props = &W.props; P.sp = &W.sp;
  if (getenv("PILOT_AT")) { float sx, sz, y0 = 50.f, sh; sscanf(getenv("PILOT_AT"), "%f,%f,%f,%f", &sx, &sz, &y0, &sh); float gy; colGround(bwCol(), sx, sz, y0, -1e6f, &gy, 0); bwPlace(&W, sx, gy + .3f, sz, sh); }
  bwDtNow = 1.f / 60.f; for (int f = 0; f < 60; f++) bwStep(&W, 0, 0.f, 0.f);
  P.lx = W.car.x; P.lz = W.car.z;
  int N = (int)(secs * 60), frames = 0; double simT = 0; unsigned long q0[64], s0, c0; memset(cntQ, 0, sizeof cntQ); cntSub = cntCalls = 0;
  double phN = 0, ph_still = 0, ph_near = 0, ph_mid = 0, ph_far = 0, ph_quiet = 0, ph_park = 0, ph_hb = 0, aw_cont = 0, aw_body = 0, aw_sup = 0, aw_w4 = 0, aw_side = 0, aw_air = 0, aw_n = 0, aw_vel = 0, ph_rest = 0, ph_wreck = 0; int still = 0;   // игрок стоит (скорость < 0,5 м/с)
  float px = W.car.x, pz = W.car.z;
  for (int f = 0; f < N; f++) {
    float dt;
    switch (mode) { case 1: dt = 1.f / 30.f; break; case 2: dt = 1.f / 30.f + (rnd() - .5f) * .008f; break; case 3: dt = 1.f / 45.f; break; default: dt = 1.f / 60.f; }
    bwDtNow = dt; simT += dt;
    CarInput in; smkPilotStep(&P, t, &W.car, dt, &in);
    unsigned b = 0; if (in.throttle > .5f) b |= GP_R; if (in.brake > .1f) b |= GP_L; if (in.handbrake > .5f) b |= GP_SQUARE;
    bwStep(&W, b, in.steer, 0.f); frames++;
    for (int i = 0; i < t->maxCars; i++) if (t->car[i].mode == TRM_PHYS) {
      const Car *k = &t->car[i].car; float d = hypotf(k->x - W.car.x, k->z - W.car.z); phN++;
      if (fabsf(k->v) < .3f) ph_still++; if (d < 30.f) ph_near++; else if (d < 80.f) ph_mid++; else ph_far++; if (fabsf(k->v) < .3f) { if (k->parkN >= 30) ph_park++; else { aw_n++; if (k->contacts) aw_cont++; if (k->bodyHit) aw_body++; if (k->supOn) aw_sup++; if (k->wheels >= 4) aw_w4++; if (fabsf(k->roll) > .6f) aw_side++; if (!k->grounded) aw_air++; if (fabsf(k->vel[0]) + fabsf(k->vel[1]) + fabsf(k->vel[2]) >= .02f) aw_vel++; } if (k->quietSteps >= 240) ph_rest++; if (k->quietSteps > 0) ph_quiet++; } if (k->destroyed) ph_wreck++; }
    if (hypotf(W.car.x - px, W.car.z - pz) / dt < .5f) still++; px = W.car.x; pz = W.car.z;
  }
  (void)q0; (void)s0; (void)c0;
  double k = 1.0 / frames;
  printf("кадров %d (%.0f с, шаг %s), игрок стоял в %d кадрах\n", frames, simT, mode == 1 ? "1/30" : mode == 2 ? "1/30±4 мс" : mode == 3 ? "1/45" : "1/60", still);
  printf("за кадр: вызовов carUpdate %.3f, подшагов %.3f, обращений к миру всего %.2f\n", cntCalls * k, cntSub * k, ({ double s = 0; for (int i = 0; i < COSTN; i++) s += cntQ[i]; s * k; }));
  printf("  трафик в физике (TRM_PHYS) за кадр: всего %.3f, из них стоят (|v|<0,3) %.3f, ближе 30 м %.3f, 30–80 м %.3f, дальше 80 м %.3f, из стоящих: «спят» по стоянке (parkN≥30) %.3f, с покоем на опоре (quietSteps>0) %.3f, с покоем 240+ %.3f, разбитые %.3f\n", phN * k, ph_still * k, ph_near * k, ph_mid * k, ph_far * k, ph_park * k, ph_quiet * k, ph_rest * k, ph_wreck * k);
  printf("  кто вызывает carUpdate за кадр: игрок %.3f, трафик на ходу %.3f, проснувшиеся припаркованные %.3f, машины миссий %.3f, бонус-игра %.3f\n", cntWho[0] * k, cntWho[1] * k, cntWho[2] * k, cntWho[3] * k, cntWho[4] * k);
  printf("  стоящие и НЕ спящие (за кадр %.3f): с контактами колёс/стен %.3f, bodyHit %.3f, на крыше машины %.3f, все 4 колеса %.3f, на боку %.3f, в воздухе %.3f, скорость ≥ 0,02 %.3f\n", aw_n * k, aw_cont * k, aw_body * k, aw_sup * k, aw_w4 * k, aw_side * k, aw_air * k, aw_vel * k);
  printf("  подшагов за кадр: игрок %.3f, трафик на ходу %.3f, припаркованные %.3f, миссии %.3f\n", cntSubWho[0] * k, cntSubWho[1] * k, cntSubWho[2] * k, cntSubWho[3] * k);
  printf("  время Мака внутри carUpdate, мкс на кадр: игрок %.2f, трафик на ходу %.2f, припаркованные %.2f, миссии %.2f (только для долей; PSP медленнее в ~5,8 раза)\n", cntNs[0] * k / 1000, cntNs[1] * k / 1000, cntNs[2] * k / 1000, cntNs[3] * k / 1000);
  for (int i = 0; i < COSTN; i++) if (cntQ[i]) printf("  %-22s %8.3f\n", costNames[i], cntQ[i] * k);
  return 0;
}
