// Модели-96, позиции вершин f32 -> s16: хост-проверка НАСТОЯЩИМ читателем (viewer/skin.c, traffic.c).
//   cmp skin    OLD.shr NEW.shr     одна модель (игрок, персонаж миссии)
//   cmp traffic OLD.shr NEW.shr     пакет пешеходов (все модели)
// Для каждой модели: каждая пачка проходит skinBatchValid; число вершин, костей и текстура те же; позиция s16 в диапазоне и масштаб — степень двойки;
// затем СКИНОВАННЫЕ позиции (так их увидит GE: сумма вес × палитра кости × позиция из вершины) и нормали (по направлению) старой и новой модели
// в позе покоя и в 4 отсчётах каждого клипа — разница в мм и градусах. Пороги: 0,5 мм и 1°.
// Красный контроль: POS16_RED=1 умножает позиции новой модели на 1,01; POS16_RED=2 не делит нормаль на F (оба обязаны упасть).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "traffic.h"
#include "homer.h"
int (*sceneReadyHook)(const ColWorld *w, float x, float z, int *failed);   // заглушка: сцена и сторож в проверке не участвуют
#ifndef POS16_TOL
#define POS16_TOL 1.0
#endif
static unsigned char *rd(const char *p, unsigned *sz) {
  FILE *g = fopen(p, "rb"); if (!g) return 0; fseek(g, 0, SEEK_END); *sz = (unsigned)ftell(g); fseek(g, 0, SEEK_SET);
  unsigned char *b = aligned_alloc(64, (*sz + 63) & ~63u); if (fread(b, 1, *sz, g) != *sz) return 0; fclose(g); return b;
}
static int al(int o, int a) { return (o + a - 1) & ~(a - 1); }
typedef struct { float p[3], n[3], len; } Sv;
// Все вершины полос модели (после раскрытия индексов), позиция и нормаль скинованы палитрой pal ТАК, КАК ЭТО ДЕЛАЕТ GE: s16 читается как целое / 32768 (позиция и нормаль),
// s8 нормаль — / 128, float как есть; нормаль складывается без нормирования (длину сохраняем: проверка, что новая модель не растягивает свет)
static void skinned(const SkinModel *m, const ScePspFMatrix4 *pal, Sv *out, int red) {
  unsigned o = 0;
  for (unsigned bi = 0; bi < m->h->nBatches; bi++) {
    const SkinBatch *b = &m->b[bi]; int pf = (b->vtype >> 7) & 3, nf = (b->vtype >> 5) & 3, nb = b->nb, uvo = al(nb, 2), nro = uvo + 4, nsz = nf == 2 ? 6 : 3;
    int poo = pf == 2 ? al(nro + nsz, 2) : al(nro + nsz, 4);
    for (unsigned i = 0; i < b->nverts; i++, o++) {
      unsigned k = (b->flags & 4) ? ((const unsigned short *)(m->base + b->pad[0]))[i] : i;
      const unsigned char *v = m->base + b->offVerts + (size_t)k * b->stride; float p[3], n[3];
      if (pf == 2) { short q[3]; memcpy(q, v + poo, 6); for (int c = 0; c < 3; c++) p[c] = q[c] / 32768.f; } else memcpy(p, v + poo, 12);
      if (red == 1 && pf == 2) for (int c = 0; c < 3; c++) p[c] *= 1.01f;
      if (nf == 2) { short q[3]; memcpy(q, v + nro, 6); for (int c = 0; c < 3; c++) n[c] = q[c] / 32768.f; }
      else { const signed char *nn = (const signed char *)(v + nro); for (int c = 0; c < 3; c++) n[c] = nn[c] / 128.f; }
      if (red == 2 && nf == 2) for (int c = 0; c < 3; c++) n[c] *= skinPosGE(m->h);   // красный 2: нормаль без деления на F (забытая компенсация) — разница нормалей обязана вылезти
      double P[3] = {0, 0, 0}, N[3] = {0, 0, 0};
      for (int w = 0; w < nb; w++) {
        float wt = v[w] / 128.f; const float *M = (const float *)&pal[b->bones[w]];
        for (int r = 0; r < 3; r++) {
          P[r] += wt * (M[r] * p[0] + M[4 + r] * p[1] + M[8 + r] * p[2] + M[12 + r]);
          N[r] += wt * (M[r] * n[0] + M[4 + r] * n[1] + M[8 + r] * n[2]);
        }
      }
      double l = sqrt(N[0] * N[0] + N[1] * N[1] + N[2] * N[2]);
      out[o].len = (float)l; for (int r = 0; r < 3; r++) { out[o].p[r] = (float)P[r]; out[o].n[r] = (float)N[r]; }   // нормаль без нормирования: как её получает GE после кости
    }
  }
}
static unsigned totalLogical(const SkinModel *m) { unsigned t = 0; for (unsigned i = 0; i < m->h->nBatches; i++) t += m->b[i].nverts; return t; }
static const unsigned char *m_off(const SkinModel *m, const SkinBatch *b, unsigned v, int off) { return m->base + b->offVerts + (size_t)v * b->stride + off; }
static int pow2(float s) { int e; float f = frexpf(s, &e); return f == 0.5f; }
typedef struct { double pos, nrm, len; unsigned poses, verts, s16, f32, bad; } Acc;
static int cmpModel(const SkinModel *a, const SkinModel *b, Acc *A, int red, const char *tag) {
  int bad = 0;
  if (a->h->nBatches != b->h->nBatches || a->h->nJoints != b->h->nJoints || a->h->nClips != b->h->nClips) { printf("FAIL %s: состав модели\n", tag); return 1; }
  float S = skinPosScale(b->h);
  for (unsigned i = 0; i < b->h->nBatches; i++) {
    const SkinBatch *x = &a->b[i], *y = &b->b[i]; int pf = (y->vtype >> 7) & 3;
    if (!skinBatchValid(b, y)) { printf("FAIL %s пачка %u: skinBatchValid\n", tag, i); bad++; continue; }
    if (x->nverts != y->nverts || x->nb != y->nb || x->tex != y->tex || memcmp(x->bones, y->bones, 8)) { printf("FAIL %s пачка %u: поля\n", tag, i); bad++; continue; }
    if (pf == 2) {
      A->s16++;
      if (!(S > 0.f) || !pow2(S) || !isfinite(S)) { printf("FAIL %s: масштаб %g не степень двойки\n", tag, S); bad++; break; }
      int nb = y->nb, poo = al(al(nb, 2) + 4 + 6, 2);
      if (((y->vtype >> 5) & 3) != 2) { printf("FAIL %s пачка %u: нормаль не s16 при позиции s16\n", tag, i); bad++; continue; }
      if (y->stride != (unsigned)al(poo + 6, 2)) { printf("FAIL %s пачка %u: шаг %u\n", tag, i, y->stride); bad++; continue; }
      unsigned sv = skinBatchStoredVerts(y);
      for (unsigned v = 0; v < sv; v++) { short q[3]; memcpy(q, m_off(b, y, v, poo), 6); if (q[0] == -32768 || q[1] == -32768 || q[2] == -32768 || abs(q[0]) > 32700 || abs(q[1]) > 32700 || abs(q[2]) > 32700) { printf("FAIL %s пачка %u: позиция у края диапазона s16\n", tag, i); bad++; break; } }
    } else A->f32++;
  }
  if (bad) { A->bad += bad; return bad; }
  unsigned nv = totalLogical(a); Sv *sa = malloc(nv * sizeof *sa), *sb = malloc(nv * sizeof *sb);
  ScePspFMatrix4 pa[SKIN_MAXJ] __attribute__((aligned(16))), pb[SKIN_MAXJ] __attribute__((aligned(16))), wd[SKIN_MAXJ] __attribute__((aligned(16)));
  SkinXf xa[SKIN_MAXJ], xb[SKIN_MAXJ]; unsigned nj = b->h->nJoints;
  for (unsigned c = 0; c <= b->h->nClips; c++) {
    unsigned ns = c == b->h->nClips ? 1 : 4;
    for (unsigned s = 0; s < ns; s++) {
      if (c == b->h->nClips) for (unsigned j = 0; j < nj; j++) { memcpy(xa[j].q, a->j[j].q, 16); memcpy(xa[j].t, a->j[j].t, 12); xb[j] = xa[j]; }   // поза покоя
      else { float t = b->c[c].length * s / 3.f; skinSample(a, (int)c, t, xa); skinSample(b, (int)c, t, xb); }
      skinPalette(a, xa, pa, wd); skinPalette(b, xb, pb, wd); skinned(a, pa, sa, 0); skinned(b, pb, sb, red);
      A->poses++;
      for (unsigned v = 0; v < nv; v++) {
        double dx = sa[v].p[0] - sb[v].p[0], dy = sa[v].p[1] - sb[v].p[1], dz = sa[v].p[2] - sb[v].p[2], d = sqrt(dx * dx + dy * dy + dz * dz) * 1000.0;
        double la = sa[v].len, lb = sb[v].len, ex = sa[v].n[0] - sb[v].n[0], ey = sa[v].n[1] - sb[v].n[1], ez = sa[v].n[2] - sb[v].n[2];
        double dvec = sqrt(ex * ex + ey * ey + ez * ez) * 100.0;               // разница нормалей как векторов (без нормирования), % единичной длины: ловит и поворот, и растяжение
        double dn = (sa[v].n[0] * sb[v].n[0] + sa[v].n[1] * sb[v].n[1] + sa[v].n[2] * sb[v].n[2]) / (la * lb > 1e-12 ? la * lb : 1); if (dn > 1) dn = 1;
        double ang = (la < .5 || lb < .5) ? 0.0 : acos(dn) * 180 / M_PI;      // угол — только у нормалей не короче 0,5 (у короче старая s8 грубее самой разницы: 1/128 на компоненту)
        double lr = dvec;
        if (d > A->pos) A->pos = d; if (ang > A->nrm) A->nrm = ang; if (lr > A->len) A->len = lr;
      }
    }
  }
  A->verts += nv; free(sa); free(sb); return 0;
}
int main(int argc, char **argv) {
  if (argc < 4) return 2;
  int red = getenv("POS16_RED") ? atoi(getenv("POS16_RED")) : 0; Acc A; memset(&A, 0, sizeof A); int models = 0, bad = 0;
  unsigned s1, s2; unsigned char *x = rd(argv[2], &s1), *y = rd(argv[3], &s2); if (!x || !y) { puts("нет файла"); return 2; }
  if (!strcmp(argv[1], "skin")) {
    SkinModel *a = skinLoad(argv[2]), *b = skinLoad(argv[3]); if (!a || !b) { printf("FAIL skinLoad %p %p\n", (void *)a, (void *)b); return 1; }
    bad += cmpModel(a, b, &A, red, "model"); models = 1;
  } else {
    static Traffic TA, TB; int r1 = trafficInitMem(&TA, x, s1, 0), r2 = trafficInitMem(&TB, y, s2, 0);
    if (r1 || r2) { printf("FAIL trafficInitMem %d %d\n", r1, r2); return 1; }
    if (TA.npeds != TB.npeds || !TA.npeds) { printf("FAIL число пешеходов %d / %d\n", TA.npeds, TB.npeds); return 1; }
    for (int i = 0; i < TA.npeds; i++) { char tag[32]; snprintf(tag, sizeof tag, "ped%d", i); bad += cmpModel(&TA.peds[i], &TB.peds[i], &A, red, tag); models++; }
  }
  double tp = 0.5 * POS16_TOL, tn = 1.0 * POS16_TOL, tl = 2.5 * POS16_TOL; int ok = !bad && !A.bad && A.pos <= tp && A.nrm <= tn && A.len <= tl;
  printf("моделей %d (пачек s16 %u, f32 %u), поз %u, вершин %u; файл %u -> %u Б; макс. сдвиг позиции %.3f мм (порог %.3f), нормали %.3f° (порог %.3f), разница нормалей как векторов %.3f %% (порог %.1f), нарушений %d\n",
         models, A.s16, A.f32, A.poses, A.verts, s1, s2, A.pos, tp, A.nrm, tn, A.len, tl, bad);
  puts(ok ? "PASS" : "FAIL"); return ok ? 0 : 1;
}
