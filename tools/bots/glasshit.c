// Стенд «стекло» (07.11, Данные-93, ПРАВКИ_РОБА №1224): машина наезжает на каждое сбиваемое стекло уровня (проп с обломком 19) с разных
// скоростей, и Гомер пешком идёт сквозь него. Закон Роба: целое стекло — стена, разбитое — проход.
//   glasshit LEVEL PLAYER [--v V1,V2,...] [--only I] [-v]
// Строки: G <i> <v> <итог: БИТО|ЦЕЛО_ПРОШЛА|ЦЕЛО_СТОИТ> ...; H <i> <итог Гомера: ТВЁРДОЕ|ПУСТОЕ>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
static void quatToR(const float q[4], float R[3][3]) {   // копия props.c (там static)
  float w = q[0], x = q[1], y = q[2], z = q[3];
  R[0][0] = 1.f - 2.f * (y * y + z * z); R[0][1] = 2.f * (x * y - w * z); R[0][2] = 2.f * (x * z + w * y);
  R[1][0] = 2.f * (x * y + w * z); R[1][1] = 1.f - 2.f * (x * x + z * z); R[1][2] = 2.f * (y * z - w * x);
  R[2][0] = 2.f * (x * z - w * y); R[2][1] = 2.f * (y * z + w * x); R[2][2] = 1.f - 2.f * (x * x + y * y);
}
extern int glassSolid1224;   // viewer/props.c №1224
extern int propsSolidHomer(const Props *p, float *x, float *z, float y, float r, float stepH, float bodyH, float *top, float *onx, float *onz);

// --walk: Гомер пешком идёт на стекло (от 6 м по нормали), 4 с стик вперёд; потом то же с пинком □ у стекла.
static int gameOnFootW(BotWorld *w) { return gameOnFoot(&w->G); }
static void walkTest(int i, int kick) {
  bwPropsSolid = 1;
  static BotWorld W; memset(&W, 0, sizeof W); if (bwNew(&W)) return;
  const PropState *s = &W.props.st[i]; float R[3][3]; quatToR(s->q, R);
  float h[3] = {s->hx, s->hy, s->hz}; int th = 0; for (int m = 1; m < 3; m++) if (h[m] < h[th]) th = m;
  float nx = R[0][th], nz = R[2][th], nl = hypotf(nx, nz); nx /= nl; nz /= nl;
  float sx = s->x - nx * 6.f, sz = s->z - nz * 6.f, g, n3[3];
  if (!colGround(bwCol(), sx, sz, s->y + 3.f, s->y - 12.f, &g, n3)) { printf("W\t%d\t%d\tпропуск: нет опоры\n", i, kick); return; }
  bwPlace(&W, sx, g, sz, atan2f(nx, nz)); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  for (int f = 0; f < 120 && !gameOnFootW(&W); f++) bwStep(&W, (f % 20) == 0 ? GP_TRIANGLE : 0, 0, 0);
  if (!gameOnFootW(&W)) { printf("W\t%d\t%d\tне вышел из машины\n", i, kick); return; }
  for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  float side0 = (W.homer.x - s->x) * nx + (W.homer.z - s->z) * nz; int crossedIntact = 0, brokeF = -1, crossedAfter = 0;
  for (int f = 0; f < 360; f++) {
    float yaw = W.homer.camYaw, r = atan2f(s->x - W.homer.x, s->z - W.homer.z) - yaw;   // идём к центру стекла (в плоскости), стик как у бота
    unsigned b = 0; float dist = hypotf(s->x - W.homer.x, s->z - W.homer.z);
    if (kick && dist < 2.2f && (f % 24) == 0) b = GP_SQUARE;
    bwStep(&W, b, -sinf(r), -cosf(r));
    const PropState *q = &W.props.st[i];
    float sd = (W.homer.x - q->x) * nx + (W.homer.z - q->z) * nz;
    if (q->hidden && brokeF < 0) brokeF = f;
    if (sd * side0 < 0) {
      float lat = (W.homer.x - q->x) * (-nz) + (W.homer.z - q->z) * nx, hgt = W.homer.y - q->y;
      int inside = fabsf(lat) < fmaxf(h[0], h[2]) - 0.2f && hgt > -h[1] - .2f && hgt < h[1];
      if (!q->hidden) { if (inside) crossedIntact = 1; } else crossedAfter = 1; }
  }
  const PropState *q = &W.props.st[i];
  printf("W\t%d\t%s\t%s\tнизстекла_над_землёй %.1f центр(%.1f %.1f %.1f) разбито %s (кадр %d) · прошёл через целое %s · прошёл после разбития %s · конец (%.1f %.1f)\n", i, kick ? "с пинком" : "без пинка", crossedIntact ? "ПРОШЁЛ_СКВОЗЬ_ЦЕЛОЕ" : "стена держит", (s->y - s->hy) - g, s->x, s->y, s->z,
         q->hidden ? "да" : "нет", brokeF, crossedIntact ? "ДА" : "нет", crossedAfter ? "да" : "нет", W.homer.x, W.homer.z);
}
int main(int argc, char **argv) {
  if (argc < 3 || bwData(argv[1], argv[2], 0)) { fprintf(stderr, "usage: glasshit LEVEL PLAYER [--v V1,V2] [--only I] [-v]\n"); return 2; }
  float vs[8] = {8, 15, 25, 40, 60}; int nv = 5, only = -1, verbose = 0, walk = 0, doors = 0;
  for (int i = 3; i < argc; i++) {
    if (!strcmp(argv[i], "--v") && i + 1 < argc) { nv = 0; char *s = strtok(argv[++i], ","); while (s && nv < 8) { vs[nv++] = (float)atof(s); s = strtok(NULL, ","); } }
    else if (!strcmp(argv[i], "--only") && i + 1 < argc) only = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-v")) verbose = 1;
    else if (!strcmp(argv[i], "--old")) glassSolid1224 = 0;   // красный контроль: целое стекло для Гомера пустое, как было
    else if (!strcmp(argv[i], "--homer-only")) nv = 0;
    else if (!strcmp(argv[i], "--walk")) walk = 1;
    else if (!strcmp(argv[i], "--doors")) doors = 1;
  }
  bwPropsSolid = 1;
  static BotWorld W; if (bwNew(&W) || !W.haveProps) { fprintf(stderr, "нет мира\n"); return 2; }
  int nglass = 0, bad = 0, homerOpen = 0; if (doors) printf("комнат загружено: %d\n", bwRooms());
  for (unsigned i = 0; i < W.props.ninst; i++) {
    int part, brk; propsTypeFx(&W.props, W.props.def[i].type, &part, &brk); if (brk != 19) continue;
    if (only >= 0 && (int)i != only) continue;
    nglass++;
    if (walk) { walkTest((int)i, 0); walkTest((int)i, 1); continue; }
    const PropState *s0 = &W.props.st[i]; float R[3][3]; quatToR(s0->q, R);
    float h[3] = {s0->hx, s0->hy, s0->hz}; int th = 0; for (int m = 1; m < 3; m++) if (h[m] < h[th]) th = m;   // тонкая ось в локальных координатах
    float nx = R[0][th], nz = R[2][th], nl = hypotf(nx, nz); if (nl < 1e-3f) { printf("G\t%u\t-\tпропуск: стекло лежит\n", i); continue; } nx /= nl; nz /= nl;
    if (doors) {   // двери комнат (интерьеров) рядом с плоскостью стекла: ≤ 2 м по нормали и в пределах ширины + 1,5 м — стекло перекроет вход
      for (int r = 0; r < bwRooms(); r++) { float d[3]; if (!bwRoomDoor(r, d)) continue;
        float tx_ = -nz, tz_ = nx, nd = (d[0] - s0->x) * nx + (d[2] - s0->z) * nz, lt = (d[0] - s0->x) * tx_ + (d[2] - s0->z) * tz_, hg = d[1] - s0->y;
        if (fabsf(nd) < 2.f && fabsf(lt) < fmaxf(s0->hx, s0->hz) + 1.5f && hg > -s0->hy - 1.f && hg < s0->hy) printf("D\t%u\tкомната %d дверь (%.1f %.1f %.1f) от плоскости %.1f м, вдоль %.1f м\n", i, r, d[0], d[1], d[2], nd, lt); }
      continue; }
    // Гомер: точка в 0.15 м перед плоскостью по нормали, ступни на опоре под стеклом (низ коробки), идёт сквозь
    {
      int closed = 0, total = 0;
      for (int side = -1; side <= 1; side += 2) for (int k = -2; k <= 2; k++) {
        float tx = -nz, tz = nx;   // вдоль плоскости
        float px = s0->x + nx * 0.15f * side + tx * k * 2.f, pz = s0->z + nz * 0.15f * side + tz * k * 2.f, top = -1e9f, ox = 0, oz = 0;
        float fy = s0->y - s0->hy + .05f; total++;
        if (propsSolidHomer(&W.props, &px, &pz, fy, .4f, .4f, 1.8f, &top, &ox, &oz)) closed++;
      }
      printf("H\t%u\t%s\t%d/%d точек вплотную выталкивают\n", i, closed >= total / 2 ? "ТВЁРДОЕ" : "ПУСТОЕ", closed, total);
      if (closed < total / 2) homerOpen++;
      { PropState *w_ = &W.props.st[i]; int was = w_->hidden; w_->hidden = 1; int c2 = 0; float px = s0->x + nx * 0.15f, pz = s0->z + nz * 0.15f, top = -1e9f, ox = 0, oz = 0;
        if (propsSolidHomer(&W.props, &px, &pz, s0->y - s0->hy + .05f, .4f, .4f, 1.8f, &top, &ox, &oz)) c2 = 1; w_->hidden = was;
        if (c2) printf("H\t%u\tРАЗБИТОЕ_ВСЁ_ЕЩЁ_ТВЁРДОЕ\n", i); }
    }
    for (int vi = 0; vi < nv; vi++) {
      float v = vs[vi];
      BotSnap sn; bwSaveAlloc(&W, &sn);
      // сначала вернуть стекло в исходное (мир новый на каждый заход: bwNew)
      bwFree(&W); memset(&W, 0, sizeof W); if (bwNew(&W)) return 2;
      const PropState *s = &W.props.st[i];
      float sx = s->x - nx * 12.f, sz = s->z - nz * 12.f, gy = -1e9f, g, n3[3];
      if (!colGround(bwCol(), sx, sz, s->y + 3.f, s->y - 12.f, &g, n3)) { printf("G\t%u\t%.0f\tпропуск: нет опоры на старте (%.1f %.1f)\n", i, v, sx, sz); continue; }
      gy = g; float hd = atan2f(nx, nz);
      bwPlace(&W, sx, gy, sz, hd); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
      // направление вперёд машины: проверить по carBox
      CarBox cb; carBox(&W.car, &cb); float fx = cb.fx, fz = cb.fz;
      W.car.vel[0] = fx * v; W.car.vel[2] = fz * v; W.car.vel[1] = 0.f; W.car.v = v;
      float side0 = (W.car.x - s->x) * nx + (W.car.z - s->z) * nz, tx_ = -nz, tz_ = nx; int crossed = 0, brokeF = -1; float lat = 0, hgt = 0;
      for (int f = 0; f < 240; f++) {
        bwStep(&W, 0, 0, 0);
        const PropState *q = &W.props.st[i];
        float sd = (W.car.x - q->x) * nx + (W.car.z - q->z) * nz;
        if (q->hidden && brokeF < 0) brokeF = f;
        if (sd * side0 < 0 && !crossed) {   // перешла плоскость: внутри ли оконного прямоугольника и целое ли стекло
          lat = (W.car.x - q->x) * tx_ + (W.car.z - q->z) * tz_; hgt = W.car.y - q->y;
          if (fabsf(lat) < fmaxf(h[0], h[2]) - 0.3f && hgt > -h[1] && hgt < h[1] && !q->hidden && !q->kicked) crossed = 1;
          else break;
        }
        if (crossed) break;
        if (hypotf(W.car.vel[0], W.car.vel[2]) < 0.5f && f > 20) break;
      }
      const PropState *q = &W.props.st[i];
      const char *res = q->hidden || q->kicked ? "БИТО" : crossed ? "ЦЕЛО_ПРОШЛА" : "ЦЕЛО_СТОИТ";
      if (!strcmp(res, "ЦЕЛО_ПРОШЛА")) bad++;
      printf("G\t%u\t%.0f\t%s\tc(%.1f %.1f %.1f) кадр_разбития %d нормаль(%.2f %.2f) машина_v_конец %.1f\n", i, v, res, s->x, s->y, s->z, brokeF, nx, nz, hypotf(W.car.vel[0], W.car.vel[2]));
      bwSnapFree(&sn);
    }
  }
  printf("ИТОГО стекол %d · ПУСТЫХ для Гомера %d · целых, но прошла машина %d\n", nglass, homerOpen, bad);
  return 0;
}
