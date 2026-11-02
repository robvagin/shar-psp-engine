// №1140/1153 (g11): коллизия без видимой геометрии — ящики, цилиндры и звенья заборов COL1, рядом с которыми (в 1 м) нет ни одного куска рисования города level.shr.
// Хост: cc -std=c11 -O2 -DSKIN_HOST -I viewer tools/ghostwall/audit.c viewer/col.c -lm -o audit;  audit level.shr -> строки «box|cyl|seg i x y z hx hy hz расстояние_до_ближайшего_куска».
// Видимым считается кусок рисования города (не небо, не плоский: высота ящика не меньше MINH) на расстоянии до R от объёма (по умолчанию 1 м). Список — места, дальше сверка с оригиналом (бывают специально: невидимые линии заборов дизайнеров) и отбор лишнего (tools/ghostwall1153.py).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
typedef struct { float bb[6]; int tex, flags, nv, ni, owner; float ox, oy, oz, sc; unsigned off, nf, cell, pad[2]; } Rec;
static ColWorld world;
#define MINH 0.5f
#define MAXE 14.f   // м: куски крупнее по любой стороне (крыши, склоны) закрывают ящик на ровном месте, а предмета не «видят»   // м: ниже — плоский кусок земли, не вид предмета
// расстояние от ящика [lo,hi] до ящика куска (0 — пересекаются)
static float gap(const float *a, const float *b) { float d2 = 0; for (int k = 0; k < 3; k++) { float g = fmaxf(a[k] - b[k + 3], b[k] - a[k + 3]); if (g > 0) d2 += g * g; } return sqrtf(d2); }
int main(int argc, char **argv) {
  if (argc < 2) return 2; float R = argc > 2 ? (float)atof(argv[2]) : 1.f;
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *d = aligned_alloc(16, (size + 15) & ~15); if (fread(d, 1, size, f) != (size_t)size) return 2; fclose(f);
  unsigned np = *(unsigned *)(d + 8), tabOff = *(unsigned *)(d + 76), co = *(unsigned *)(d + 92), po = *(unsigned *)(d + 96), pv = *(unsigned *)(d + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&world, d + co, ce - co)) return 3;
  const Rec *P = (const Rec *)(d + tabOff); unsigned nv = 0;
  float (*B)[6] = malloc(sizeof(float[6]) * (np ? np : 1));
  for (unsigned i = 0; i < np; i++) { if (P[i].owner != 0 || (P[i].flags & 4) || P[i].ni <= 0) continue; if (P[i].bb[4] - P[i].bb[1] < MINH || P[i].bb[3] - P[i].bb[0] > MAXE || P[i].bb[5] - P[i].bb[2] > MAXE) continue; /* плоские куски (дорога, газон) — не «вид» стены или забора */ memcpy(B[nv++], P[i].bb, sizeof B[0]); }
  printf("# pieces %u visible %u box %u cyl %u seg %u\n", np, nv, world.nbox, world.ncyl, world.nseg);
  for (unsigned i = 0; i < world.nbox; i++) { const ColBox *b = &world.box[i]; float e[3]; for (int k = 0; k < 3; k++) e[k] = fabsf(b->ax[k]) * b->h[0] + fabsf(b->ay[k]) * b->h[1] + fabsf(b->az[k]) * b->h[2];
    float v[6] = {b->c[0] - e[0], b->c[1] - e[1], b->c[2] - e[2], b->c[0] + e[0], b->c[1] + e[1], b->c[2] + e[2]}; float best = 1e9f; for (unsigned q = 0; q < nv; q++) { float g = gap(v, B[q]); if (g < best) best = g; }
    if (best > R) printf("box %u %.2f %.2f %.2f %.2f %.2f %.2f %.1f%s\n", i, b->c[0], b->c[1], b->c[2], e[0], e[1], e[2], best, b->pad > .5f ? " foliage" : ""); }
  for (unsigned i = 0; i < world.ncyl; i++) { const ColCyl *c = &world.cyl[i]; if (c->pad[0] == 1.f) continue;   // листва — рисуется как куст, отдельно
    float ex = fabsf(c->axis[0]) * c->halfLen + c->r, ey = fabsf(c->axis[1]) * c->halfLen + c->r, ez = fabsf(c->axis[2]) * c->halfLen + c->r;
    float v[6] = {c->c[0] - ex, c->c[1] - ey, c->c[2] - ez, c->c[0] + ex, c->c[1] + ey, c->c[2] + ez}; float best = 1e9f; for (unsigned q = 0; q < nv; q++) { float g = gap(v, B[q]); if (g < best) best = g; }
    if (best > R) printf("cyl %u %.2f %.2f %.2f %.2f %.2f %.2f %.1f\n", i, c->c[0], c->c[1], c->c[2], ex, ey, ez, best); }
  for (unsigned i = 0; i < world.nseg; i++) { const ColSeg *s = &world.seg[i]; float mx = (s->a[0] + s->b[0]) * .5f, mz = (s->a[1] + s->b[1]) * .5f, g = 0;
    if (!colGround(&world, mx, mz, 80.f, -80.f, &g, 0)) continue;
    float v[6] = {fminf(s->a[0], s->b[0]), g - 1.f, fminf(s->a[1], s->b[1]), fmaxf(s->a[0], s->b[0]), g + 3.f, fmaxf(s->a[1], s->b[1])}; float best = 1e9f; for (unsigned q = 0; q < nv; q++) { float gp = gap(v, B[q]); if (gp < best) best = gp; }
    if (best > R) printf("seg %u %.2f %.2f %.2f %.2f %.2f %.2f %.1f%s\n", i, mx, g, mz, fabsf(s->b[0] - s->a[0]) * .5f, 0.f, fabsf(s->b[1] - s->a[1]) * .5f, best, s->y0 < COL_SEG_FOL_Y0 * .5f ? " foliage" : ""); }
  return 0;
}
