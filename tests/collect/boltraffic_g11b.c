// Круг 7 (g11b-collect, P04 W5): trafficBoltHit — снаряд осы и пешеходы / машины трафика. Мир собран вручную (Traffic из нулей, один пешеход, одна машина).
// Ждём: отрезок через пешехода — доля пути в пределах отрезка, мимо — больше 1; удар (apply) бросает пешехода 10 м/с под 45° по направлению; машина получает урон 0,1 (не выше 0,999); лежащий пешеход не задевается.
// Красный контроль: -DTRF_BOLT_OLD=1 (снаряд трафик не задевает).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "traffic.h"
static int fail;
#define CHECK(ok, ...) do { int o_ = (ok); printf("%s ", o_ ? "PASS" : "FAIL"); printf(__VA_ARGS__); printf("\n"); fail += !o_; } while (0)
int main(void) {
  Traffic *t = calloc(1, sizeof(Traffic)); if (!t) return 2;
  t->maxPeds = 1; t->maxCars = 1;
  TrPed *e = &t->ped[0]; e->mode = TRP_WALK; e->x = 10.f; e->y = 0.f; e->z = 0.f;
  TrCar *c = &t->car[0]; c->mode = TRM_DRIVE; c->car.x = 20.f; c->car.y = 0.f; c->car.z = 0.f; c->car.h = 1.5707963f;   // смотрит по +x
  float dir[3] = {1.f, 0.f, 0.f};
  float a[3] = {9.f, 1.f, 0.f}, b[3] = {11.f, 1.f, 0.f};          // через пешехода
  float f = trafficBoltHit(t, a, b, dir, 0);
  CHECK(f >= 0.f && f <= 1.f, "пешеход на пути отрезка: доля %.2f в пределах 0..1", f);
  float a2[3] = {9.f, 1.f, 3.f}, b2[3] = {11.f, 1.f, 3.f};        // мимо на 3 м
  float f2 = trafficBoltHit(t, a2, b2, dir, 0);
  CHECK(f2 > 1.f, "отрезок мимо пешехода на 3 м: доли нет (%.2f)", f2);
  float a3[3] = {9.f, 3.5f, 0.f}, b3[3] = {11.f, 3.5f, 0.f};      // выше головы
  CHECK(trafficBoltHit(t, a3, b3, dir, 0) > 1.f, "над головой пешехода: доли нет");
  float ac[3] = {19.f, 1.f, 0.f}, bc[3] = {21.f, 1.f, 0.f};       // через машину
  float fc = trafficBoltHit(t, ac, bc, dir, 0);
  CHECK(fc >= 0.f && fc <= 1.f, "машина трафика на пути: доля %.2f", fc);
  float d0 = c->car.damage; trafficBoltHit(t, ac, bc, dir, 1);
  CHECK(fabsf(c->car.damage - d0 - .1f) < 1e-4f, "удар по машине: урон +0,1 (было %.2f, стало %.2f)", d0, c->car.damage);
  for (int i = 0; i < 20; i++) trafficBoltHit(t, ac, bc, dir, 1);
  CHECK(c->car.damage <= .999f + 1e-4f, "один снаряд не разбивает: урон не выше 0,999 (%.3f)", c->car.damage);
  e->mode = TRP_DOWN; CHECK(trafficBoltHit(t, a, b, dir, 0) > 1.f, "лежащий пешеход снарядом не задевается (доли нет)");
  e->mode = TRP_WALK; c->mode = TRM_OFF;
  trafficBoltHit(t, a, b, dir, 1);
  CHECK(e->mode == TRP_FLY && fabsf(e->vx - 7.07f) < .05f && fabsf(e->vy - 7.07f) < .05f && fabsf(e->vz) < .05f, "удар по пешеходу: летит (mode %d), vx %.2f vy %.2f vz %.2f (ждём 7,07 / 7,07 / 0 = 10 м/с под 45°)", e->mode, e->vx, e->vy, e->vz);
  printf("RESULT %s\n", fail ? "FAIL" : "PASS"); return !!fail;
}
