// Issue #2 (konsolenschatz, PC-версия): Гомер на бегу. Стенд: разгон, торможение, разворот и круг при зажатом ○. Числа против таблицы тестера (PC) и исходника (темп поворота бега 2/с, ходьбы 10/с, ход вдоль лица при ○).
//   turnrun LEVEL HOMER [--hz 30|60]  (открытая площадка L1 (40; −300), 8 направлений на 40 м свободны)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
static int fails;
static void chk(const char *w, float got, float lo, float hi, const char *exp) { int ok = got >= lo && got <= hi; fails += !ok; printf("  %-4s %-52s %8.2f  (%s)\n", ok ? "ok" : "FAIL", w, got, exp); }
static BotWorld W; static float DT = 1.f / 60.f, SX = 40.f, SZ = -300.f, HD = 1.5708f;
static float spd(void) { return hypotf(W.homer.vx, W.homer.vz); }
static float hdg(void) { return atan2f(W.homer.vx, W.homer.vz); }
static float wrap(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static void step(float mx, float my, int dash) { HomerInput in; memset(&in, 0, sizeof in); in.mx = mx; in.my = my; in.camYaw = 0.f; in.dash = (unsigned char)dash; homerUpdate(&W.homer, bwCol(), NULL, &in, DT); }
static void dbg(const char *w) { fprintf(stderr, "%s: pos %.1f %.1f %.1f grounded %d spawnWait %d v %.2f mode?\n", w, W.homer.x, W.homer.y, W.homer.z, W.homer.grounded, W.homer.spawnWait, spd()); }
static void runUp(float h) { float g_ = 0.f; colGround(bwCol(), SX, SZ, 30.f, -50.f, &g_, 0); homerPlace(&W.homer, bwCol(), SX, g_ + .3f, SZ, h); for (int i = 0; i < 60; i++) step(0, 0, 0); dbg("после постановки"); for (int i = 0; i < (int)(3.f / DT); i++) step(sinf(h), cosf(h), 1); dbg("после разгона"); }
int main(int argc, char **argv) {
  if (bwData(argv[1], argv[2], 0)) return 2; memset(&W, 0, sizeof W); if (bwNew(&W)) return 2;
  for (int i = 3; i + 1 < argc; i++) { if (!strcmp(argv[i], "--hz")) DT = 1.f / (float)atoi(argv[i + 1]); else if (!strcmp(argv[i], "--at") && i + 3 < argc) { SX = (float)atof(argv[i + 1]); SZ = (float)atof(argv[i + 2]); HD = (float)atof(argv[i + 3]); } }
  printf("шаг %.4f с\n", DT);
  // A: всё отпущено на бегу 8 -> 0
  runUp(HD); float v0 = spd(); int n = 0; while (spd() > 0.05f && n < 600) { step(0, 0, 0); n++; } chk("A всё отпущено на 8 м/с: время до 0, с", n * DT, .35f, .55f, "PS2 0,44, PC 0,48"); printf("A отпущено всё: скорость до %.2f, до 0 за %.2f с (%.1f м/с²)   [тестер PC 16,7 м/с², 0,48 с]\n", v0, n * DT, v0 / (n * DT));
  // B: ○ отпущен, стик держат: 8 -> 4
  runUp(HD); v0 = spd(); n = 0; while (spd() > 4.05f && n < 600) { step(sinf(HD), cosf(HD), 0); n++; } chk("B ○ отпущен, стик держат: время до 4 м/с, с", n * DT, 0.f, .07f, "PS2: за один тик; PC 0,10"); printf("B ○ отпущен, стик держат: до 4 м/с за %.2f с (%.1f м/с²)   [тестер PC 41 м/с², 0,10 с]\n", n * DT, (v0 - 4.f) / (n * DT));
  // C: разворот стика на 180° при ○
  runUp(HD); float h0 = hdg(); float t = 0.f; float hmax = 0.f; int k;
  for (k = 0; k < (int)(6.f / DT); k++) { step(-sinf(HD), -cosf(HD), 1); float d = fabsf(wrap(hdg() - h0)); if (d > hmax) hmax = d; if (d > 3.14159265f * 170.f / 180.f) break; }
  t = k * DT; chk("C разворот на 180° при ○: средний темп, °/с", 170.f / t, 90.f, 140.f, "PC 98-129, 115 в среднем"); printf("C разворот на 180° (стик назад, ○ зажат): курс скорости на 170° за %.2f с, ср. %.0f °/с, скорость в конце %.1f   [тестер PC 115 °/с; радиус ~4 м]\n", t, 170.f / t, spd());
  // D: стик описывает круг (вращается 230 °/с и 100 °/с) при ○
  for (int pass = 0; pass < 2; pass++) { float w = pass ? 100.f : 230.f, a = 0.f; runUp(HD); float hh0 = hdg(), tot = 0.f, prev = hdg(), slide = 0.f; int N = (int)(3.f / DT);
    for (int i = 0; i < N; i++) { a += w * 3.14159265f / 180.f * DT; step(sinf(a), cosf(a), 1); float h = hdg(); tot += wrap(h - prev); prev = h; float sl = fabsf(wrap(atan2f(W.homer.vx, W.homer.vz) - W.homer.h)); if (sl > slide) slide = sl; }
    chk(pass ? "D2 угол скорости к лицу при круге стика, °" : "D1 угол скорости к лицу при круге стика, °", slide * 57.2958f, 0.f, 12.f, "скорость скаляр вдоль лица, без бокового скольжения"); if (!pass) chk("D1 круг стика 230 °/с: темп курса, °/с", fabsf(tot) * 57.2958f / 3.f, 140.f, 260.f, "PC ≈ 230 при плавном круге; с разгона 3 с"); printf("D стик крутится %.0f °/с 3 с: курс скорости повернулся на %.0f° (%.0f °/с), скорость %.1f, макс. угол скорости к лицу %.0f°   [тестер PC: плавный круг ≈ 230 °/с]\n", w, tot * 57.2958f, tot * 57.2958f / 3.f, spd(), slide * 57.2958f); (void)hh0; }
  printf("RESULT %s\n", fails ? "FAIL" : "OK"); return fails != 0; }
