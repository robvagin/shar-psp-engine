// 1323: гейт «неваляшка по контакту». Три проверки тем же carUpdate, что в игре, cletu_v (бортовые пары) и famil_v (диагональ):
//  A одним бортом на кочку 0,45 м (оба колеса борта без контакта): число шагов момента неваляшки (carWeebleN) > 0 на новом, 0 на старом (comp <= 0 по борту не включал: диагональ).
//  B свободный полёт (падение с 3 м на плиту): до касания момент есть на новом (carWeebleN > 0), на старом нет.
//  C круг с ручником на 80 км/ч: крен не больше прежнего (17,0° cletu_v с запасом 0,5°).
//  D ровная плита, четыре колеса на земле, прямая: момента нет ни на старом, ни на новом (ложных срабатываний 0).
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "car.h"
#include "carparams.h"
#define NT 10
static struct { ColHdr h; ColTri t[NT]; unsigned start[2], refs[NT]; } fd;
static void quad(int i, float x0, float x1, float z0, float y0, float z1, float y1) {
  float ny = (z1 - z0), nz = -(y1 - y0), l = sqrtf(ny * ny + nz * nz); ny /= l; nz /= l;
  fd.t[i] = (ColTri){{x0, y0, z0}, {x1, y1, z1}, {x1, y0, z0}, {0, ny, nz}};
  fd.t[i + 1] = (ColTri){{x0, y0, z0}, {x0, y1, z1}, {x1, y1, z1}, {0, ny, nz}};
}
static ColWorld W;
static int mk(float H) {
  memset(&fd, 0, sizeof fd); memcpy(fd.h.magic, "COL1", 4);
  fd.h.ox = fd.h.oz = -2000; fd.h.cell = 4000; fd.h.W = fd.h.H = 1; fd.h.ntri = fd.h.nref = NT;
  quad(0, -2000, 2000, -2000, 0, 2000, 0);
  quad(2, 0.3f, 1.6f, 60, 0, 61.2f, H); quad(4, 0.3f, 1.6f, 61.2f, H, 62.4f, 0);      // кочка одним бортом
  quad(6, -3, 3, 100, 0, 112, 1.5f); quad(8, -3, 3, 112, 1.5f, 112.1f, 0);            // трамплин на обе стороны: свободный полёт
  fd.start[1] = NT; for (int i = 0; i < NT; i++) fd.refs[i] = i;
  return colInit(&W, &fd, sizeof fd); }
int main(int argc, char **argv) {
  const char *nm = argc > 1 ? argv[1] : "cletu_v"; float dt = 1.f / 30.f;
  if (mk(argc > 3 ? atof(argv[3]) : 0.45f)) return 2;
  CarParams p; carParamsFor(nm, &p); Car c; unsigned nA, nB, nD; float rC = 0;
  // A: правило пар. Стоим на плите, один шаг с подставленным контактом: колёса борта (0,2) без контакта / колёса диагонали (0,3) без контакта, comp > 0 (подвеска сжата: прежнее правило по comp их «оторванными» не считает)
  carInit(&c, &p, &W, 0, 0, 0, 0); c.huskDrive = 1;
  for (int f = 0; f < 20; f++) { CarInput in; memset(&in, 0, sizeof in); carUpdate(&c, &W, &in, dt); }
  unsigned aS, aD;
  { Car d = c; for (int i = 0; i < 4; i++) d.comp[i] = 0.05f; d.wcon = 0x5; CarInput in; memset(&in, 0, sizeof in); unsigned n0 = carWeebleN; carUpdate(&d, &W, &in, dt / 4); aS = carWeebleN - n0; }
  { Car d = c; for (int i = 0; i < 4; i++) d.comp[i] = 0.05f; d.wcon = 0x9; CarInput in; memset(&in, 0, sizeof in); unsigned n0 = carWeebleN; carUpdate(&d, &W, &in, dt / 4); aD = carWeebleN - n0; }
  nA = aS; unsigned nA2 = aD;
  // B: трамплин (z 100..112, 1,5 м) на 70 км/ч: шаги полёта (ни одного колеса на земле) с моментом неваляшки
  carInit(&c, &p, &W, 0, 0, 80, 0); c.huskDrive = 1; carWeebleN = 0; int air = 0; unsigned b0 = 0;
  for (int f = 0; f < 30 * 10 && c.z < 170; f++) { CarInput in; memset(&in, 0, sizeof in); in.throttle = c.v < 70 / 3.6f ? 1.f : 0.f; carUpdate(&c, &W, &in, dt);
    if (c.wcon == 0 && c.z > 112) { air++; b0 = carWeebleN; } }
  nB = air > 0 ? b0 : 0;
  // D: прямая по плите 60 км/ч
  carInit(&c, &p, &W, -500, 0, 0, 0); c.huskDrive = 1; for (int f = 0; f < 30 * 20; f++) { CarInput in; memset(&in, 0, sizeof in); in.throttle = c.v < 60 / 3.6f ? 1.f : 0.f; carUpdate(&c, &W, &in, dt); } carWeebleN = 0;
  for (int f = 0; f < 30 * 5; f++) { CarInput in; memset(&in, 0, sizeof in); in.throttle = c.v < 60 / 3.6f ? 1.f : 0.f; carUpdate(&c, &W, &in, dt); }
  nD = carWeebleN;
  // C: круг 80 км/ч с ручником 1 с
  carInit(&c, &p, &W, 800, 0, 800, 0); c.huskDrive = 1; for (int f = 0; f < 30 * 25; f++) { CarInput in; memset(&in, 0, sizeof in); in.throttle = c.v < 80 / 3.6f ? 1.f : 0.f; carUpdate(&c, &W, &in, dt); }
  for (int f = 0; f < 30 * 6; f++) { CarInput in; memset(&in, 0, sizeof in); in.throttle = 1.f; in.steer = 1.f; in.handbrake = f < 30; carUpdate(&c, &W, &in, dt); float r = fabsf(c.roll) * 57.2958f; if (r > rC) rC = r; }
  printf("w1323 %s A_борт=%u A_диаг=%u B_полёт=%u(%d шагов в воздухе) C_крен=%.1f D_прямая=%u\n", nm, nA, nA2, nB, air, rC, nD);
  return 0; }
