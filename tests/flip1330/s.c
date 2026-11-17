// 1330: стенд «машина лежит» без езды: время от переворота до сброса у нас (car.c carUpdate, машина игрока) и по правилу оригинала (Vehicle::Update игрока).
// Правило оригинала (по поведению): вверх колёсами (ось вверх тела против мировой) и ни одного колеса с контактом — как только сглаженные скорость < 0,15 м/с и вращение < 0,3 рад/с — сразу сброс;
// сглаживание: скользящее среднее с коэффициентом 1/15 за кадр, начало 0,1 м/с и 0,6 рад/с, обновляется только в кадрах «вверх колёсами без контакта»;
// на боку (ось вверх < 0,2 со знаком, то есть и вверх колёсами) и скорость < 1 м/с — дольше 1 с — тот же сброс. Сам сброс в оригинале: белое затемнение 1,0 с, перенос, пауза 0,25 с, раскрытие.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "car.h"
#include "carparams.h"
#define NT 4
static struct { ColHdr h; ColTri t[NT]; unsigned start[2], refs[NT]; } fd;
static ColWorld W;
static void quad(int i, float x0, float x1, float z0, float y0, float z1, float y1) {
  float ny = (z1 - z0), nz = -(y1 - y0), l = sqrtf(ny * ny + nz * nz); ny /= l; nz /= l;
  fd.t[i] = (ColTri){{x0, y0, z0}, {x1, y1, z1}, {x1, y0, z0}, {0, ny, nz}};
  fd.t[i + 1] = (ColTri){{x0, y0, z0}, {x0, y1, z1}, {x1, y1, z1}, {0, ny, nz}};
}
static void quat(float *q, float ax, float ay, float az, float a) { float s = sinf(a / 2); q[0] = cosf(a / 2); q[1] = ax * s; q[2] = ay * s; q[3] = az * s; }
typedef struct { const char *name; float roll, pitch, wz, wx, y; } Pose;
int main(int argc, char **argv) {
  const char *nm = argc > 1 ? argv[1] : "famil_v"; float dt = 1.f / 30.f;
  memset(&fd, 0, sizeof fd); memcpy(fd.h.magic, "COL1", 4); fd.h.ox = fd.h.oz = -2000; fd.h.cell = 4000; fd.h.W = fd.h.H = 1; fd.h.ntri = fd.h.nref = NT;
  quad(0, -2000, 2000, -2000, 0, 2000, 0); fd.start[1] = NT; for (int i = 0; i < NT; i++) fd.refs[i] = i;
  if (colInit(&W, &fd, sizeof fd)) return 2;
  CarParams p; carParamsFor(nm, &p);
  Pose P[] = {
    {"крыша ровно, в покое",         3.14159f, 0, 0, 0, -1.f},
    {"крыша, упала с 0,6 м",           3.14159f, 0, 0, 0, 2.0f},
    {"крыша, кувырок после прыжка",  3.0f, 0.2f, 2.5f, 0.8f, 3.0f},
    {"на боку 90°, в покое",         1.5708f, 0, 0, 0, -1.f},
    {"на боку, упала с 0,5 м",         1.5708f, 0, 0, 0, 2.0f},
    {"на боку, качается (крен 2 рад/с)", 1.4f, 0, 2.0f, 0, 1.5f},
    {"наклон 60° (опора о бордюр)",  1.047f, 0, 0, 0, 1.3f},
  };
  printf("%-34s %8s %8s %8s %8s\n", "поза", "наш,с", "оригин,с", "лежит,с", "колёс@");
  for (unsigned i = 0; i < sizeof P / sizeof *P; i++) {
    Car c; carInit(&c, &p, &W, 0, 0, 0, 0); c.huskDrive = 1; for (int f = 0; f < 10; f++) { CarInput in; memset(&in, 0, sizeof in); carUpdate(&c, &W, &in, dt); }
    float yy = P[i].y; if (yy < 0) yy = P[i].roll > 2.f ? p.bodyHi - .02f : p.halfW - .02f; if (P[i].y == -2.f) yy += 0.f;
    quat(c.q, 0, 0, 1, P[i].roll); if (P[i].pitch) { float q2[4]; quat(q2, 1, 0, 0, P[i].pitch); float *a = c.q; float w = a[0] * q2[0] - a[1] * q2[1] - a[2] * q2[2] - a[3] * q2[3], x = a[0] * q2[1] + a[1] * q2[0] + a[2] * q2[3] - a[3] * q2[2], y = a[0] * q2[2] - a[1] * q2[3] + a[2] * q2[0] + a[3] * q2[1], z = a[0] * q2[3] + a[1] * q2[2] - a[2] * q2[1] + a[3] * q2[0]; c.q[0] = w; c.q[1] = x; c.q[2] = y; c.q[3] = z; }
    c.y = yy; c.vel[0] = c.vel[1] = c.vel[2] = 0; c.w[0] = P[i].wx; c.w[1] = 0; c.w[2] = P[i].wz;
    unsigned r0 = c.rights; float lin = .1f, ang = .6f, sideT = 0, tOur = -1, tOrg = -1, tLie = 0; int wAt = -1;
    for (int f = 0; f < 30 * 12; f++) {
      CarInput in; memset(&in, 0, sizeof in); float t = (f + 1) * dt;
      unsigned rb = c.rights + c.recoveries; float ybef = c.y; (void)ybef;
      carUpdate(&c, &W, &in, dt);
      float up = 1.f - 2.f * (c.q[1] * c.q[1] + c.q[3] * c.q[3]), sp = sqrtf(c.vel[0] * c.vel[0] + c.vel[1] * c.vel[1] + c.vel[2] * c.vel[2]), an = sqrtf(c.w[0] * c.w[0] + c.w[1] * c.w[1] + c.w[2] * c.w[2]);
      if (c.rights + c.recoveries > rb && tOur < 0) { tOur = t; }
      if (tOur >= 0) break;
      if (up < 0.f && c.wheels == 0) { lin += (sp - lin) / 15.f; ang += (an - ang) / 15.f; if (lin < .15f && ang < .3f && tOrg < 0) { tOrg = t; wAt = c.wheels; } }
      if (sp < 1.f && up < .2f) { sideT += dt; if (sideT > 1.f && tOrg < 0) tOrg = t; } else sideT = 0.f;
      if (up < .35f) tLie = t;
      if (getenv("V1330") && !strcmp(P[i].name, getenv("V1330")) && (f % 6 == 0 || f < 6)) printf("   rights=%u rec=%u grounded=%d contacts=%d flipT=%.2f pen=%.2f\n", c.rights, c.recoveries, c.grounded, c.contacts, c.flipT, c.pen), printf("   t=%.2f y=%.2f up=%.2f sp=%.2f an=%.2f wheels=%d lin=%.2f ang=%.2f side=%.2f\n", t, c.y, up, sp, an, c.wheels, lin, ang, sideT);
    }
    printf("%-34s %8.2f %8.2f %8.2f %8d\n", P[i].name, tOur, tOrg, tLie, wAt);
    (void)r0;
  }
  return 0;
}
