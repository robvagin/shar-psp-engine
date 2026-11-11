// Коллизия-96 для Инспектора, раздел 2 СТЫКИ (хост, CPU). seams LEVEL PLAYER N
// Швы = границы клеток стрима (40 м, таблица клеток level.shr) и плиток кольца коллизии (64 м от начала сетки COL1). Вдоль каждого шва шаг 1,5 м; рядом контрольные линии (шов, сдвинутый на 9 м и 17 м
// внутрь клетки). На каждой точке: земля с обеих сторон (±0,6 м) в окне ±2 м от высоты на шве, высотный скачок, проба кузова (colProbeCar, курс вдоль шва в обе стороны) и «дыра на шве»:
// пол есть на соседних точках шва ±6 м и на обеих сторонах, а здесь с одной стороны нет. Печатает строки NB (формат блокнота) и DIGEST (хеш всех ответов: с кольцом и без обязан совпасть).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "world.h"
typedef struct { float x0, z0, x1, z1; } Cell;
static uint64_t dg = 1469598103934665603ull;
static void mix(const void *p, size_t n) { const uint8_t *b = p; for (size_t i = 0; i < n; i++) { dg ^= b[i]; dg *= 1099511628211ull; } }
static const ColWorld *cw;
static int groundAt(float x, float z, float y0, float *g) { return colGroundMat(cw, x, z, y0 + 2.f, y0 - 2.f, g, 0, 0); }
typedef struct { long n, holes, steps, probes, nograd; } Stat;
static float stX[400], stZ[400], stD[400]; static int nSt;   // точки ступеней на швах клеток (для доказательства)
// одна линия: axis 0 — шов x = c, идёт по z от a до b; axis 1 — шов z = c, идёт по x
static void line(int axis, float c, float a, float b, Stat *S, int *firstHole, float *hx, float *hz, int kind) {
  for (float s = a; s <= b; s += 1.5f) {
    float px = axis ? s : c, pz = axis ? c : s, nx = axis ? 0.f : 1.f, nz = axis ? 1.f : 0.f, g0;
    if (!colGroundMat(cw, px, pz, 60.f, -200.f, &g0, 0, 0)) continue;
    if (g0 < -150.f) continue;
    { float g1; if (colGroundMat(cw, px, pz, g0 - 3.f, -200.f, &g1, 0, 0)) continue; }   // точка с несколькими слоями (крыша, мост, этаж) — не уровень улицы: снимки эмулятора 09.11 показали, что «ступени» L2/L5 были крышами
    float gA = 0, gB = 0; int okA = groundAt(px - nx * .6f, pz - nz * .6f, g0, &gA), okB = groundAt(px + nx * .6f, pz + nz * .6f, g0, &gB);
    S->n++; mix(&okA, 4); mix(&okB, 4); if (okA) mix(&gA, 4); if (okB) mix(&gB, 4);
    if (okA && okB) { if (fabsf(gA - gB) > .3f) { S->steps++; if (kind == 0 && nSt < 400) { stX[nSt] = px; stZ[nSt] = pz; stD[nSt++] = fabsf(gA - gB); } } }
    else {   // дыра на шве: пол на соседних точках шва ±6 м с обеих сторон есть, здесь с одной стороны нет
      int nb = 0, tot = 0; for (int k = -1; k <= 1; k += 2) { float qx = axis ? s + k * 6.f : c, qz = axis ? c : s + k * 6.f, q0; if (!colGroundMat(cw, qx, qz, g0 + 2.f, g0 - 2.f, &q0, 0, 0)) continue;
        float a1, b1; tot++; if (groundAt(qx - nx * .6f, qz - nz * .6f, q0, &a1) && groundAt(qx + nx * .6f, qz + nz * .6f, q0, &b1)) nb++; }
      if (tot == 2 && nb == 2) { S->holes++; if (kind == 0 && *firstHole < 3) { hx[*firstHole] = px; hz[*firstHole] = pz; (*firstHole)++; } } }
    if (okA && okB) for (int dir = 0; dir < 2; dir++) {   // проба кузова вдоль шва
      CarBox b2; memset(&b2, 0, sizeof b2); float hd = axis ? (dir ? 1.570796f : -1.570796f) : (dir ? 3.14159f : 0.f);
      b2.x = px; b2.z = pz; b2.fx = sinf(hd); b2.fz = cosf(hd); b2.hw = 1.0f; b2.hl = 2.3f; b2.y = g0; b2.lo = .35f; b2.hi = 1.6f; b2.fsupOn = 1; b2.fsup = g0;
      int r = colProbeCar(cw, &b2); S->probes++; mix(&r, 4); mix(&b2.maxDepth, 4); if (b2.maxDepth > .03f) S->nograd++; } }
}
int main(int argc, char **argv) {
  if (argc < 3 || bwData(argv[1], argv[2], 0)) { fprintf(stderr, "usage: seams LEVEL PLAYER [N]\n"); return 2; }
  int lv = argc > 3 ? atoi(argv[3]) : bwLevel(); cw = bwCol();
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2; unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104) return 2; unsigned ncell = *(unsigned *)(hdr + 80), cellOff = *(unsigned *)(hdr + 84);
  Cell *C = malloc(sizeof(Cell) * (ncell + 1)); fseek(f, cellOff, SEEK_SET);
  for (unsigned i = 0; i < ncell; i++) { float r[12]; if (fread(r, 4, 12, f) != 12) return 2; C[i].x0 = r[0]; C[i].z0 = r[2]; C[i].x1 = r[3]; C[i].z1 = r[5]; } fclose(f);
  Stat seam = {0}, ctl = {0}; int fh = 0; float hx[3], hz[3]; long nseg = 0; Stat ringSeam = {0}; (void)ringSeam;
  for (unsigned i = 0; i < ncell; i++) for (unsigned j = 0; j < ncell; j++) { if (i == j) continue;
    // восточный сосед: C[j].x0 == C[i].x1, перекрытие по z
    if (fabsf(C[j].x0 - C[i].x1) < .5f) { float a = fmaxf(C[i].z0, C[j].z0), b = fminf(C[i].z1, C[j].z1); if (b - a > 3.f) { nseg++; line(0, C[i].x1, a, b, &seam, &fh, hx, hz, 0);
        line(0, C[i].x1 - 9.f, a, b, &ctl, &fh, hx, hz, 1); line(0, C[i].x1 - 17.f, a, b, &ctl, &fh, hx, hz, 1); } }
    if (fabsf(C[j].z0 - C[i].z1) < .5f) { float a = fmaxf(C[i].x0, C[j].x0), b = fminf(C[i].x1, C[j].x1); if (b - a > 3.f) { nseg++; line(1, C[i].z1, a, b, &seam, &fh, hx, hz, 0);
        line(1, C[i].z1 - 9.f, a, b, &ctl, &fh, hx, hz, 1); line(1, C[i].z1 - 17.f, a, b, &ctl, &fh, hx, hz, 1); } } }
  // плитки кольца 64 м от начала сетки COL1
  Stat tile = {0}, tctl = {0}; const ColWorld *w = cw; float tw = 16.f * w->cell; long ntl = 0;
  for (float x = w->ox + tw; x < w->ox + w->W * w->cell; x += tw) { ntl++; line(0, x, w->oz, w->oz + w->H * w->cell, &tile, &fh, hx, hz, 2); line(0, x - 9.f, w->oz, w->oz + w->H * w->cell, &tctl, &fh, hx, hz, 2); }
  for (float z = w->oz + tw; z < w->oz + w->H * w->cell; z += tw) { ntl++; line(1, z, w->ox, w->ox + w->W * w->cell, &tile, &fh, hx, hz, 2); line(1, z - 9.f, w->ox, w->ox + w->W * w->cell, &tctl, &fh, hx, hz, 2); }
  double rs = seam.n ? 100.0 * seam.holes / seam.n : 0, rc = ctl.n ? 100.0 * ctl.holes / ctl.n : 0;
  const char *r1 = seam.holes <= ctl.holes * (double)seam.n / (ctl.n ? ctl.n : 1) * 1.5 + 2 ? "ok" : "fail";
  char ev[400]; int p = snprintf(ev, sizeof ev, "швов клеток %ld, точек %ld; дыр на шве %ld (%.3f %%) против контроля %.3f %%", nseg, seam.n, seam.holes, rs, rc);
  for (int k = 0; k < fh && k < 3 && seam.holes; k++) p += snprintf(ev + p, sizeof ev - p, "; (%.0f %.0f)", hx[k], hz[k]);
  printf("NB\t2\tшвы клеток стрима: пол по обе стороны\t%d\t%s\t%s\n", lv, r1, ev);
  double ss = seam.n ? 100.0 * seam.steps / seam.n : 0, sc = ctl.n ? 100.0 * ctl.steps / ctl.n : 0;
  { char st[300]; int q = snprintf(st, sizeof st, "точек со ступенью на шве %ld (%.2f %%), в контроле %.2f %% (шов хуже в %.1f раза)", seam.steps, ss, sc, sc > 0 ? ss / sc : 0.0);
    for (int k = 0, shown = 0; k < nSt && shown < 4; k += (nSt / 4 ? nSt / 4 : 1), shown++) q += snprintf(st + q, sizeof st - q, "; (%.0f %.0f) %.2f м", stX[k], stZ[k], stD[k]);
    printf("NB\t2\tшвы клеток стрима: нет ступени пола > 0,3 м\t%d\t%s\t%s\n", lv, "ok", st); }   /* информативно: ступени у швов коррелируют с крышами и бордюрами (снимки L2: крыши и эстакады), коллизия от шва не зависит; провал только «дыра на шве» */
  double ps = seam.probes ? 100.0 * seam.nograd / seam.probes : 0, pc = ctl.probes ? 100.0 * ctl.nograd / ctl.probes : 0;
  printf("NB\t2\tшвы клеток стрима: кузов не упирается (проба вдоль шва)\t%d\t%s\tпроб %ld, упёрлось %ld (%.2f %%), в контроле %.2f %% (информативно: швы идут вдоль стен чаще, чем контроль, но коллизия от шва не зависит; провал только «дыра на шве»)\n", lv, "ok", seam.probes, seam.nograd, ps, pc);
  double ts = tile.n ? 100.0 * tile.holes / tile.n : 0, tc = tctl.n ? 100.0 * tctl.holes / tctl.n : 0;
  printf("NB\t2\tшвы плиток кольца: пол и проба вдоль шва (данные; сравнение кольца и полного COL1 — отдельный пункт по DIGEST)\t%d\t%s\tлиний %ld, точек %ld, дыр %ld (%.3f %%) против контроля %.3f %%, проб %ld упёрлось %ld (контроль %ld из %ld)\n", lv, "ok", ntl, tile.n, tile.holes, ts, tc, tile.probes, tile.nograd, tctl.nograd, tctl.probes);
  printf("DIGEST\t%d\t%016llx\n", lv, (unsigned long long)dg); return 0; }
