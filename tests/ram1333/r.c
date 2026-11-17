// 1333: таран «школьный автобус против седана» без езды: цель стоит, таран идёт на неё на 60/80/90 км/ч с газом, борт и зад. Печатает: наибольшую высоту низа цели над землёй, поднялась ли она на крышу тарана
// (низ цели выше крыши тарана), урон цели и тарана, скорость цели через 1 с, смещение цели.  r_host [таран] [цель]
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "car.h"
#include "carparams.h"
#define NT 2
static struct { ColHdr h; ColTri t[NT]; unsigned start[2], refs[NT]; } fd; static ColWorld W;
typedef struct { float maxLift, maxOver, dmgT, dmgR, vT1, shift; int onRoof; } Res;
static Res run(const char *ram, const char *tgt, float kmh, int rear) {
  CarParams pa, pb; carParamsFor(ram, &pa); carParamsFor(tgt, &pb); Car a, b; float dt = 1.f / 30.f;
  carInit(&a, &pa, &W, 0, 0, -30, 0); carInit(&b, &pb, &W, 0, 0, 0, rear ? 0.f : 1.5708f);   // таран идёт вдоль +z; цель: зад (тот же курс) или борт (поперёк)
  a.huskDrive = 1; for (int f = 0; f < 15; f++) { CarInput in; memset(&in, 0, sizeof in); carUpdate(&a, &W, &in, dt); carUpdate(&b, &W, &in, dt); }
  a.vel[2] = kmh / 3.6f; Res r; memset(&r, 0, sizeof r); float t = 0, tc = -1, z0 = b.z, x0 = b.x;
  for (int f = 0; f < 30 * 6; f++, t += dt) {
    CarInput ia; memset(&ia, 0, sizeof ia); ia.throttle = tc < 0 ? 1.f : 0.f; CarInput ib; memset(&ib, 0, sizeof ib);
    carUpdate(&a, &W, &ia, dt); carUpdate(&b, &W, &ib, dt);
    int hit = carHitCar(&a, &b, &W); if (hit && tc < 0) tc = t;
    if (getenv("T1333") && tc >= 0 && t - tc < 1.2f) printf("   t=%.2f hit=%d B: y=%.2f vy=%.2f vx=%.2f vz=%.2f w=(%.1f %.1f %.1f) up=%.2f | A: y=%.2f vz=%.2f sup=%d/%d\n", t - tc, hit, b.y, b.vel[1], b.vel[0], b.vel[2], b.w[0], b.w[1], b.w[2], 1.f - 2.f * (b.q[1] * b.q[1] + b.q[3] * b.q[3]), a.y, a.vel[2], a.supOn, b.supOn);
    float lift = b.y + pb.bodyLo; if (lift > r.maxLift) r.maxLift = lift;
    float roofA = a.y + pa.bodyHi, over = (b.y + pb.bodyLo) - roofA; if (over > r.maxOver) r.maxOver = over;
    if (b.supOn || a.supOn) r.onRoof = 1;
    if (tc >= 0 && t - tc < 1.f && t - tc + dt >= 1.f) r.vT1 = sqrtf(b.vel[0] * b.vel[0] + b.vel[2] * b.vel[2]);
  }
  r.dmgT = b.damage; r.dmgR = a.damage; r.shift = hypotf(b.x - x0, b.z - z0); (void)z0; return r; }
int main(int argc, char **argv) {
  const char *ram = argc > 1 ? argv[1] : "schoolbu", *tgt = argc > 2 ? argv[2] : "sedana";
  memset(&fd, 0, sizeof fd); memcpy(fd.h.magic, "COL1", 4); fd.h.ox = fd.h.oz = -2000; fd.h.cell = 4000; fd.h.W = fd.h.H = 1; fd.h.ntri = fd.h.nref = NT;
  { float x0 = -2000, x1 = 2000, z0 = -2000, z1 = 2000; fd.t[0] = (ColTri){{x0, 0, z0}, {x1, 0, z1}, {x1, 0, z0}, {0, 1, 0}}; fd.t[1] = (ColTri){{x0, 0, z0}, {x0, 0, z1}, {x1, 0, z1}, {0, 1, 0}}; }
  fd.start[1] = 2; fd.refs[0] = 0; fd.refs[1] = 1; if (colInit(&W, &fd, sizeof fd)) return 2;
  CarParams pa; carParamsFor(ram, &pa);
  printf("1333 %s -> %s (крыша тарана %.2f м)\n  %-10s %-5s %10s %12s %7s %8s %8s %8s\n", ram, tgt, pa.bodyHi, "км/ч", "куда", "низ цели,м", "выше крыши,м", "на крыше", "урон ц.", "урон т.", "сдвиг,м");
  int only = getenv("T1333") ? atoi(getenv("T1333")) : 0; for (int rear = 0; rear < 2; rear++) for (int k = 0; k < 3; k++) { if (only && (60 + 15 * k != only || rear)) continue; float kmh = 60 + 15 * k; Res r = run(ram, tgt, kmh, rear);
    printf("  %-10.0f %-5s %10.2f %12.2f %7d %8.2f %8.2f %8.1f\n", kmh, rear ? "зад" : "борт", r.maxLift, r.maxOver, r.onRoof, r.dmgT, r.dmgR, r.shift); }
  return 0; }
