// №1118: блик и оттенок на панели зеркала (Moe's, BartRoom) — чистые числа без PSP, общие с хост-тестом tests/rooms/glint1118.c.
//   положение блика по стеклу (0..1 вдоль боковой оси, выходит за края при крайней камере): 0,5 + 0,5 x зажатое(смещение камеры вдоль стекла / (расстояние до стекла + 1,5), +-1,3),
//   плюс дрейф +-0,06 за 12 с и 0,03 за метр высоты камеры над серединой панели;
//   яркость вершины h (0..1): главная полоса шириной 0,28 вокруг положения (сглаженная), вторая полоса шириной 0,07 на 0,34 левее, вдвое слабее; полоса идёт по диагонали (выше — правее, 0,4);
//   цвет: холодный оттенок (150,190,235) -> белый, прозрачность от 40 до 160 из 255.
#ifndef MIRRORGLINT_H
#define MIRRORGLINT_H
#include <math.h>

static inline float glintPos(float eyeLat, float lc, float eyeNrm, float plane, float eyeY, float ymid, float tSec) {
  float dist = fabsf(eyeNrm - plane) + 1.5f, d = (eyeLat - lc) / dist;
#ifdef GLINT_RED_NOCLAMP   // красный контроль: без зажима
  float cl = d;
#else
  float cl = fmaxf(-1.3f, fminf(1.3f, d));
#endif
#ifdef GLINT_RED_SIGN      // красный контроль: блик бежит против камеры
  cl = -cl;
#endif
  return .5f + .5f * cl + .06f * sinf(tSec * .5236f) + (eyeY - ymid) * .03f;
}
// u, w — доли по боковой оси и высоте панели (0..1); -> яркость 0..1
static inline float glintH(float u, float w, float cpos) {
  float s = u + .4f * (w - .5f);
  float d1 = fabsf(s - cpos) / .28f, d2 = fabsf(s - (cpos - .34f)) / .07f;
  float h1 = d1 < 1.f ? 1.f - d1 : 0.f, h2 = d2 < 1.f ? 1.f - d2 : 0.f;
  h1 = h1 * h1 * (3.f - 2.f * h1); h2 = h2 * h2 * (3.f - 2.f * h2) * .5f;
  float h = h1 + h2; return h > 1.f ? 1.f : h;
}
// -> цвет ABGR (прозрачность старшим байтом)
static inline unsigned glintColor(float h) {
  float r = 150.f + 105.f * h, g = 190.f + 65.f * h, b = 235.f + 20.f * h, a = 40.f + 120.f * h;
  return (unsigned)a << 24 | (unsigned)b << 16 | (unsigned)g << 8 | (unsigned)r;
}
#endif
