// Правка 164 (Роб 01.10): «невидимая стена» в полёте с трамплина — повтор прыжка физикой порта на Маке.
// Тот же кадр, что viewer/main.c: carUpdate -> propsUpdate -> spUpdate, машина уровня из CAR1 (как в игре).
// Режим scan — веер прыжков из точки (сдвиг вбок, курс, скорость); на каждый удар о стену мира COL1 (car.wallV) —
// строка: где, на какой высоте, что за примитив, есть ли на картинке препятствие в слое кузова у точки удара (vis, как
// перепись стен tools/cartest/walls.c: горизонтальные лучи 0.4..1.4 м над полом кузова в сторону препятствия).
//   tests/airwalls/host.sh scan X Z HEADING_DEG [--y Y] [--lat a:b:s] [--dh a:b:s] [--kmh a:b:s] [--secs S] [-v]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "props.h"
#include "stateprops.h"
__attribute__((weak)) unsigned homerSolidFrame = 0;   // заглушка: props.c тянет его из homer.c, в хосте Гомера нет (как tests/jumps/twin.c); weak — не мешает ветке, где символ уже есть

static Props props; static StateProps sp; static ColWorld world; static Car car; static CarParams carP; static int carOk;
static unsigned char *file; static long fileSize;
static int onKick(void *u, unsigned i, float dx, float dz) {
  (void)u; (void)dx; (void)dz;
  int b = spBreakOf(&sp, props.def[i].type); if (b < 0) return 0;
  float pos[3] = {props.def[i].x, props.def[i].y, props.def[i].z};
  return spBreak(&sp, (unsigned)b, pos, props.def[i].q);
}

// ---------------------------------------------------------------- рисуемая геометрия (как tools/cartest/walls.c)
typedef struct { float bb[6]; int32_t tex; uint32_t flags, nv, ni, owner; float ox, oy, oz, sc; uint32_t off, nf, cell, pad[2]; } Rec;
typedef struct { float bb[6]; uint32_t first, n, owner, off, size, pad; } CellRec;
static float *T; static unsigned NT; static unsigned *gS, *gL; static int GW, GH; static float GOX, GOZ; static const float GC = 2.f;
static void loadRender(void) {
  uint32_t npc, tableOff, cellOff; memcpy(&npc, file + 8, 4); memcpy(&tableOff, file + 76, 4); memcpy(&cellOff, file + 84, 4);
  const Rec *recs = (const Rec *)(file + tableOff); const CellRec *cells = (const CellRec *)(file + cellOff);
  unsigned cap = 1 << 20; T = malloc(sizeof(float) * 9 * cap);
  for (unsigned i = 0; i < npc; i++) {
    const Rec *r = &recs[i]; if (r->owner != 0 || (r->flags & 4) || !(r->flags & 16)) continue;   // город, не небо, без индексов
    int vs = (r->flags & 32) ? 12 : 16; const uint8_t *b = file + cells[r->cell].off + r->off;
    for (unsigned k = 0; k + 2 < r->ni; k += 3) {
      if (NT == cap) { cap *= 2; T = realloc(T, sizeof(float) * 9 * cap); }
      for (int j = 0; j < 3; j++) {
        const int16_t *q = (const int16_t *)(b + (size_t)(k + j) * vs + (vs == 12 ? 6 : 8));
        T[9 * NT + 3 * j] = r->ox + q[0] * (r->sc / 32768.f); T[9 * NT + 3 * j + 1] = r->oy + q[1] * (r->sc / 32768.f);
        T[9 * NT + 3 * j + 2] = r->oz + q[2] * (r->sc / 32768.f);
      }
      NT++;
    }
  }
  GOX = world.ox; GOZ = world.oz; GW = (int)(world.W * world.cell / GC) + 1; GH = (int)(world.H * world.cell / GC) + 1;
  gS = calloc((size_t)GW * GH + 1, 4);
  for (int pass = 0; pass < 2; pass++) {
    for (unsigned t = 0; t < NT; t++) {
      const float *a = T + 9 * t;
      int cx0 = (int)floorf((fminf(a[0], fminf(a[3], a[6])) - GOX) / GC), cx1 = (int)floorf((fmaxf(a[0], fmaxf(a[3], a[6])) - GOX) / GC);
      int cz0 = (int)floorf((fminf(a[2], fminf(a[5], a[8])) - GOZ) / GC), cz1 = (int)floorf((fmaxf(a[2], fmaxf(a[5], a[8])) - GOZ) / GC);
      if (cx0 < 0) cx0 = 0; if (cz0 < 0) cz0 = 0; if (cx1 >= GW) cx1 = GW - 1; if (cz1 >= GH) cz1 = GH - 1;
      for (int z = cz0; z <= cz1; z++) for (int x = cx0; x <= cx1; x++) { size_t c = (size_t)z * GW + x; if (pass == 0) gS[c + 1]++; else gL[gS[c]++] = t; }
    }
    if (pass == 0) { for (size_t c = 0; c < (size_t)GW * GH; c++) gS[c + 1] += gS[c]; gL = malloc(sizeof(unsigned) * (gS[(size_t)GW * GH] + 1)); }
    else { for (size_t c = (size_t)GW * GH; c > 0; c--) gS[c] = gS[c - 1]; gS[0] = 0; }
  }
}
static float rayTri(const float *o, const float *d, const float *a) {
  float e1[3] = {a[3] - a[0], a[4] - a[1], a[5] - a[2]}, e2[3] = {a[6] - a[0], a[7] - a[1], a[8] - a[2]};
  float p[3] = {d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0]};
  float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2]; if (fabsf(det) < 1e-9f) return -1;
  float inv = 1.f / det, s[3] = {o[0] - a[0], o[1] - a[1], o[2] - a[2]};
  float u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv; if (u < 0 || u > 1) return -1;
  float q[3] = {s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0]};
  float v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv; if (v < 0 || u + v > 1) return -1;
  return (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
}
static float rayCast(const float *o, const float *d, float tmax) {
  float ex = o[0] + d[0] * tmax, ez = o[2] + d[2] * tmax, best = -1;
  int cx0 = (int)floorf((fminf(o[0], ex) - GOX) / GC), cx1 = (int)floorf((fmaxf(o[0], ex) - GOX) / GC);
  int cz0 = (int)floorf((fminf(o[2], ez) - GOZ) / GC), cz1 = (int)floorf((fmaxf(o[2], ez) - GOZ) / GC);
  if (cx0 < 0) cx0 = 0; if (cz0 < 0) cz0 = 0; if (cx1 >= GW) cx1 = GW - 1; if (cz1 >= GH) cz1 = GH - 1;
  for (int z = cz0; z <= cz1; z++) for (int x = cx0; x <= cx1; x++) {
    size_t c = (size_t)z * GW + x;
    for (unsigned k = gS[c]; k < gS[c + 1]; k++) { float t = rayTri(o, d, T + 9 * gL[k]); if (t >= 0 && t <= tmax && (best < 0 || t < best)) best = t; }
  }
  return best;
}
// есть ли на картинке препятствие за точкой удара в слое кузова (лучи 0.4..1.4 м над полом, три со сдвигом вбок 0.5 м)
static int visAt(const float *P, float nx, float nz) {
  static const float H[4] = {.4f, .7f, 1.f, 1.4f};
  for (int s = -1; s <= 1; s++) for (int k = 0; k < 4; k++) {
    float o[3] = {P[0] + nx * .3f + nz * .5f * s, P[1] + H[k], P[2] + nz * .3f - nx * .5f * s}, dd[3] = {-nx, 0, -nz};
    if (rayCast(o, dd, 1.3f) >= 0) return 1;
  }
  return 0;
}
static float primTop(int type, unsigned i) {
  if (type == COL_TRI && i < world.ntri) { const ColTri *t = &world.tri[i]; return fmaxf(t->a[1], fmaxf(t->b[1], t->c[1])); }
  if (type == COL_BOX && i < world.nbox) { const ColBox *b = &world.box[i]; return b->c[1] + fabsf(b->ax[1]) * b->h[0] + fabsf(b->ay[1]) * b->h[1] + fabsf(b->az[1]) * b->h[2]; }
  if (type == COL_CYL && i < world.ncyl) { const ColCyl *c = &world.cyl[i]; return c->c[1] + fabsf(c->axis[1]) * c->halfLen + (c->flat ? 0.f : c->r); }
  if (type == COL_SEG && i < world.nseg) return world.seg[i].y1;
  return NAN;
}

static int load(const char *path) {
  FILE *f = fopen(path, "rb"); if (!f) return 2;
  fseek(f, 0, SEEK_END); fileSize = ftell(f); rewind(f);
  file = aligned_alloc(16, (fileSize + 15) & ~15);
  if (!file || fread(file, 1, fileSize, f) != (size_t)fileSize) return 2;
  fclose(f);
  unsigned co = *(unsigned *)(file + 92), po = *(unsigned *)(file + 96), pv = *(unsigned *)(file + 88), cr = *(unsigned *)(file + 100), ce = (unsigned)fileSize;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&world, file + co, ce - co) || propsInit(&props, file + po, *(unsigned *)(file + po + 40)) || spInit(&sp, props.spr, props.sprSize)) return 2;
  props.onKick = onKick;
  carDefaults(&carP);
  if (cr && cr < (unsigned)fileSize) { unsigned ce2 = (unsigned)fileSize; if (pv > cr && pv < ce2) ce2 = pv; carOk = carParamsFromCar1(file + cr, ce2 - cr, &carP) == 1; }
  { const char *e; if ((e = getenv("BAND_HI"))) { carP.bandHi = atof(e); carOk = 1; } if ((e = getenv("HALF_W"))) { carP.halfW = atof(e); carOk = 1; } }   // опыт: кузов оригинала (1.09, 0.978)
  loadRender();
  return 0;
}

typedef struct { float lat, dh, kmh; } RunArg;
typedef struct { int airHits, airHitsInvis, groundHits, stuck, recov; float maxAlong, maxY, endV, worstInvisV; int worstType; unsigned worstIdx; float wx, wy, wz, wtop; float ex, ey, ez; } RunRes;
static PropState *st0;
static int verbose;
static RunRes run(float x, float y, float z, float hdeg, RunArg a, float secs) {
  RunRes r = {0}; r.worstType = -1;
  memcpy(props.st, st0, sizeof(PropState) * props.ninst); props.havePreviousCar = 0;
  spFree(&sp); spInit(&sp, props.spr, props.sprSize);
  float h = (hdeg + a.dh) * 0.01745329f, fx = sinf(h), fz = cosf(h), h0 = hdeg * 0.01745329f;
  float sx = x + cosf(h0) * a.lat, sz = z - sinf(h0) * a.lat;   // вбок: вправо от курса (-fz, fx) при руле +1 вправо — здесь просто перпендикуляр
  float g = 0; colGround(&world, sx, sz, y < 1e5f ? y + 3.f : 400.f, -1e6f, &g, 0);
  carInit(&car, carOk ? &carP : NULL, &world, sx, g, sz, h);
  for (int k = 0; k < 20; k++) { CarInput in = {0}; carUpdate(&car, &world, &in, 1 / 60.f); }
  carSetSpeed(&car, a.kmh / 3.6f);
  float ax0 = car.x, az0 = car.z; int stopped = 0, rec0 = car.recoveries;
  for (int fr = 0; fr < secs * 60; fr++) {
    CarInput in = {0}; in.throttle = car.v * 3.6f < a.kmh ? 1.f : 0.f;
    int air0 = !car.grounded;
    carUpdate(&car, &world, &in, 1 / 60.f);
    propsUpdate(&props, &world, &car, 1 / 60.f);
    spUpdate(&sp, &world, &car, NULL, 1 / 60.f);
    float along = (car.x - ax0) * fx + (car.z - az0) * fz; if (along > r.maxAlong) r.maxAlong = along;
    if (car.y > r.maxY) r.maxY = car.y;
    if (car.wallV > 2.f) {
      int air = air0 && car.wheels == 0;
      int vis = visAt(car.wallP, car.wallN[0], car.wallN[1]);
      float top = primTop(car.wallType, car.wallIdx);
      if (air) { r.airHits++; if (!vis) { r.airHitsInvis++; if (car.wallV > r.worstInvisV) { r.worstInvisV = car.wallV; r.worstType = car.wallType; r.worstIdx = car.wallIdx; r.wx = car.wallP[0]; r.wy = car.wallP[1]; r.wz = car.wallP[2]; r.wtop = top; } } }
      else r.groundHits++;
      if (verbose) printf("   hit t=%.2f %s v %.1f at (%.1f %.1f %.1f) n (%.2f %.2f) prim %d#%u top %.1f vis %d car y %.2f\n", fr / 60.f, air ? "AIR" : "gnd",
                          car.wallV, car.wallP[0], car.wallP[1], car.wallP[2], car.wallN[0], car.wallN[1], car.wallType, car.wallIdx, top, vis, car.y);
    }
    if (verbose > 1 && fr % 6 == 0)
      printf("   t=%.2f pos %.2f %.2f %.2f vel %.1f %.1f %.1f roll %.0f pitch %.0f h %.0f wh %d gr %d ct %d boosts %d\n", fr / 60.f, car.x, car.y, car.z,
             car.vel[0], car.vel[1], car.vel[2], car.roll * 57.3f, car.pitch * 57.3f, car.h * 57.3f, car.wheels, car.grounded, car.contacts, car.boosts);
    float hv = hypotf(car.vel[0], car.vel[2]);
    if (hv < 2.f && fr > 30) { if (++stopped > 90) { r.stuck = 1; break; } } else stopped = 0;
  }
  r.recov = car.recoveries - rec0; r.endV = hypotf(car.vel[0], car.vel[2]) * 3.6f; r.ex = car.x; r.ey = car.y; r.ez = car.z;
  return r;
}
static int range(const char *s, float *a, float *b, float *st) { return sscanf(s, "%f:%f:%f", a, b, st) == 3; }

int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: host level.shr scan X Z HEADING_DEG [opts]\n"); return 2; }
  if (load(argv[1])) { fprintf(stderr, "level load failed\n"); return 2; }
  st0 = malloc(sizeof(PropState) * props.ninst); memcpy(st0, props.st, sizeof(PropState) * props.ninst);
  printf("level car: %s (CAR1 %s)\n", carP.name, carOk ? "v2" : "нет — famil_v");
  if (!strcmp(argv[2], "scan") && argc >= 6) {
    float x = atof(argv[3]), z = atof(argv[4]), hd = atof(argv[5]), y = 1e6f, secs = 4;
    float l0 = 0, l1 = 0, ls = 1, d0 = 0, d1 = 0, ds = 1, k0 = 60, k1 = 120, ks = 10;
    for (int i = 6; i < argc; i++) {
      if (!strcmp(argv[i], "--y") && i + 1 < argc) y = atof(argv[++i]);
      else if (!strcmp(argv[i], "--lat") && i + 1 < argc) range(argv[++i], &l0, &l1, &ls);
      else if (!strcmp(argv[i], "--dh") && i + 1 < argc) range(argv[++i], &d0, &d1, &ds);
      else if (!strcmp(argv[i], "--kmh") && i + 1 < argc) range(argv[++i], &k0, &k1, &ks);
      else if (!strcmp(argv[i], "--secs") && i + 1 < argc) secs = atof(argv[++i]);
      else if (!strcmp(argv[i], "-v")) verbose = 1;
      else if (!strcmp(argv[i], "-vv")) verbose = 2;
    }
    int n = 0, nInv = 0, nStuck = 0;
    for (float lat = l0; lat <= l1 + 1e-4f; lat += ls) for (float dh = d0; dh <= d1 + 1e-4f; dh += ds) for (float k = k0; k <= k1 + 1e-4f; k += ks) {
      RunArg a = {lat, dh, k};
      if (verbose) printf("run lat %.1f dh %.0f kmh %.0f\n", lat, dh, k);
      RunRes r = run(x, y, z, hd, a, secs); n++; nInv += r.airHitsInvis > 0; nStuck += r.stuck;
      printf("lat %5.1f dh %4.0f kmh %4.0f: along %5.1f maxY %5.1f endV %4.0f air hits %d (invisible %d) ground hits %d%s%s",
             lat, dh, k, r.maxAlong, r.maxY, r.endV, r.airHits, r.airHitsInvis, r.groundHits, r.stuck ? " STUCK" : "", r.recov ? " RECOVER" : ""); printf(" | end (%.1f %.1f %.1f)", r.ex, r.ey, r.ez);
      if (r.worstType >= 0) printf(" | worst invisible: prim %d#%u v %.1f at (%.1f %.1f %.1f) top %.1f", r.worstType, r.worstIdx, r.worstInvisV, r.wx, r.wy, r.wz, r.wtop);
      printf("\n");
    }
    printf("runs %d, with invisible air hits %d, stuck %d\n", n, nInv, nStuck);
    return 0;
  }
  if (!strcmp(argv[2], "wrenches")) {   // гаечные ключи уровня (JMP1 reserved[0]): место и радиус (служебно, правка 200)
    for (unsigned i = 0; i < world.nwrench; i++) printf("wrench %u %.1f %.1f %.1f r %.1f %s\n", i, world.wrench[i].p[0], world.wrench[i].p[1], world.wrench[i].p[2], world.wrench[i].r, world.wrench[i].name);
    return 0;
  }
  if (!strcmp(argv[2], "prof") && argc >= 8) {   // prof X Z0 Z1 STEP YTOP — высота земли вдоль z по линии x (служебно, сверка 164)
    float x = atof(argv[3]), z0 = atof(argv[4]), z1 = atof(argv[5]), st = atof(argv[6]), yt = atof(argv[7]);
    for (float z = z0; st > 0 ? z <= z1 : z >= z1; z += st) {
      float gy, n[3]; int ok = colGround(&world, x, z, yt, yt - 30.f, &gy, n);
      printf("x %.2f z %.2f ground %s %.2f n %.2f %.2f %.2f\n", x, z, ok ? "" : "НЕТ", ok ? gy : 0.f, n[0], n[1], n[2]);
    }
    return 0;
  }
  if (!strcmp(argv[2], "near") && argc >= 7) {   // near X Y Z R — ящики и цилиндры коллизии, чей центр ближе R по xz (служебно, сверка 164)
    float x = atof(argv[3]), y = atof(argv[4]), z = atof(argv[5]), r = atof(argv[6]);
    for (unsigned i = 0; i < world.nbox; i++) { const ColBox *b = &world.box[i];
      float hr = fmaxf(fmaxf(b->h[0], b->h[1]), b->h[2]);
      if (hypotf(b->c[0] - x, b->c[2] - z) > r + hr) continue;
      printf("box %u c %.2f %.2f %.2f h %.2f %.2f %.2f ax %.2f %.2f %.2f ay %.2f %.2f %.2f az %.2f %.2f %.2f (dy %.1f)\n", i, b->c[0], b->c[1], b->c[2],
             b->h[0], b->h[1], b->h[2], b->ax[0], b->ax[1], b->ax[2], b->ay[0], b->ay[1], b->ay[2], b->az[0], b->az[1], b->az[2], b->c[1] - y); }
    for (unsigned i = 0; i < world.ncyl; i++) { const ColCyl *c = &world.cyl[i];
      if (hypotf(c->c[0] - x, c->c[2] - z) > r + c->halfLen + c->r) continue;
      printf("cyl %u c %.2f %.2f %.2f axis %.2f %.2f %.2f r %.2f half %.2f flat %u mat %s\n", i, c->c[0], c->c[1], c->c[2], c->axis[0], c->axis[1], c->axis[2], c->r, c->halfLen, c->flat, c->pad[0] == 1.f ? "листва" : "твёрдое"); }
    return 0;
  }
  if (!strcmp(argv[2], "fol") && argc >= 6) {   // fol X Z R — листва (кусты, изгороди) ближе R по xz: ящики, цилиндры (правка 200)
    float x = atof(argv[3]), z = atof(argv[4]), r = atof(argv[5]);
    for (unsigned i = 0; i < world.nbox; i++) { const ColBox *b = &world.box[i]; if (b->pad != 1.f) continue;
      float hr = fmaxf(fmaxf(b->h[0], b->h[1]), b->h[2]); float d = hypotf(b->c[0] - x, b->c[2] - z);
      if (d > r + hr) continue;
      printf("folbox %u d %.1f c %.2f %.2f %.2f h %.2f %.2f %.2f ax %.2f %.2f %.2f\n", i, d, b->c[0], b->c[1], b->c[2], b->h[0], b->h[1], b->h[2], b->ax[0], b->ax[1], b->ax[2]); }
    for (unsigned i = 0; i < world.ncyl; i++) { const ColCyl *c = &world.cyl[i]; if (c->pad[0] != 1.f) continue;
      float d = hypotf(c->c[0] - x, c->c[2] - z); if (d > r + c->halfLen + c->r) continue;
      printf("folcyl %u d %.1f c %.2f %.2f %.2f axis %.2f %.2f %.2f r %.2f half %.2f\n", i, d, c->c[0], c->c[1], c->c[2], c->axis[0], c->axis[1], c->axis[2], c->r, c->halfLen); }
    return 0;
  }
  if (!strcmp(argv[2], "box") && argc >= 4) {   // box N — ящик коллизии N: центр, оси, полуразмеры (служебно, сверка 164)
    for (int k = 3; k < argc; k++) {
      unsigned i = (unsigned)atoi(argv[k]); if (i >= world.nbox) { printf("box %u: нет (всего %u)\n", i, world.nbox); continue; }
      const ColBox *b = &world.box[i];
      printf("box %u c %.2f %.2f %.2f h %.2f %.2f %.2f ax %.2f %.2f %.2f ay %.2f %.2f %.2f az %.2f %.2f %.2f mat %s\n", i, b->c[0], b->c[1], b->c[2],
             b->h[0], b->h[1], b->h[2], b->ax[0], b->ax[1], b->ax[2], b->ay[0], b->ay[1], b->ay[2], b->az[0], b->az[1], b->az[2], b->pad == 1.f ? "листва" : "твёрдое");
    }
    return 0;
  }
  if (!strcmp(argv[2], "drop") && argc >= 10) {
    // drop X Y Z HEADING_DEG VFWD VDOWN SECS [VERBOSE] — машина в воздухе над точкой (низ колёс на Y), скорость вперёд и вниз
    float x = atof(argv[3]), y = atof(argv[4]), z = atof(argv[5]), h = atof(argv[6]) * 0.01745329f, vf = atof(argv[7]), vd = atof(argv[8]), secs = atof(argv[9]);
    verbose = argc > 10 ? atoi(argv[10]) : 1;
    carInit(&car, carOk ? &carP : NULL, &world, x, y, z, h);
    { const char *e;   // сверка с оригиналом (правка 164/187): подбор отклика удара без пересборки — AW_REST, AW_POP, AW_FRIC, AW_ANG
      if ((e = getenv("AW_REST"))) car.p.restitution = atof(e); if ((e = getenv("AW_POP"))) car.p.pop = atof(e);
      if ((e = getenv("AW_FRIC"))) car.p.wallFriction = atof(e); if ((e = getenv("AW_ANG"))) car.p.angDamp = atof(e); }
    car.q[0] = cosf(h * .5f); car.q[1] = 0; car.q[2] = sinf(h * .5f); car.q[3] = 0; memset(car.w, 0, sizeof car.w);
    // 388/377 (08.10): у близнеца телепорт задаёт car_vel отдельно от курса кузова (jump.py --place: скорость по осям, курс — какой был);
    // AW_VHEAD=град — курс скорости, если он не равен курсу кузова (по умолчанию — как кузов, прежнее поведение)
    float vh = getenv("AW_VHEAD") ? atof(getenv("AW_VHEAD")) * 0.01745329f : h;
    car.x = x; car.y = y; car.z = z; car.vel[0] = sinf(vh) * vf; car.vel[1] = -vd; car.vel[2] = cosf(vh) * vf; car.fallTop = y;
    float maxRoll = 0, maxPitch = 0, maxLat = 0; int flips0 = car.rights;
    // правка 164 (C7-6, 03.10): газ как у бота близнеца tools/twins/jump.py — AW_VT=м/с: «разгон» газ, пока скорость ниже VT; через
    // AW_RUNUP м от старта — «полёт»: полный газ 0,4 с (20 кадров PAL), потом без газа. Без AW_VT — AW_GAS постоянно (как было)
    float vt = getenv("AW_VT") ? atof(getenv("AW_VT")) : 0.f, runup = getenv("AW_RUNUP") ? atof(getenv("AW_RUNUP")) : 0.f, flyT = -1.f;
    for (int fr = 0; fr < secs * 60; fr++) {
      CarInput in = {0}; { const char *g = getenv("AW_GAS"); if (g) in.throttle = atof(g); }   // правка 164 (C7-6): газ держит, как бот близнеца
      if (vt > 0.f) {
        float along = hypotf(car.x - x, car.z - z), v = hypotf(car.vel[0], car.vel[2]);
        if (flyT < 0.f && along >= runup) flyT = fr / 60.f;
        in.throttle = flyT < 0.f ? (v < vt ? 1.f : 0.f) : (fr / 60.f - flyT < .4f ? 1.f : 0.f);
      }
      { const char *tx = getenv("AW_TX"), *tz = getenv("AW_TZ");   // правка 200: руль на точку (AW_TX, AW_TZ), как бот близнеца arch.py
        if (tx && tz) { float e = atan2f(atof(tx) - car.x, atof(tz) - car.z) - car.h;
          while (e > 3.14159265f) e -= 6.2831853f; while (e < -3.14159265f) e += 6.2831853f;
          in.steer = fmaxf(-1.f, fminf(1.f, -e * 2.f)); } }   // руль порта: плюс — курс убывает
      int r0 = car.reverts; float v0 = hypotf(car.vel[0], car.vel[2]);
#ifdef COL_DEBUG
      { extern int colDebug; const char *d0 = getenv("AW_DBG0"), *d1 = getenv("AW_DBG1");   // правка 200: разбор толчков кузова
        colDebug = d0 && d1 && fr / 60.f >= atof(d0) && fr / 60.f <= atof(d1);
        if (colDebug) printf("== кадр t=%.3f машина %.3f %.3f %.3f\n", fr / 60.f, car.x, car.y, car.z); }
#endif
      carUpdate(&car, &world, &in, 1 / 60.f);
      int folHits = colFoliageHits();
      propsUpdate(&props, &world, &car, 1 / 60.f); spUpdate(&sp, &world, &car, NULL, 1 / 60.f);
      float fx = sinf(car.h), fz = cosf(car.h), lat = fabsf(-fz * car.vel[0] + fx * car.vel[2]);
      if (fabsf(car.roll) > maxRoll) maxRoll = fabsf(car.roll); if (fabsf(car.pitch) > maxPitch) maxPitch = fabsf(car.pitch); if (lat > maxLat) maxLat = lat;
      if (verbose > 1 || (verbose && (car.contacts || car.reverts != r0 || car.wallV > 1.f || fr % 15 == 0)))
        printf("t=%.2f pos %.2f %.2f %.2f vel %.1f %.1f %.1f |v| %.1f->%.1f roll %.0f pitch %.0f h %.0f wh %d gr %d ct %d wallV %.1f prim %d#%u lat %.2f fol %d%s\n", fr / 60.f, car.x, car.y, car.z,
               car.vel[0], car.vel[1], car.vel[2], v0, hypotf(car.vel[0], car.vel[2]), car.roll * 57.3f, car.pitch * 57.3f, car.h * 57.3f, car.wheels, car.grounded,
               car.contacts, car.wallV, car.wallV > 0 ? car.wallType : -1, car.wallV > 0 ? car.wallIdx : 0, -fz * car.vel[0] + fx * car.vel[2], folHits,
               car.reverts != r0 ? " REVERT" : "");
      if (verbose > 2)   // 388/377: опора по колёсам (сжатие подвески FL FR RL RR, м), касание днищем, типы контактов кузова/сфер колёс
        printf("   comp %.3f %.3f %.3f %.3f body %d ct %d%s%s%s%s pen %.3f w %.2f %.2f %.2f\n", car.comp[0], car.comp[1], car.comp[2], car.comp[3], car.bodyHit,
               car.contacts, car.contacts > 0 ? (car.ctype[0] == COL_BOX ? " box" : car.ctype[0] == COL_TRI ? " tri" : car.ctype[0] == COL_CYL ? " cyl" : " seg") : "",
               car.contacts > 1 ? (car.ctype[1] == COL_BOX ? " box" : car.ctype[1] == COL_TRI ? " tri" : car.ctype[1] == COL_CYL ? " cyl" : " seg") : "",
               car.contacts > 2 ? (car.ctype[2] == COL_BOX ? " box" : car.ctype[2] == COL_TRI ? " tri" : car.ctype[2] == COL_CYL ? " cyl" : " seg") : "",
               car.contacts > 3 ? (car.ctype[3] == COL_BOX ? " box" : car.ctype[3] == COL_TRI ? " tri" : car.ctype[3] == COL_CYL ? " cyl" : " seg") : "",
               car.pen, car.w[0], car.w[1], car.w[2]);
    }
    // 388/377 (08.10): конец — начало модели (низ колёс + подъём модели по оси вверх кузова), как точка трассы оригинала (mat[12..14]);
    // при крене 35° это 0,4 м вбок от низа колёс. base — прежняя точка (низ колёс)
    { const float *q = car.q; float ux = 2 * (q[1] * q[2] - q[0] * q[3]), uy = 1 - 2 * (q[1] * q[1] + q[3] * q[3]), uz = 2 * (q[2] * q[3] + q[0] * q[1]);
      float lift = carModelLift(&car, car.p.modelLift);
      printf("SUMMARY end %.2f %.2f %.2f |v| %.1f maxRoll %.0f maxPitch %.0f maxLat %.1f rights %d recoveries %d base %.2f %.2f %.2f\n",
             car.x + ux * lift, car.y + uy * lift, car.z + uz * lift, hypotf(car.vel[0], car.vel[2]), maxRoll * 57.3f, maxPitch * 57.3f, maxLat,
             car.rights - flips0, car.recoveries, car.x, car.y, car.z); }
    return 0;
  }
  fprintf(stderr, "unknown mode\n"); return 2;
}
