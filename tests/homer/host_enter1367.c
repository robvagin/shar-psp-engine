// Native (Mac) harness for viewer/homer.c: movement numbers, COL1 collision and the Ф3 episode without the PSP.
//   tests/homer/host.sh            (builds with -DSKIN_HOST and runs on build/level.shr + build/homer.shr)
// Same modules as the engine: homer.c, skin.c (host mode), col.c, car.c, props.c, stateprops.c, coins.c.
// Prints one line per check with the measured number and the expected (original) value; exit code = failed checks.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "homer.h"
#include "props.h"
#include "stateprops.h"
#include "coins.h"

static ColWorld world; static Car car; static Props props; static StateProps sp; static Coins coins;
static int haveSP, haveCoins, fails;
static float doorOpen, doorMax; static int doneIn = -1, doneOut = -1;
static void onDoor(void *u, float o) { (void)u; doorOpen = o; if (o > doorMax) doorMax = o; }
static void onDone(void *u, int in) { (void)u; if (in) doneIn = 1; else doneOut = 1; }

static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) {
  unsigned char *m = aligned_alloc(16, (sz + 15) & ~15u);
  fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; }
  return m;
}
static int hostNoDoor(void) { return 0; }   // №667: у рисуемой машины нет шарнира двери
static void check(const char *what, float got, float lo, float hi, const char *expect) {
  int ok = got >= lo && got <= hi; if (!ok) fails++;
  printf("%-4s %-46s %8.3f   (%s)\n", ok ? "ok" : "FAIL", what, got, expect);
}

static const float DT = 1.f / 60.f;
// подложенная стена у двери водителя (слева от машины) для проверки выхода через верх: твёрдое тело через HomerSolidFn
static float wallSide0 = -1.15f, wallSide1 = -3.5f, wallAlong = 3.f;
static int testWall(void *u, float *x, float *z, float y, float r, float *top, float *nx, float *nz) {
  (void)y; (void)top; const Car *c = (const Car *)u; float fx = sinf(c->h), fz = cosf(c->h), rx = -fz, rz = fx;
  float dx = *x - c->x, dz = *z - c->z, side = dx * rx + dz * rz, along = dx * fx + dz * fz;
  if (side - r >= wallSide0 || side + r <= wallSide1 || fabsf(along) >= wallAlong) return 0;
  float d = wallSide0 + r - side; *x += rx * d; *z += rz * d; *nx = rx; *nz = rz; return 1;
}
static HomerInput in0(float camYaw) { HomerInput i; memset(&i, 0, sizeof i); i.camYaw = camYaw; return i; }
static void stepCar(float dt) { CarInput ci = {0, 0, 0, 1}; carUpdate(&car, &world, &ci, dt); }
static void frame(Homer *H, HomerInput *in) {
  HomerCar hc; homerCarFrom(&hc, &car, 0.62f);
  homerUpdate(H, &world, &hc, in, DT); homerPose(H);
  if (!homerDriving(H)) stepCar(DT);
}
static void drainEvents(Homer *H, int *hits, int *stomps, float t, float *tHit) {
  HomerEvent e;
  while (homerPopEvent(H, &e)) {
    if (e.type == HOMER_EV_HIT) { (*hits)++; if (tHit && *tHit < 0) *tHit = t; }
    if (e.type == HOMER_EV_STOMP) (*stomps)++;
    if ((e.type == HOMER_EV_HIT || e.type == HOMER_EV_STOMP) && haveSP) {   // same code as the main.c patch
      for (unsigned i = 0; i < sp.ninst; i++) {
        float c[3], r; spBounds(&sp, i, c, &r);
        if (homerHitTest(&e, c[0], c[1], c[2], fminf(r, 1.f))) spEvent(&sp, i, e.type == HOMER_EV_HIT ? SP_EV_HIT : SP_EV_STOMP);
      }
    }
  }
}

#if !COLRING_OLD
#include <unistd.h>
#include <fcntl.h>
#include "colring.h"
static int homerRingRead(void *u, unsigned off, void *dst, unsigned n) { return pread(*(int *)u, dst, n, off) == (ssize_t)n ? 0 : -1; }
#endif
int main(int argc, char **argv) {
  const char *lvl = argc > 1 ? argv[1] : "build/level.shr", *hom = argc > 2 ? argv[2] : "build/homer.shr";
  FILE *f = fopen(lvl, "rb"); if (!f) { printf("no %s\n", lvl); return 1; }
  unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104 || memcmp(hdr, "SHR7", 4)) { printf("bad level\n"); return 1; }
  const float *hd = (const float *)(hdr + 12);
  float spx = hd[6], spy = hd[7], spz = hd[8], sph = hd[9];
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned fsize = (unsigned)ftell(f);
  unsigned end = fsize; if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff);
#if !COLRING_OLD
  /* Коллизия-96: BW_COLRING=путь к colring.shr — мир из кольца (без тиков: недостающее дочитывается, кэш вне кольца тримится); та же проверка, что на целом COL1 */
  if (cm && getenv("BW_COLRING")) {
    static int rfd; rfd = open(getenv("BW_COLRING"), O_RDONLY); unsigned hh[64]; ColRing *R = 0; int err = 0;
    if (rfd >= 0 && pread(rfd, hh, sizeof hh, 0) == sizeof hh) { ColRingIO io = { homerRingRead, &rfd, 0 }; R = colRingOpen(&io, malloc((size_t)(hh[35] + 24) * CRING_PAGEB), (hh[35] + 24) * CRING_PAGEB, &err); }
    if (!R || colInitRing(&world, R)) { printf("colring failed %d\n", err); return 1; }
    colRingEnsure(R, spx, spz);
  } else
#endif
  if (!cm || colInit(&world, cm, end - colOff)) { printf("COL1 failed\n"); return 1; }
  if (propOff) {
    unsigned char *ph = readAt(f, propOff, 64);
    unsigned sz = !memcmp(ph, "PRP2", 4) ? *(unsigned *)(ph + 40) : 0;
    if (sz) { unsigned char *pm = readAt(f, propOff, sz);
      if (pm && propsInit(&props, pm, sz) == 0) {
        if (props.coinType >= 0 && props.ncoin && coinsInit(&coins, props.coinPos, props.ncoin) == 0) haveCoins = 1;
        if (props.spr && spInit(&sp, props.spr, props.sprSize) == 0) haveSP = 1;
      } }
  }
  fclose(f);
  SkinModel *m = skinLoad(hom); if (!m) { printf("no %s\n", hom); return 1; }
  static Homer H; if (homerInit(&H, m)) { printf("homerInit: clips missing\n"); return 1; }
  if (haveSP) { H.solid = homerSolidSprops; H.solidUser = &sp; }   // стейтпропы твёрдые (как в патче main.c)
  
  for (int i = 0; i < 120; i++) stepCar(DT);                       // car settles
  CarBox bw0; carBox(&car, &bw0); float ENT_W = bw0.hw + .8f;   // 04.11: сесть можно в объёме «габарит кузова + 1 м» (ГОМЕР 5.1/6.2, HOMER_ENTER_BOX_OLD=1 — прежние 5 м от двери): тесты ставят Гомера у борта (0,8 м), а не за 3 м от центра
  /* 1367: посадка героя в машину с разных мест вокруг неё: время до «в машине», ирис ли, не застрял ли. Для всех машин таблицы carparams.h (или одной: argv[3]).
     Выход: строка на машину и место; код = число случаев дольше LIMIT или без посадки. Тот же код homer.c, что в игре, без езды. */
  const char *only = argc > 3 && argv[3][0] ? argv[3] : NULL; float LIMIT = argc > 4 ? (float)atof(argv[4]) : 5.f; int bad = 0, total = 0; HomerCarCb cb = {onDoor, onDone, 0};
  extern const char *const carNames1367[]; extern int carNamesN1367;
  for (int ci = 0; ci < carNamesN1367; ci++) {
    const char *name = carNames1367[ci]; if (only && strcmp(only, name)) continue;
    CarParams p; if (!carParamsFor(name, &p)) continue;
    float ox = argc > 5 ? (float)atof(argv[5]) : 0.f, oz = argc > 6 ? (float)atof(argv[6]) : 0.f, oh = argc > 7 ? (float)atof(argv[7]) : 0.f;   /* сдвиг места и поворот машины от точки старта уровня: чтобы дверь не упиралась в бордюр */
    float gy = spy; colGround(&world, spx + ox, spz + oz, spy + 3.f, spy - 30.f, &gy, 0);
    carInit(&car, &p, &world, spx + ox, gy, spz + oz, sph + oh); for (int i = 0; i < 120; i++) stepCar(DT);
    CarBox b; carBox(&car, &b); if (getenv("H1367_V")) printf("   %s: v=%.3f hl=%.2f hw=%.2f\n", name, car.v, b.hl, b.hw); 
    float fx = b.fx, fz = b.fz, rx = -b.fz, rz = b.fx;
    struct { const char *n; float a, s; } P[] = {   /* a — вдоль кузова от центра (+вперёд), s — вбок (+право, к пассажиру); все точки в пределах «габарит + 1 м», дальше посадки нет (ГОМЕР 5.1/6.2) */
      {"пассажир, середина", 0.f, b.hw + .7f}, {"пассажир, у зада", -b.hl + .3f, b.hw + .7f}, {"пассажир, у носа", b.hl - .3f, b.hw + .7f},
      {"сзади по центру", -b.hl - .7f, 0.f}, {"спереди по центру", b.hl + .7f, 0.f}, {"сзади слева", -b.hl - .5f, -b.hw + .3f},
      {"спереди справа", b.hl + .5f, b.hw - .3f}, {"водитель, у двери", 0.f, -b.hw - .8f} };
    for (unsigned k = 0; k < sizeof P / sizeof P[0]; k++) {
      HomerCar hc; homerCarFrom(&hc, &car, .62f);
      float px = b.x + fx * P[k].a + rx * P[k].s, pz = b.z + fz * P[k].a + rz * P[k].s;
      homerPlace(&H, &world, px, car.y + 1.f, pz, car.h);
      H.irisOn = 1; int s0 = H.irisSeat, c0 = H.irisCarIn; doorMax = 0; doneIn = -1;
      int ok = homerEnterCar(&H, &hc, &cb); float t = 0.f;
      while (ok && H.state != HS_IN_CAR && t < 90.f) { HomerInput in = in0(car.h); frame(&H, &in); t += DT; }
      int iris = (H.irisSeat - s0) + (H.irisCarIn - c0); int irisWhy = (H.irisCarIn - c0) ? 2 : (H.irisSeat - s0) ? 1 : 0;   /* 2 — по данным машины/шарниру двери, 1 — упор/занята/едет/внутри габарита */
      total++; int slow = !ok || H.state != HS_IN_CAR || t > LIMIT; bad += slow;
      printf("%-4s %-9s %-24s ok=%d t=%6.2f iris=%d(%d) door=%.1f state=%d\n", slow ? "LONG" : "ok", name, P[k].n, ok, t, iris, irisWhy, doorMax, H.state);
      if (H.state == HS_IN_CAR) { H.state = HS_FOOT; }   /* следующий случай — с земли */
      homerPlace(&H, &world, px, car.y + 1.f, pz, car.h);
    }
  }
  printf("1367: случаев %d, дольше %.1f с или без посадки: %d\n", total, LIMIT, bad);
  return bad ? 1 : 0;
}
