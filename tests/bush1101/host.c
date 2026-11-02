// №1101 (g11): кусты-шары (cls 2) — середина между «насквозь» (наш) и «твёрдый» (оригинал). Рой на настоящих car.c, stateprops.c, col.c, данные уровня, без эмулятора.
// Каждый одинокий куст cls 2 (в 9 м нет других экземпляров; до 40 на уровень), 4 направления, скорости входа 3, 5, 10, 14 м/с (с 12 м до центра, до касания скорость входа держится газом, после — полный газ).
//   медленно (3, 5): не прошла насквозь (держит как стена), можно сдать назад и выехать (расстояние растёт больше чем на 1,0 м), отскок наружу не больше 25 % скорости входа и
//   не больше 4 смен знака нормальной скорости (пружины нет); быстро (10, 14): прошла насквозь, сразу после входа остаётся не больше 65 % скорости входа; всё считается по заездам, где до куста машина не ударилась о твёрдое.
// Печатает ключи для accept.sh. Параметр: LEVEL.shr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "props.h"
#include "stateprops.h"
unsigned homerSolidFrame = 0;
void uiFlash(void) {}
static Props props; static StateProps sp; static ColWorld world; static Car car;
typedef struct { int n, crossed, back, backOk, springBad; float outMax, exitRatioMax, exitSum; } Cls;
static int run(unsigned i, float ang, float v, int *crossed, int *backOk, int *springBad, float *outRatio, float *exitRatio) {
  const SpRun *r = &sp.run[i]; const SpType *T = &sp.type[sp.inst[i].type];
  float c[3] = {r->x + T->c[0], r->y + T->c[1], r->z + T->c[2]}, fx = sinf(ang), fz = cosf(ang), d0 = 12.f;
  float x = c[0] - fx * d0, z = c[2] - fz * d0, g = 0; if (!colGround(&world, x, z, c[1] + 4.f, -50, &g, 0)) return -1;
  spFree(&sp); if (spInit(&sp, props.spr, props.sprSize)) return -2;
  carInit(&car, 0, &world, x, g + .6f, z, ang); carSetSpeed(&car, v);
  int touched = 0, flips = 0, tf = -1, hardSeen = 0; float vin = 0, vout = 0, sideLast = 0, outMax = 0, sepAtTouch = 0; CarInput in = {0, 0, 0, 0};
  for (int f = 0; f < 260; f++) {
    float cool0 = sp.run[i].cool;
    if (!touched) { float sp0 = hypotf(car.vel[0], car.vel[2]); in.throttle = sp0 < v ? 1.f : 0.f; }   // до касания держим скорость входа; потом газа нет
    else in.throttle = 1.f;   // водитель продавливает газом
    carUpdate(&car, &world, &in, 1.f / 30.f); spUpdate(&sp, &world, &car, NULL, 1.f / 30.f);
    if (touched && (car.hardV > 0.5f || car.foliV > 0.f)) hardSeen = 1;   // после касания куста удар о твёрдое (ствол, стена мира) или листва мира (foliV, свой отскок car.c) — отскок уже не от куста
    if (!touched && car.hardV > 1.f) return -1;   // до куста ударилась о твёрдое — заезд не про куст
    float along = (car.x - c[0]) * fx + (car.z - c[2]) * fz, vn = car.vel[0] * fx + car.vel[2] * fz;   // along < 0 — ещё перед кустом
    if (!touched && sp.run[i].cool > cool0) { touched = 1; vin = vn + (vn < 3.f ? 0.f : 0.f); if (vin < .5f) vin = v; tf = f; sepAtTouch = along; }
    if (touched) {
      if (vn < -outMax) outMax = -vn;
      if (f > tf + 1 && ((vn > 0.f) != (sideLast > 0.f)) && fabsf(vn) > .3f && fabsf(sideLast) > .3f) flips++;
      sideLast = vn;
      if (f > tf + 120 && !*crossed) break;
    }
    if (along > 1.5f) { *crossed = 1; vout = hypotf(car.vel[0], car.vel[1] * 0 + car.vel[2]); if (f > tf + 5) break; }
  }
  if (!touched) return -1;
  *outRatio = vin > .1f ? outMax / vin : 0.f; *springBad = !hardSeen && (flips > 4 || *outRatio > .25f); *exitRatio = vin > .1f ? vin / v : 0.f;   // exit — доля скорости, оставшаяся сразу после входа (до газа)
  if (!*crossed) {   // сдать назад: тормоз с места задним ходом 3 с — выехать на больше чем 1 м от точки касания
    float a0 = (car.x - c[0]) * fx + (car.z - c[2]) * fz; CarInput rv = {0, 1.f, 0, 0}; float a1 = a0;
    for (int f = 0; f < 90; f++) { carUpdate(&car, &world, &rv, 1.f / 30.f); spUpdate(&sp, &world, &car, NULL, 1.f / 30.f); a1 = (car.x - c[0]) * fx + (car.z - c[2]) * fz; }
    *backOk = a1 < a0 - 1.0f;
  }
  (void)sepAtTouch; return 1;
}
int main(int argc, char **argv) {
  if (argc < 2) return 2; FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
  fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (!data || fread(data, 1, size, f) != (size_t)size) return 2; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&world, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40)) || !props.spr || spInit(&sp, props.spr, props.sprSize)) { printf("bushes 0\n"); return 0; }
  static const float V[4] = {3.f, 5.f, 10.f, 14.f}; Cls K[4]; memset(K, 0, sizeof K); int nb = 0;
  for (unsigned i = 0; i < sp.ninst && nb < 40; i++) {
    const SpType *T = &sp.type[sp.inst[i].type]; if (T->cls != 2 || T->kind || !T->nCol || sp.inst[i].tag || !sp.run[i].colOn) continue;   // без объёма в начальном состоянии куст ничего не держит (L5)
    int alone = 1; for (unsigned q = 0; q < sp.ninst && alone; q++) if (q != i) { float dx = sp.run[q].x - sp.run[i].x, dz = sp.run[q].z - sp.run[i].z; if (dx * dx + dz * dz < 9.f * 9.f) alone = 0; }
    if (!alone) continue;   // кусты стоят рядами; рой — по одиночным (соседи и стены мира мешали бы мерить)
    nb++;
    for (int a = 0; a < 4; a++) for (int s = 0; s < 4; s++) {
      int cr = 0, bk = 0, sb = 0; float orat = 0, erat = 0; int rc = run(i, a * 1.5708f + .3f, V[s], &cr, &bk, &sb, &orat, &erat);
      if (rc != 1) continue; Cls *k = &K[s]; k->n++; k->crossed += cr; k->back += !cr; k->backOk += !cr && bk; k->springBad += sb;
      if (orat > k->outMax) k->outMax = orat; if (cr && erat > k->exitRatioMax) k->exitRatioMax = erat; if (cr) k->exitSum += erat;
    }
  }
  printf("bushes %d\n", nb);
  for (int s = 0; s < 4; s++) printf("v%.0f n %d crossed %d back %d backok %d spring %d outmax %.2f exitmax %.2f exitsum %.2f\n", V[s], K[s].n, K[s].crossed, K[s].back, K[s].backOk, K[s].springBad, K[s].outMax, K[s].exitRatioMax, K[s].exitSum);
  return 0;
}
