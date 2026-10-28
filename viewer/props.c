// Пропы: реализация (см. props.h). Чистый C99, float, без malloc в кадре.
#include "props.h"
#include "propbreak.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

int propsInit(Props *p, const void *section, unsigned size) {
  memset(p, 0, sizeof *p);
  if (!section || size < 32) return -1;
  if ((uintptr_t)section & 3) return -2;
  const unsigned char *h = (const unsigned char *)section;
  unsigned hsz = 32;
  if (!memcmp(h, "PRP2", 4)) {
    hsz = 64;
    if (size < 64) return -1;
    unsigned so = *(const unsigned *)(h + 32), ss = *(const unsigned *)(h + 36);
    if (ss && ((uint64_t)so + ss > size || (so & 15))) return -9;
    if (ss) { p->spr = h + so; p->sprSize = ss; }
    unsigned fo = *(const unsigned *)(h + 44), fn = *(const unsigned *)(h + 48);
    if (fo && fn && (uint64_t)fo + 2ull * fn <= size) { p->typeFx = h + fo; p->ntypeFx = fn; }
  } else if (memcmp(h, "PRP1", 4)) return -3;
  unsigned npiece = *(const unsigned *)(h + 4), ntype = *(const unsigned *)(h + 8), ninst = *(const unsigned *)(h + 12);
  unsigned vertBytes = *(const unsigned *)(h + 16), idxBytes = *(const unsigned *)(h + 20);
  uint64_t need = hsz + (uint64_t)vertBytes + idxBytes + (uint64_t)npiece * sizeof(PropPiece) +
                  (uint64_t)ntype * sizeof(PropType) + (uint64_t)ninst * sizeof(PropDef);
  int coinType = *(const int *)(h + 24); unsigned ncoin = *(const unsigned *)(h + 28);
  need += (uint64_t)ncoin * 12;
  if (need > size) return -4;
  if (coinType >= (int)ntype) return -8;
  const char *q = (const char *)section + hsz;
  p->vertBase = (void *)q; p->vertBytes = vertBytes; q += vertBytes;
  p->idxBase = (unsigned short *)q; p->idxBytes = idxBytes; q += idxBytes;
  p->pieces = (PropPiece *)q; p->npiece = npiece; q += (size_t)npiece * sizeof(PropPiece);
  p->types = (PropType *)q; p->ntype = ntype; q += (size_t)ntype * sizeof(PropType);
  p->def = (PropDef *)q; p->ninst = ninst; q += (size_t)ninst * sizeof(PropDef);
  p->coinType = coinType; p->ncoin = ncoin; p->coinPos = (const float *)q;
  for (unsigned i = 0; i < ninst; i++) {
    if (p->def[i].type >= ntype) return -5;
  }
  for (unsigned i = 0; i < ntype; i++) {
    if ((uint64_t)p->types[i].firstPiece + p->types[i].nPiece > npiece) return -6;
  }
  p->st = calloc(ninst ? ninst : 1, sizeof(PropState));
  if (!p->st) return -7;
  // коробка каждого типа в осях модели: по вершинам всех кусочков (модель = o + sc*v)
  float *bb = calloc(ntype ? ntype * 6 : 6, sizeof(float));
  if (!bb) return -7;
  for (unsigned t = 0; t < ntype; t++) {
    float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
    for (unsigned k = 0; k < p->types[t].nPiece; k++) {
      const PropPiece *pp = &p->pieces[p->types[t].firstPiece + k];
      if ((uint64_t)pp->vOff + (uint64_t)pp->nv * 16 > vertBytes) continue;
      const short *v = (const short *)((const char *)p->vertBase + pp->vOff);
      for (unsigned j = 0; j < pp->nv; j++, v += 8) {
        float k = pp->sc * (1.f / 32768.f);   // 16-бит вершины GE нормированы: v/32768
        float m[3] = {pp->ox + k * v[4], pp->oy + k * v[5], pp->oz + k * v[6]};
        for (int a = 0; a < 3; a++) { if (m[a] < lo[a]) lo[a] = m[a]; if (m[a] > hi[a]) hi[a] = m[a]; }
      }
    }
    if (lo[0] > hi[0]) { lo[0] = lo[2] = -p->types[t].radius; hi[0] = hi[2] = p->types[t].radius; lo[1] = 0; hi[1] = 2 * p->types[t].halfH; }
    for (int a = 0; a < 3; a++) { bb[t * 6 + a] = (lo[a] + hi[a]) * 0.5f; bb[t * 6 + 3 + a] = fmaxf((hi[a] - lo[a]) * 0.5f, 0.05f); }
  }
  // №488: ствол тонкого предмета (фонарь, столб, знак). ty->radius — половина диагонали ВСЕГО меша: у фонаря с вылетом лампы это 1.5–2 м
  // от столба — машина «упиралась в воздух». Радиус удара — по вершинам полосы кузова над основанием меша (0.3 м … не выше 1.2 м и не выше
  // трети высоты, но не уже 0.6 м полосы); центр — среднее по полосе. Тонким считается, если ствол ≤ PROP_TRUNK_MAXR и меш шире ствола
  // в PROP_TRUNK_RATIO раз; толстые (ящики, автоматы) — как было. -DPROP_HIT_TRUNK_OLD=1 — прежнее (нет таблицы)
#ifndef PROP_HIT_TRUNK_OLD
#define PROP_HIT_TRUNK_OLD 0
#endif
#ifndef PROP_TRUNK_MAXR
#define PROP_TRUNK_MAXR 0.45f
#endif
#ifndef PROP_TRUNK_RATIO
#define PROP_TRUNK_RATIO 1.35f
#endif
  if (!PROP_HIT_TRUNK_OLD) {
    p->trunk = calloc(ntype ? ntype * 3 : 3, sizeof(float));
    for (unsigned t = 0; p->trunk && t < ntype; t++) {
      float ylo = bb[t * 6 + 1] - bb[t * 6 + 4], H = 2.f * bb[t * 6 + 4];
      float y0 = ylo + 0.3f, y1 = ylo + fmaxf(0.6f, fminf(1.2f, H * 0.34f));
      double sx = 0, sz = 0; unsigned cnt = 0;
      for (int pass = 0; pass < 2; pass++) {
        float cxm = cnt ? (float)(sx / cnt) : 0.f, czm = cnt ? (float)(sz / cnt) : 0.f, r2 = 0.f;
        if (pass) { sx = sz = 0; }
        for (unsigned k = 0; k < p->types[t].nPiece; k++) {
          const PropPiece *pp = &p->pieces[p->types[t].firstPiece + k];
          if ((uint64_t)pp->vOff + (uint64_t)pp->nv * 16 > vertBytes) continue;
          const short *v = (const short *)((const char *)p->vertBase + pp->vOff);
          for (unsigned j = 0; j < pp->nv; j++, v += 8) {
            float kk = pp->sc * (1.f / 32768.f), mx = pp->ox + kk * v[4], my = pp->oy + kk * v[5], mz = pp->oz + kk * v[6];
            if (my < y0 || my > y1) continue;
            if (!pass) { sx += mx; sz += mz; cnt++; }
            else { float d2 = (mx - cxm) * (mx - cxm) + (mz - czm) * (mz - czm); if (d2 > r2) r2 = d2; }
          }
        }
        if (pass) {
          float r = sqrtf(r2), full = p->types[t].radius;
          if (cnt >= 3 && r <= PROP_TRUNK_MAXR && full > PROP_TRUNK_RATIO * fmaxf(r, 0.05f)) { p->trunk[t * 3] = cxm; p->trunk[t * 3 + 1] = czm; p->trunk[t * 3 + 2] = fmaxf(r, 0.08f); }
        }
        if (!pass && !cnt) break;
      }
      // №903 (Роб 23.10: «деревья сбиваются, даже если проезжаешь в метре от них — слишком большие зоны; коллизия по стволу, как в оригинале»):
      // у ствола-цилиндра вершины только внизу и у кроны — в полосе бампера их нет (L1 тип 8, 20 деревьев: ствол не найден, удар по радиусу
      // всей кроны 3,5 м). Тогда ствол — по срезам треугольников плоскостями на высоте бампера и выше (рёбра, пересёкшие их): центр и радиус
      // плотного кольца точек, наименьший из трёх срезов, те же пороги тонкости. PROP_TRUNK_SLICE_OLD=1 — без среза
#ifndef PROP_TRUNK_SLICE_OLD
#define PROP_TRUNK_SLICE_OLD 0
#endif
#define PROP_SLICE_MAX 256
      if (!PROP_TRUNK_SLICE_OLD && p->trunk[t * 3 + 2] <= 0.f) {
        float ylo = bb[t * 6 + 1] - bb[t * 6 + 4], H = 2.f * bb[t * 6 + 4], bestR = 1e9f, bx = 0.f, bz = 0.f;
        static float sp_[PROP_SLICE_MAX][2];
        for (int hi = 0; hi < 3; hi++) {   // срезы на 0,8 / 1,2 / 1,6 м (не выше середины): у корней комель шире, у кроны — ветки
          float ys = ylo + fminf(0.8f + 0.4f * hi, H * 0.5f); int n = 0, over = 0;
          for (unsigned k = 0; k < p->types[t].nPiece && !over; k++) {
            const PropPiece *pp = &p->pieces[p->types[t].firstPiece + k];
            if (pp->ni != pp->nv || (uint64_t)pp->vOff + (uint64_t)pp->nv * 16 > vertBytes) continue;   // пропы без индексов: тройки вершин подряд
            const short *v = (const short *)((const char *)p->vertBase + pp->vOff); float kk = pp->sc * (1.f / 32768.f);
            for (unsigned j = 0; j + 2 < pp->nv && !over; j += 3, v += 24)
              for (int e = 0; e < 3; e++) {
                const short *a = v + 8 * e, *b2 = v + 8 * ((e + 1) % 3);
                float ay = pp->oy + kk * a[5], by = pp->oy + kk * b2[5];
                if ((ay - ys) * (by - ys) >= 0.f) continue;
                if (n >= PROP_SLICE_MAX) { over = 1; break; }
                float s = (ys - ay) / (by - ay);
                sp_[n][0] = pp->ox + kk * (a[4] + s * (b2[4] - a[4])); sp_[n][1] = pp->oz + kk * (a[6] + s * (b2[6] - a[6])); n++;
              }
          }
          if (over || n < 3) continue;
          // ствол — самое плотное кольцо точек среза (соседи в 0,6 м), кольцо — точки в 0,9 м от его середины; ствол один: в кольце ≥ 3/4 среза
          // (щит на нескольких столбах, куст и широкое у земли стволом не считаются)
          int seed = 0, bestCnt = -1;
          for (int i = 0; i < n; i++) { int c = 0; for (int j = 0; j < n; j++) { float dx = sp_[i][0] - sp_[j][0], dz = sp_[i][1] - sp_[j][1]; c += dx * dx + dz * dz < .36f; } if (c > bestCnt) { bestCnt = c; seed = i; } }
          double sx = 0, sz = 0; int m = 0;
          for (int j = 0; j < n; j++) { float dx = sp_[seed][0] - sp_[j][0], dz = sp_[seed][1] - sp_[j][1]; if (dx * dx + dz * dz < .81f) { sx += sp_[j][0]; sz += sp_[j][1]; m++; } }
          if (m < 3 || 4 * m < 3 * n) continue;
          float cx = (float)(sx / m), cz = (float)(sz / m), r2 = 0.f;
          for (int j = 0; j < n; j++) { float dx = sp_[seed][0] - sp_[j][0], dz = sp_[seed][1] - sp_[j][1]; if (dx * dx + dz * dz >= .81f) continue;
            float ex = sp_[j][0] - cx, ez = sp_[j][1] - cz; if (ex * ex + ez * ez > r2) r2 = ex * ex + ez * ez; }
          if (sqrtf(r2) < bestR) { bestR = sqrtf(r2); bx = cx; bz = cz; }
        }
        float full = p->types[t].radius;
        if (bestR <= PROP_TRUNK_MAXR && full > PROP_TRUNK_RATIO * fmaxf(bestR, 0.05f)) { p->trunk[t * 3] = bx; p->trunk[t * 3 + 1] = bz; p->trunk[t * 3 + 2] = fmaxf(bestR, 0.08f); }
      }
    }
  }
  for (unsigned i = 0; i < ninst; i++) {
    PropState *st = &p->st[i]; const float *b = bb + p->def[i].type * 6;
    st->q[0] = p->def[i].q[0]; st->q[1] = p->def[i].q[1]; st->q[2] = p->def[i].q[2]; st->q[3] = p->def[i].q[3];
    st->cx = b[0]; st->cy = b[1]; st->cz = b[2]; st->hx = b[3]; st->hy = b[4]; st->hz = b[5];
    float m[16]; st->x = st->y = st->z = 0;
    propPieceMatrix(st, 0, 0, 0, 1, m);   // где начало модели относительно центра при этой ориентации
    st->x = p->def[i].x - m[12]; st->y = p->def[i].y - m[13]; st->z = p->def[i].z - m[14];
    st->home[0] = st->x; st->home[1] = st->y; st->home[2] = st->z;
    st->asleep = 1; st->fresh = 1; st->fadeT = 0.f;
  }
  free(bb);
  return 0;
}

void propsTypeFx(const Props *p, unsigned t, int *particle, int *breakIdx) {
  *particle = 0; *breakIdx = -1;
  if (!p->typeFx || t >= p->ntypeFx) return;
  *particle = p->typeFx[2 * t]; *breakIdx = p->typeFx[2 * t + 1] == 0xFF ? -1 : p->typeFx[2 * t + 1];
}

void propsFree(Props *p) { if (p->st) free(p->st); free(p->trunk); memset(p, 0, sizeof *p); }

void propPieceMatrix(const PropState *st, float ox, float oy, float oz, float sc, float m[16]) {
  float w = st->q[0], x = st->q[1], y = st->q[2], z = st->q[3];
  float R[3][3] = {
    {1.f - 2.f * (y * y + z * z), 2.f * (x * y - w * z), 2.f * (x * z + w * y)},
    {2.f * (x * y + w * z), 1.f - 2.f * (x * x + z * z), 2.f * (y * z - w * x)},
    {2.f * (x * z - w * y), 2.f * (y * z + w * x), 1.f - 2.f * (x * x + y * y)}};
  ox -= st->cx; oy -= st->cy; oz -= st->cz;   // st — центр коробки, начало модели сдвинуто на -c
  m[0] = R[0][0] * sc; m[1] = R[1][0] * sc; m[2] = R[2][0] * sc; m[3] = 0;
  m[4] = R[0][1] * sc; m[5] = R[1][1] * sc; m[6] = R[2][1] * sc; m[7] = 0;
  m[8] = R[0][2] * sc; m[9] = R[1][2] * sc; m[10] = R[2][2] * sc; m[11] = 0;
  m[12] = st->x + R[0][0] * ox + R[0][1] * oy + R[0][2] * oz;
  m[13] = st->y + R[1][0] * ox + R[1][1] * oy + R[1][2] * oz;
  m[14] = st->z + R[2][0] * ox + R[2][1] * oy + R[2][2] * oz;
  m[15] = 1;
}

static void quatMulAngVel(float q[4], const float w[3], float dt) {
  float wlen = sqrtf(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
  if (wlen < 1e-5f) return;
  float ang = wlen * dt, ax = w[0] / wlen, ay = w[1] / wlen, az = w[2] / wlen;
  float hs = sinf(ang * 0.5f), hc = cosf(ang * 0.5f);
  float dq[4] = {hc, ax * hs, ay * hs, az * hs};
  float r[4] = {
    dq[0] * q[0] - dq[1] * q[1] - dq[2] * q[2] - dq[3] * q[3],
    dq[0] * q[1] + dq[1] * q[0] + dq[2] * q[3] - dq[3] * q[2],
    dq[0] * q[2] - dq[1] * q[3] + dq[2] * q[0] + dq[3] * q[1],
    dq[0] * q[3] + dq[1] * q[2] - dq[2] * q[1] + dq[3] * q[0]};
  float n = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3]); if (n < 1e-6f) n = 1.f;
  q[0] = r[0] / n; q[1] = r[1] / n; q[2] = r[2] / n; q[3] = r[3] / n;
}

// А25д (находки PS2-Наблюдателя 10.10): у PS2 падающие сбитые предметы g = 16 м/с2 (для машины 16 уже сделано), порт 15. PROP_GRAVITY_OLD=1 — 15
#ifndef PROP_GRAVITY_OLD
#define PROP_GRAVITY_OLD 0
#endif
#define PROP_GRAVITY (PROP_GRAVITY_OLD ? 15.f : 16.f)
#define PROP_BOUNCE 0.25f
#define PROP_FRICTION 0.6f

static void quatToR(const float q[4], float R[3][3]) {
  float w = q[0], x = q[1], y = q[2], z = q[3];
  R[0][0] = 1.f - 2.f * (y * y + z * z); R[0][1] = 2.f * (x * y - w * z); R[0][2] = 2.f * (x * z + w * y);
  R[1][0] = 2.f * (x * y + w * z); R[1][1] = 1.f - 2.f * (x * x + z * z); R[1][2] = 2.f * (y * z - w * x);
  R[2][0] = 2.f * (x * z - w * y); R[2][1] = 2.f * (y * z + w * x); R[2][2] = 1.f - 2.f * (x * x + y * y);
}
void propsClosestPoint(const Props *p, unsigned i, const float point[3], float closest[3]) {
  const PropState *s = &p->st[i];
  float R[3][3]; quatToR(s->q, R);
  float d[3] = {point[0] - s->x, point[1] - s->y, point[2] - s->z};
  float half[3] = {s->hx, s->hy, s->hz}, local[3];
  for (int a = 0; a < 3; a++) {
    float v = R[0][a] * d[0] + R[1][a] * d[1] + R[2][a] * d[2];
    local[a] = fmaxf(-half[a], fminf(half[a], v));
  }
  float centre[3] = {s->x, s->y, s->z};
  for (int a = 0; a < 3; a++) closest[a] = centre[a] + R[a][0] * local[0] + R[a][1] * local[1] + R[a][2] * local[2];
}
static void cross3(const float a[3], const float b[3], float o[3]) {
  o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0];
}
// обратный тензор инерции в мире (на единицу массы): R * diag(D) * R^T * v
static void applyIinv(const float R[3][3], const float D[3], const float v[3], float o[3]) {
  float b[3];
  for (int a = 0; a < 3; a++) b[a] = (R[0][a] * v[0] + R[1][a] * v[1] + R[2][a] * v[2]) * D[a];
  for (int a = 0; a < 3; a++) o[a] = R[a][0] * b[0] + R[a][1] * b[1] + R[a][2] * b[2];
}
// импульс j (на единицу массы) в точке r от центра
static void applyImpulse(PropState *st, const float R[3][3], const float D[3], const float r[3], const float j[3]) {
  float t[3], dw[3];
  st->vel[0] += j[0]; st->vel[1] += j[1]; st->vel[2] += j[2];
  cross3(r, j, t); applyIinv(R, D, t, dw);
  st->w[0] += dw[0]; st->w[1] += dw[1]; st->w[2] += dw[2];
}
// «жёсткость» точки r вдоль единичного направления n: 1/m + n·((I⁻¹(r×n))×r), масса = 1
static float pointK(const float R[3][3], const float D[3], const float r[3], const float n[3]) {
  float rn[3], t[3], u[3];
  cross3(r, n, rn); applyIinv(R, D, rn, t); cross3(t, r, u);
  return 1.f + n[0] * u[0] + n[1] * u[1] + n[2] * u[2];
}

// Проп — коробка (8 углов) против земли: гравитация, удар углом с отскоком и трением => падает, кувыркается,
// ложится на бок. Известное упрощение: только пол (colGround), без стен — может пролететь сквозь тонкий забор.
static void integrateProp(PropState *st, const ColWorld *world, float dt) {
  st->vel[1] -= PROP_GRAVITY * dt;
  st->x += st->vel[0] * dt; st->y += st->vel[1] * dt; st->z += st->vel[2] * dt;
  quatMulAngVel(st->q, st->w, dt);
  float R[3][3]; quatToR(st->q, R);
  float hx = st->hx, hy = st->hy, hz = st->hz;
  float D[3] = {3.f / (hy * hy + hz * hz), 3.f / (hx * hx + hz * hz), 3.f / (hx * hx + hy * hy)};
  float ext = fabsf(R[1][0]) * hx + fabsf(R[1][1]) * hy + fabsf(R[1][2]) * hz;
  int contact = 0; float gy = 0.f;
  int hasG = colGround(world, st->x, st->z, st->y + 1.f, -1e6f, &gy, 0);
  if (!hasG && st->y < -500.f) { st->asleep = 1; return; }   // улетел за край мира — замираем
  if (st->fresh) {   // первый шаг после пробуждения: сколько коробка сидела в земле (корни дерева, основание столба)
    st->gOff = hasG ? fmaxf(-3.f, fminf(gy - (st->y - ext), 2.f * hy)) : 0.f;
    st->fresh = 0;   // gOff < 0 — стоял над землёй (склон, крыльцо)
  }
  gy -= st->gOff;
  if (st->w[0] * st->w[0] + st->w[1] * st->w[1] + st->w[2] * st->w[2] > 1.f)
    st->gOff -= fmaxf(-0.5f * dt, fminf(st->gOff, 0.5f * dt));   // проп кувыркается — «утопленность» плавно уходит
  if (hasG && st->y - ext < gy) {
    contact = 1;
    static const float up[3] = {0.f, 1.f, 0.f};
    for (int it = 0; it < 3; it++) {
      for (int c = 0; c < 8; c++) {
        float b[3] = {(c & 1) ? hx : -hx, (c & 2) ? hy : -hy, (c & 4) ? hz : -hz}, r[3];
        for (int a = 0; a < 3; a++) r[a] = R[a][0] * b[0] + R[a][1] * b[1] + R[a][2] * b[2];
        if (st->y + r[1] > gy + 0.03f) continue;
        float wr[3]; cross3(st->w, r, wr);
        float vn = st->vel[1] + wr[1];
        if (vn >= 0.f) continue;
        float e = (it == 0 && vn < -1.5f) ? PROP_BOUNCE : 0.f;
        float jn = -(1.f + e) * vn / pointK(R, D, r, up);
        float J[3] = {0.f, jn, 0.f}; applyImpulse(st, R, D, r, J);
        // трение: гасим скольжение точки по земле, не больше mu * jn
        cross3(st->w, r, wr);
        float vt[3] = {st->vel[0] + wr[0], 0.f, st->vel[2] + wr[2]};
        float vl = sqrtf(vt[0] * vt[0] + vt[2] * vt[2]);
        if (vl > 1e-4f) {
          float n[3] = {-vt[0] / vl, 0.f, -vt[2] / vl};
          float jt = fminf(vl / pointK(R, D, r, n), PROP_FRICTION * jn);
          float T[3] = {n[0] * jt, 0.f, n[2] * jt}; applyImpulse(st, R, D, r, T);
        }
      }
    }
    st->y += gy - (st->y - ext);
  }
  // сопротивление: на земле проп быстро теряет скорость и вращение (кувырок гаснет за 1–2 с), в воздухе — почти нет
#ifndef PROP_LDAMP_G
#define PROP_LDAMP_G 2.5f
#endif
#ifndef PROP_WDAMP_G
#define PROP_WDAMP_G 4.f
#endif
  float damp = 1.f - fminf(1.f, (contact ? PROP_WDAMP_G : 0.3f) * dt), ldamp = 1.f - fminf(1.f, (contact ? PROP_LDAMP_G : 0.1f) * dt);
  st->w[0] *= damp; st->w[1] *= damp; st->w[2] *= damp;
  st->vel[0] *= ldamp; st->vel[2] *= ldamp;
  float speed2 = st->vel[0] * st->vel[0] + st->vel[1] * st->vel[1] + st->vel[2] * st->vel[2];
  float wspeed2 = st->w[0] * st->w[0] + st->w[1] * st->w[1] + st->w[2] * st->w[2];
  if (contact && speed2 < 0.06f && wspeed2 < 0.1f) {
    st->sleepT += dt;
    if (st->sleepT > 0.4f) {
      st->asleep = 1; st->vel[0] = st->vel[1] = st->vel[2] = 0.f; st->w[0] = st->w[1] = st->w[2] = 0.f;
    }
  } else st->sleepT = 0.f;
}

// Интервал времени контакта вдоль одной оси. Совместный интервал всех осей не даёт
// ложного удара, когда xz пересеклись в начале кадра, а высота — только в конце.
static int sweptAxis(float d0, float d1, float radius, float *enter, float *leave) {
  float v = d1 - d0;
  if (fabsf(v) < 1e-6f) return fabsf(d0) <= radius;
  float a = (-radius - d0) / v, b = (radius - d0) / v;
  if (a > b) { float t = a; a = b; b = t; }
  *enter = fmaxf(*enter, a); *leave = fminf(*leave, b);
  return *enter <= *leave;
}
static float carRadiusOn(const CarBox *b, float x, float z) {
  return fabsf(x * b->fx + z * b->fz) * b->hl + fabsf(-x * b->fz + z * b->fx) * b->hw;
}
static float carHalfHeight(const CarBox *b) {
  return .5f * (b->hi - b->lo) + fabsf(b->gx) * carRadiusOn(b, 1, 0) + fabsf(b->gz) * carRadiusOn(b, 0, 1);
}
// Тонкая коробка вместо большого круга: стекло, щит, панель. Обычные объёмные пропы
// сохраняют прежний контакт. При повороте берём больший радиус кузова из двух кадров.
static int thinContact(const PropState *s, const float R[3][3], const CarBox *a, const CarBox *b, int *thin) {
  int k = 0; for (int j = 1; j < 3; j++) if (fabsf(R[1][j]) < fabsf(R[1][k])) k = j;
  float len = hypotf(R[0][k], R[2][k]), ux = R[0][k] / len, uz = R[2][k] / len;
  float axes[6][2] = {{ux, uz}, {-uz, ux}, {a->fx, a->fz}, {-a->fz, a->fx}, {b->fx, b->fz}, {-b->fz, b->fx}};
  float half[3] = {s->hx, s->hy, s->hz}, ext[6] = {0};
  for (int j = 0; j < 6; j++) for (int t = 0; t < 3; t++)
    ext[j] += fabsf(axes[j][0] * R[0][t] + axes[j][1] * R[2][t]) * half[t];
  *thin = fminf(ext[0], ext[1]) < .35f * fmaxf(ext[0], ext[1]);
  if (!*thin) return 0;
  float enter = 0.f, leave = 1.f;
  for (int j = 0; j < 6; j++) {
    float x = axes[j][0], z = axes[j][1], rad = ext[j] + fmaxf(carRadiusOn(a, x, z), carRadiusOn(b, x, z));
    if (!sweptAxis((a->x - s->x) * x + (a->z - s->z) * z,
                   (b->x - s->x) * x + (b->z - s->z) * z, rad, &enter, &leave)) return 0;
  }
  float ey = fabsf(R[1][0]) * s->hx + fabsf(R[1][1]) * s->hy + fabsf(R[1][2]) * s->hz;
  return sweptAxis(a->y + .5f * (a->lo + a->hi) - s->y, b->y + .5f * (b->lo + b->hi) - s->y,
                   ey + fmaxf(carHalfHeight(a), carHalfHeight(b)), &enter, &leave);
}

// №589: срок ухода сбитого с карты, с. Было 12 (число Gemini, не оригинала). Запись PS2 (ps2-prop-knocked-lamp-hydrant-l1.yaml): сбитый столб фонаря L1
// лежит поперёк дороги до конца записи, 12,5 с после удара, без исчезновения; решение дирижёра 15.10: правило Роба «через какое-то время уходят» остаётся,
// но ближе к оригиналу — 30 с и только вне взгляда камеры. PROP_FADE_OLD=1 — как до правки №589 (не уходят); PROP_FADE_S12_OLD=1 — прежние 12 с
#ifndef PROP_FADE_S12_OLD
#define PROP_FADE_S12_OLD 0
#endif
#ifndef PROP_FADE_S
#define PROP_FADE_S (PROP_FADE_S12_OLD ? 12.0f : 30.0f)
#endif
void propsRespawnStep(Props *p, float ex, float ez, float dt,
                      int (*seen)(void *, float, float, float, float), void *user) {
  if (dt <= 0.f || !seen) return;
  for (unsigned i = 0; i < p->ninst; i++) {
    PropState *s = &p->st[i]; const PropType *t = &p->types[p->def[i].type];
    if (s->noRespawn || (!s->hidden && s->asleep && s->fresh)) { s->away = 0.f; continue; }
    float dx = s->home[0] - ex, dz = s->home[2] - ez, r = t->radius + 2.f * t->halfH + 1.f;
#ifndef PROP_FADE_OLD
    // №589: через PROP_FADE_S (30 с) сбитый предмет убирается, но только когда камера его не видит: прозрачности у пропов в движке нет (куски идут общей
    // пачкой, вершины без альфы), а исчезновение на глазах — «глюк». Видимый лежит дальше; предметы миссий (noRespawn) не убираются
    if (!s->hidden && !s->fresh && s->fadeT > PROP_FADE_S && !seen(user, s->x, s->y, s->z, r)) { s->hidden = 1; continue; }
#endif
    if (dx * dx + dz * dz < 60.f * 60.f ||
        seen(user, s->home[0], s->home[1], s->home[2], r) || seen(user, s->x, s->y, s->z, r)) { s->away = 0.f; continue; }
    if ((s->away += fminf(dt, .05f)) < 5.f) continue;
    s->x = s->home[0]; s->y = s->home[1]; s->z = s->home[2]; memcpy(s->q, p->def[i].q, sizeof s->q);
    memset(s->vel, 0, sizeof s->vel); memset(s->w, 0, sizeof s->w);
    s->asleep = s->fresh = 1; s->hidden = 0; s->away = s->hitCool = s->sleepT = s->gOff = s->fadeT = 0.f; s->pole = 0;
  }
}

#ifndef PROP_TALL_FALL_OLD
#define PROP_TALL_FALL_OLD 0
#endif
#define PROP_TALL_HY 2.f
int propsTallFallOld = PROP_TALL_FALL_OLD;
// №968 (Роб 25.10: «всё должно падать как в оригинале, что столбы, что деревья»). Замер PS2 (двойник pcsx2-twins, tests/fall968/ps2, уличный фонарь L1 (245,4; 167,8), 1000 кг, машина без газа, 6 скоростей 2,4…22,8 м/с):
// фонарь падает целым столбом на шарнире у основания, угол от вертикали растёт как θ'' = 0,5·sin θ (рад/с²) от начального 1° и начальной скорости
// ω0 = 0,2 + 0,07·max(0, v − 12) рад/с; до 85° от первого наклона 3,3 с при ≤ 12 м/с (PS2 3,2–3,8), 2,0 с при 16,8 (PS2 2,1), 1,3 с при 22,8 (PS2 1,3); центр уходит на 7–9 м, основание на месте.
// Прежнее (№946) валило столб за 0,6–1,0 с и «бульдозер» гнал его перед машиной. PROP_POLE968_OLD=1 / flag pole968old 1 — как было
#ifndef PROP_POLE968_OLD
#define PROP_POLE968_OLD 0
#endif
int propsPole968Old = PROP_POLE968_OLD;
#define POLE_A 0.5f
// №1046 (Роб, больше недели: «на причале L6 C7-7 железные фонари должны сбиваться ПЕРЕДОМ машины, а сейчас сбиваются центром машины; то же с красными столбами L5 F3-2»): касание у столба уже считалось от коробки кузова (бампер, угол, борт),
// но валился он вяло: начальная угловая скорость 0,2 рад/с (+0,07 за каждый м/с выше 12) — к моменту, когда столб заметно наклонён (10° через 0,4 с), машина проехала 5 м и центром стоит под ним. Теперь столб получает толчок бампера:
// начальная угловая скорость не меньше 0,25 рад/с на каждый м/с скорости машины (4 рад/с не больше): на 12 м/с лежит за ~0,5 с, наклон ~30° к моменту, когда центр машины доезжает до столба; лежащий столб (pole 2) машину не толкает (быстро упавший столб иначе тащился бы перед капотом). POLE1046_OLD=1 / файл pole1046old / flag pole1046 0 — как в TEST73
#ifndef POLE1046_OLD
#define POLE1046_OLD 0
#endif
int pole1046Old = POLE1046_OLD;
float propsPoleKick1046(float v, float w0, float wk, float v0) {
  float w = w0 + wk * fmaxf(0.f, v - v0); if (pole1046Old) return w;
  float k = .25f * v; if (k > 4.f) k = 4.f; return w > k ? w : k;
}
#define POLE_W0 0.2f
#define POLE_WK 0.07f
#define POLE_V0 12.f
static void poleStep(PropState *st, float dt) {
  float th = st->poleTh, w = st->poleW, h = dt / 4.f; int done = 0;
  for (int k = 0; k < 4 && !done; k++) { w += POLE_A * sinf(th) * h; th += w * h; if (th >= 1.5707963f) { th = 1.5707963f; done = 1; } }
  st->poleTh = th; st->poleW = done ? 0.f : w;
  float dx = st->poleD[0], dz = st->poleD[1];
  float hs = sinf(th * 0.5f), hc = cosf(th * 0.5f), dq[4] = {hc, dz * hs, 0.f, -dx * hs};   // ось = вверх × направление = (dz, 0, −dx)
  const float *q = st->poleQ0;
  float r[4] = {
    dq[0] * q[0] - dq[1] * q[1] - dq[2] * q[2] - dq[3] * q[3],
    dq[0] * q[1] + dq[1] * q[0] + dq[2] * q[3] - dq[3] * q[2],
    dq[0] * q[2] - dq[1] * q[3] + dq[2] * q[0] + dq[3] * q[1],
    dq[0] * q[3] + dq[1] * q[2] - dq[2] * q[1] + dq[3] * q[0]};
  float n = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3]); if (n < 1e-6f) n = 1.f;
  for (int i = 0; i < 4; i++) st->q[i] = r[i] / n;
  float sn = sinf(th), cs = cosf(th), thick = fminf(st->hx, st->hz);
  st->x = st->poleB[0] + st->hy * sn * dx; st->z = st->poleB[2] + st->hy * sn * dz; st->y = st->poleB[1] + st->hy * cs + thick * sn;
  st->vel[0] = st->vel[1] = st->vel[2] = 0.f; st->w[0] = st->w[1] = st->w[2] = 0.f;
  if (done) { st->pole = 2; st->asleep = 1; st->fresh = 0; st->sleepT = 0.f; }
}
void propsAdvance(Props *p, const ColWorld *world, float dt) {
  if (dt <= 0.f) return;
  if (dt > 0.05f) dt = 0.05f;
  for (unsigned i = 0; i < p->ninst; i++) {
    PropState *st = &p->st[i]; if (st->hidden) continue;
    st->hitCool = fmaxf(0.f, st->hitCool - dt);
    if (!st->asleep) { if (st->pole == 1) poleStep(st, dt); else integrateProp(st, world, dt); }
#ifndef PROP_FADE_OLD
    // №589 (метка #18): сбитое/пнутое копит время лёжа на карте; уход — в propsRespawnStep (вне вида камеры, без предметов миссий)
    if (!st->fresh && !st->noRespawn) st->fadeT += dt;
#endif
  }
}

void propsHitCar(Props *p, const ColWorld *world, Car *car, const CarBox *previous) {
  if (!car) return;
  CarBox cb = {0}; carBox(car, &cb);
  CarBox prev = previous ? *previous : cb;
  float sx = cb.x - prev.x, sy = cb.y - prev.y, sz = cb.z - prev.z;
  if (sx * sx + sy * sy + sz * sz > 100.f) prev = cb; // перенос/возврат машины, а не движение сквозь весь мир
  float sweep = hypotf(cb.x - prev.x, cb.z - prev.z);
  float carSpeed = sqrtf(car->vel[0] * car->vel[0] + car->vel[2] * car->vel[2]);
  float rax = -cb.fz, raz = cb.fx;   // ось «вправо» кузова в xz
  float inv = carSpeed > 1e-3f ? 1.f / carSpeed : 0.f;
  float dirx = car->vel[0] * inv, dirz = car->vel[2] * inv;
  float roof = cb.y + cb.hi;
  // Правка «быстрый расчёт» (10.10): при скорости < 0,5 м/с цикл каждого экземпляра только пропускал — не обходим вовсе (PROP_HITSLOW_OLD=1 — прежнее)
#ifndef PROP_HITSLOW_OLD
#define PROP_HITSLOW_OLD 0
#endif
  if (!PROP_HITSLOW_OLD && carSpeed < 0.5f) return;
  for (unsigned i = 0; i < p->ninst; i++) {
    PropState *st = &p->st[i]; const PropType *ty = &p->types[p->def[i].type];
    if (st->hidden || st->pole == 1 || (!pole1046Old && st->pole == 2)) continue;   // №968; №1046: лежащий столб машину не толкает: падающий на шарнире столб машину не трогает (скорость машина потеряла при ударе)
    int awake = !st->asleep;
    if (carSpeed < 0.5f) continue;
    float dx = st->x - cb.x, dz = st->z - cb.z;
    float rr = ty->radius + cb.hl + cb.hw + 1.f + sweep;
    if (dx * dx + dz * dz > rr * rr) continue;
    float lf = dx * cb.fx + dz * cb.fz, lr = dx * rax + dz * raz;
    float cf = fmaxf(-cb.hl, fminf(cb.hl, lf)), cr = fmaxf(-cb.hw, fminf(cb.hw, lr));
    float ddx = lf - cf, ddz = lr - cr;
    float R[3][3]; quatToR(st->q, R);
#ifndef PROP_TRUNK_FIRST_OLD
#define PROP_TRUNK_FIRST_OLD 0   // 1 — фонарь с вылетом считается тонкой коробкой всей модели (прежнее: стоит у воздуха на вылете, jz16 задевал столб 2,9 м сбоку)
#endif
    // №488: у стоящего пропа с известным стволом контакт по стволу, а не по тонкой коробке всей модели (коробка фонаря с вылетом охватывает дорогу)
    int trunkKnown = !PROP_TRUNK_FIRST_OLD && p->trunk && p->trunk[p->def[i].type * 3 + 2] > 0.f && R[1][1] > 0.9f;
    int thin = 0, touching = trunkKnown ? 0 : thinContact(st, R, &prev, &cb, &thin);
    float hitR = ty->radius; int byTrunk = 0;
    if (!thin && p->trunk && p->trunk[p->def[i].type * 3 + 2] > 0.f && R[1][1] > 0.9f) {   // №488: стоящий тонкий предмет — по стволу, не по вылету
      const float *tk = &p->trunk[p->def[i].type * 3]; float lx = tk[0] - st->cx, lz = tk[1] - st->cz;
      float tcx = st->x + R[0][0] * lx + R[0][2] * lz, tcz = st->z + R[2][0] * lx + R[2][2] * lz;
      float tdx = tcx - cb.x, tdz = tcz - cb.z, tlf = tdx * cb.fx + tdz * cb.fz, tlr = tdx * rax + tdz * raz;
      ddx = tlf - fmaxf(-cb.hl, fminf(cb.hl, tlf)); ddz = tlr - fmaxf(-cb.hw, fminf(cb.hw, tlr)); hitR = tk[2]; byTrunk = 1;
    }
    if (thin ? !touching : ddx * ddx + ddz * ddz > hitR * hitR) continue;
    // по высоте: коробка пропа должна пересекать кузов (светофоры/вывески над дорогой не сбиваем снизу)
#ifndef PROP_GHOST_OLD
    // №588 (#20 столб, #25 ларёк/лавочка): не быть призраком при отсутствии colGround; при посадке вниз ловить верх пропа
    float loPad = (car->vel[1] < -1.5f) ? 0.4f : 0.f;
    if (!thin) {
      float ext = fabsf(R[1][0]) * st->hx + fabsf(R[1][1]) * st->hy + fabsf(R[1][2]) * st->hz;
      if (st->y - ext > roof || st->y + ext < cb.y + cb.lo - loPad) continue;
    }
    float g; int hasGround = colGround(world, st->x, st->z, st->y + 1.f, -1e6f, &g, 0);
    int particle, breakIdx; propsTypeFx(p, p->def[i].type, &particle, &breakIdx);
    if (!hasGround && awake && st->hitCool > 0.f) continue;
#else
    if (!thin) {
      float ext = fabsf(R[1][0]) * st->hx + fabsf(R[1][1]) * st->hy + fabsf(R[1][2]) * st->hz;
      if (st->y - ext > roof || st->y + ext < cb.y + cb.lo) continue;
    }
    float g; int hasGround = colGround(world, st->x, st->z, st->y + 1.f, -1e6f, &g, 0);
    int particle, breakIdx; propsTypeFx(p, p->def[i].type, &particle, &breakIdx);
    if (!hasGround && (awake || breakIdx < 0 || st->hitCool > 0.f)) continue;
#endif
    float side = fmaxf(-1.f, fminf(1.f, lr / (cb.hw + ty->radius)));
    if (awake) {
      // уже летит/лежит и машина его догнала: толкаем вперёд не медленнее машины (бульдозер), выше крыши не трогаем
      float vd = st->vel[0] * dirx + st->vel[2] * dirz, need = carSpeed;
      if (vd < need) {
        st->vel[0] += dirx * (need - vd) + rax * side * 0.5f; st->vel[2] += dirz * (need - vd) + raz * side * 0.5f;
        st->sleepT = 0.f;
      }
      continue;
    }
    // удар: отбрасываем по ходу машины и чуть вбок (от центра кузова), подпрыгивает невысоко,
    // верх опрокидывается вперёд. massRatio в квадрате — тяжёлые фонари/светофоры летят заметно слабее.
    if (p->onKick && p->onKick(p->kickUser, i, dirx, dirz)) {   // BreakableObject с мешами: вместо пропа играет обломок
      st->hidden = 1; st->asleep = 1;
      if (PROP_BREAK_PASS_OLD) { float k = fmaxf(ty->mass, 1.f) / (car->p.mass + fmaxf(ty->mass, 1.f)) * 0.15f; car->vel[0] *= 1.f - k; car->vel[2] *= 1.f - k; }
      continue;
    }
    if (!hasGround) { st->hitCool = .5f; continue; } // нет ни обломка, ни пола: не запускать бесконечное падение/звук
    float massRatio = car->p.mass / (car->p.mass + fmaxf(ty->mass, 1.f));
    // №968: столб (высокий предмет с известным стволом, без обломка) валится на шарнире у основания по замеру PS2; машина теряет скорость как у тяжёлого (ниже)
    int pole968 = !propsPole968Old && trunkKnown && st->hy >= PROP_TALL_HY && st->pole != 1 && carSpeed > 0.5f;
    // Правка «В138» (10.10, PS2-Наблюдатель b2_mail, почтовый ящик 125 кг, машина 11.28 м/с): ящик уходит ~10 м/с (порт при 1.2 давал 11.6)
    // и машина за 0.04 с теряет 1.19 м/с по импульсу ящика (m·v / ~1.1 т; порт «терял» 0.1: car->v не двигал скорость). Коэффициент
    // 1.04 и потеря импульсом (m·v / (0.75·M), не больше 0.3 скорости). -DPROP_KICK_OLD=1 — прежнее (1.2 и мнимая потеря через car->v)
#ifndef PROP_KICK_OLD
#define PROP_KICK_OLD 0
#endif
    // Только лёгкие пропы (класс почтового ящика, масса до PROP_LIGHT_MASS по ATC диска): тяжёлые (фонари ~1000 кг, светофоры) — прежняя
    // реакция (закон Роба «сбил — стены нет, летишь дальше»; данных PS2 по ним нет)
#ifndef PROP_LIGHT_MASS
#define PROP_LIGHT_MASS 200.f
#endif
    const int light = !PROP_KICK_OLD && ty->mass <= PROP_LIGHT_MASS;
    float kick = fminf(carSpeed * massRatio * massRatio * (light ? 1.04f : 1.2f), carSpeed * 1.5f + 2.f);
    st->vel[0] = dirx * kick + rax * side * kick * 0.3f;
    st->vel[2] = dirz * kick + raz * side * kick * 0.3f;
    st->vel[1] = fminf(kick * 0.25f, 4.f);
    float ang = kick * 0.8f / fmaxf(2.f * st->hy, 0.6f);
    // №946 (Роб 25.10, TEST59: «очень много столбов не сбивается»; L7 тематические, уровень Барта): у высокого предмета (столб, фонарь, hy >= PROP_TALL_HY) угол падения рос бы как 1/высота —
    // столб 10 м валился со скоростью 1 рад/с, а машина («бульдозер» выше) гнала его перед собой стоймя 1,5–2 с, 25–70 м. Столб ломается у земли и падает за ~0,8 с: темп падения не ниже
    // min(0,6·kick, 3) рад/с. PROP_TALL_FALL_OLD=1 / flag tallfall946old 1 — как было
    if (!propsTallFallOld && st->hy >= PROP_TALL_HY) ang = fmaxf(ang, fminf(kick * 0.6f, 3.f));
    st->w[0] = dirz * ang; st->w[1] = -side * 3.f; st->w[2] = -dirx * ang;
    st->asleep = 0; st->sleepT = 0.f; st->fresh = 1;
    if (pole968) {   // кинематика шарнира вместо импульса коробки: основание на месте, угол по θ'' = 0,5·sin θ
      st->pole = 1; st->poleTh = 0.0174533f; st->poleW = propsPoleKick1046(carSpeed, POLE_W0, POLE_WK, POLE_V0);
      st->poleD[0] = dirx; st->poleD[1] = dirz; memcpy(st->poleQ0, st->q, sizeof st->poleQ0);
      st->poleB[0] = st->x; st->poleB[1] = st->y - st->hy; st->poleB[2] = st->z;
      st->vel[0] = st->vel[1] = st->vel[2] = 0.f; st->w[0] = st->w[1] = st->w[2] = 0.f;
    }
#ifndef PROP_HEAVY_OLD
#define PROP_HEAVY_OLD 0   // 1 — тяжёлый проп скорость не гасит (прежняя потеря через car->v, на скорость не действует)
#endif
    // №488 (Стенд 16.10, ps2s_ram.py, famil_v, фонарь 1000 кг, газ держит V): сразу после касания (0,08 с) скорость падает на 2,5 / 3,75 / 6,4 м/с при
    // 5,3 / 10,3 / 20,2 м/с (≈ 1,0 + 0,28·v), дальше газ возвращает; фонарь сбивается и летит вперёд с машиной. Потеря по импульсу, как у лёгких, но с потолком 1,0 + 0,28·v
    if (!light && !PROP_HEAVY_OLD && trunkKnown && carSpeed > 0.5f) {   // только пропы с известным стволом (фонарь, столб); щиты, кроны и касание по радиусу меша прежние
      float dv = fminf(fmaxf(ty->mass, 1.f) * kick / (0.75f * car->p.mass), 1.0f + 0.28f * carSpeed), k = 1.f - dv / carSpeed;
      car->vel[0] *= k; car->vel[2] *= k; car->v *= k;
    } else
    if (!light) car->v -= car->v * massRatio * 0.15f;   // прежняя потеря (тяжёлый проп, и -DPROP_KICK_OLD=1)
    else if (carSpeed > 0.5f) {
      float dv = fminf(fmaxf(ty->mass, 1.f) * kick / (0.75f * car->p.mass), 0.3f * carSpeed), k = 1.f - dv / carSpeed;
      car->vel[0] *= k; car->vel[2] *= k; car->v *= k;
    }
  }
}

void propsUpdate(Props *p, const ColWorld *world, Car *car, float dt) {
  if (dt <= 0.f) return;
  propsAdvance(p, world, dt);
  propsHitCar(p, world, car, p->havePreviousCar ? &p->previousCar : 0);
  if (car) { carBox(car, &p->previousCar); p->havePreviousCar = 1; }
}

// Правка Роба 290 (04.10: «Гомер проходит через почтовые ящики и гидранты — так быть не должно»): пропы уровня (почтовые ящики,
// гидранты, урны, конусы — Props) твёрдые для Гомера пешком, как стейтпропы (homer.c homerSolidSprops): коробка пропа в его
// нынешней ориентации, срез по высоте, след на xz — прямоугольник по самой горизонтальной оси; низкое (верх не выше stepH над
// ступнями) — опора сверху. Правило порта; у машины пропы прежние (propsHitCar). Возврат 1 — Гомера вытолкнули (нормаль в onx, onz)
#define PROP_HOMER_MAXHALF .8f   // м: полуширина коробки пропа, до которой он твёрдый для Гомера (ящик 0,25–0,46; дерево 2,6)
// №1034 (Роб, TEST72 на PSP, L5 проспект: «Апу могу пройти сквозь скамейки, решил запрыгнуть на неё — не могу»): скамейки — пропы с коробкой 2,5×0,8×0,8 м (полуразмеры 1,25 0,41 0,40 на L2, L3, L5, L6), мусорные
// баки и ящики 1,5–1,8 м (L2, L3, L5, L6, L7) тоже шире PROP_HOMER_MAXHALF и потому для Гомера пешком были пустыми: проходил насквозь и не мог встать сверху. Теперь твёрдые и широкие низкие пропы:
// длинная сторона до PROP_HOMER_WIDE, короткая не тоньше PROP_HOMER_THICK (деревья шире и выше, щиты и афиши тонкие — прежнее правило), высота по-прежнему не больше 1,2 м полувысоты. Низкое — опора сверху,
// на скамейку можно запрыгнуть. BENCH1034_OLD=1 / файл bench1034old / flag bench1034 0 — как было
#ifndef BENCH1034_OLD
#define BENCH1034_OLD 0
#endif
int bench1034 = !BENCH1034_OLD;
#define PROP_HOMER_WIDE 1.6f   // м: скамейки 1,25, баки 1,27, ящики 0,9–1,2; кроны и большие щиты шире, их тут нет (hy > 1.2 или полуширина больше)
#define PROP_HOMER_THICK .3f
static int homerSolidSize(const PropState *st) {
  float w = fmaxf(st->hx, st->hz);
  if (st->hy > 1.2f) return 0;
  if (w <= PROP_HOMER_MAXHALF) return 1;
  return bench1034 && w <= PROP_HOMER_WIDE && fminf(st->hx, st->hz) >= PROP_HOMER_THICK;
}
// Правка «плавность» (тур SHARTEST4): список «кто рядом» строится раз за homerUpdate (homer.c homerSolidFrame), подшаги идут по короткому
// списку в том же индексном порядке. Вне homerUpdate кеша нет. -DHOMER_SOLID_CACHE_OLD=1 — прежний обход (флаг общий с homer.c)
extern unsigned homerSolidFrame;
#ifndef HOMER_SOLID_CACHE_OLD
#define HOMER_SOLID_CACHE_OLD 0
#endif
#define PS_CACHE_M 6.f
#define PS_CACHE_MAX 512
int propsSolidHomer(const Props *p, float *x, float *z, float y, float r, float stepH, float bodyH, float *top, float *onx, float *onz) {
  int pushed = 0;
  if (!p || !p->st) return 0;
  static unsigned cacheFrame, cacheN, cacheList[PS_CACHE_MAX]; static float cacheX, cacheZ, cacheR; static const void *cacheOwner; static int cacheOk;
  int useCache = !HOMER_SOLID_CACHE_OLD && homerSolidFrame != 0;
  if (useCache && (cacheFrame != homerSolidFrame || cacheOwner != p || cacheR != r || !cacheOk ||
                   (*x - cacheX) * (*x - cacheX) + (*z - cacheZ) * (*z - cacheZ) > PS_CACHE_M * PS_CACHE_M)) {
    cacheFrame = homerSolidFrame; cacheOwner = p; cacheR = r; cacheX = *x; cacheZ = *z; cacheN = 0; cacheOk = 1;
    for (unsigned i = 0; i < p->ninst; i++) {
      const PropState *st = &p->st[i];
      if (st->hidden) continue;
      if (!homerSolidSize(st)) continue;
      float br = sqrtf(st->hx * st->hx + st->hy * st->hy + st->hz * st->hz) + r + .3f + PS_CACHE_M, dx = *x - st->x, dz = *z - st->z;
      if (dx * dx + dz * dz > br * br) continue;
      if (cacheN >= PS_CACHE_MAX) { cacheOk = 0; break; }
      cacheList[cacheN++] = i; }
  }
  if (useCache && !cacheOk) useCache = 0;
  unsigned nIter = useCache ? cacheN : p->ninst;
  for (unsigned q = 0; q < nIter; q++) {
    unsigned i = useCache ? cacheList[q] : q;
    const PropState *st = &p->st[i];
    if (st->hidden) continue;
    if (!homerSolidSize(st)) continue;   // деревья, щиты, большие знаки — их коробка шире ствола/стойки
    float h[3] = {st->hx, st->hy, st->hz}, br = sqrtf(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]) + r + .3f;
    float dx = *x - st->x, dz = *z - st->z;
    if (dx * dx + dz * dz > br * br) continue;
    float R[3][3]; quatToR(st->q, R);
    float ey = 0.f; for (int m = 0; m < 3; m++) ey += h[m] * fabsf(R[1][m]);
    float lo = st->y - ey, hi = st->y + ey;
    if (hi < y - .35f || lo > y + bodyH) continue;           // ниже ступней / выше головы
    int hk = 0; for (int m = 1; m < 3; m++) if (fabsf(R[1][m]) < fabsf(R[1][hk])) hk = m;
    float ux = R[0][hk], uz = R[2][hk], ul = sqrtf(ux * ux + uz * uz);
    if (ul < 1e-4f) { ux = 1.f; uz = 0.f; } else { ux /= ul; uz /= ul; }
    float vx = -uz, vz = ux, e1 = 0.f, e2 = 0.f;
    for (int m = 0; m < 3; m++) { e1 += h[m] * fabsf(R[0][m] * ux + R[2][m] * uz); e2 += h[m] * fabsf(R[0][m] * vx + R[2][m] * vz); }
    float lu = dx * ux + dz * uz, lv = dx * vx + dz * vz;
    float qu = fmaxf(-e1, fminf(e1, lu)), qv = fmaxf(-e2, fminf(e2, lv)), eu = lu - qu, ev = lv - qv, d2 = eu * eu + ev * ev, depth;
    if (d2 >= r * r) continue;
    if (hi <= y + stepH) { if (hi > *top) *top = hi; continue; }   // низкое — можно встать сверху
    if (d2 > 1e-8f) { float d = sqrtf(d2); eu /= d; ev /= d; depth = r - d; }
    else { float pu = e1 - fabsf(lu), pv = e2 - fabsf(lv);
      if (pu < pv) { eu = lu < 0 ? -1.f : 1.f; ev = 0.f; depth = pu + r; } else { eu = 0.f; ev = lv < 0 ? -1.f : 1.f; depth = pv + r; } }
    float nx = eu * ux + ev * vx, nz = eu * uz + ev * vz;
    *x += nx * depth; *z += nz * depth; *onx = nx; *onz = nz; pushed = 1;
  }
  return pushed;
}
