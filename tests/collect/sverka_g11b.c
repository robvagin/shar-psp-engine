// Гейт сверки g11b (02.11.2026): осы — наблюдение/агрессия (P04 W3), скорость снаряда в км/ч и размер пула (W1), сфера появления по «пятому» аргументу (W2).
// Пакет build/collect-l1.shr (уровень 1: ATTRACTION 4/10/10, ATTACK 10/5/10, снаряд 60 км/ч, пул 5). Без мира и Гомера: collectStep(NULL...).
// Красные контроли: -DWASP_AGGR_OLD=1 (агрессия от дальности стрельбы), -DWASP_BOLT_KMH_OLD=1 (60 м/с) — гейт обязан упасть. См. tests/collect/sverka_g11b.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "collect.h"
static Collect c; static unsigned char *blob; static int fail;
#define CHECK(ok, ...) do { int o_ = (ok); printf("%s ", o_ ? "PASS" : "FAIL"); printf(__VA_ARGS__); printf("\n"); fail += !o_; } while (0)
static int load(const char *path, int idx0, float dist, int *idx) {
  FILE *f = fopen(path, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); unsigned n = ftell(f); rewind(f);
  blob = aligned_alloc(16, (n + 15u) & ~15u); if (fread(blob, 1, n, f) != n) return 0; fclose(f);
  if (collectInit(&c, blob, n)) return 0;
  for (unsigned i = 0; i < c.nwasp; i++) if (!strcmp(c.wasp[i].name, "w_bonuscar")) *idx = (int)i;
  (void)idx0; (void)dist; return 1;
}
static void freeAll(void) { collectFree(&c); free(blob); }
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  int idx = -1; if (!load(argv[1], 0, 0, &idx) || idx < 0) return 2;
  const CollWasp *w = &c.wasp[idx];
  // W2: сфера появления — «пятый» аргумент (roam 50 для w_bonuscar); на 45 м оса появляется, на 55 м нет
  CollPlayer p = {w->x + 45.f, w->y - 1.2f, w->z, 1, -1, 0};
  collectStep(&c, &p, 0, 0, 0, 0, 1.f / 60.f);
  CHECK(c.st.waspSt[idx] == 1, "W2 оса появилась на 45 м (сфера %.0f м)", w->roam);
  freeAll(); load(argv[1], 0, 0, &idx); w = &c.wasp[idx];
  p = (CollPlayer){w->x + 55.f, w->y - 1.2f, w->z, 1, -1, 0};
  collectStep(&c, &p, 0, 0, 0, 0, 1.f / 60.f);
  CHECK(c.st.waspSt[idx] != 1, "W2 оса не появилась на 55 м");
  freeAll(); load(argv[1], 0, 0, &idx); w = &c.wasp[idx];
  // W3: на 7 м (между минимумом 4 и максимумом 10) оса наблюдает — не заряжается и не стреляет
  p = (CollPlayer){w->x, w->y - 1.2f, w->z + 7.f, 1, -1, 0};
  for (int f = 0; f < 60 * 8; f++) collectStep(&c, &p, 0, 0, 0, 0, 1.f / 60.f);
  CHECK(c.st.waspSt[idx] == 1 && !c.st.waspAggr[idx] && c.st.boltFired == 0, "W3 на 7 м: наблюдает (агрессия %d, выстрелов %u)", c.st.waspAggr[idx], c.st.boltFired);
  // ближе минимума (3 м): агрессия до конца; стреляет, скорость снаряда = км/ч / 3,6, пул не больше 5
  p.z = w->z + 3.f;
  float vmax = 0; unsigned busyMax = 0; int fired = 0;
  for (int f = 0; f < 60 * 90; f++) {
    collectStep(&c, &p, 0, 0, 0, 0, 1.f / 60.f);
    unsigned busy = 0;
    for (unsigned k = 0; k < c.nbolt; k++) if (c.st.boltOn[k]) {
      busy++; float v = sqrtf(c.st.boltV[k][0] * c.st.boltV[k][0] + c.st.boltV[k][1] * c.st.boltV[k][1] + c.st.boltV[k][2] * c.st.boltV[k][2]); if (v > vmax) vmax = v; }
    if (busy > busyMax) busyMax = busy;
    if (c.st.boltFired) fired = 1;
    CollEvent e; while (collectPop(&c, &e)) {}
  }
  CHECK(c.st.waspAggr[idx] == 1 || c.st.waspSt[idx] != 1, "W3 на 3 м: агрессия включена");
  CHECK(fired, "W4 оса стреляет по неподвижному игроку на 3 м (выстрелов %u)", c.st.boltFired);
  float want = w->boltSpeed / 3.6f;
  CHECK(fabsf(vmax - want) < .02f, "W1 скорость снаряда %.2f м/с (данные %.0f км/ч = %.2f м/с)", vmax, w->boltSpeed, want);
  CHECK(busyMax <= 5, "W1 пул снарядов: одновременно не больше 5 (было %u)", busyMax);
  freeAll();
  printf("RESULT %s\n", fail ? "FAIL" : "PASS"); return !!fail;
}
