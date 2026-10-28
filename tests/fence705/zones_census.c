// №705 ч.2: перепись зон прыжка (COL1/JMP1) уровня: какие задевает правило jumpZoneDrive (полудлина вдоль въезда < JUMP_ZONE_MIN_ALONG) и что меняется.
// cc -std=c11 -O2 -w -DSKIN_HOST -include stdio.h -Iviewer tests/fence705/zones_census.c viewer/car.c viewer/col.c viewer/coins.c -lm; ./a.out level.shr LEVEL
// Направление въезда = крутейший подъём земли вокруг центра зоны (рампа идёт вверх к кромке); вдоль него — полудлина объёма в плане (как car.c zoneAlong).
// Для каждой зоны: код места (viewer/placecode.h), полудлины, окно разгона прежнее/новое (м вдоль въезда), и рой заездов с газом (скорости 15/20/25 м/с, 3 смещения) на прежнем и новом
// правиле: скорость в момент отрыва и дальность до посадки (м от центра зоны). Строка: ПОМЕНЯЛОСЬ, если хоть один заезд разошёлся.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "car.h"
#include "placecode.h"
#define MINALONG 6.f
static float along(const ColZone *v, float fx, float fz) {
  if (!v->shape) return v->h[0];
  const float *A[3] = {v->ax, v->ay, v->az}; float e = 0.f;
  for (int k = 0; k < 3; k++) e += v->h[k] * fabsf(A[k][0] * fx + A[k][2] * fz);
  return e;
}
static int ground(ColWorld *w, float x, float z, float yref, float *g) { return colGround(w, x, z, yref + 12.f, yref - 25.f, g, 0); }
typedef struct { float vtake, land, vexit; int took, landed, boosts; } Res;
static Res run(ColWorld *w, const ColZone *Z, float dx, float dz, float lat, float vt, int old) {
  Res r = {0, 0, 0, 0, 0, 0}; float g; carJumpZoneGateOld = old;
  float sx = Z->c[0] - dx * 40.f - dz * lat, sz = Z->c[2] - dz * 40.f + dx * lat;
  if (!ground(w, sx, sz, Z->c[1], &g)) return r;
  float h = atan2f(dx, dz); Car c; carInit(&c, NULL, w, sx, g, sz, h);
  for (int i = 0; i < 20; i++) carUpdate(&c, w, &(CarInput){0}, 1.f / 60);
  if (hypotf(c.x - sx, c.z - sz) > 1.f) return r;
  carSetSpeed(&c, vt);
  for (int fr = 0; fr < 420; fr++) {
    CarInput in = {0}; in.throttle = c.v < vt ? 1.f : 0.f; carUpdate(&c, w, &in, 1.f / 60);
    float along_c = (c.x - Z->c[0]) * dx + (c.z - Z->c[2]) * dz;
    if (!r.vexit && along_c >= 10.f) r.vexit = hypotf(c.vel[0], c.vel[2]);
    if (!r.took && !c.grounded && along_c > -25.f && c.y > g + 1.f) { r.took = 1; r.vtake = hypotf(c.vel[0], c.vel[2]); }
    if (r.took && !r.landed && c.grounded) { r.landed = 1; r.land = along_c; }
    if (along_c > 30.f) break;
  }
  r.boosts = c.boosts;
  return r;
}
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); unsigned n = ftell(f); rewind(f); unsigned char *b = malloc(n); if (fread(b, 1, n, f) != n) return 2; fclose(f);
  int L = atoi(argv[2]);
  unsigned co = *(unsigned *)(b + 92), po = *(unsigned *)(b + 96), pv = *(unsigned *)(b + 88), end = n; if (po > co && po < end) end = po; if (pv > co && pv < end) end = pv;
  ColWorld w; if (colInit(&w, b + co, end - co)) return 2;
  Car probe; carInit(&probe, NULL, &w, 0, 0, 0, 0); float halfL = probe.p.halfL;
  printf("# уровень зона имя код x y z | курс° вдоль_м окно_прежнее окно_новое | газ: подшагов прежнее новое, +скорость на 10 м за зоной (макс), отрыв в из, разошлись, +скорость отрыва, +дальность\n");
  for (unsigned i = 0; i < w.nzone; i++) {
    const ColZone *Z = &w.zone[i]; char code[16]; placeCode(Z->c[0], Z->c[2], code, sizeof code);
    float axp[2][2] = {{Z->ax[0], Z->ax[2]}, {Z->az[0], Z->az[2]}};
    for (int k = 0; k < 2; k++) { float m = hypotf(axp[k][0], axp[k][1]); if (m < 0.3f) continue;
      for (int sg = -1; sg <= 1; sg += 2) {
        float dx = sg * axp[k][0] / m, dz = sg * axp[k][1] / m, e = along(Z, dx, dz);
        if (e >= MINALONG) continue;
        float eo = e + halfL, en = MINALONG + halfL; int tot = 0, diff = 0, took = 0, mb = 0, nb = 0; float mx = 0, mv = 0, ml = 0;
        const float vs[3] = {15, 20, 25};
        for (int iv = 0; iv < 3; iv++) for (int lt = -1; lt <= 1; lt++) {
          Res o = run(&w, Z, dx, dz, lt * 1.5f, vs[iv], 1), nn = run(&w, Z, dx, dz, lt * 1.5f, vs[iv], 0);
          if (o.vexit == 0 && nn.vexit == 0) continue; tot++;
          if (fabsf(nn.vexit - o.vexit) > fabsf(mx)) mx = nn.vexit - o.vexit; if (iv == 1 && lt == 0) { mb = o.boosts; nb = nn.boosts; }
          if (o.took || nn.took) { took++; float d1 = nn.vtake - o.vtake, d2 = nn.landed && o.landed ? nn.land - o.land : 0.f;
            if (fabsf(d1) > 0.05f || fabsf(d2) > 0.3f || o.took != nn.took) diff++; if (fabsf(d1) > fabsf(mv)) mv = d1; if (fabsf(d2) > fabsf(ml)) ml = d2; } }
        printf("L%d\t%u\t%s\t%s\t%.1f\t%.1f\t%.1f\t%.0f\t%.2f\t%.1f\t%.1f\t%d\t%d\t%+.1f\t%d/%d\t%d\t%+.1f\t%+.1f\n", L, i, Z->name, code, Z->c[0], Z->c[1], Z->c[2],
               atan2f(dx, dz) * 57.2958f, e, 2 * eo, 2 * en, mb, nb, mx, took, tot, diff, mv, ml); } }
  }
  return 0;
}
