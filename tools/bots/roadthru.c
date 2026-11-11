// Коллизия-96: гейт «ящик не закрывает проезд». roadthru LEVEL PLAYER [roadId]
// Для каждой дороги трафика, чья осевая проходит внутри ящика коллизии (как pocket roadbox): машина ставится за 14 м до первого входа в ящик, едет по осевой (преследование точки на 8 м вперёд, газ) 25 с.
// Печать: дорога, ящики, прошла ли до точки на 10 м за последним выходом из ящика, где встала и на какой v. Код 0 всегда (диагностика), строка ИТОГО: проходят N из M.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "traffic.h"
static BotWorld W;
typedef struct { float x, y, z; } P3;
int main(int argc, char **argv) {
  if (argc < 3 || bwData(argv[1], argv[2], 0)) { fprintf(stderr, "usage: roadthru LEVEL PLAYER [road]\n"); return 2; }
  memset(&W, 0, sizeof W); if (bwNew(&W)) return 2; const ColWorld *cw = bwCol(); const Traffic *t = (const Traffic *)bwTraffic();
  if (!t || !t->hd) { puts("нет трафика"); return 2; }
  int only = argc > 3 ? atoi(argv[3]) : -1, total = 0, okn = 0;
  for (uint32_t r = 0; r < t->hd->nRoad; r++) {
    if (only >= 0 && (int)r != only) continue;
    const TrRoad *R_ = &t->road[r]; static P3 S[4096]; int n = 0;
    for (uint32_t k = 0; k + 1 < R_->nPt && n < 4000; k++) { const TrPt *p = &t->pt[R_->firstPt + k], *q = &t->pt[R_->firstPt + k + 1]; float L = hypotf(q->x - p->x, q->z - p->z); int m = (int)(L) + 1;
      for (int s = 0; s < m && n < 4000; s++) { float u = (float)s / m; S[n].x = p->x + (q->x - p->x) * u; S[n].y = p->y + (q->y - p->y) * u; S[n].z = p->z + (q->z - p->z) * u; n++; } }
    int i0 = -1, i1 = -1; char names[160] = ""; int hb[16], nhb = 0;
    for (int i = 0; i < n; i++) for (uint32_t b = 0; b < cw->nbox; b++) { const ColBox *B = &cw->box[b]; float d[3] = {S[i].x - B->c[0], S[i].y + 1.5f - B->c[1], S[i].z - B->c[2]};
      float lx = d[0] * B->ax[0] + d[1] * B->ax[1] + d[2] * B->ax[2], ly = d[0] * B->ay[0] + d[1] * B->ay[1] + d[2] * B->ay[2], lz = d[0] * B->az[0] + d[1] * B->az[1] + d[2] * B->az[2];
      if (fabsf(lx) < B->h[0] && fabsf(ly) < B->h[1] && fabsf(lz) < B->h[2]) { if (i0 < 0) i0 = i; i1 = i; int f = 0; for (int h = 0; h < nhb; h++) if (hb[h] == (int)b) f = 1; if (!f && nhb < 16) hb[nhb++] = (int)b; } }
    if (i0 < 0) continue; total++;
    for (int h = 0; h < nhb; h++) { char q[16]; snprintf(q, sizeof q, " b%d", hb[h]); strncat(names, q, sizeof names - strlen(names) - 1); }
    int is = i0 - 14 < 0 ? 0 : i0 - 14, ie = i1 + 10 >= n ? n - 1 : i1 + 10;
    float hd = atan2f(S[is + 1].x - S[is].x, S[is + 1].z - S[is].z), gy = -1e9f; if (!colGroundMat(cw, S[is].x, S[is].z, S[is].y + 3.f, S[is].y - 6.f, &gy, 0, 0)) { printf("дорога %u:%s: нет земли в точке старта (%.0f %.0f)\n", r, names, S[is].x, S[is].z); continue; }
    memset(&W, 0, sizeof W); bwNew(&W); bwPlace(&W, S[is].x, gy + .3f, S[is].z, hd); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0.f, 0.f);
    int cur = is, reached = 0; float vmin = 1e9f, lastx = W.car.x, lastz = W.car.z; int stall = 0;
    for (int f = 0; f < 25 * 60; f++) {
      float bd = 1e18f; for (int i = cur; i < n && i < cur + 40; i++) { float dx = S[i].x - W.car.x, dz = S[i].z - W.car.z, d = dx * dx + dz * dz; if (d < bd) { bd = d; cur = i; } }
      int ti = cur + 8 >= n ? n - 1 : cur + 8; float ang = atan2f(S[ti].x - W.car.x, S[ti].z - W.car.z), err = ang - W.car.h; while (err > 3.14159f) err -= 6.28318f; while (err < -3.14159f) err += 6.28318f;
      float st = err * 1.5f; if (st > 1.f) st = 1.f; if (st < -1.f) st = -1.f;
      float vh = hypotf(W.car.vel[0], W.car.vel[2]); bwStep(&W, vh < 9.f ? GP_R : 0, getenv("RT_SIGN") ? -st : st, 0.f);
      if (vh < vmin && f > 120) vmin = vh;
      if (cur >= ie) { reached = 1; break; }
      if (f % 60 == 0) { if (hypotf(W.car.x - lastx, W.car.z - lastz) < 0.5f) stall++; else stall = 0; lastx = W.car.x; lastz = W.car.z; if (stall >= 4) break; } }
    if (reached) okn++;
    printf("дорога %3u (ящики%s, точки %d..%d из %d): %s, конец (%.1f %.1f) v мин %.1f, до цели осталось %d м\n", r, names, i0, i1, n, reached ? "ПРОЕХАЛА" : "НЕ ПРОЕХАЛА", W.car.x, W.car.z, vmin > 1e8f ? 0.f : vmin, ie - cur); }
  printf("ИТОГО: проезжают %d из %d дорог с осевой в ящике\n", okn, total); return 0; }
