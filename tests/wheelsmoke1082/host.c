// №1082 + круг 6 (g11): уровень пробуксовки Car.burnLevel (сила дыма из-под задних колёс). L1, машина игрока с места, 30 Гц.
// Газ вперёд с места: уровень на старте высокий, падает по мере разгона, рывок в 2,5 с и больше ставит burnBig; тормоз с места задним ходом: уровень = сила тормоза.
// Печатает ключи для accept.sh. Параметр: LEVEL.shr
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "car.h"
void uiFlash(void) {}
unsigned homerSolidFrame = 0;
static ColWorld world; static Car c; static Props props;
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (fread(data, 1, size, f) != (size_t)size) return 2;
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  colInit(&world, data + co, ce - co); (void)props;
  float gy = 0; colGround(&world, 220, 172, 20, -50, &gy, 0);
  carInit(&c, 0, &world, 220, gy + .6f, 172, 1.57f); c.svPlayer = 1;   // машина игрока (правило «только игрок»)
  for (int i = 0; i < 20; i++) { CarInput z = {0, 0, 0, 0}; carUpdate(&c, &world, &z, 1.f / 30.f); }   // осесть
  float b03 = -1, b1 = -1, b3 = -1, b6 = -1, b2 = -1, mx = 0; int big = 0;
  for (int fr = 1; fr <= 180; fr++) {
    CarInput in = {1.f, 0, 0, 0}; carUpdate(&c, &world, &in, 1.f / 30.f);
    if (c.burnLevel > mx) mx = c.burnLevel; if (c.burnBig) big = 1;
    if (fr == 9) b03 = c.burnLevel; if (fr == 30) b1 = c.burnLevel; if (fr == 18) b2 = c.burnLevel; if (fr == 90) b3 = c.burnLevel; if (fr == 180) b6 = c.burnLevel;
  }
  // задний ход: машине задана скорость назад, тормоз нажат — уровень = сила тормоза
  float rev = 0; carSetSpeed(&c, -6.f);
  for (int i = 0; i < 20; i++) { CarInput in = {0, 1.f, 0, 0}; carUpdate(&c, &world, &in, 1.f / 30.f); if (c.burnLevel > rev) rev = c.burnLevel; }
  // рывок со старта (круг 7): с места газ и ручник 2 с — таймер копится, отпустили ручник — огонь (burnBig), пока таймер не меньше 2,5 с, потом пробуксовка продолжается без огня
  carInit(&c, 0, &world, 220, gy + .6f, 172, 1.57f); c.svPlayer = 1; if (c.p.burstTime < 3.f) c.p.burstTime = 3.f; if (c.p.burnoutRange < .2f) c.p.burnoutRange = .3f;   // параметры рывка по диску, если у машины по умолчанию их нет
  for (int i = 0; i < 20; i++) { CarInput z = {0, 0, 0, 0}; carUpdate(&c, &world, &z, 1.f / 30.f); }
  int bigHb = 0, bigAfter = 0, bigStale = 0, burnAfter = 0;
  for (int fr = 0; fr < 60; fr++) { CarInput in = {1.f, 0, 0, 1.f}; carUpdate(&c, &world, &in, 1.f / 30.f); if (c.burnBig) bigHb = 1; }
  for (int fr = 0; fr < 90; fr++) {
    CarInput in = {1.f, 0, 0, 0}; carUpdate(&c, &world, &in, 1.f / 30.f);
    if (c.burnBig) bigAfter = 1;
    if (c.burnLevel > 0.f) burnAfter++;
    if (c.burnBig && c.burstT < 2.5f && c.burstUse < 2.5f) bigStale++;   // огонь при убывшем таймере
  }
  printf("# burstTime %.2f burnoutRange %.2f burstT %.2f burstUse %.2f\n", c.p.burstTime, c.p.burnoutRange, c.burstT, c.burstUse);
  printf("big_hb %d\nbig_after %d\nburn_after_frames %d\nbig_stale %d\n", bigHb, bigAfter, burnAfter, bigStale);
  printf("fwd_06 %.3f\nfwd_03 %.3f\nfwd_1 %.3f\nfwd_3 %.3f\nfwd_6 %.3f\nfwd_max %.3f\nbig %d\nrev_max %.3f\n", b2, b03, b1, b3, b6, mx, big, rev);
  return 0;
}
