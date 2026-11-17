// №946 (Роб 25.10, TEST59: «очень много столбов не сбивается: L7 тематические, уровень Барта тоже»): перепись ВСЕХ сбиваемых предметов уровня и удар машиной в лоб.
// Настоящие car.c / col.c / props.c / stateprops.c на данных уровня, без эмулятора. Две семьи:
//   P — пропы PRP1 (propsHitCar: столбы, фонари, знаки, гидранты, деревья): заезд в лоб 12 м/с по 4 курсам прямо в ствол (или центр коробки);
//       сбит = проп проснулся / спрятан / отдан обломку (onKick);
//   S — стейтпропы SPR1 (spHitCar), у чьего начального состояния есть событие машины (HIT / VEHICLE_HIT / DESTROYED), без миссионных тегов:
//       заезд в центр объёма по 8 курсам 8 и 15 м/с; сбит = состояние сменилось / убран / событие машины.
// Исход заезда: 1 сбит, 0 касание есть, а реакции нет (ДЫРА), -1 стена мира остановила машину до встречи (в переписи не ставится в вину предмету, но считается отдельно).
// Строка на экземпляр: tsv  семья уровень тип/номер ... исход. Итог — по типам и в целом: доля сбитых из тех заездов, где машина дошла.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "stateprops.h"
#include "car.h"
unsigned homerSolidFrame = 0;
void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }   // 107б: заглушка сторожа (car.c его зовёт)
static Props props; static StateProps sp; static ColWorld world; static Car car;
static float gUp[400], gX[400], gY[400], gZ[400]; static int gN;
static int kicked, kickI; static float gTilt;   // gTilt: наименьшая «вертикальность» (R[1][1]) сбитого предмета за 4 с после удара; 1 — стоит прямо
static PropState *snap946;   // исходное состояние всех предметов: каждый заезд начинается с чистого уровня (упавшие прошлым заездом не лежат дальше)
static float gSt[4][4]; static float gFall;   // gFall: сколько секунд от удара до up < 0,3 (9 — не упал за 4 с) static float gSt[4][4];   // старт заезда по курсам: x, z, курс, земля
static int onKick(void *u, unsigned i, float dx, float dz) { (void)u; (void)dx; (void)dz; kicked = 1; kickI = (int)i; return 0; }

static int runP(unsigned i, int head, float v0, float *gap) {
  if (getenv("TALLOLD946")) propsTallFallOld = 1; if (getenv("POLE968OLD")) propsPole968Old = 1;
  float T0r = props.types[props.def[i].type].radius; if (getenv("V946")) v0 = (float)atof(getenv("V946"));
  for (unsigned k = 0; k < props.ninst; k++) props.st[k] = snap946[k];
  PropState *s = &props.st[i]; float hx = s->x, hz = s->z; const float h0[3] = {s->x, s->y, s->z};
  const float *tk = props.trunk ? props.trunk + props.def[i].type * 3 : 0;
  if (tk && tk[2] > 0.f) { float m[16]; propPieceMatrix(s, tk[0], s->cy, tk[1], 1.f, m); hx = m[12]; hz = m[14]; }
  else { float m[16]; propPieceMatrix(s, s->cx, s->cy, s->cz, 1.f, m); hx = m[12]; hz = m[14]; }
  float ang = head * 1.5707963f, fx = sinf(ang), fz = cosf(ang), lx = fz, lz = -fx, d = 9.f;
  float gy = s->y, base = s->y - s->hy, g0 = 0; int found = 0;
  { float gb = 0; if (colGround(&world, hx, hz, s->y + 1.f, -1e6f, &gb, 0)) base = gb; }
  static const float ds[] = {9.f, 7.f, 11.f, 5.f, 14.f};
  for (int q = 0; q < 5 && !found; q++) { d = ds[q]; int okg = 1;   // земля по всему подходу в пределах 1,2 м от основания: машина едет по одному уровню
    for (float t = d; t >= 0.f; t -= 1.f) { if (!colGround(&world, hx - fx * t, hz - fz * t, base + 1.5f, base - 1.5f, &g0, 0) || fabsf(g0 - base) > 1.2f) { okg = 0; break; } }
    if (okg) { found = 1; gy = g0; colGround(&world, hx - fx * d, hz - fz * d, base + 1.5f, base - 1.5f, &gy, 0); } }
  if (!found) { *gap = 0.f; return -3; }
  gSt[head][0] = hx - fx * d; gSt[head][1] = hz - fz * d; gSt[head][2] = ang; gSt[head][3] = gy;
  carInit(&car, 0, &world, hx - fx * d, gy + 1.f, hz - fz * d, ang); carSetSpeed(&car, v0);
  CarInput in = {getenv("GAS968") ? (float)atof(getenv("GAS968")) : 1.f, 0, 0, 0}; kicked = 0; kickI = -1; float xs = hx - fx * d, zs = hz - fz * d;
  int touchedWall = 0;
  for (int st = 0; st < 60; st++) {
    float lat = (car.x - hx) * lx + (car.z - hz) * lz - (getenv("OFF946") ? (float)atof(getenv("OFF946")) * T0r : 0.f);   // держим курс на ствол: боковое смещение убираем (OFF946 — доля радиуса: удар вскользь)
    car.x -= lat * lx; car.z -= lat * lz; car.vel[0] -= (car.vel[0] * lx + car.vel[2] * lz) * lx; car.vel[2] -= (car.vel[0] * lx + car.vel[2] * lz) * lz;
    carUpdate(&car, &world, &in, 1.f / 30.f);
    if (car.hardV > 2.f && ((car.x - hx) * fx + (car.z - hz) * fz) < -2.5f) touchedWall = 1;
    propsUpdate(&props, &world, &car, 1.f / 30.f);
    if ((kicked && kickI == (int)i) || !s->asleep || s->hidden || fabsf(s->x - h0[0]) + fabsf(s->z - h0[2]) > .05f) {
      gTilt = 1.f; gFall = 9.f; gN = 0;   // дальше 4 с следим, падает ли предмет (машина продолжает ехать с тем же газом)
      for (int k = 0; k < 120 && !s->hidden; k++) { carUpdate(&car, &world, &in, 1.f / 30.f); propsUpdate(&props, &world, &car, 1.f / 30.f);
        float q0 = s->q[0], q1 = s->q[1], q2 = s->q[2], q3 = s->q[3]; (void)q0; (void)q1; (void)q3; float up = 1.f - 2.f * (q1 * q1 + s->q[3] * s->q[3]); (void)q2; if (gN < 400) { gUp[gN] = up; gX[gN] = s->x; gZ[gN] = s->z; gY[gN] = s->y; gN++; } if (up < gTilt) gTilt = up; if (up < .3f && gFall > 8.f) gFall = (k + 1) / 30.f; if (getenv("DBG946") && k % 10 == 0) printf("   t=%d up=%.2f pos %.1f %.1f %.1f vel %.1f %.1f %.1f w %.2f %.2f %.2f asleep %d car %.1f %.1f v %.1f\n", k, up, s->x, s->y, s->z, s->vel[0], s->vel[1], s->vel[2], s->w[0], s->w[1], s->w[2], s->asleep, car.x, car.z, car.v); }
      return 1; }
    if (touchedWall) { *gap = -((car.x - hx) * fx + (car.z - hz) * fz); return -1; }
    (void)xs; (void)zs;
  }
  *gap = -((car.x - hx) * fx + (car.z - hz) * fz);
  if (getenv("DBG946")) printf("   P нет реакции: курс %d машина %.2f %.2f %.2f v %.1f до ствола %.2f; проп %.2f %.2f %.2f asleep %d hitCool %.2f\n", head, car.x, car.y, car.z, car.v, *gap, s->x, s->y, s->z, s->asleep, s->hitCool);
  return *gap > 2.5f ? -1 : 0;
}

static void quatR3(const float q[4], float R[9]) {
  float w = q[0], x = q[1], y = q[2], z = q[3];
  R[0] = 1 - 2 * (y * y + z * z); R[3] = 2 * (x * y - w * z); R[6] = 2 * (x * z + w * y);
  R[1] = 2 * (x * y + w * z); R[4] = 1 - 2 * (x * x + z * z); R[7] = 2 * (y * z - w * x);
  R[2] = 2 * (x * z - w * y); R[5] = 2 * (y * z + w * x); R[8] = 1 - 2 * (x * x + y * y);
}
static int carEvent(unsigned i) {
  const SpType *T = &sp.type[sp.inst[i].type]; const SpState *S = &sp.state[T->firstState];
  for (unsigned k = 0; k < S->nEvent; k++) { int e = sp.event[S->firstEvent + k].event; if (e == SP_EV_VEHICLE_HIT || e == SP_EV_HIT || e == SP_EV_DESTROYED) return 1; }
  return 0;
}
static int runS(unsigned i, float x, float y, float z, float h, float v, int *wall) {
  spFree(&sp); if (spInit(&sp, props.spr, props.sprSize)) return -2;
  carInit(&car, 0, &world, x, y, z, h); carSetSpeed(&car, v);
  CarInput in = {1.f, 0, 0, 0}; int touched = 0; *wall = 0;
  for (int n = 0; n < 48; n++) {
    carUpdate(&car, &world, &in, 1.f / 30.f);
    if (!touched && car.hardV > 1.f) { *wall = 1; if (getenv("DBG946")) printf("   стена: шаг %d машина %.2f %.2f %.2f v %.1f hardV %.1f, предмет %.2f %.2f %.2f\n", n, car.x, car.y, car.z, car.v, car.hardV, sp.run[i].x, sp.run[i].y, sp.run[i].z); return -1; }
    float c0 = sp.run[i].cool; sp.run[i].cool = 0.f; int h0 = sp.hits, st0 = sp.run[i].state;
    spUpdate(&sp, &world, &car, NULL, 1.f / 30.f);
    if (sp.run[i].cool > 0.f) { touched = 1; if (sp.hits > h0 && c0 <= 0.f) return 1; } else sp.run[i].cool = fmaxf(0.f, c0 - 1.f / 30.f);
    if (sp.run[i].state != st0 || !sp.run[i].alive) return 1;
  }
  return touched ? 0 : -1;
}

int main(int argc, char **argv) {
  if (getenv("POLE1276OLD")) propsPoleKick1276Old = 1;   // 107б П6: красный контроль — толчок бампера 1046
  if (argc < 3) return 2; const char *mode = argv[2]; const char *only = argc > 3 ? argv[3] : 0;
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
  fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (!data || fread(data, 1, size, f) != (size_t)size) return 2; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&world, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40))) { printf("нет данных\n"); return 2; }
  props.onKick = onKick; snap946 = malloc(sizeof(PropState) * props.ninst); memcpy(snap946, props.st, sizeof(PropState) * props.ninst);
  int tot = 0, ok = 0, hole = 0, wall = 0;
  if (!strcmp(mode, "T")) {   // типы пропов: радиус, полувысота, масса, ствол, число экземпляров — видно, какие «столбы» вообще имеют массу (сбиваемы)
    for (unsigned t = 0; t < props.ntype; t++) { unsigned n = 0; for (unsigned i = 0; i < props.ninst; i++) n += props.def[i].type == t;
      const PropType *T = &props.types[t]; const float *tk = props.trunk ? props.trunk + t * 3 : 0;
      printf("T\t%u\tn %u\tr %.2f hH %.2f m %.0f trunk %.2f\n", t, n, T->radius, T->halfH, T->mass, tk ? tk[2] : -1.f); }
    return 0; }
  if (!strcmp(mode, "Y")) {   // №968: типы с hH >= 2: число, радиус, масса, ствол, частицы удара, обломок (-1 нет: падает целым предметом)
    for (unsigned t = 0; t < props.ntype; t++) { const PropType *T = &props.types[t]; if (T->halfH < 2.f || T->mass <= 0.f) continue;
      unsigned n = 0; for (unsigned i = 0; i < props.ninst; i++) n += props.def[i].type == t; int part = -9, brk = -9; propsTypeFx(&props, t, &part, &brk);
      const float *tk = props.trunk ? props.trunk + t * 3 : 0;
      printf("Y\t%u\tn %u\tr %.2f hH %.2f m %.0f trunk %.2f\tparticle %d break %d\n", t, n, T->radius, T->halfH, T->mass, tk ? tk[2] : -1.f, part, brk); }
    return 0; }
  if (!strcmp(mode, "F")) {   // №968: падение предмета i при ударе машины на заданных скоростях (м/с): моменты (с после первого наклона >1°) до 10/30/60/85°, итоговый наклон, смещение центра
    unsigned i = (unsigned)atoi(only); extern int propsTallFallOld;
    for (int a = 4; a < argc; a++) { float v = (float)atof(argv[a]); float gap; int r = runP(i, 0, v, &gap);
      int k1 = -1, k10 = -1, k30 = -1, k60 = -1, k85 = -1; float fin = 0, dm = 0;
      for (int k = 0; k < gN; k++) { float deg = acosf(fmaxf(-1.f, fminf(1.f, gUp[k]))) * 57.29578f;
        if (k1 < 0 && deg > 1.f) k1 = k; if (k1 >= 0) { if (k10 < 0 && deg >= 10.f) k10 = k; if (k30 < 0 && deg >= 30.f) k30 = k; if (k60 < 0 && deg >= 60.f) k60 = k; if (k85 < 0 && deg >= 85.f) k85 = k; } }
      if (gN) { fin = acosf(fmaxf(-1.f, fminf(1.f, gUp[gN - 1]))) * 57.29578f; dm = hypotf(gX[gN - 1] - gX[0], gZ[gN - 1] - gZ[0]); }
      #define TT(k) (k1 >= 0 && (k) >= 0 ? ((k) - k1) / 30.f : -1.f)
      printf("F v %.1f res %d: 10deg %.2f 30deg %.2f 60deg %.2f 85deg %.2f s | final %.0f deg | centre moved %.1f m | steps %d\n", v, r, TT(k10), TT(k30), TT(k60), TT(k85), fin, dm, gN); }
    return 0; }
  if (!strcmp(mode, "P")) {
    for (unsigned i = 0; i < props.ninst; i++) {
      const PropType *T = &props.types[props.def[i].type]; if (T->mass <= 0.f) continue;
      if (only && (int)i != atoi(only)) continue;
      int best = -1; float gap = 0; int res[4];
      for (int hd = 0; hd < 4; hd++) { res[hd] = runP(i, hd, 12.f, &gap); if (res[hd] > best) best = res[hd]; }
      const float *tk = props.trunk ? props.trunk + props.def[i].type * 3 : 0;
      // экземпляр: сбит, если хоть с одного курса (стена закрывает другие); дыра — если ни с одного не сбит, а хоть с одного машина дошла
      int any1 = 0, any0 = 0, anyW = 0; for (int hd = 0; hd < 4; hd++) { any1 |= res[hd] == 1; any0 |= res[hd] == 0; anyW |= res[hd] <= -1; }
      int r = any1 ? 1 : any0 ? 0 : -1;
      printf("P\t%u\ttype %u\tr %.2f hH %.2f m %.0f trunk %.2f\tpos %.1f %.1f %.1f\t%d %d %d %d\ttilt %.2f fall %.2f\t%s\n", i, props.def[i].type, T->radius, T->halfH, T->mass, tk ? tk[2] : -1.f,
             props.st[i].home[0], props.st[i].home[1], props.st[i].home[2], res[0], res[1], res[2], res[3], gTilt, gFall, r == 1 ? "OK" : r == 0 ? "ДЫРА" : "СТЕНА");
      { int hb = -1; for (int hd = 0; hd < 4; hd++) if (res[hd] == 1) { hb = hd; break; } if (hb >= 0) printf("ST\t%u\t%d\t%.2f\t%.2f\t%.4f\t%.2f\ttype %u\n", i, hb, gSt[hb][0], gSt[hb][1], gSt[hb][2], gSt[hb][3], props.def[i].type); }   // старт заезда, сбивший предмет (для сценария эмулятора)
      tot++; ok += r == 1; hole += r == 0; wall += r == -1; (void)anyW;
    }
  } else {
    if (!props.spr || spInit(&sp, props.spr, props.sprSize)) { printf("нет стейтпропов\n"); return 0; }
    for (unsigned i = 0; i < sp.ninst; i++) {
      const SpInst *I = &sp.inst[i]; const SpType *T = &sp.type[I->type];
      if (T->kind || !T->nCol || I->tag || !carEvent(i)) continue;
      if (only && (int)i != atoi(only)) continue;
      float R[9]; quatR3(I->q, R);
      float c[3] = {I->x + R[0] * T->c[0] + R[3] * T->c[1] + R[6] * T->c[2], I->y + R[1] * T->c[0] + R[4] * T->c[1] + R[7] * T->c[2], I->z + R[2] * T->c[0] + R[5] * T->c[1] + R[8] * T->c[2]};
      float ext = sqrtf(T->h[0] * T->h[0] + T->h[2] * T->h[2]) + 1.f; int n1 = 0, n0 = 0, nw = 0, w;
      for (int a = 0; a < 8; a++) for (int s = 0; s < 2; s++) {
        float ang = a * 0.7853982f, fx = sinf(ang), fz = cosf(ang), d = ext + 6.f;
        float gs = 0; int okg = colGround(&world, c[0] - fx * d, c[2] - fz * d, c[1] + 3.f, -1e6f, &gs, 0) && fabsf(gs - (c[1] - T->h[1])) < 2.5f;
        int r = okg ? runS(i, c[0] - fx * d, c[1] + 3.f, c[2] - fz * d, ang, s ? 15.f : 8.f, &w) : -1;
        if (r == 1) n1++; else if (r == 0) n0++; else nw++;
      }
      int r = n1 ? 1 : n0 ? 0 : -1;
      printf("S\t%u\ttype %u\tcls %u atc %u mass %.0f col %u k%u h %.2f %.2f %.2f\tpos %.1f %.1f %.1f\tOK %d дыра %d стена %d\t%s\n", i, I->type, T->cls, T->atc, T->mass, T->nCol, sp.col[T->firstCol].kind, T->h[0], T->h[1], T->h[2],
             I->x, I->y, I->z, n1, n0, nw, r == 1 ? "OK" : r == 0 ? "ДЫРА" : "СТЕНА");
      tot++; ok += r == 1; hole += r == 0; wall += r == -1;
    }
  }
  printf("ИТОГ %s %s: предметов %d, сбито %d, ДЫРА %d, стена %d; доля сбитых из дошедших %.1f %%\n", mode, argv[1], tot, ok, hole, wall, ok + hole ? 100.f * ok / (ok + hole) : 100.f);
  return hole ? 1 : 0;
}
