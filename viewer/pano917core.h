// №917 (слово Роба: «боты-репортёры телепортом по дальним местам снимают даль и пекут картинки-подставы»): чистая часть кольца-панорамы — без заголовков PSP, её же гоняет tests/horizon/pano917.sh.
// В зонах 250 м (hzzone913.h) город рисуется до 150 м, а слой 150–250 м вместо геометрии даёт кольцо: 16 плоских сегментов по 22,5° вокруг центра клетки, текстура CLUT4,
// снятая ботом на Маке (tools/pano917/, окно «Контур»). Центры панорам — центры клеток зоны. Смена панорам между клетками: две ближайшие, доля второй растёт от 0 до 1/2 на подходе к границе
// клеток (середина между центрами), у центра клетки — одна панорама целиком (слово дирижёра 24.10: плавная смена двух); за краем сетки кольцо гаснет по расстоянию до ближайшего центра.
#pragma once
#include <math.h>
#ifndef PNO_DFULL
#define PNO_DFULL 48.f   // м: до этого расстояния от ближайшего центра панорама в полную силу (клетка 60 м: полудиагональ 42)
#endif
#ifndef PNO_DFADE
#define PNO_DFADE 96.f   // м: дальше вес 0 (кольцо гаснет, как гасла бы даль 250 м за границей зоны)
#endif
#ifndef PNO_DPRE
#define PNO_DPRE 120.f   // м: панорамы в этом радиусе подгружаются заранее
#endif
#ifndef PNO_MIXW
#define PNO_MIXW 4.f   // м: вторая панорама подмешивается, только когда её центр дальше ближайшего меньше чем на столько (граница клеток ± 5 м); дальше — ближайшая целиком (правило Контура, замер 24.10)
#endif
static float pnoMixW = PNO_MIXW;   // м: окно смешивания (flag pano917mix N; 10 — прежнее, при нём на склонах виден призрак у двух панорам)
#define PNO_SLOTS 3
#define PNO_NAZ 16
typedef struct { float x, z, ey, lo, hi; unsigned off, size, rsv; } PnoE;
static inline float pno917Smooth(float a, float b, float x) { float t = (x - a) / (b - a); t = t < 0.f ? 0.f : t > 1.f ? 1.f : t; return t * t * (3.f - 2.f * t); }
// из n записей выбирает до PNO_SLOTS ближайших в радиусе PNO_DPRE (ids[] по возрастанию расстояния, *pre — сколько) и доли двух первых f[0..1] (сумма ≤ 1); возвращает, сколько слоёв рисовать (0..2)
static inline int pno917Pick(const PnoE *e, unsigned n, float ex, float ez, int *ids, float *f, int *pre) {
  float d2[PNO_SLOTS]; int m = 0; *pre = 0; f[0] = f[1] = 0.f;
  for (unsigned i = 0; i < n; i++) {
    float dx = e[i].x - ex, dz = e[i].z - ez, d = dx * dx + dz * dz;
    if (d >= PNO_DPRE * PNO_DPRE) continue;
    int p = m < PNO_SLOTS ? m : PNO_SLOTS;
    while (p > 0 && d2[p - 1] > d) { if (p < PNO_SLOTS) { d2[p] = d2[p - 1]; ids[p] = ids[p - 1]; } p--; }
    if (p < PNO_SLOTS) { d2[p] = d; ids[p] = (int)i; if (m < PNO_SLOTS) m++; }
  }
  *pre = m; if (!m) return 0;
  float d1 = sqrtf(d2[0]), cov = (PNO_DFADE - d1) / (PNO_DFADE - PNO_DFULL); cov = cov >= 1.f ? 1.f : cov < 0.f ? 0.f : cov;
  float s2 = 0.f; if (m >= 2) { float t = (d1 + pnoMixW - sqrtf(d2[1])) / pnoMixW; s2 = .5f * (t >= 1.f ? 1.f : t > 0.f ? t : 0.f); }   // на равном расстоянии 0,5/0,5, вне полосы — одна панорама
  f[0] = (1.f - s2) * cov; f[1] = s2 * cov;
  return f[1] > 1e-3f ? 2 : f[0] > 1e-3f ? 1 : 0;
}
// непрозрачность слоя при наложении «поверх» в порядке 0..c−1: итоговая доля слоя k = a[k]·Π(1−a[j>k]); задана доля f[k] (сумма ≤ 1)
static inline void pno917Alphas(const float *f, int c, float *a) {
  float tail = 0.f;
  for (int k = c - 1; k >= 0; k--) { float rest = 1.f - tail; a[k] = rest > 1e-4f ? f[k] / rest : 0.f; if (a[k] > 1.f) a[k] = 1.f; tail += f[k]; }
}
