// render_fast.h — ускорение отрисовки (05.10, облако, ветка cloud/render-fast; кандидаты docs/ОПТИМИЗАЦИИ_ОТОБРАЖЕНИЯ_2026-10-05.md).
// Три правки, каждая — под своим битом «прежнего» (сценарий: «flag rfold N», сборка: -DRF_OLD=N; 7 — всё как до правки):
//   RF_GG (1) О11: в кадре с низкой ближней плоскостью (правка 340) клетка, чей бокс задевает полосу GE, раньше шла целиком по кускам
//             (без листа). Теперь — по кускам только если хоть один её непрозрачный кусок drawGeoG действительно резал бы; иначе —
//             листом клетки: drawGeoG такие куски и так отдаёт GE целиком (drawGeo), картинка та же, меньше команд процессора
//   RF_DL (2) О4: dlCity шёл по ВСЕМ видимым кускам и для каждого читал Piece (~100 Б) — даже если его клетку уже вызвали листом.
//             Теперь клетка куска лежит рядом с ключом сортировки (u16), повтор клетки отсекается без чтения Piece
//   RF_SP (4) О2: стейтпропы — поза (spPose) и матрицы кусков считались для всех в круге 0,92·дальности, и вне кадра тоже. Теперь
//             до позы — проверка пирамиды по сфере экземпляра с ЧЕСТНЫМ радиусом типа (rfSpTypeRadius: верхняя граница по цепочке
//             суставов при любой позе клипа — анимация не выходит за сферу, «выскакивания» нет). Поза ленивая (poseValid): пропуск
//             не меняет состояния
// Без зависимостей от PSP: tests/render/host.c собирает этот файл на хосте вместе с настоящим viewer/stateprops.c.
#ifndef RENDER_FAST_H
#define RENDER_FAST_H
#include <math.h>
#include "stateprops.h"

#ifndef RF_OLD
#define RF_OLD 7   // Политики проверены по картинке; скорость PSP ещё не измерена
#endif
#define RF_GG 1
#define RF_DL 2
#define RF_SP 4
#define RF_SP_JMAX 128      // суставов в типе для расчёта радиуса; больше — тип не отсекается (радиус < 0)
#ifndef CULL_SPHERE_OLD
#define CULL_SPHERE_OLD 0   // №544/546 включены по умолчанию (слово Роба 27.10: машины, что бьют игрока, видны всегда; грузовик не пропадает у края кадра; выключили без слова Роба 15.10); прежнее -DCULL_SPHERE_OLD=1 / -DNEAR_CAR_DRAW_OLD=1; TEST17 (слово Роба/дирижёра 15.10: отрисовка как в 1721 f32c8c31): 1 — прежний запас; 0 — №546
#endif
#if CULL_SPHERE_OLD
#define RF_SP_PAD 0.25f     // м: запас на float24 GE и округления
#else
#define RF_SP_PAD 1.5f      // №546: запас 1–2 м по краям
#endif

// Радиус сферы вокруг (run.x, run.y, run.z), внутри которой лежат ВСЕ вершины кусков типа t при ЛЮБОЙ позе клипа
// (spPose: нормированная интерполяция поворота, линейная — переноса; масштаб сустава постоянный). Граница по цепочке:
//   ||W_R(j)|| <= ||W_R(parent)|| * max|s_j|,  |W_t(j)| <= |W_t(parent)| + ||W_R(parent)|| * max_кадров |t_j|;
// вершина куска (s16/32768, |v_i| <= 1) — не дальше |W_t| + ||W_R|| * (|o| + sc*sqrt(3)). Поворот экземпляра R длину не меняет.
// -1 — посчитать нельзя (суставов больше RF_SP_JMAX, сустав куска вне типа): тогда не отсекать.
static inline float rfSpTypeRadius(const StateProps *sp, unsigned t) {
  if (!sp || t >= sp->ntype) return -1.f;
  const SpType *T = &sp->type[t];
  if (T->nJoint > RF_SP_JMAX) return -1.f;
  float nR[RF_SP_JMAX], nT[RF_SP_JMAX];   // граница нормы поворота-масштаба и длины переноса сустава (в осях объекта)
  unsigned nfr = T->frames & 0xFFFF, ntr = T->frames >> 16;
  for (unsigned j = 0; j < T->nJoint; j++) {
    const SpJoint *J = &sp->joint[T->firstJoint + j];
    float s = fabsf(J->s[0]); if (fabsf(J->s[1]) > s) s = fabsf(J->s[1]); if (fabsf(J->s[2]) > s) s = fabsf(J->s[2]);
    float tm = sqrtf(J->t[0] * J->t[0] + J->t[1] * J->t[1] + J->t[2] * J->t[2]);
    if (J->track < 0 || !nfr) {   // поворот из данных без нормировки (quatR): ||R(m·u)|| <= |1 - m²| + m²
      float m2 = J->q[0] * J->q[0] + J->q[1] * J->q[1] + J->q[2] * J->q[2] + J->q[3] * J->q[3]; s *= fabsf(1.f - m2) + m2;
    } else {
      tm = 0.f;
      for (unsigned f = 0; f < nfr; f++) {
        const float *a = sp->key + ((size_t)T->keyOff + (size_t)f * ntr + J->track) * 7;
        float l = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); if (l > tm) tm = l;
      }
    }
    if (J->parent < 0) { nR[j] = s; nT[j] = tm; }
    else if ((unsigned)J->parent < j) { nR[j] = nR[J->parent] * s; nT[j] = nT[J->parent] + nR[J->parent] * tm; }
    else return -1.f;   // spPose считает родителя раньше ребёнка; иной порядок — не наш случай, не отсекать
  }
  float rad = 0.f;
  for (unsigned k = 0; k < T->nPiece; k++) {
    const SpPiece *P = &sp->piece[T->firstPiece + k];
    unsigned j = ((unsigned)P->flags >> 8) & 0xFF;
    if (!T->nJoint || j >= T->nJoint) return -1.f;
    float o = sqrtf(P->ox * P->ox + P->oy * P->oy + P->oz * P->oz);
    float r = nT[j] + nR[j] * (o + fabsf(P->sc) * 1.7320508f);
    if (r > rad) rad = r;
  }
  return rad + RF_SP_PAD;
}

// Сфера (центр c, радиус r) целиком снаружи одной из 4 боковых плоскостей кадра (pl — внутренние нормали setupPlanes, e — глаз)?
// 1 — не видна. Та же проверка, что visBox по боксу ±r вокруг c (visBox берёт угол бокса по знаку нормали), — бокс шире сферы,
// поэтому здесь бокс: решение совпадает с прежней проверкой пропов (main.c, этап 3б)
static inline int rfSphereOut(const float pl[4][3], const float e[3], const float c[3], float r) {
  for (int i = 0; i < 4; i++) {
    float px = (pl[i][0] > 0 ? c[0] + r : c[0] - r) - e[0];
    float py = (pl[i][1] > 0 ? c[1] + r : c[1] - r) - e[1];
    float pz = (pl[i][2] > 0 ? c[2] + r : c[2] - r) - e[2];
    if (px * pl[i][0] + py * pl[i][1] + pz * pl[i][2] < 0) return 1;
  }
  return 0;
}
#endif
