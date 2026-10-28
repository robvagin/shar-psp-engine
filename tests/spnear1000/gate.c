// №1000: сон далёких стейтпропов-деревьев (stateprops.c spUpdate, spNear1000). Две копии стейтпропов уровня шагают в ногу: A — все экземпляры каждый кадр (сон выключен),
// B — далёкие от глаза/машины/Гомера спят и догоняют время при пробуждении. Машина (настоящие car.c/col.c) в обоих мирах одна и та же траектория: каждые 240 кадров ставится
// к очередному экземпляру (каждому 3-му) и едет в него, между ними — руль змейкой. Сверка по кадрам: состояние, живость, объёмы, удары, скорость удара; в конце все будятся —
// время t и кадр позы poseF каждого экземпляра побитно (dt разные: допуск 3 мс по t, позы не сверяются). Итог: строка ИТОГ …, последняя строка PASS/FAIL. SPN_RED (сборка -DSPNEAR1000_RED) — сон без догона времени: гейт должен упасть.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "stateprops.h"
#include "car.h"
unsigned homerSolidFrame = 0;
static Props props; static ColWorld world; static Car car;
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
  fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (!data || fread(data, 1, size, f) != (size_t)size) return 2; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&world, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40))) { printf("нет данных\n"); return 2; }
  static StateProps A, B; if (spInit(&A, props.spr, props.sprSize) || spInit(&B, props.spr, props.sprSize)) { printf("нет SPR1\n"); return 2; }
  unsigned n = A.ninst; int frames = argc > 2 ? atoi(argv[2]) : 3600; float dt = 1.f / 30.f;
  if (getenv("MIXDT")) dt = 0.f;   // dt меняется от кадра к кадру: сумма вместо повторных сложений (допустимое расхождение в последнем знаке времени; гейт сверяет с допуском)
  static Car cA; unsigned bad = 0, badFrame = 0, sumAsleep = 0, maxAsleep = 0, wakeBad = 0; float tolT = getenv("MIXDT") ? 3e-3f : 0.f;   // dt разные: накопление по одному кадру на t≈60 с теряет до ~1 мс против суммы (сумма точнее), поза расходится на сотые кадра
  unsigned nxt = 0; int hitsA = 0;
  for (int fr = 0; fr < frames; fr++) {
    if (fr % 240 == 0) {   // новая заездная цель: каждый 3-й живой экземпляр с объёмом
      for (unsigned k = 0; k < n; k++) { unsigned i = (nxt + k) % n; if (A.run[i].alive && A.type[A.inst[i].type].nCol) { nxt = i + 3; float x = A.run[i].x, z = A.run[i].z, gy = 0.f; float ang = 0.7f * (float)(fr / 240);
        float fx = sinf(ang), fz = cosf(ang); if (!colGround(&world, x - fx * 14.f, z - fz * 14.f, A.run[i].y + 3.f, A.run[i].y - 6.f, &gy, 0)) continue;
        carInit(&cA, 0, &world, x - fx * 14.f, gy + 1.f, z - fz * 14.f, ang); carSetSpeed(&cA, 14.f); break; } } }
    CarInput in = {1.f, 0, 0, 0}; if (fr % 240 > 90) in.steer = 0.4f * sinf(fr * 0.05f);
    float d = dt; if (getenv("MIXDT")) d = (fr % 3 == 0) ? 1.f / 30.f : (fr % 3 == 1 ? 1.f / 31.f : 1.f / 29.f);
    carUpdate(&cA, &world, &in, d); Car cB2 = cA;   // одна машина на оба мира: траектория одна и та же, сверяется только реакция предметов
    spNear1000 = 0; A.nearOn = 0; spUpdate(&A, &world, &cA, NULL, d);
    spNear1000 = 1; B.nearOn = 1; B.nearP[0][0] = cA.x - 4.f * sinf(cA.h); B.nearP[0][1] = cA.z - 4.f * cosf(cA.h); B.nearP[1][0] = cA.x; B.nearP[1][1] = cA.z; spUpdate(&B, &world, &cB2, NULL, d);
    hitsA = A.hits; if (A.hits != B.hits || A.destroyed != B.destroyed || A.coins != B.coins) bad++;
    sumAsleep += B.nearSleep; if (B.nearSleep > maxAsleep) maxAsleep = B.nearSleep;
    for (unsigned i = 0; i < n; i++) { const SpRun *a = &A.run[i], *b = &B.run[i];
      if (a->state != b->state || a->alive != b->alive || a->colOn != b->colOn || a->col2On != b->col2On || a->fall != b->fall || memcmp(a->R, b->R, sizeof a->R) || a->x != b->x || a->y != b->y || a->z != b->z) { if (!bad) badFrame = (unsigned)fr; bad++; }
      }
    if (getenv("DUMPWAKE") && fr == frames - 1) {}
  }
  // все просыпаются: один шаг без сна, потом время и поза побитно
  spNear1000 = 1; B.nearOn = 0; Car c1 = cA, c2 = cA; spUpdate(&A, &world, &c1, NULL, 1.f / 30.f); spUpdate(&B, &world, &c2, NULL, 1.f / 30.f);
  unsigned slept = 0; for (unsigned i = 0; i < n; i++) { const SpRun *a = &A.run[i], *b = &B.run[i]; if (getenv("DBG") && (fabsf(a->t - b->t) > 1e-6f)) printf("  i=%u state %d/%d t %.6f %.6f poseF %.4f %.4f\n", i, a->state, b->state, a->t, b->t, a->poseF, b->poseF); slept += 0; if (fabsf(a->t - b->t) > tolT || (!getenv("MIXDT") && a->poseF != b->poseF) || a->state != b->state) wakeBad++; }
  (void)slept; (void)hitsA;
  printf("ИТОГ экземпляров %u, кадров %d, спящих в среднем %.1f (наибольшее %u), расхождений по кадрам %u (первый кадр %u), время/поза после пробуждения: расхождений %u, ударов машины %d\n", n, frames, (double)sumAsleep / frames, maxAsleep, bad, badFrame, wakeBad, A.hits);
  int ok = !bad && !wakeBad && sumAsleep > (unsigned)frames * 10;
  printf("%s\n", ok ? "PASS" : "FAIL"); return ok ? 0 : 1;
}
