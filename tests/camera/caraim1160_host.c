// №1160: Car Aim — сдвиг кадра по высоте (viewer/camuser.h, viewer/menu_rules.h) — чистые законы на CPU Мака.
//   cc -std=c11 -I viewer tests/camera/caraim1160_host.c -lm -o /tmp/caraim1160 && /tmp/caraim1160
// Красные контроли (каждый обязан дать FAIL): -DCARAIM1160_OLD=1 (строки нет, сдвиг 0) · -DCARAIM_SIGN=-1.f (перевёрнутый знак: машина поднимается вместо спуска)
#include <stdio.h>
#include <math.h>
#include <float.h>
#include "menu_rules.h"
#include "camuser.h"
static int bad;
#define CHK(c, m) do { if (!(c)) { printf("FAIL %s\n", m); bad++; } else printf("ok   %s\n", m); } while (0)
// модель проекции без видеочипа: вид из точки (0,0,0) вперёд −z, матрица как у sceGumPerspective с поправкой z.y; ndc.y вверх
static float ndcY(float fovDeg, float zy, float y, float z) { float f = 1.f / tanf(fovDeg * (3.14159265f / 360.f)); return (f * y + zy * z) / -z; }
int main(void) {
  // ступени меню
  CHK(mrCamAim(2) == 0.f, "ступень 2 (по умолчанию) = нулевой сдвиг, кадр как был");
#if CARAIM1160_OLD
  puts("(режим CARAIM1160_OLD=1: строки нет)");
  CHK(mrCamAim(4) > 0.f && mrCamAim(0) < 0.f, "ступени 4 / 0 опускают / поднимают машину в кадре");
  CHK(camAimProjZY(.2f) > 0.f, "сдвиг применяется");
#else
  CHK(mrCamAim(3) > 0.f && mrCamAim(4) > mrCamAim(3), "ступени 3 и 4 опускают машину в кадре, 4 сильнее 3");
  CHK(mrCamAim(1) < 0.f && mrCamAim(0) < mrCamAim(1), "ступени 0 и 1 поднимают машину в кадре, 0 сильнее 1");
  CHK(mrCamAim(9) == 0.f && mrCamAim(-1) == 0.f, "вне 0..4 — как ступень 2");
  for (int k = 0; k < 5; k++) { float c = mrCamAim(k); if (!(fabsf(c) <= CAMU_AIM_MAX)) { CHK(0, "ступень внутри предела"); } }
  CHK(fabsf(mrCamAim(4)) <= CAMU_AIM_MAX && fabsf(mrCamAim(0)) <= CAMU_AIM_MAX, "крайние ступени внутри абсолютного предела");
  // знак: точка прямо по взгляду (y = 0) при сдвиге вниз (c > 0) уходит ниже центра, на c полувысот кадра; при c < 0 — выше
  { float c = mrCamAim(4), yy = ndcY(62.f, camAimProjZY(c), 0.f, -10.f);
    CHK(yy < 0.f && fabsf(yy + c) < 1e-5f, "знак: ступень 4 — точка по оси взгляда ниже центра ровно на c полувысот");
    float c0 = mrCamAim(0), y0 = ndcY(62.f, camAimProjZY(c0), 0.f, -10.f);
    CHK(y0 > 0.f && fabsf(y0 + c0) < 1e-5f, "знак: ступень 0 — та же точка выше центра"); }
  // без поворота: горизонт (бесконечно далёкая точка на уровне глаза) сдвигается ровно так же, как точка у машины, перспектива не меняется
  { float zy = camAimProjZY(.2f), a = ndcY(62.f, zy, 0.f, -1e4f), b = ndcY(62.f, zy, 0.f, -5.f);
    CHK(fabsf(a - b) < 1e-3f, "сдвиг один на всех расстояниях (поворота нет: горизонт и машина сдвинуты одинаково)");
    float u0 = ndcY(62.f, 0.f, 1.f, -10.f) - ndcY(62.f, 0.f, 0.f, -10.f), u1 = ndcY(62.f, zy, 1.f, -10.f) - ndcY(62.f, zy, 0.f, -10.f);
    CHK(fabsf(u0 - u1) < 1e-5f, "масштаб по вертикали не меняется (метр у машины занимает столько же пикселей)"); }
  // расширенный кадр содержит сдвинутый: любая точка, видимая в сдвинутом кадре, лежит внутри расширенного симметричного (отсечение не даёт дыр сверху и снизу)
  { int miss = 0, n = 0; for (int k = 0; k < 5; k++) { float c = mrCamAim(k), zy = camAimProjZY(c), tv = tanf(62.f * (3.14159265f / 360.f)), tvw = camAimWideTv(tv, c);
      for (float z = 1.f; z < 400.f; z *= 1.7f) for (float y = -3.f * z; y < 3.f * z; y += z * .01f) { float ny = ndcY(62.f, zy, y, -z); if (ny >= -1.f && ny <= 1.f) { n++; if (fabsf(y) > tvw * z * (1.f + 1e-5f)) miss++; } } }
    CHK(n > 1000 && miss == 0, "все точки сдвинутого кадра внутри расширенного симметричного (отсечение по расширенному кадру без дыр)"); }
  { float tv = .6f; CHK(camAimWideTv(tv, 0.f) == tv && camAimWideFov(62.f, 0.f) == 62.f, "при c = 0 отсечение и проверки — ровно прежние числа (ступень 2 побитово как было)"); }
  { float w = camAimWideFov(62.f, .2f); CHK(w > 62.f && fabsf(tanf(w * (3.14159265f / 360.f)) - tanf(62.f * (3.14159265f / 360.f)) * 1.2f) < 1e-5f, "расширенный угол кадра: полувысота ×(1 + |c|)"); }
  // пределы и плавность
  { float bad_[] = {NAN, INFINITY, -INFINITY, 1e9f, -1e9f}; int ok = 1; for (unsigned i = 0; i < 5; i++) { float v = camUserAimWant(bad_[i]); if (!(v >= -CAMU_AIM_MAX && v <= CAMU_AIM_MAX)) ok = 0; }
    CHK(ok && camUserAimWant(NAN) == 0.f, "любое значение безопасно: зажим в пределы, не число — 0"); }
  { float now = 0.f, mx = 0.f; int n = 0; for (; now < .2f && n < 400; n++) { float nx = camUserEase(now, .2f, CAMU_AIM_RATE, 1.f / 30.f, 0); if (nx - now > mx) mx = nx - now; now = nx; }
    CHK(n > 3 && now == .2f && mx <= CAMU_AIM_RATE / 30.f + 1e-6f, "сдвиг идёт плавно, не быстрее заданной скорости");
    CHK(camUserEase(0.f, .2f, CAMU_AIM_RATE, 0.f, 0) == .2f, "под паузой (dt = 0) — сразу на месте"); }
  // обратные знаки и расстояние матрицы
  CHK(camAimProjZY(0.f) == 0.f, "c = 0 — матрица не трогается");
#endif
  puts(bad ? "FAIL" : "PASS"); return bad;
}
