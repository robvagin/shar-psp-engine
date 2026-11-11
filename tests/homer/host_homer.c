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
  carInit(&car, NULL, &world, spx, spy, spz, sph);
  for (int i = 0; i < 120; i++) stepCar(DT);                       // car settles
  CarBox bw0; carBox(&car, &bw0); float ENT_W = bw0.hw + .8f;   // 04.11: сесть можно в объёме «габарит кузова + 1 м» (ГОМЕР 5.1/6.2, HOMER_ENTER_BOX_OLD=1 — прежние 5 м от двери): тесты ставят Гомера у борта (0,8 м), а не за 3 м от центра
  printf("level %s: spawn %.1f %.1f %.1f h %.2f; stateprops %u, coins %u; homer %u clips\n", lvl, spx, spy, spz, sph,
         haveSP ? sp.ninst : 0, haveCoins ? coins.nworld : 0, m->h->nClips);
  printf("loco clip speeds: walk %.2f run %.2f dash %.2f m/s\n", H.locoV[1], H.locoV[2], H.locoV[3]);

  // ---------------- 1. run on flat ground (road next to the spawn, away from the car)
  float fx = sinf(sph), fz = cosf(sph), rx = -fz, rz = fx;          // car forward / right (h convention)
  float bx = car.x - rx * 6.f, bz = car.z - rz * 6.f;               // 6 m left of the car
  homerPlace(&H, &world, bx, car.y + 1.f, bz, sph);
  { HomerInput in = in0(sph); in.my = 1.f; in.dash = 1; float t8 = -1, t4 = -1, t = 0;   // ○ зажат — бег (ControllerPS2.pag DASH / SPRINT)
    for (int i = 0; i < 90; i++, t += DT) { frame(&H, &in); if (t4 < 0 && H.speed >= 3.99f) t4 = t + DT; if (t8 < 0 && H.speed >= 7.99f) t8 = t + DT; }
    check("run: time to 8 m/s, s", t8, .55f, .7f, "PS2 gh2 record: ramp ~13 m/s2 -> 0.62 (path 2.3 m in 0.6 s, 7.1 m in 1.2 s)");   // 04.11: бег разгоняется (HOMER_RUNRAMP_OLD=1 — сразу 8)
    check("run: top speed, m/s", H.speed, 7.9f, 8.01f, "8");
    in.my = 0.f; t = 0; float d0x = H.x, d0z = H.z;
    for (int i = 0; i < 90 && H.speed > .01f; i++, t += DT) frame(&H, &in);
    check("stop from 8 m/s, s", t, .7f, .9f, "decel 10 -> 0.80");
    check("stop distance, m", hypotf(H.x - d0x, H.z - d0z), 2.8f, 3.6f, "v^2/2a = 3.2");
    in.my = .5f; for (int i = 0; i < 60; i++) frame(&H, &in);
    check("half stick with O: speed, m/s", H.speed, 7.9f, 8.01f, "8: with O any stick runs full speed (original source, circle 3; old harness expected half of run)");
    in.my = 1.f; in.dash = 0; for (int i = 0; i < 60; i++) frame(&H, &in);
    check("full stick without O: speed, m/s", H.speed, 3.9f, 4.01f, "4 (walk; srr2: PRESS AND HOLD O TO RUN)");
    in.dash = 1; for (int i = 0; i < 60; i++) frame(&H, &in);
    check("hold O again: speed, m/s", H.speed, 7.9f, 8.01f, "8 (run)"); }
  // ---------------- №709: машина сбивает Гомера пешком — он падает и встаёт (красный контроль: HOST_CFLAGS=-DHOMER_CARHIT_OLD=1)
  { HomerInput in = in0(sph); for (int i = 0; i < 30; i++) frame(&H, &in);
    float hx = H.x, hz = H.z, gy9 = car.y;
    carInit(&car, NULL, &world, hx, gy9, hz, sph); CarBox b9; carBox(&car, &b9);
    float fx9 = b9.fx, fz9 = b9.fz;
    carInit(&car, NULL, &world, hx - fx9 * 14.f, gy9, hz - fz9 * 14.f, sph);
    CarInput ci9 = {1, 0, 0, 0}; int hurtSeen = 0, hits0 = H.carHits; float vyMax = 0.f, vHit = 0.f, t9 = 0.f, tBack = -1.f;
    for (int i = 0; i < 30 * 8; i++, t9 += DT) {
      carUpdate(&car, &world, &ci9, DT); HomerCar h9; homerCarFrom(&h9, &car, .62f); h9.hostile = 1;   // сверка 5.7 (исходник): пешего сбивают только машины ИИ миссий — тестовая машина «миссионная»
      if (H.state == HS_HURT && !hurtSeen) { hurtSeen = 1; vHit = fabsf(h9.v); }
      if (H.state == HS_HURT && H.vy > vyMax) vyMax = H.vy;
      homerUpdate(&H, &world, &h9, &in, DT); homerPose(&H);
      if (hurtSeen) { ci9.throttle = 0; ci9.brake = 1; }   // водитель сбросил газ после удара
      if (hurtSeen && H.state == HS_FOOT && tBack < 0) tBack = t9;
    }
    check("709: car hit on foot: Homer is hurt (falls)", (float)(H.carHits - hits0), 1, 1, "carHits++");
    check("709: car hit: car speed at the hit, m/s", vHit, 3.f, 60.f, "faster than 3 m/s");
    check("709: car hit: thrown up, vy m/s", vyMax, 2.5f, 6.1f, "2.5 + 0.25 v, max 6");
    check("709: car hit: gets up again (back on foot), s", tBack, .1f, 7.f, "flail, land, get_up");
    homerPlace(&H, &world, hx, gy9 + 1.f, hz, sph); for (int i = 0; i < 30; i++) frame(&H, &in); }
  // ---------------- 2. jump / double jump
  { HomerInput in = in0(sph); for (int i = 0; i < 60; i++) frame(&H, &in);
    float y0 = H.y; H.maxY = H.y; in.jump = 1; frame(&H, &in); in.jump = 0; float t = DT;
    for (int w_ = 0; H.grounded && w_ < 60; w_++) frame(&H, &in);   // прыжок с места: сначала заводка (P02-7), отрыв позже — ждём отрыва
    t = DT;
    while (!H.grounded && t < 3) { frame(&H, &in); t += DT; }
    check("jump: apex above ground, m", H.maxY - y0, 1.8f, 1.95f, "1.9");
    check("jump: air time, s", t, .72f, .85f, "2*sqrt(2h/g) = 0.78");
    for (int i = 0; i < 30; i++) frame(&H, &in);
    y0 = H.y; H.maxY = H.y; in.jump = 1; frame(&H, &in); in.jump = 0;
    for (int w_ = 0; H.grounded && w_ < 60; w_++) frame(&H, &in);   // прыжок с места: сначала заводка (P02-7), отрыв позже — ждём отрыва
    while (H.vy > 2.3f) frame(&H, &in);                             // второй ✕ как в записи PS2 (через 0,32 с, вертикаль ≈ +2,1…2,5; прежнее окно 1,5)
    in.jump = 1; frame(&H, &in); in.jump = 0;
    check("double jump: jumps", (float)H.jumps, 2, 2, "2");
    while (!H.grounded) frame(&H, &in);
#if HOMER_JUMP_PS2_OLD
    check("double jump: apex, m", H.maxY - y0, 2.75f, 2.95f, "1.9 + 1.0 - (vy 1.5..2 at press) ~2.85");
#else   // ДОБИВКА 25: эталон — запись PS2 ps2_homer_double_jump (корень 0xa245b0), окно как в tests/homer_jump (уже прежнего)
    check("double jump: apex, m", H.maxY - y0, 2.669f, 2.754f, "PS2 record: sampled 2.7062, parabola 2.7121");
#endif
    for (int i = 0; i < 40; i++) frame(&H, &in); }
  // ---------------- 3. kick / stomp events
  { int hits = 0, stomps = 0; float tHit = -1, t = 0; HomerInput in = in0(sph);
    in.attack = 1; frame(&H, &in); in.attack = 0; drainEvents(&H, &hits, &stomps, t, &tHit);
    for (int i = 0; i < 60; i++) { t += DT; frame(&H, &in); drainEvents(&H, &hits, &stomps, t, &tHit); }
    check("kick: HIT events", (float)hits, 1, 1, "1 per kick");
    check("kick: HIT time after press, s", tHit + DT, .25f, .3f, "frame 8 of hom_break = 0.27");
    in.jump = 1; frame(&H, &in); in.jump = 0; for (int w_ = 0; H.grounded && w_ < 60; w_++) frame(&H, &in);   // прыжок с места: сначала заводка (P02-7), отрыв позже — ждём отрыва
    for (int i = 0; i < 12; i++) frame(&H, &in);
    in.attack = 1; frame(&H, &in); in.attack = 0; for (int i = 0; i < 10; i++) frame(&H, &in);   // X + □: удар в прыжке, не стомп
    drainEvents(&H, &hits, &stomps, 0, NULL);
    check("jump + attack: jump kick (no stomp)", (float)(H.state == HS_STOMP), 0, 0, "hom_jump_kick");
    while (!H.grounded) frame(&H, &in); for (int i = 0; i < 30; i++) frame(&H, &in);
    in.jump = 1; frame(&H, &in); in.jump = 0; for (int w_ = 0; H.grounded && w_ < 60; w_++) frame(&H, &in);   // прыжок с места: сначала заводка (P02-7), отрыв позже — ждём отрыва
    while (H.vy > 1.5f) frame(&H, &in);   // X X □ — стомп (второй прыжок, потом удар)
    in.jump = 1; frame(&H, &in); in.jump = 0; frame(&H, &in);
    in.attack = 1; frame(&H, &in); in.attack = 0; t = 0; hits = stomps = 0;
    check("double jump + attack: STOMP state", (float)(H.state == HS_STOMP), 1, 1, "X X [] = stomp");
    while (H.state == HS_STOMP && t < 4) { frame(&H, &in); t += DT; drainEvents(&H, &hits, &stomps, t, NULL); }
    check("stomp: STOMP events", (float)stomps, 1, 1, "1 on landing");
    check("stomp: whole move, s", t, .8f, 1.4f, "antic .27 + fall + land .43");
    for (int i = 0; i < 30; i++) frame(&H, &in); drainEvents(&H, &hits, &stomps, 0, NULL); }
  // ---------------- 4. collision sweep: 16 directions x 5 s at full run from 4 spots around the spawn
  { float worst = 0, maxStep = 0, maxDrop = 0; int stuckFree = 0, falls = 0, runs = 0;
    float spots[4][2] = {{bx, bz}, {car.x + fx * 12, car.z + fz * 12}, {car.x - fx * 12, car.z - fz * 12}, {car.x + rx * 8, car.z + rz * 8}};
    for (int s = 0; s < 4; s++) for (int d = 0; d < 16; d++) {
      homerPlace(&H, &world, spots[s][0], car.y + 2.f, spots[s][1], 0); runs++;
      HomerInput in = in0(d * 6.2831853f / 16.f); in.my = 1.f; in.dash = 1; float py = H.y, dist = 0, px = H.x, pz = H.z;
      for (int i = 0; i < 300; i++) {
        frame(&H, &in);
        if (H.grounded) { float dy = H.y - py; if (dy > maxStep) maxStep = dy; if (-dy > maxDrop) maxDrop = -dy; }
        py = H.y; dist += hypotf(H.x - px, H.z - pz); px = H.x; pz = H.z;
        CarBox b; memset(&b, 0, sizeof b); b.x = H.x; b.z = H.z; b.fz = 1; b.hw = b.hl = HOMER_RADIUS * .8f; b.y = H.y; b.lo = HOMER_STEP + .05f; b.hi = HOMER_HEIGHT;
        colProbeCar(&world, &b); if (b.maxDepth > worst) worst = b.maxDepth;
      }
      if (H.y < spy - 20.f) falls++;
      if (dist > 1.f) stuckFree++;
    }
    check("sweep: worst wall penetration, m", worst, 0, .06f, "~0 (radius 0.3 pushed out)");
    check("sweep: biggest step up in one frame, m", maxStep, 0, HOMER_STEP + .01f, "<= step 0.4");
    check("sweep: runs that fell out of the world", (float)falls, 0, 0, "0");
    printf("     sweep: %d runs, %d moved > 1 m, largest snap down %.2f m\n", runs, stuckFree, maxDrop); }
  // ---------------- 5. car: enter from the passenger side (walks around), sit, exit
  { homerPlace(&H, &world, car.x + rx * ENT_W, car.y + 1.f, car.z + rz * ENT_W, sph);  // passenger side
    HomerCar hc; homerCarFrom(&hc, &car, 0.62f);
    HomerCarCb cb = {onDoor, onDone, 0};
    printf("     can enter from 3 m (passenger side): %d\n", homerCanEnter(&H, &hc));
    int ok = homerEnterCar(&H, &hc, &cb); float t = 0, tSeq = -1;
    while (ok && H.state != HS_IN_CAR && t < 8) { HomerInput in = in0(sph); frame(&H, &in); t += DT; if (tSeq < 0 && H.state == HS_GET_IN) tSeq = t;
      if (getenv("HDEBUG") && H.state == HS_TO_DOOR && ((int)(t * 60) % 10 == 0)) { float dx = H.x - car.x, dz = H.z - car.z;
        HomerCar q; homerCarFrom(&q, &car, .62f); float ax = q.m[0] * H.anchor[0] + q.m[4] * H.anchor[1] + q.m[8] * H.anchor[2] + q.m[12] - car.x,
          az = q.m[2] * H.anchor[0] + q.m[6] * H.anchor[1] + q.m[10] * H.anchor[2] + q.m[14] - car.z;
        printf("  todoor t %.2f along %.2f side %.2f v %.2f h %.2f  anchor along %.2f side %.2f  carh %.2f box f %.2f %.2f\n", t, dx * fx + dz * fz, dx * rx + dz * rz, H.speed, H.h,
               ax * fx + az * fz, ax * rx + az * rz, car.h, q.box.fx, q.box.fz); } }
    check("enter: reached IN_CAR", (float)(H.state == HS_IN_CAR && doneIn == 1), 1, 1, "done(1)");
    check("enter: walk around to the door, s", tSeq, .5f, 4.f, "auto-run");
    check("enter: door sequence, s", t - tSeq, .95f, 1.05f, "open .43 + in .57 = 1.0: управление и IN_CAR по концу get_into_car (P02-27/P05-14), закрытие 0,2 с идёт уже за рулём (HOMER_CTRL_EARLY_OLD=1: 1.37)");
    for (int i = 0; i < 30; i++) { HomerInput in = in0(sph); frame(&H, &in); }   // дверь закрывается уже за рулём
    check("enter: door opened fully, then closed", doorMax - doorOpen, .99f, 1.01f, "1 then 0 (closes in the car)");
    // exit
    for (int i = 0; i < 30; i++) { HomerInput in = in0(sph); frame(&H, &in); }
    homerCarFrom(&hc, &car, 0.62f); doorMax = 0;
    ok = homerExitCar(&H, &hc, &cb); t = 0;
    while (ok && H.state != HS_FOOT && t < 5) { HomerInput in = in0(sph); frame(&H, &in); t += DT; }
    check("exit: back on foot", (float)(H.state == HS_FOOT && doneOut == 1), 1, 1, "done(0)");
    check("exit: sequence, s", t, 1.05f, 1.2f, "open .37 + out .40 + close .37 = 1.13");
    float dx = H.x - car.x, dz = H.z - car.z, side = dx * rx + dz * rz, along = dx * fx + dz * fz;
    check("exit: stands left of the car (driver side), m", -side, 1.1f, 2.2f, "outside the body (half width 1.1)");
    printf("     exit pose: along %.2f m, heading diff %.1f deg, door max %.2f\n", along,
           remainderf(H.h - car.h, 6.2831853f) * 57.2958f, doorMax);
    // камера пешком у машины (Роб 28.09 «подхожу к машине — вижу внутренности»): курс камеры в сторону машины и вдоль
    // борта назад; глаз не заходит в кузов ближе HOMER_CAR_CLEAR, ближняя плоскость не режет ни кузов, ни Гомера
    { float yaws[3] = { atan2f(H.x - car.x, H.z - car.z), car.h + 3.1415927f, car.h };   // спиной к машине; вдоль борта
      const char *nm[3] = { "camera: back to the car", "camera: along the side, rear", "camera: along the side, front" };
      for (int k = 0; k < 3; k++) {
        H.camYaw = yaws[k]; H.camInit = 1; H.camY = H.y; H.speed = 0.f;
        homerCarFrom(&H.camCar, &car, .62f);
        float e[3], l[3]; for (int q = 0; q < 60; q++) homerCamera(&H, &world, DT, e, l);   // 1 с: камера успевает отойти
        const CarBox *b = &H.camCar.box; float top = b->y + b->hi, bot = b->y + b->lo;
        #define CLR(px, py, pz) ({ float dx_ = (px) - b->x, dz_ = (pz) - b->z; \
          float u_ = fmaxf(fabsf(dx_ * b->fx + dz_ * b->fz) - b->hl, 0.f), v_ = fmaxf(fabsf(-dx_ * b->fz + dz_ * b->fx) - b->hw, 0.f); \
          float y_ = (py), dy_ = y_ > top ? y_ - top : (y_ < bot ? bot - y_ : 0.f); sqrtf(u_ * u_ + v_ * v_ + dy_ * dy_); })
        float clr = CLR(e[0], e[1], e[2]), los = 1e9f;
        for (int q = 0; q <= 50; q++) { float t = q / 50.f; los = fminf(los, CLR(e[0] + (l[0] - e[0]) * t, e[1] + (l[1] - e[1]) * t, e[2] + (l[2] - e[2]) * t)); }
        float nearEff = H.camNear > 0.f ? fminf(1.5f, H.camNear) : 1.5f;   // как main.c (NEAR_Z 1.5)
        // правка 167 (близнец 02.10, v23_exit_cam): у оригинала машина МОЖЕТ стоять между камерой и Гомером (линия взгляда вдоль
        // борта, поверх крыши) — это не провал; проверяется только глаз вне кузова и ближняя плоскость
        char w[96]; printf("     %s: sight line to Homer clear of body %.2f m (info: the original allows the car in between)\n", nm[k], los);
        snprintf(w, sizeof w, "%s: eye clear of body, m", nm[k]); check(w, clr, HOMER_CAR_CLEAR - .01f, 99.f, "eye not in the body");
        snprintf(w, sizeof w, "%s: near plane - eye to body", nm[k]); check(w, nearEff - clr, -99.f, 0.f, "body not cut by near plane");
        float th = sqrtf((e[0] - H.x) * (e[0] - H.x) + (e[2] - H.z) * (e[2] - H.z) + (e[1] - H.y - 1.2f) * (e[1] - H.y - 1.2f)) - .45f;
        snprintf(w, sizeof w, "%s: near plane - eye to Homer", nm[k]); check(w, nearEff - th, -99.f, 0.f, "Homer not clipped");
        #undef CLR
      }
      H.camInit = 0; } }
  // ---------------- 5б. выход, когда у двери водителя занято (Роб 28.09: «прижат — повалился поверх машины», как в оригинале):
  //      (а) подложенное твёрдое тело слева от машины; (б) настоящая стена COL1 — машину сдвигаем влево до касания
  { HomerCar hc; HomerCarCb cb = {onDoor, onDone, 0};
    HomerSolidFn s0 = H.solid; void *u0 = H.solidUser; float cx0 = car.x, cz0 = car.z;
    for (int kase = 0; kase < 2; kase++) {
      const char *nm = kase ? "blocked exit (COL1 wall)" : "blocked exit (solid)";
      if (kase == 0) { H.solid = testWall; H.solidUser = &car; }
      else {   // стена COL1: вокруг спавна (сетка 10 м, 16 курсов) ищем место, где кузов упирается левым бортом в стену ростом
               // с Гомера (столб 0.35..1.8 м над полом в <= 0.45 м от борта у двери водителя) — туда и ставим машину
        H.solid = s0; H.solidUser = u0; int found = 0; float D = 99.f;
        for (int g = 0; g < 81 && !found; g++) for (int k = 0; k < 16 && !found; k++) {
          float ox = spx + ((g % 9) - 4) * 10.f, oz = spz + ((g / 9) - 4) * 10.f, h = k * 6.2831853f / 16.f, rx = -cosf(h), rz = sinf(h);
          float gy; if (!colGround(&world, ox, oz, spy + 20.f, spy - 20.f, &gy, NULL)) continue;
          carInit(&car, NULL, &world, ox, gy + .5f, oz, h);
          CarBox b0; carBox(&car, &b0); if (colProbeCar(&world, &b0) && b0.maxDepth > .01f) continue;   // старт уже в стене
          float d = 0.f; int touch = 0;
          for (; d < 8.f; d += .05f) { car.x = ox - rx * d; car.z = oz - rz * d; CarBox b; carBox(&car, &b);
            if (colProbeCar(&world, &b) && b.maxDepth > .01f) { touch = 1; break; } }
          if (!touch) continue;
          d -= .1f; carInit(&car, NULL, &world, ox - rx * d, gy + .5f, oz - rz * d, h); for (int i = 0; i < 30; i++) stepCar(DT);
          rx = -cosf(car.h); rz = sinf(car.h); D = 99.f;
          for (float dd = 0.f; dd < 1.f; dd += .02f) { CarBox b; memset(&b, 0, sizeof b); b.x = car.x - rx * (1.1f + dd); b.z = car.z - rz * (1.1f + dd);
            b.fx = 0.f; b.fz = 1.f; b.hw = b.hl = .05f; b.y = car.y; b.lo = .35f; b.hi = 1.8f;
            if (colProbeCar(&world, &b) && b.maxDepth > .005f) { D = dd + .05f; break; } }
          found = D <= .45f;
        }
        if (!found) { printf("     %s: no place near the spawn to park against a wall, skipped\n", nm); fails++; continue; }
        printf("     %s: car at %.1f %.1f heading %.0f deg, wall %.2f m from the driver door\n", nm, car.x, car.z, car.h * 57.2958f, D);
      }
      homerCarFrom(&hc, &car, .62f); homerSetInCar(&H, &hc); doorMax = 0; doneOut = -1;
      int ok = homerExitCar(&H, &hc, &cb); float t = 0.f, y03 = -1e9f, roof = hc.roof;
      while (ok && t < 3.f) { HomerInput in = in0(car.h); frame(&H, &in); t += DT; if (t < .3f) y03 = fmaxf(y03, H.y); }
      char w[96];
      snprintf(w, sizeof w, "%s: on foot (done(0))", nm); check(w, (float)(H.state == HS_FOOT && doneOut == 1), 1, 1, "out of the car");
      snprintf(w, sizeof w, "%s: driver door stays shut", nm); check(w, doorMax, 0.f, .01f, "no room to open it");
      snprintf(w, sizeof w, "%s: over the top, height - roof, m", nm); check(w, y03 - roof, -.05f, 2.f, "starts on the roof");
      float fx = sinf(car.h), fz = cosf(car.h), rx = -fz, rz = fx, side = (H.x - car.x) * rx + (H.z - car.z) * rz;
      int inWall = kase == 0 && side < wallSide0 && side > wallSide1;
      snprintf(w, sizeof w, "%s: not inside the obstacle", nm); check(w, (float)!inWall, 1, 1, "outside the wall");
      printf("     %s: after 3 s at side %.2f m, y - roof %.2f, overTop %d\n", nm, side, H.y - roof, H.overTop);
    }
    H.solid = s0; H.solidUser = u0; (void)cx0; (void)cz0;
    carInit(&car, NULL, &world, spx, spy, spz, sph); for (int i = 0; i < 60; i++) stepCar(DT);
    homerCarFrom(&hc, &car, .62f); homerSetInCar(&H, &hc); doorMax = 0; int ok = homerExitCar(&H, &hc, &cb); float t = 0.f;
    while (ok && H.state != HS_FOOT && t < 5) { HomerInput in = in0(car.h); frame(&H, &in); t += DT; }   // обратно: обычный выход
    check("free exit after the blocked ones: door opened", doorMax, .99f, 1.01f, "normal driver-side exit");
  }
  // ---------------- 5в. переходы (пакет «переходы как в оригинале»):
  //      (а) взрыв машины выбрасывает Гомера (Роб 28.09): тот же вызов, что game.c (homerEject от центра машины); через 3 с —
  //          пешком, вне кузова, был в полёте; в остов сесть нельзя. HOST_NOEJECT=1 — отрицательный контроль (без выброса)
  //      (б) посадка при занятой двери с ирисом (irisOn): сразу на сиденье, дверь не открывалась. HOST_NOIRIS=1 — контроль
  { HomerCar hc; HomerCarCb cb = {onDoor, onDone, 0};
    carInit(&car, NULL, &world, spx, spy, spz, sph); for (int i = 0; i < 60; i++) stepCar(DT);
    homerCarFrom(&hc, &car, .62f); homerSetInCar(&H, &hc); H.cb = cb; doneOut = -1;
    for (int i = 0; i < 4; i++) { HomerInput in4 = in0(car.h); homerUpdate(&H, &world, &hc, &in4, DT); homerPose(&H); }   // 04.11: в игре водитель каждый кадр на месте сиденья; без кадров Гомер оставался там, где его оставил прошлый тест (41 м от машины)
    car.damage = 1.f; car.destroyed = 1; homerCarFrom(&hc, &car, .62f);
    float y0 = H.y, maxY = -1e9f, t = 0.f; int ej = 0;
    if (!getenv("HOST_NOEJECT")) ej = homerEject(&H, &world, &hc, car.x, car.z, HOMER_EJECT_POWER);
    // правка Роба 147 (выброс как в оригинале): трасса по кадрам — голова и самый низкий сустав в мире (H.place * H.world)
    int jHead = skinFindJoint(H.m, "Head"), jExtra = skinFindJoint(H.m, "Extra_Attributes"); float gX = 0, gZ = 0, gT = -1.f, hurtT = -1.f, footT = -1.f, dotV = -2.f;
    float upEarly = 0.f, flyHead = -1.f, minBody = 9.f, guHead = 9.f, guLow = 9.f, guHead1 = -1.f, guDist = -1.f, hx0 = 0, hz0 = 0;
    int inCar05 = 0, flyN = 0; float prevHy = -1.f, headJump = 0.f, ftx = 0.f, ftz = 0.f;   // ftx, ftz — где встал на ноги
    while (t < 4.f) {
      HomerInput in = in0(car.h); frame(&H, &in); t += DT; maxY = fmaxf(maxY, H.y);
      const ScePspFMatrix4 *P = &H.place; float lo = 1e9f, hy = 0.f;
      for (unsigned j = 0; j < H.m->h->nJoints; j++) {
        if (j == H.m->h->rootJoint || (int)j == jExtra) continue;       // Motion_Root, Extra_Attributes — не части тела
        const ScePspFVector4 *q = &H.world[j].w;
        float y = P->x.y * q->x + P->y.y * q->y + P->z.y * q->z + P->w.y;
        if (y < lo) lo = y; if ((int)j == jHead) hy = y;
      }
      if (t >= .5f - DT * .5f && t < .5f + DT * .5f) inCar05 = H.state == HS_IN_CAR;
      if (H.state == HS_IN_CAR) continue;
      if (hurtT < 0.f) { hurtT = t; hx0 = H.x; hz0 = H.z; float v = hypotf(H.vx, H.vz);   // первый кадр вне машины
        if (v > 1e-3f) dotV = (sinf(H.h) * H.vx + cosf(H.h) * H.vz) / v; }
      if (t < .9f && t - hurtT > .15f && hy - lo > .9f) upEarly = 1.f;   // вертикален вне машины раньше 0.9 с (после наклона 0.13 с)
      if (H.state == HS_HURT) {
        if (!H.grounded && t - hurtT >= .15f) { flyHead = fmaxf(flyHead, hy - lo); flyN++; }
        minBody = fminf(minBody, lo - H.groundY);
        if (H.clip == H.cGetUp && prevHy >= 0.f) headJump = fmaxf(headJump, fabsf(hy - H.groundY - prevHy));   // флейл -> get_up: голова не скачет
        prevHy = H.grounded ? hy - H.groundY : -1.f;
        if (H.clip == H.cGetUp && gT < 0.f) { gT = t; gX = H.x; gZ = H.z; guDist = hypotf(gX - hx0, gZ - hz0); }
        if (gT >= 0.f && H.pitch == 0.f && guHead > 8.f) { guHead = hy - H.groundY; guLow = lo - H.groundY; }   // переход в get_up закончен
        if (gT >= 0.f && t - gT >= .9f - DT * .5f && guHead1 < 0.f) guHead1 = hy - H.groundY;
      } else if (H.state == HS_FOOT && footT < 0.f && hurtT >= 0.f) { footT = t; ftx = H.x; ftz = H.z; }
      if (footT >= 0.f) break;
    }
    if (flyN == 0) flyHead = 9.f;
#if HOMER_DODGE_OLD   // прежний выброс (полёт, флейл, get_up по записи PS2 147): после сверки круга 5 вместо него уворот dodge — тогда эти проверки не применимы
    check("147: still in the burning car at 0.5 s", (float)inCar05, 1, 1, "PS2 33:33.43 hit -> 33:34.23 out: ~0.7 s delay");
    check("147: not upright out of the car before 0.9 s", upEarly, 0, 0, "flies, lies, then gets up");
    check("147: heading along the flight (dot)", dotV, .7f, 1.01f, "head first, not backwards");
    check("147: in flight head over lowest joint, m", flyHead, -.3f, .6f, "body horizontal (upright ~1.1)");
    check("147: body never below ground, m", minBody, -.15f, 2.f, "tilt pivot keeps it out of the ground");
    check("147: get_up start (tilt gone) head over ground, m", guHead, 0.f, .45f, "lying prone (disc: 0.22)");
    check("147: flail -> get_up head jump per frame, m", headJump, 0.f, .1f, "tilt pivot: no pop between flail and get_up");
    check("147: get_up start lowest joint over ground, m", guLow, -.15f, .3f, "not hanging in the air");
    check("147: 0.9 s into get_up head over ground, m", guHead1, 1.1f, 1.5f, "stands (disc frames 24-30: 1.23-1.27)");
    check("147: flight + slide from the door, m", guDist, 2.5f, 5.5f, "PS2 record ~3-5 m");
    check("147: get_up played whole, s", gT >= 0.f && footT >= 0.f ? footT - gT : 0.f, .95f, 1.2f, "hom_get_up 31 frames = 1.03 s");
#else   // 04.11: уворот dodge (оригинал, круг 5): водитель выходит через ~0,5 с клипом уворота, скользит от машины и встаёт; полёта и флейла нет
    check("dodge: still in the burning car at 0.5 s", (float)inCar05, 0, 1, "out at 0.5 s (dodge clip starts, tests/g04 circle 5)");
    check("dodge: slide from the car at the moment he is on foot, m", hypotf(ftx - hc.box.x, ftz - hc.box.z), 2.5f, 9.f, "tests/g04 circle 5: 3..9 m from the car centre");
    check("dodge: on foot after, s", footT, .8f, 3.f, "dodge clip ~1.4 s");
#endif
    { float s0 = H.speed; for (int i = 0; i < 30; i++) { HomerInput in = in0(car.h); in.my = 1.f; frame(&H, &in); }
      check("147: control back after get_up, m/s", H.speed - s0, 1.f, 20.f, "stick moves him"); }
    printf("     147: out at %.2f s, get_up at %.2f s (head %.2f, low %.2f), on foot at %.2f s, %.2f m from the door\n",
           hurtT, gT, guHead, guLow, footT, guDist);
    const CarBox *b = &hc.box; float ex = H.x - b->x, ez = H.z - b->z;
    float ua = fmaxf(fabsf(ex * b->fx + ez * b->fz) - b->hl, 0.f), us = fmaxf(fabsf(-ex * b->fz + ez * b->fx) - b->hw, 0.f);
    { float ex2 = ftx - b->x, ez2 = ftz - b->z; ua = fmaxf(fabsf(ex2 * b->fx + ez2 * b->fz) - b->hl, 0.f); us = fmaxf(fabsf(-ex2 * b->fz + ez2 * b->fx) - b->hw, 0.f); }   // где встал на ноги, а не после последующей ходьбы
    float clear = sqrtf(ua * ua + us * us);
    check("explosion: ejected (homerEject)", (float)ej, 1, 1, "Homer was in the car");
    check("explosion: on foot after 3 s", (float)(H.state == HS_FOOT), 1, 1, "flail -> get up -> foot");
    check("explosion: outside the husk, m", clear, .3f, 12.f, "not inside the body box");
#if HOMER_DODGE_OLD
    check("explosion: flew up, m", maxY - y0, .5f, 4.f, "a flight, not a teleport");
#endif
    homerCarFrom(&hc, &car, .62f);
    check("explosion: cannot enter the husk", (float)homerCanEnter(&H, &hc), 0, 0, "hc.dead");
    printf("     explosion: landed %.2f m from the car centre, clear of the body %.2f m, apex +%.2f m\n",
           hypotf(H.x - car.x, H.z - car.z), clear, maxY - y0);
    car.damage = 0.f; car.destroyed = 0;
    // (б) занятая дверь: подложенное твёрдое тело слева (как 5б-а), Гомер справа от машины
    HomerSolidFn s0 = H.solid; void *u0 = H.solidUser; H.solid = testWall; H.solidUser = &car;
    float fx = sinf(car.h), fz = cosf(car.h), rx = -fz, rz = fx;
    homerPlace(&H, &world, car.x + rx * ENT_W, car.y + 1.f, car.z + rz * ENT_W, car.h);
    H.irisOn = getenv("HOST_NOIRIS") ? 0 : 1; int seat0 = H.irisSeat; doorMax = 0; doneIn = -1;
    homerCarFrom(&hc, &car, .62f); int ok = homerEnterCar(&H, &hc, &cb); t = 0.f;
    while (ok && H.state != HS_IN_CAR && t < 8.f) { HomerInput in = in0(car.h); frame(&H, &in); t += DT; }
    check("blocked door, iris: seated (done(1))", (float)(H.state == HS_IN_CAR && doneIn == 1), 1, 1, "in the car");
    check("blocked door, iris: seated under the iris", (float)(H.irisSeat - seat0), 1, 1, "irisSeat++");
    check("blocked door, iris: door never opened", doorMax, 0.f, .01f, "no room to open it");
    check("blocked door, iris: at once, s", t, 0.f, .1f, "first frame of TO_DOOR");
    H.irisOn = 0; H.solid = s0; H.solidUser = u0;
    // (в) правка Роба 212: обход машины к двери водителя долгий — с ирисом сразу на сиденье (путь длиннее HOMER_IRIS_PATH);
    //     со стороны водителя в 2 м от двери — пешком, дверь анимацией (красный контроль: -DHOMER_IRIS_PATH=0)
    homerPlace(&H, &world, car.x + rx * ENT_W, car.y + 1.f, car.z + rz * ENT_W, car.h);   // справа (сторона пассажира), дверь свободна
    H.irisOn = 1; seat0 = H.irisSeat; doorMax = 0; doneIn = -1;
    homerCarFrom(&hc, &car, .62f); ok = homerEnterCar(&H, &hc, &cb); t = 0.f;
    while (ok && H.state != HS_IN_CAR && t < 8.f) { HomerInput in = in0(car.h); frame(&H, &in); t += DT; }
    check("walk around, iris: seated under the iris", (float)(H.irisSeat - seat0), 1, 1, "правка 212");
    check("walk around, iris: at once, s", t, 0.f, .1f, "first frame of TO_DOOR");
    { float dp[3]; homerCarFrom(&hc, &car, .62f);
      dp[0] = hc.m[0] * hc.door[0] + hc.m[8] * hc.door[1] + hc.m[12]; dp[2] = hc.m[2] * hc.door[0] + hc.m[10] * hc.door[1] + hc.m[14];
      homerPlace(&H, &world, dp[0] - rx * .2f, car.y + 1.f, dp[2] - rz * .2f, car.h);   // слева, 2 м от двери водителя
      seat0 = H.irisSeat; doorMax = 0; doneIn = -1; ok = homerEnterCar(&H, &hc, &cb); t = 0.f;
      float eyeP = 0.f, eyeStep = 0.f, pe[3] = {0}; int ne = 0;   // правка 223: путь глаза камеры пешком на посадке (HS_GET_IN)
      while (ok && H.state != HS_IN_CAR && t < 8.f) { HomerInput in = in0(car.h); frame(&H, &in); t += DT;
        float e3[3], l3[3]; homerCamera(&H, &world, DT, e3, l3);
        if (H.state == HS_GET_IN) { if (ne++) { float d = sqrtf((e3[0] - pe[0]) * (e3[0] - pe[0]) + (e3[2] - pe[2]) * (e3[2] - pe[2])); eyeP += d; if (d > eyeStep) eyeStep = d; }
                                    memcpy(pe, e3, sizeof pe); } }
      check("driver side 2 m, iris on: walks, no iris", (float)(H.irisSeat - seat0), 0, 0, "door animation");
      check("driver side 2 m: door opened", doorMax, .99f, 1.01f, "open, in, close");
      check("get in: camera eye path while seating, m", eyeP, 0.f, .4f, "правка 223: holds (was rigid on the body)");
      check("get in: camera biggest step per frame, m", eyeStep, 0.f, .03f, "no jerks"); }
    // (в2) №667: у машины без шарнира двери (угнанная, машина миссии, замок сорван; homerDoorCan() == 0) посадка со стороны водителя в 2 м —
    //      сразу на сиденье под ирисом, дверь не открывается. Красный контроль: HOST_ENTERIRIS_OLD=1 (хозяин кадра не отвечает) — как прежде, клипами
    { homerDoorCan = getenv("HOST_ENTERIRIS_OLD") ? NULL : hostNoDoor;
      float dp[3]; homerCarFrom(&hc, &car, .62f);
      dp[0] = hc.m[0] * hc.door[0] + hc.m[8] * hc.door[1] + hc.m[12]; dp[2] = hc.m[2] * hc.door[0] + hc.m[10] * hc.door[1] + hc.m[14];
      homerPlace(&H, &world, dp[0] - rx * .2f, car.y + 1.f, dp[2] - rz * .2f, car.h);
      H.irisOn = 1; seat0 = H.irisSeat; int c0 = H.irisCarIn; doorMax = 0; doneIn = -1; ok = homerEnterCar(&H, &hc, &cb); t = 0.f;
      while (ok && H.state != HS_IN_CAR && t < 8.f) { HomerInput in = in0(car.h); frame(&H, &in); t += DT; }
      check("667: no door hinge, iris: seated under the iris", (float)(H.irisCarIn - c0), 1, 1, "irisCarIn++");
      check("667: no door hinge, iris: door never opened", doorMax, 0.f, .01f, "nothing to open");
      check("667: no door hinge, iris: at once, s", t, 0.f, .1f, "first frame of TO_DOOR");
      // №666: высота посадки по сиденью машины: высокая машина (seatY 1,04 м, как marge_v) поднимает таз на разницу к famil_v (0,499 м); без данных
      //       сиденья (seatData 0) и у SEAT_REF — как прежде. Красный контроль: -DSEAT_OLD=1 (HOST_CFLAGS) — высота не меняется
      float yy[3], seat1 = car.p.seat[1]; int sd = car.p.seatData;   // frame() собирает HomerCar из car: сиденье задаём в нём
      for (int k = 0; k < 3; k++) {
        car.p.seatData = k != 2; homerCarFrom(&hc, &car, .62f); car.p.seat[1] = (k == 1 ? 0.499f : 1.04f) - hc.lift; homerCarFrom(&hc, &car, .62f);   // hc.lift: carModelLift меняет подъём
        homerPlace(&H, &world, dp[0] - rx * .2f, car.y + 1.f, dp[2] - rz * .2f, car.h);
        H.irisOn = 1; ok = homerEnterCar(&H, &hc, &cb); t = 0.f;
        while (ok && H.state != HS_IN_CAR && t < 8.f) { HomerInput in = in0(car.h); frame(&H, &in); t += DT; }
        { HomerInput in = in0(car.h); frame(&H, &in); }   // кадр сидения: y берётся из якоря
        yy[k] = H.y; }
      car.p.seat[1] = seat1; car.p.seatData = sd;
      check("666: tall car seat raises the driver, m", yy[0] - yy[1], .535f, .545f, "seatY 1,04 - 0,499");
      check("666: no seat data (fallback famil_v): height as at the reference, m", yy[2] - yy[1], -.01f, .01f, "seatData 0: ignore seatY");
      homerDoorCan = NULL; }
    H.irisOn = 0;
    // (г) правка Роба 333: машина с SetIrisTransition(1) в .con (Plow King) — с ирисом посадка и выход сразу, дверь не открывается
    //     (запись PS2 869,25 / 1085,20 с); у famil_v признака нет. HOST_IRISCAR_OLD=1 (homerIrisCarOld) — красный контроль
    { CarParams P; carParamsFor("plowk_v", &P); carInit(&car, &P, &world, spx, spy, spz, sph); for (int i = 0; i < 60; i++) stepCar(DT);
      homerIrisCarOld = getenv("HOST_IRISCAR_OLD") ? 1 : 0;
      homerCarFrom(&hc, &car, .62f); int famIris = carIrisFor("famil_v");
      check("333: plowk_v SetIrisTransition, famil_v not", (float)(hc.iris * 2 + famIris), 2, 2, "scripts/cars/*.con");
      float dp[3]; dp[0] = hc.m[0] * hc.door[0] + hc.m[8] * hc.door[1] + hc.m[12]; dp[2] = hc.m[2] * hc.door[0] + hc.m[10] * hc.door[1] + hc.m[14];
      homerPlace(&H, &world, dp[0] - rx * .2f, car.y + 1.f, dp[2] - rz * .2f, car.h);   // слева, 2 м от двери водителя (famil_v тут идёт клипами)
      H.irisOn = 1; int in0c = H.irisCarIn, out0c = H.irisCarOut, seat1 = H.irisSeat; doorMax = 0; doneIn = -1;
      int ok = homerEnterCar(&H, &hc, &cb); t = 0.f;
      while (ok && H.state != HS_IN_CAR && t < 8.f) { HomerInput in = in0(car.h); frame(&H, &in); t += DT; }
      check("333: plow king enter: under the iris (irisCarIn++)", (float)(H.irisCarIn - in0c), 1, 1, "car_in");
      check("333: plow king enter: at once, s", t, 0.f, .05f, "PS2: iris on triangle, no walk to the door");
      check("333: plow king enter: door never opened", doorMax, 0.f, .01f, "no door clips");
      check("333: plow king enter: seated, not the blocked-door seat", (float)(H.state == HS_IN_CAR && doneIn == 1 && H.irisSeat == seat1), 1, 1, "in the car");
      for (int i = 0; i < 30; i++) { HomerInput in = in0(car.h); frame(&H, &in); }
      homerCarFrom(&hc, &car, .62f); doorMax = 0; doneOut = -1; ok = homerExitCar(&H, &hc, &cb); t = 0.f;
      while (ok && H.state != HS_FOOT && t < 8.f) { HomerInput in = in0(car.h); frame(&H, &in); t += DT; }
      const CarBox *b = &hc.box; float ex = H.x - b->x, ez = H.z - b->z;
      float ua = fmaxf(fabsf(ex * b->fx + ez * b->fz) - b->hl, 0.f), us = fmaxf(fabsf(-ex * b->fz + ez * b->fx) - b->hw, 0.f);
      check("333: plow king exit: under the iris (irisCarOut++)", (float)(H.irisCarOut - out0c), 1, 1, "car_out");
      check("333: plow king exit: at once, s", t, 0.f, .05f, "PS2 1085.2: iris, Homer at the driver door");
      check("333: plow king exit: door never opened", doorMax, 0.f, .01f, "no door clips");
      check("333: plow king exit: outside the body, m", sqrtf(ua * ua + us * us), .05f, 2.f, "stands by the door");
      H.irisOn = 0; homerIrisCarOld = 0; }
    carInit(&car, NULL, &world, spx, spy, spz, sph); for (int i = 0; i < 60; i++) stepCar(DT);
  }
  // ---------------- 5в. правки Роба 220/231: за рулём — поза и жесты водителя ndr оригинала (homer.cho *_driver)
  { HomerCar hc; homerCarFrom(&hc, &car, .62f); homerSetInCar(&H, &hc);
    int idle = skinFindClip(H.m, "ndr_in_car_idle");
    check("driver: seated clip is ndr_in_car_idle (hands on the wheel)", (float)(H.cInCar == idle && idle >= 0 && H.clip == idle), 1, 1, "not hom_in_car_idle");
    int g0 = H.drvGestures, sawTurn = 0, sawCrash = 0, back = 0; float t = 0.f;
    for (int i = 0; i < 240; i++) {   // 4 с: руль вправо на 0,3 с, отпустить; на 2-й секунде удар
      float st = (i >= 10 && i < 28) ? 1.f : 0.f;
      homerDriveInput(&H, st, 1.f, 0.f, i < 130 ? 12.f : 8.f, 0);   // удар: скорость падает 12 → 8 м/с за кадр — замедление за порогом 25 м/с² (оригинал, P02-29: жест выбирается по замедлению, а не флагом; прежний порт: флаг crash, HOMER_GESTURE_OLD=1)
      HomerInput in = in0(car.h); frame(&H, &in); t += DT;
      if (H.clip == H.cDrvTurn[1]) sawTurn = 1;
      if (H.clip == H.cDrvCrash) sawCrash = 1;
      if (i > 100 && i < 125 && H.clip == H.cInCar) back = 1;
    }
    check("driver: steering right plays turn_right", (float)sawTurn, 1, 1, "ndr_turn_right");
    check("driver: back to the idle after the gesture", (float)back, 1, 1, "0.67 s");
    check("driver: a hit plays the crash gesture", (float)sawCrash, 1, 1, "ndr_crash");
    check("driver: gestures started", (float)(H.drvGestures - g0), 2, 3, "turn + crash (+ gas)");
    check("driver: stays seated (in the car)", (float)(H.state == HS_IN_CAR), 1, 1, "");
    homerPlace(&H, &world, spx, spy, spz, sph);
  }
  // ---------------- 6. Ф3 episode (audit §10.8): car parked next to a coin crate; Homer gets out, walks to the crate,
  //                    kicks it (stateprops HIT -> coins), collects the coins on foot, walks back, gets in and drives
  if (haveSP && haveCoins) {
    int best = -1; float bd = 1e9f;
    for (unsigned i = 0; i < sp.ninst; i++) {
      const SpType *T = &sp.type[sp.inst[i].type]; int coinsCall = 0;
      if (T->kind != 0) continue;
      for (unsigned s = 0; s < T->nState; s++) { const SpState *S = &sp.state[T->firstState + s];
        for (unsigned c = 0; c < S->nCall; c++) if (sp.call[S->firstCall + c].op == SP_OP_COINS && sp.call[S->firstCall + c].arg >= 10) coinsCall = 1; }
      if (!coinsCall) continue;
      float d = hypotf(sp.inst[i].x - spx, sp.inst[i].z - spz); if (d < bd) { bd = d; best = (int)i; }
    }
    if (best < 0) { printf("FAIL episode: no crate\n"); fails++; }
    else {
      const SpInst *ci = &sp.inst[best]; float c[3], r; spBounds(&sp, best, c, &r);
      // park the car 7 m from the crate where the ground is free (8 tries around it)
      int parked = 0;
      for (int k = 0; k < 16 && !parked; k++) {
        float a = k * 6.2831853f / 16.f, px = ci->x + sinf(a) * 7.f, pz = ci->z + cosf(a) * 7.f, g, n[3];
        if (!colGround(&world, px, pz, ci->y + 2.f, ci->y - 2.f, &g, n)) continue;
        carPlace(&car, &world, px, g, pz, a + 1.5708f);
        CarBox b; carBox(&car, &b); colProbeCar(&world, &b); if (b.maxDepth > .02f) continue;
        parked = 1;
      }
      for (int i = 0; i < 90; i++) stepCar(DT);
      printf("     episode: crate #%d at %.1f %.1f %.1f (%.0f m from spawn); car parked %s at %.1f %.1f\n", best, ci->x, ci->y, ci->z, bd,
             parked ? "ok" : "ANYWAY", car.x, car.z);
      { // crate is solid: run straight into it for 2 s from 4 m
        float ax = ci->x - car.x, az = ci->z - car.z, al = hypotf(ax, az); ax /= al; az /= al;
        homerPlace(&H, &world, ci->x - ax * 4.f, ci->y + 1.f, ci->z - az * 4.f, atan2f(ax, az));
        float dmin = 1e9f; HomerInput in = in0(0.f); in.mx = -sinf(atan2f(ax, az)); in.my = cosf(atan2f(ax, az));
        for (int i = 0; i < 120; i++) { frame(&H, &in); float d = hypotf(H.x - c[0], H.z - c[2]); if (d < dmin) dmin = d; }
        check("crate is solid: closest approach to its centre, m", dmin, .6f, 1.5f, "box half ~0.5 + radius 0.3");
      }
      HomerCar hc; homerCarFrom(&hc, &car, .62f); HomerCarCb cb = {onDoor, onDone, 0};
      homerSetInCar(&H, &hc); coins.dropAuto = 0;
      float t = 0, tOut = -1, tAtCrate = -1, tKicked = -1, tCollected = -1, tIn = -1; int before = coins.collected, hits = 0, stomps = 0;
      unsigned st0 = sp.run[best].state; int kicks = 0, kickWait = 0;
      doneOut = doneIn = -1; homerExitCar(&H, &hc, &cb);
      for (int i = 0; i < 60 * 40; i++, t += DT) {
        HomerInput in = in0(0.f); float tx = H.x, tz = H.z, want = 0.f;
        if (H.state == HS_FOOT && tOut < 0) tOut = t;
        if (tOut >= 0 && H.state == HS_FOOT) {
          float dc = hypotf(ci->x - H.x, ci->z - H.z);
          if (tAtCrate < 0) { tx = ci->x; tz = ci->z; want = dc > r + .45f ? 1.f : 0.f; if (!want && H.speed < .3f) tAtCrate = t; }
          else if (kicks < 3 && sp.run[best].alive && sp.run[best].colOn) {        // three kicks: s0 -> s1 -> s2 -> s3 (10 + 10 + 10)
            if (--kickWait <= 0) { float ex = ci->x - H.x, ez = ci->z - H.z; H.h = atan2f(ex, ez); in.attack = 1; kicks++; kickWait = 50; }
          } else if (tCollected < 0) {
            if (tKicked < 0) tKicked = t;
            float bestD = 1e9f; int any = 0;
            for (unsigned k = coins.nworld; k < coins.n; k++) if (coins.c[k].active && !coins.c[k].collecting) {
              any = 1; float d = hypotf(coins.c[k].x - H.x, coins.c[k].z - H.z); if (d < bestD) { bestD = d; tx = coins.c[k].x; tz = coins.c[k].z; } }
            if (!any && t - tKicked > 1.5f) tCollected = t; else want = fminf(1.f, bestD / 2.f);
          } else if (tIn < 0) {
            homerCarFrom(&hc, &car, .62f);
            if (homerCanEnter(&H, &hc)) homerEnterCar(&H, &hc, &cb);
            else { tx = car.x; tz = car.z; want = 1.f; }
          }
        }
        if (want > 0.f) { float d = atan2f(tx - H.x, tz - H.z); in.mx = -sinf(d) * want; in.my = cosf(d) * want; }
        frame(&H, &in); drainEvents(&H, &hits, &stomps, t, NULL);
        spUpdate(&sp, &world, NULL, &coins, DT);
        float p[3]; homerCoinPoint(&H, p); coinsUpdate(&coins, p[0], p[1], p[2], homerDriving(&H), DT);
        if (getenv("HDEBUG") && tKicked >= 0 && tCollected < 0 && i % 30 == 0) {
          int na = 0; for (unsigned k = coins.nworld; k < coins.n; k++) na += coins.c[k].active;
          printf("  t %.1f homer %.2f %.2f %.2f v %.1f -> %.2f %.2f  coins active %d collected %d\n", t, H.x, H.y, H.z, H.speed, tx, tz, na, coins.collected - before);
          for (unsigned k = coins.nworld; k < coins.n && i % 120 == 0; k++) if (coins.c[k].active) printf("     c %.2f %.2f %.2f air %d v %.1f %.1f %.1f\n", coins.c[k].x, coins.c[k].y, coins.c[k].z, coins.c[k].airborne, coins.c[k].vx, coins.c[k].vy, coins.c[k].vz);
        }
        if (H.state == HS_IN_CAR && tIn < 0 && tCollected >= 0) { tIn = t; break; }
      }
      printf("     episode timeline, s: out %.2f  at crate %.2f  kicked x%d %.2f  coins done %.2f  in car %.2f\n", tOut, tAtCrate, kicks, tKicked, tCollected, tIn);
      check("episode: crate state after 3 kicks (s3 = broken)", (float)sp.run[best].state, 3, 3, "HIT s0->s1->s2->s3");
      check("episode: coins collected on foot (of 30 dropped)", (float)(coins.collected - before), 1, 30, "coins.c scatters them 10-30 m, see report");
      check("episode: back in the car", (float)(H.state == HS_IN_CAR && doneIn == 1), 1, 1, "IN_CAR");
      CarInput ci2 = {1, 0, 0, 0}; float cx0 = car.x, cz0 = car.z;
      for (int i = 0; i < 120; i++) { carUpdate(&car, &world, &ci2, DT); HomerCar h2; homerCarFrom(&h2, &car, .62f); homerUpdate(&H, &world, &h2, NULL, DT); }
      check("episode: drove away, m", hypotf(car.x - cx0, car.z - cz0), 3.f, 100.f, "car moves with Homer inside");
      check("episode: Homer follows the car (seat), m", hypotf(H.x - car.x, H.z - car.z), 0.f, 1.2f, "in the driver seat");
    }
  }
  printf("%s: %d failed\n", fails ? "FAILED" : "PASSED", fails);
  return fails;
}
