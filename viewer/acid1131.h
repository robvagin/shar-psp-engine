// №1131 (g11): числа и падение капель кислоты (АЭС L1) — чистая арифметика, без PSP; рисует drip770.h. Яркость и размеры наши, цикл 2,5 с и зелёный цвет — из данных оригинала.
#ifndef ACID1131_H
#define ACID1131_H
#include <math.h>
#ifndef ACID1131_OLD
#define ACID1131_OLD 0
#endif
#define ACID_CYCLE 2.5f    // с: цикл клипа капли (75 кадров, 30 в секунду) — из данных
#define ACID_DROPS 3       // капель-штрихов на столб
// альфа столба сверху и снизу, пятна на полу (из 255): прежние 70, 22, 50
static inline float acidAlphaTop(int old) { return old ? 70.f : 150.f; }
static inline float acidAlphaLow(int old) { return old ? 22.f : 70.f; }
static inline float acidAlphaPool(int old) { return old ? 50.f : 120.f; }
// капля j (0..ACID_DROPS-1) столба k в момент t (с): голова yh (м), длина штриха len (м); 0 — капли сейчас нет. Падение с ускорением 9,8 м/с², не дольше 2,2 с, хвост тянется со скоростью
static inline int acidDropAt(float t, int j, int k, float T, float B, float *yh, float *len) {
  if (ACID1131_OLD) return 0;
  float H = T - B, tf = sqrtf(2.f * (H > .5f ? H : .5f) / 9.8f); if (tf > 2.2f) tf = 2.2f;
  float u = t / ACID_CYCLE + j * (1.f / ACID_DROPS) + k * .137f; u -= floorf(u); float tau = u * ACID_CYCLE; if (tau >= tf) return 0;
  float y = T - .5f * 9.8f * tau * tau, l = .6f + 9.8f * tau * .09f; if (l > 1.9f) l = 1.9f; if (y < B + .1f) return 0;
  *yh = y; *len = l; return 1;
}
#endif
