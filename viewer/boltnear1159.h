// №1159 (g11, регресс скорости TEST83→85, подсистема предметов): снаряд осы (gameBoltProps, круг 7 g11b) на КАЖДЫЙ летящий снаряд и КАЖДЫЙ кадр обходил все стейтпропы уровня (spBounds: матрица + корень)
// и все подвижные предметы PRP1 (propsClosestPoint: поворот в оси коробки) — ~200 + ~475 записей на снаряд, а снарядов за кадр бывает несколько. Теперь перед дорогой частью каждая запись проходит дешёвый
// отсев: точка отрезка полёта, ближайшая к записи, дальше её возможного охвата — запись пропускается. Охват ограничен сверху, поэтому отсев ничего не теряет: центр объёма стейтпропа отстоит от положения
// экземпляра ровно на |T->c| (поворот не меняет длину), рамка предмета целиком лежит в шаре радиуса корня из суммы квадратов её полуразмеров вокруг центра. Результат (доля пути, номер, вид) тот же,
// что при полном обходе: тест tests/objcost1159 сверяет их на тысячах отрезков и на данных L1–L7. BOLTNEAR1159_OLD=1 — полный обход, как в TEST85
#ifndef BOLTNEAR1159_H
#define BOLTNEAR1159_H
#include <math.h>
#include "props.h"
#include "stateprops.h"
#ifdef BOLTNEAR1159_RED
#define BOLT_REACH_K .01f   // красный контроль теста: охват урезан в 100 раз — отсев теряет попадания
#else
#define BOLT_REACH_K 1.f
#endif
#ifndef BOLTNEAR1159_OLD
#define BOLTNEAR1159_OLD 0
#endif
// квадрат расстояния от точки c до отрезка a -> a+s (sl2 — квадрат длины s)
static inline float boltSeg2(const float a[3], const float s[3], float sl2, const float c[3]) {
  float u = sl2 > 1e-9f ? ((c[0] - a[0]) * s[0] + (c[1] - a[1]) * s[1] + (c[2] - a[2]) * s[2]) / sl2 : 0.f; u = u < 0.f ? 0.f : u > 1.f ? 1.f : u;
  float dx = a[0] + s[0] * u - c[0], dy = a[1] + s[1] * u - c[1], dz = a[2] + s[2] * u - c[2]; return dx * dx + dy * dy + dz * dz;
}
// счётчики для теста и журнала: [0] стейтпропов просмотрено, [1] из них дошло до дорогой проверки, [2] предметов просмотрено, [3] дошло до дорогой проверки
static inline float boltScan(StateProps *sp, Props *pr, const float a[3], const float b[3], const float dir[3], int apply, unsigned stat[4]) {
  float best = 2.f; int bi = -1, bk = 0;
  float sx = b[0] - a[0], sy = b[1] - a[1], sz = b[2] - a[2], sl2 = sx * sx + sy * sy + sz * sz, s3[3] = {sx, sy, sz};
#define BOLT_U(cx, cy, cz) ({ float u_ = sl2 > 1e-9f ? (((cx) - a[0]) * sx + ((cy) - a[1]) * sy + ((cz) - a[2]) * sz) / sl2 : 0.f; u_ < 0.f ? 0.f : u_ > 1.f ? 1.f : u_; })
  if (sp) for (unsigned i = 0; i < sp->ninst; i++) {
    const SpRun *r = &sp->run[i]; if (!r->alive || (!r->colOn && !r->col2On)) continue;
    const SpType *T = &sp->type[sp->inst[i].type]; if (T->kind == 1 || (sp->follow && sp->follow[sp->inst[i].type])) continue;   // обломок; ящик и торгомат
    stat[0]++;
    if (!BOLTNEAR1159_OLD) {   // центр объёма — в |T->c| от положения; сфера удара не больше 1,5 м: (|c| + 1,5)² ≤ 2·(|c|² + 2,25)
      float p3[3] = {r->x, r->y, r->z}, tc2 = T->c[0] * T->c[0] + T->c[1] * T->c[1] + T->c[2] * T->c[2];
      if (boltSeg2(a, s3, sl2, p3) > BOLT_REACH_K * 2.f * (tc2 + 2.25f)) continue; }
    stat[1]++;
    float c[3], rad; spBounds(sp, i, c, &rad); rad = fminf(rad, 1.5f);
    float u = BOLT_U(c[0], c[1], c[2]), dx = a[0] + sx * u - c[0], dy = a[1] + sy * u - c[1], dz = a[2] + sz * u - c[2];
    if (dx * dx + dy * dy + dz * dz <= rad * rad && u < best) { best = u; bi = (int)i; bk = 1; }
  }
  if (pr) for (unsigned i = 0; i < pr->ninst; i++) {
    const PropState *st = &pr->st[i]; if (st->hidden) continue;
    stat[2]++;
    if (!BOLTNEAR1159_OLD) {   // рамка предмета лежит в шаре радиуса R вокруг центра; (R + 0,25)² ≤ 2·(R² + 0,0625)
      float c3[3] = {st->x, st->y, st->z}, R2 = st->hx * st->hx + st->hy * st->hy + st->hz * st->hz;
      if (boltSeg2(a, s3, sl2, c3) > BOLT_REACH_K * 2.f * (R2 + .0625f)) continue; }
    stat[3]++;
    float u = BOLT_U(st->x, st->y, st->z), pt[3] = {a[0] + sx * u, a[1] + sy * u, a[2] + sz * u}, cl[3]; propsClosestPoint(pr, i, pt, cl);
    float dx = pt[0] - cl[0], dy = pt[1] - cl[1], dz = pt[2] - cl[2];
    if (dx * dx + dy * dy + dz * dz <= .25f * .25f && u < best) { best = u; bi = (int)i; bk = 2; }
  }
#undef BOLT_U
  if (apply && bi >= 0) {
    if (bk == 1) { spEvent(sp, (unsigned)bi, SP_EV_HIT); spImpulse(sp, (unsigned)bi, dir[0], dir[2], 400.f); }
    else { PropState *st = &pr->st[bi]; const PropType *ty = &pr->types[pr->def[bi].type];
      float dl = sqrtf(dir[0] * dir[0] + dir[2] * dir[2]), dx = dl > 1e-4f ? dir[0] / dl : 0.f, dz = dl > 1e-4f ? dir[2] / dl : 1.f, v = 400.f / (ty->mass > 0.f ? ty->mass : 100.f), h45 = v * .70710678f;
      st->vel[0] = dx * h45; st->vel[2] = dz * h45; st->vel[1] = h45; st->w[0] = dz * h45; st->w[2] = -dx * h45; st->asleep = 0; st->sleepT = 0.f; st->fresh = 1; }
  }
  return best;
}
#endif
