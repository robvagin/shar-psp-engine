// №969 «Земля как ровная бумага» (Роб 25.10: пятна шума на газоне и тротуаре выглядят как высохшие лужи, «намоченная бумага»; штрихи не нужны). Газон и тротуар получают
// свою текстуру шума: только мелкое равномерное зерно (без низких частот — нет пятен), низкий контраст, тайл крупнее, чем у общего шума земли (№912/№949). Строится кодом при запуске,
// детерминированно (хэш точки, без файлов-картинок и без случайных чисел): кто собирает со своего диска, получает те же пиксели. Чистые функции без движка (проверка на хосте: tests/ground969/host.c).
// GROUND969_OLD=1 / файл ground969old рядом с EBOOT / «flag ground969 0» — прежний шум земли
#ifndef GROUND969_H
#define GROUND969_H
#include <stdint.h>
#ifndef GROUND969_OLD
#define GROUND969_OLD 0
#endif
#ifndef GROUND969_STYLE
#define GROUND969_STYLE 1       // вариант газона: 1 — «бумага» 8 м, 2 — «бумага» 12 м, 3 — фрагмент родного асфальта 6 м, 4 — 10 м («flag ground969 1..4»)
#endif
#define GD_CLS_GRASS 1
#define GD_CLS_WALK 2
#define GD_CLS_CURB 3
#define GD_CLS_FLOOR 4   // №1030: остальная земля (песок, глина, дорожки): та же нежная бумага вместо общего пятнистого шума №912
#define GD_NCLS 4
#define GD_W 64
#define GD_L 7
typedef struct { float pitch, amp; } GdPar;   // тайл в метрах и размах яркости (доля): 1 − amp · n, n в 0..1
static inline GdPar gdPar(int cls, int style) {
  GdPar p;
  if (cls == GD_CLS_GRASS) { p.pitch = style == 2 ? 12.f : 8.f; p.amp = .14f; } else { p.pitch = 3.5f; p.amp = .12f; }   // «бумага»; класс 3 (бордюр) бумагой не строится
  return p;
}
static inline uint32_t gdHash(int x, int y, uint32_t s) { uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u; h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }
// поле яркости уровня 0 (GD_W × GD_W): белый шум по текселю минус его же размытая копия с заворотом через край (только высокие частоты, рецепт docs/handoff/ИССЛЕДОВАНИЕ_ТЕКСТУРА_ИЗ_ОДНОГО_КУСКА_2026-10-25.md),
// значения поровну по 16 уровням (ранг -> уровень), контраст ±amp вокруг 1; mean — среднее
static void gdField(float *f, int cls, float amp, float *mean) {
  float n[GD_W * GD_W], h[GD_W * GD_W]; int ord[GD_W * GD_W];
  for (int i = 0; i < GD_W * GD_W; i++) n[i] = (float)(gdHash(i % GD_W, i / GD_W, (uint32_t)cls * 7u + 969u) & 0xffff) / 65535.f;
  for (int y = 0; y < GD_W; y++) for (int x = 0; x < GD_W; x++) { float s = 0.f;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) s += n[((y + dy + GD_W) % GD_W) * GD_W + (x + dx + GD_W) % GD_W];
    h[y * GD_W + x] = n[y * GD_W + x] - s / 9.f; }
  for (int i = 0; i < GD_W * GD_W; i++) ord[i] = i;   // ранги: сортировка вставками по h (4096 элементов, один раз при запуске)
  for (int i = 1; i < GD_W * GD_W; i++) { int v = ord[i], j = i - 1; while (j >= 0 && h[ord[j]] > h[v]) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = v; }
  float m = 0.f;
  for (int r = 0; r < GD_W * GD_W; r++) { int lv = r * 16 / (GD_W * GD_W); float q = (lv - 7.5f) / 7.5f; float v = 1.f - amp * .5f + amp * .5f * q; f[ord[r]] = v; m += v; }   // 16 уровней поровну: v в [1 − amp, 1]
  *mean = m / (GD_W * GD_W);
}
// мипы: усреднение 2×2; к далёким уровням — к среднему (fade), чтобы вдали было ровно. out — буфер T8 (строка не меньше 16 байт, начало уровня кратно 16); lv — начала уровней; возвращает размер
static const float GD_FADE[8] = { 1.f, .85f, .55f, .25f, .08f, 0.f, 0.f, 0.f };
static unsigned gdPack(const float *f0, float mean, unsigned char *out, unsigned char **lv) {
  float a[GD_W * GD_W], b[GD_W * GD_W / 4]; const float *src = f0; float *dst = a; int lw = GD_W; unsigned off = 0;
  for (int l = 0; l < GD_L; l++) {
    int stride = lw < 16 ? 16 : lw; unsigned char *p = out + off; lv[l] = p;
    for (int y = 0; y < lw; y++) for (int x = 0; x < lw; x++) { float g = mean + (src[y * lw + x] - mean) * GD_FADE[l]; g = g < 0.f ? 0.f : g > 1.f ? 1.f : g; p[y * stride + x] = (unsigned char)(g * 255.f + .5f); }
    off += (unsigned)(stride * lw); off = (off + 15u) & ~15u;
    int nw = lw >> 1; if (nw < 1) break;
    float *o = (l & 1) ? a : b; (void)dst;
    for (int y = 0; y < nw; y++) for (int x = 0; x < nw; x++) o[y * nw + x] = .25f * (src[2 * y * lw + 2 * x] + src[2 * y * lw + 2 * x + 1] + src[(2 * y + 1) * lw + 2 * x] + src[(2 * y + 1) * lw + 2 * x + 1]);
    src = o; lw = nw;
  }
  return off;
}
#define GD_BYTES 8192
#endif
