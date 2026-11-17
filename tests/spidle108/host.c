// 108 О11: ранний выход шага стейтпропов по признаку trivial в первых байтах записи (stateprops.c). Хост собирается в двух деревьях (прежнее rel/test107 и новое) из одного файла; печатает журнал: хэш всех полей всех экземпляров
// SpRun (без указателей) и счётчиков после каждого кадра, и модель кэша PSP — сколько разных строк по 64 Б затрагивает проход шага по экземплярам (по настоящим смещениям полей данной сборки).
// Машина (car.c, col.c) едет по тем же правилам, что в tests/spnear1000/gate.c (каждые 240 кадров к очередному экземпляру). Параметры: LEVEL.shr [кадров]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include "col.h"
#include "props.h"
#include "stateprops.h"
#include "car.h"
unsigned homerSolidFrame = 0;
void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
void uiFlash(void) {}
static Props props; static ColWorld world; static Car car, car2;
static uint32_t H;
#define HF(x) do { H ^= 0; const unsigned char *p_ = (const unsigned char *)&(x); for (size_t k_ = 0; k_ < sizeof(x); k_++) { H ^= p_[k_]; H *= 16777619u; } } while (0)
static void hashRun(const SpRun *r) {
  HF(r->state); HF(r->alive); HF(r->colOn); HF(r->col2On); HF(r->poseValid); HF(r->burst); HF(r->posed); HF(r->fxStarted); HF(r->callDone); HF(r->t); HF(r->poseF); HF(r->liftK); HF(r->cool); HF(r->away); HF(r->noRespawn);
  HF(r->fxAcc); HF(r->x); HF(r->y); HF(r->z); HF(r->R); HF(r->dirC); HF(r->dirS); HF(r->fall); HF(r->fallTh); HF(r->fallW); HF(r->fallD); HF(r->hv); HF(r->ownFx); HF(r->bushPass); HF(r->hitPend);
  HF(r->loopOn); HF(r->loopA); HF(r->loopB); HF(r->slN); HF(r->slMix); HF(r->slD0); HF(r->slSum);
}
static unsigned linesStep(const StateProps *sp) {   // поля раннего выхода шага: alive, t, fall, cool, loopOn, burst, (признак trivial, если есть) + запись экземпляра (тип) — дедуп по строкам
#ifdef SP_HAS_TRIVIAL
  static const size_t f[] = {offsetof(SpRun, alive), offsetof(SpRun, t), offsetof(SpRun, fall), offsetof(SpRun, cool), offsetof(SpRun, loopOn), offsetof(SpRun, burst), offsetof(SpRun, trivial)};
#else
  static const size_t f[] = {offsetof(SpRun, alive), offsetof(SpRun, t), offsetof(SpRun, fall), offsetof(SpRun, cool), offsetof(SpRun, loopOn), offsetof(SpRun, burst)};
#endif
  unsigned long long tot = 0;
  for (unsigned i = 0; i < sp->ninst; i++) {
    uintptr_t seen[16]; int ns = 0;
    for (size_t k = 0; k < sizeof f / sizeof *f; k++) { uintptr_t l = ((uintptr_t)sp->run + (size_t)i * sizeof(SpRun) + f[k]) >> 6; int d = 0; for (int j = 0; j < ns; j++) if (seen[j] == l) d = 1; if (!d) seen[ns++] = l; }
    tot += (unsigned long long)ns;
#ifdef SP_HAS_TRIVIAL
    (void)0;
#else
    tot += 1;   // прежний путь ещё читает SpInst экземпляра (тип), у типа — таблицу состояний; строка записи экземпляра
#endif
  }
  return (unsigned)tot;
}
#ifdef SP_HAS_TRIVIAL
extern int spIdle108Old;
#endif
int main(int argc, char **argv) {
  if (argc < 2) return 2;
#ifdef SP_HAS_TRIVIAL
  if (getenv("IDLEOLD")) spIdle108Old = 1;   // сверка «О11 выключен» в той же сборке
#endif
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (!data || fread(data, 1, size, f) != (size_t)size) return 2; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&world, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40))) { printf("нет данных\n"); return 2; }
  static StateProps S; if (spInit(&S, props.spr, props.sprSize)) { printf("нет SPR1\n"); return 2; }
  int frames = argc > 2 ? atoi(argv[2]) : 1800; float dt = 1.f / 30.f; unsigned n = S.ninst, nxt = 0; unsigned long long lines = 0; uint32_t J = 2166136261u;
  for (int fr = 0; fr < frames; fr++) {
    if (fr % 240 == 0) {
      for (unsigned k = 0; k < n; k++) { unsigned i = (nxt + k) % n; if (S.run[i].alive && S.type[S.inst[i].type].nCol) { nxt = i + 3; float x = S.run[i].x, z = S.run[i].z, gy = 0.f; float ang = 0.7f * (float)(fr / 240);
        float fx = sinf(ang), fz = cosf(ang); if (!colGround(&world, x - fx * 14.f, z - fz * 14.f, S.run[i].y + 3.f, S.run[i].y - 6.f, &gy, 0)) continue;
        carInit(&car, 0, &world, x - fx * 14.f, gy + 1.f, z - fz * 14.f, ang); carSetSpeed(&car, 14.f); break; } } }
    CarInput in = {1.f, 0, 0, 0}; if (fr % 240 > 90) in.steer = 0.4f * sinf(fr * 0.05f);
    carUpdate(&car, &world, &in, dt);
#ifdef SP_HAS_TRIVIAL
    if (getenv("OTHER")) {   // чужая машина (трафик, погоня) бьёт стейтпропы тем же заездом, но своей копией: состояние меняет spHitOther (107б П3)
      if (fr % 240 == 60) { car2 = car; car2.x += 3.f; }
      CarInput in2 = {1.f, 0, 0, 0}; carUpdate(&car2, &world, &in2, dt); spHitOther(&S, &world, &car2); }
#endif
    S.nearOn = 1; S.nearP[0][0] = car.x - 4.f * sinf(car.h); S.nearP[0][1] = car.z - 4.f * cosf(car.h); S.nearP[1][0] = car.x; S.nearP[1][1] = car.z;
    spUpdate(&S, &world, &car, NULL, dt);
    H = 2166136261u; for (unsigned i = 0; i < n; i++) hashRun(&S.run[i]); HF(S.hits); HF(S.destroyed); HF(S.coins); HF(S.nfx); HF(S.hnrHits); HF(S.breaks);
    J ^= H; J *= 16777619u; S.nfx = 0;
    if (fr % 30 == 0) lines += linesStep(&S);
  }
  unsigned samples = (unsigned)((frames + 29) / 30), triv = 0;
#ifdef SP_HAS_TRIVIAL
  for (unsigned i = 0; i < n; i++) triv += S.run[i].trivial;
#endif
  printf("instances %u\njournal %08x\nlines %.1f\ntrivial %u\nsizeof_run %zu\n", n, J, (double)lines / samples, triv, sizeof(SpRun));
  return 0;
}
