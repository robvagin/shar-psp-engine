// №1185 (Роб 04.11: «так как это место меньше, чем сама карта, нужно придумать секторы более конкретные»): код места на трассе бонус-игры для правок по месту.
// Трасса режется по ходу ПУТИ, КОТОРЫМ ЕДУТ СОПЕРНИКИ (не по BnsPath: те точки старого правила нав. и лежат мимо дороги): маршрут по полосам дорог трассы (BRD1) от точки WPL/WPR к следующей,
// все участки круга подряд, начало — у линии старта (ближайшая точка круга к первой точке пути BnsPath, x≈0, z≈40), — на равные доли ≈ BSC_STEP метров: ровно ceil(длина / 5 м) секторов,
// номер 01…NN растёт по ходу круга от начала пути (у старта) к линии финиша. Точка трассы -> ближайший отрезок пути (по x, z) -> длина пути до проекции -> номер сектора.
// Нумерация одна на трассу и не зависит от направления «CW/CCW» (всегда по прямому ходу), поэтому у одного места один код. Код: «B4-12» (B — бонус, 4 — трасса, 12 — сектор),
// как «L1 J4-5» основной игры (дефис, потому что в шрифте HUD нет средней точки). Чистый C99 без PSP-заголовков: один код для EBOOT (viewer/bonus_play.h) и гейта на Маке
// (tests/bonus1167/sector_host.c). Показ — служебная строка R+SELECT во время гонки. BONUSSECTOR1185_OLD=1 — кода нет
#ifndef SHAR_BNSSECTOR_H
#define SHAR_BNSSECTOR_H
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bonus.h"
#include "msncar.h"   // mcPlan: тот же планировщик по полосам, что ведёт соперников

#ifndef BONUSSECTOR1185_OLD
#define BONUSSECTOR1185_OLD 0
#endif
#ifndef BSC_DY
#define BSC_DY 2.5f   // м: разница высот, выше которой путь считается другим этажом (машина стоит на 0,5 м выше точки пути, склоны меньше)
#endif
#ifndef BSC_SHORTCUTS
#define BSC_SHORTCUTS 1   // как водитель соперников (msncar.c replan: срезы разрешены, пока в данных нет запрета)
#endif
#ifndef BSC_REPLAN_M
#define BSC_REPLAN_M 14.f   // м вдоль плана до очередной перепланировки
#endif
#ifndef BSC_STEP
#define BSC_STEP 5.f
#endif
#define BSC_MAXP 1100
typedef struct { float q[BSC_MAXP][3]; int n; float cum[BSC_MAXP]; float len, step; int nsec; } BscMap;   // q — круг по дорогам от линии старта до неё же

// ГОТОВАЯ ЛИНИЯ (основной путь): файл bonus/bNN.sec — след круга лидера гонки полигона (tools/bonus/race_host --sec-out: настоящие модули движка, те же дороги и водитель, шаг 0,1 с),
// от линии старта до финиша; формат: 'SEC1', u32 n, n × (x, y, z float). Так линия лежит там, где соперники едут на самом деле (повороты через центр, объезды), а не по прямым между точками
static inline int bscFinish(BscMap *m);
static inline int bscInitPoints(BscMap *m, const float (*pts)[3], int n) {
  if (!pts || n < 2 || n > BSC_MAXP) { m->n = m->nsec = 0; return -5; }
  memcpy(m->q, pts, (size_t)n * 12); m->n = n; return bscFinish(m);
}
static inline int bscLoad(BscMap *m, const void *data, unsigned size) {   // файл .sec целиком в памяти -> линия секторов; 0 — ок
  const unsigned char *d = (const unsigned char *)data; unsigned n;
  if (!d || size < 8 || memcmp(d, "SEC1", 4)) return -6;
  memcpy(&n, d + 4, 4); if (n < 2 || n > BSC_MAXP || size < 8u + n * 12u) return -7;
  memcpy(m->q, d + 8, (size_t)n * 12); m->n = (int)n; return bscFinish(m);
}
static inline int bscFinish(BscMap *m) {   // длины и доли по готовой ломаной m->q[0..n)
  m->len = m->step = 0.f; m->nsec = 0; if (m->n < 2) { m->n = 0; return -1; }
  m->cum[0] = 0.f;
  for (int i = 1; i < m->n; i++) { float dx = m->q[i][0] - m->q[i - 1][0], dz = m->q[i][2] - m->q[i - 1][2]; m->cum[i] = m->cum[i - 1] + sqrtf(dx * dx + dz * dz); }
  m->len = m->cum[m->n - 1]; if (m->len < 1.f) { m->n = 0; return -2; }
  m->nsec = (int)ceilf(m->len / BSC_STEP - 1e-4f); if (m->nsec < 1) m->nsec = 1;
  m->step = m->len / (float)m->nsec; return 0;
}
// путь круга: маршрут по полосам дорог tr между точками WPL/WPR w[0..nw) по кругу; начало — ближайшая к origin точка круга. 0 — ок, < 0 — дорог/точек нет
static inline int bscPlanBasic(void *u, const float pos[3], float hdg, const float g[3], float (*out)[3], int cap) {   // только дороги (без мира и состояния водителя): для проверок геометрии
  int off = 0; return mcPlan((const Traffic *)u, pos[0], pos[1], pos[2], hdg, g, BSC_SHORTCUTS, out, cap, &off);
}
typedef int (*BscPlan)(void *u, const float pos[3], float hdg, const float g[3], float (*out)[3], int cap);   // план маршрута от места и курса к точке g: число точек
static inline int bscInitRoute(BscMap *m, BscPlan plan, void *pu, const BnsWp *w, int nw, const float origin[3]) {
  static float cyc[BSC_MAXP][3]; int nc = 0; m->n = 0; m->nsec = 0;
  if (!plan || !w || nw < 2) return -3;
  // «виртуальный проезд»: машина без физики стоит у последней точки пути и на каждом шаге перепланирует маршрут к следующей точке WPL/WPR так же, как водитель соперников (план от текущего места
  // и курса), проходит по плану не больше BSC_REPLAN_M и снова планирует — получается путь, которым ведёт планировщик, а не прямые между точками
  float pos[3]; memcpy(pos, w[nw - 1].p, 12); float hdg = atan2f(w[0].p[0] - pos[0], w[0].p[2] - pos[2]); memcpy(cyc[nc++], pos, 12);
  for (int k = 0; k < nw && nc < BSC_MAXP - 40; k++) {
    const float *g = w[k].p;
    for (int it = 0; it < 80 && nc < BSC_MAXP - 40; it++) {
      float dx = g[0] - pos[0], dz = g[2] - pos[2]; if (dx * dx + dz * dz < 6.f * 6.f) break;   // точка взята
      float r[MC_ROUTE][3]; int n = plan(pu, pos, hdg, g, r, MC_ROUTE);
      if (n < 1) { memcpy(pos, g, 12); memcpy(cyc[nc++], pos, 12); break; }   // дороги не дали плана — прямо к точке
      float run = 0.f; int adv = 0;
      for (int q = 0; q < n && run < BSC_REPLAN_M; q++) { float ex = r[q][0] - pos[0], ez = r[q][2] - pos[2], l = sqrtf(ex * ex + ez * ez); if (l < .3f) continue;
        run += l; hdg = atan2f(ex, ez); memcpy(pos, r[q], 12); memcpy(cyc[nc++], pos, 12); adv++; if (nc >= BSC_MAXP - 40) break; }
      if (!adv) { memcpy(pos, g, 12); memcpy(cyc[nc++], pos, 12); break; }
    }
  }
  { float d2 = (cyc[nc - 1][0] - cyc[0][0]) * (cyc[nc - 1][0] - cyc[0][0]) + (cyc[nc - 1][2] - cyc[0][2]) * (cyc[nc - 1][2] - cyc[0][2]); if (d2 > .01f) memcpy(cyc[nc++], cyc[0], 12); }   // замкнуть круг
  if (nc < 3) return -4;
  // точка круга, ближайшая к origin (проекция на отрезок), — начало отсчёта
  int bi = 0; float bt = 0.f, bd = 1e30f;
  for (int i = 0; i + 1 < nc; i++) { float ex = cyc[i + 1][0] - cyc[i][0], ez = cyc[i + 1][2] - cyc[i][2], l2 = ex * ex + ez * ez, t = l2 > 1e-6f ? ((origin[0] - cyc[i][0]) * ex + (origin[2] - cyc[i][2]) * ez) / l2 : 0.f;
    t = t < 0.f ? 0.f : t > 1.f ? 1.f : t; float px = cyc[i][0] + ex * t - origin[0], pz = cyc[i][2] + ez * t - origin[2], d2 = px * px + pz * pz; if (d2 < bd) { bd = d2; bi = i; bt = t; } }
  int n = 0; float s0[3]; for (int j = 0; j < 3; j++) s0[j] = cyc[bi][j] + (cyc[bi + 1][j] - cyc[bi][j]) * bt;
  memcpy(m->q[n++], s0, 12);
  for (int c = 1; c < nc && n < BSC_MAXP - 2; c++) { int i = (bi + c) % (nc - 1); memcpy(m->q[n++], cyc[i], 12); }
  memcpy(m->q[n++], s0, 12); m->n = n;
  return bscFinish(m);
}
// длина пути до проекции точки (x, z) на путь, м; *d — расстояние до пути
static inline float bscArc(const BscMap *m, float x, float y, float z, float *d) {
  float bd = 1e30f, bs = 0.f;
  for (int i = 0; i + 1 < m->n; i++) {
    float ax = m->q[i][0], az = m->q[i][2], bx = m->q[i + 1][0], bz = m->q[i + 1][2], ex = bx - ax, ez = bz - az, l2 = ex * ex + ez * ez;
    float t = l2 > 1e-6f ? ((x - ax) * ex + (z - az) * ez) / l2 : 0.f; t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
    float px = ax + ex * t - x, pz = az + ez * t - z, d2 = px * px + pz * pz;
    float py = m->q[i][1] + (m->q[i + 1][1] - m->q[i][1]) * t - y; if (fabsf(py) > BSC_DY) d2 += py * py;   // эстакада над самой собой: другой этаж (разница выше BSC_DY) — дальше, чем тот же уровень
    if (d2 < bd) { bd = d2; bs = m->cum[i] + (m->cum[i + 1] - m->cum[i]) * t; }   // при равенстве — более ранний отрезок: ответ однозначен
  }
  if (d) *d = sqrtf(bd);
  return bs;
}
// то же, но только отрезки пути в окне [sPrev - back, sPrev + fwd]: водитель за игрока не перескакивает на другой участок, где путь проходит рядом сам с собой (эстакады, соседние полосы)
static inline float bscArcNear(const BscMap *m, float x, float y, float z, float sPrev, float back, float fwd, float *d) {
  float bd = 1e30f, bs = sPrev;
  for (int i = 0; i + 1 < m->n; i++) {
    if (m->cum[i + 1] < sPrev - back || m->cum[i] > sPrev + fwd) continue;
    float ax = m->q[i][0], az = m->q[i][2], bx = m->q[i + 1][0], bz = m->q[i + 1][2], ex = bx - ax, ez = bz - az, l2 = ex * ex + ez * ez;
    float t = l2 > 1e-6f ? ((x - ax) * ex + (z - az) * ez) / l2 : 0.f; t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
    float px = ax + ex * t - x, pz = az + ez * t - z, d2 = px * px + pz * pz;
    float py = m->q[i][1] + (m->q[i + 1][1] - m->q[i][1]) * t - y; if (fabsf(py) > BSC_DY) d2 += py * py;
    if (d2 < bd) { bd = d2; bs = m->cum[i] + (m->cum[i + 1] - m->cum[i]) * t; }
  }
  if (d) *d = sqrtf(bd);
  return bs;
}
// номер сектора 1…nsec (0 — пути нет)
static inline int bscSector(const BscMap *m, float x, float y, float z) {
  if (m->nsec < 1) return 0;
  int k = (int)floorf(bscArc(m, x, y, z, 0) / m->step) + 1;
#ifdef BSC_BREAK   // красный контроль гейта: номер по прямой от первой точки пути (не по ходу пути) — соседние по ходу сектора перестают быть соседними
  { float dx = x - m->q[0][0], dz = z - m->q[0][2]; (void)y; k = (int)floorf(sqrtf(dx * dx + dz * dz) / m->step) + 1; }
#endif
  return k < 1 ? 1 : k > m->nsec ? m->nsec : k;
}
static inline void bscCode(const BscMap *m, int track, float x, float y, float z, char *out, int n) {
  if (!(x == x) || !(z == z) || m->nsec < 1) { snprintf(out, (size_t)n, "B%d--", track); return; }
  snprintf(out, (size_t)n, "B%d-%02d", track, bscSector(m, x, y, z));
}
// точка пути на длине s (0…len), м, и направление пути там
static inline void bscPointAt(const BscMap *m, float s, float p[3], float dir[2]) {
  s = s < 0.f ? 0.f : s > m->len ? m->len : s; int i = 0; while (i + 2 < m->n && m->cum[i + 1] < s) i++;
  float seg = m->cum[i + 1] - m->cum[i], t = seg > 1e-6f ? (s - m->cum[i]) / seg : 0.f; t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
  for (int j = 0; j < 3; j++) p[j] = m->q[i][j] + (m->q[i + 1][j] - m->q[i][j]) * t;
  float ex = m->q[i + 1][0] - m->q[i][0], ez = m->q[i + 1][2] - m->q[i][2], l = sqrtf(ex * ex + ez * ez); if (l < 1e-6f) l = 1.f;
  dir[0] = ex / l; dir[1] = ez / l;
}
// граница сектора k (1…nsec+1): точка пути на длине (k-1)·step и направление пути там (для листа-карты)
static inline void bscBoundary(const BscMap *m, int k, float p[3], float dir[2]) { bscPointAt(m, (float)(k - 1) * m->step, p, dir); }
#endif
