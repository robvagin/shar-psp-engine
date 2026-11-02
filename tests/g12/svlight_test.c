// Хост-тест читателя объёмов «смена света» (viewer/svdata.c, круг 6 g12-data, P12-08). Без эмулятора.
// cc -I viewer tests/g12/svlight_test.c viewer/svdata.c -lm -o /tmp/svlight_test && /tmp/svlight_test [svdata-l1.shr]
// С -DSVLIGHT_OLD=1 читатель выключен: тест обязан упасть (красный контроль).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "svdata.h"
static int bad;
#define CHECK(c, ...) do { if (!(c)) { bad++; printf("ПАДЕНИЕ: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static void put32(unsigned char *p, uint32_t v) { memcpy(p, &v, 4); }
static void putf(unsigned char *p, float v) { memcpy(p, &v, 4); }
// синтетический файл: два ящика-объёма, A (множитель 0.5) и B (0.2), пересекаются; без осей поворота
static void vol(unsigned char *r, float m, float cx, float hx) {
  memset(r, 0, 120); putf(r + 32, m); putf(r + 36, m); putf(r + 40, m); put32(r + 48, 1);
  putf(r + 52, cx); putf(r + 64, hx); putf(r + 68, 5); putf(r + 72, 5);
  putf(r + 76, 1); putf(r + 92, 1); putf(r + 108, 1);
}
int main(int argc, char **argv) {
  unsigned char f[64 + 240]; memset(f, 0, sizeof f); memcpy(f, "SVD1", 4); put32(f + 4, 1); put32(f + 8, 1); put32(f + 12, 2); put32(f + 16, 64);
  vol(f + 64, 128.f / 255.f, 0.f, 10.f); vol(f + 184, 51.f / 255.f, 15.f, 10.f);   // A: x -10..10, B: x 5..25
  float o[3], A = 128.f / 256.f, B = 51.f / 256.f;
  CHECK(svdataParse(f, sizeof f, 1) == 1 && svdataLightCount() == 2, "синтетический файл не принят");
  CHECK(svdataParse(f, sizeof f, 2) == 0, "уровень 2 принял файл уровня 1");   // чужой уровень
  svdataParse(f, sizeof f, 1);
  float out[3]; float p[3] = {-50, 0, 0};
  svLightStep(p, 0.f, out); CHECK(out[0] == 1.f, "вне объёмов множитель %g, нужен 1", out[0]);
  p[0] = 0; svLightStep(p, 0.5f, out);   // вошёл в A, прошло 0,5 с: середина между 1 и A
  CHECK(fabsf(out[0] - (1.f + (A - 1.f) * .5f)) < 1e-4f, "через 0,5 с множитель %g, ждали %g", out[0], 1.f + (A - 1.f) * .5f);
  svLightStep(p, 0.6f, out); CHECK(fabsf(out[0] - A) < 1e-4f, "через 1,1 с множитель %g, ждали %g (A = байт 128 / 256)", out[0], A);
  p[0] = 8; svLightStep(p, 0.f, out);    // вошёл и в B (x 5..25): два объёма, назначение B
  svLightStep(p, 1.f, out); CHECK(fabsf(out[0] - B) < 1e-4f, "в двух объёмах множитель %g, ждали B = %g", out[0], B);
  p[0] = 20; svLightStep(p, 1.f, out);   // вышел из A (остался в B): свет остаётся B
  CHECK(fabsf(out[0] - B) < 1e-4f, "вышел из одного из двух: множитель %g, ждали B = %g (число объёмов считается)", out[0], B);
  p[0] = 50; svLightStep(p, 0.5f, out);  // вышел из последнего: плавно назад к 1 за 1 с
  CHECK(out[0] > B && out[0] < 1.f, "полпути назад: множитель %g, ждали между %g и 1", out[0], B);
  svLightStep(p, 0.6f, out); CHECK(out[0] == 1.f, "после выхода множитель %g, нужен ровно 1", out[0]);
  // пауза: dt = 0 ничего не двигает
  p[0] = 0; svLightStep(p, 0.f, out); float a0 = out[0]; svLightStep(p, 0.f, out); CHECK(out[0] == a0, "на паузе множитель двигается");
  // настоящие данные уровня
  if (argc > 1) {
    FILE *g = fopen(argv[1], "rb"); CHECK(g != 0, "нет файла %s", argv[1]);
    if (g) { static unsigned char b[1 << 16]; size_t n = fread(b, 1, sizeof b, g); fclose(g);
      int ok = svdataParse(b, (unsigned)n, 1); CHECK(ok == 1, "svdata-l1.shr не принят"); CHECK(svdataLightCount() == 9, "объёмов %d, в отчёте строки 9", svdataLightCount());
      for (unsigned i = 0; i < 9 && ok; i++) {   // центр каждого объёма: через 1 с множитель из данных
        float c[3]; memcpy(c, b + 16 + 0, 0); uint32_t off; memcpy(&off, b + 16, 4); memcpy(c, b + off + i * 120 + 52, 12);
        float m[3]; memcpy(m, b + off + i * 120 + 32, 12); svdataParse(b, (unsigned)n, 1);
        float out2[3]; svLightStep(c, 0.f, out2); svLightStep(c, 1.f, out2);
        CHECK(fabsf(out2[0] - m[0] * 255.f / 256.f) < 1e-4f && fabsf(out2[2] - m[2] * 255.f / 256.f) < 1e-4f, "объём %u: множитель %g, данные %g", i, out2[0], m[0] * 255.f / 256.f);
      } }
  }
  printf(bad ? "КРАСНЫЙ: %d\n" : "ЗЕЛЁНЫЙ\n", bad); return bad != 0;
}
