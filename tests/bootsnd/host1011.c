// №1011: проверка звука заставки после правки (Роб 26.10: «убрать шипение; пищалка спикера — 4 последние ударные ноты темы, в том же темпе»):
// тот же viewer/bootbeep.h, что на PSP, синтез кусками по BOOT_SND_CHUNK в память на Маке, замер по самим сэмплам.
//   1) шипения нет: до первой ноты (0–1020 мс) все сэмплы 0 — ни щелчка, ни кулера
//   2) ровно 4 ноты: начала 1024 + {0, 170, 320, 520} мс ±10 мс (замер темы на диске, см. bootbeep.h)
//   3) высота каждой ноты 554,4 Гц ±2 % (C#5, делитель 8253 = 2152); 3-я/1-я гармоника < 0,2 (мягкий меандр, как бип POST)
//   4) три первые короткие (60–200 мс), последняя самая длинная (≥ 300 мс); между нотами тишина (провал ≥ 20 дБ)
//   5) пик ≤ 4000 (громкость бипа POST); вся партитура ≤ 4320 мс (ход заставки), в конце тишина
//   6) кусок кратен 64, кусками = одним куском; START — затухание за один кусок без скачка; скорость синтеза ≥ 50× реального времени
// Красное: -DBOOTSND1011_OLD=1 (шипение, бип 1000 Гц) | -DBOOT_TUNE_DIV=1193 (не та нота) | -DBOOT_TUNE_LP=0.55f (резко) | -DBOOT_SND_CHUNK=1000 -> FAIL.
// Пишет build/bootsnd1011.wav — послушать.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "bootbeep.h"
#define R BOOT_BEEP_RATE
#define NMAX (BOOT_SND_N + 2 * BOOT_SND_CHUNK)
static short all[NMAX], part[NMAX];
static int render(short *b, int chunk, int stopAt) {
  BootSnd s; bootSndInit(&s, BOOT_SND_VARIANT); int tot = 0, k, off = 0;
  memset(b, 0, sizeof all);
  while (off + chunk <= NMAX) { if (stopAt >= 0 && off >= stopAt) bootSndStop(&s);
    if ((k = bootSndFill(&s, b + off, chunk)) <= 0) break; tot += k; off += chunk; }
  return tot;
}
static void wav(const char *fn, const short *b, int n) {
  FILE *f = fopen(fn, "wb"); if (!f) return;
  unsigned bytes = n * 2, r = R, br = r * 2, v; unsigned short s;
  fwrite("RIFF", 1, 4, f); v = 36 + bytes; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
  v = 16; fwrite(&v, 4, 1, f); s = 1; fwrite(&s, 2, 1, f); fwrite(&s, 2, 1, f); fwrite(&r, 4, 1, f); fwrite(&br, 4, 1, f);
  s = 2; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
  fwrite(b, 2, n, f); fclose(f);
}
static double goertzel(const short *b, int a, int n, double hz) {   // амплитуда гармоники hz в окне [a, a+n)
  double w = 2 * M_PI * hz / R, c = 2 * cos(w), s1 = 0, s2 = 0;
  for (int i = 0; i < n; i++) { double s0 = b[a + i] + c * s1 - s2; s2 = s1; s1 = s0; }
  return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / n;
}
int main(void) {
  int fails = 0, tot = render(all, BOOT_SND_CHUNK, -1);
  // 1) шипение: до первой ноты тишина
  int at = R / 1000 * 1024, quiet = 1; for (int i = 0; i < at - R / 1000 * 4; i++) if (all[i]) { quiet = 0; break; }
  printf("до первой ноты (0–1020 мс) тишина, щелчка и кулера нет: %s\n", quiet ? "да" : "нет"); fails += !quiet;
  // 2) ноты: огибающая по 2 мс, начало ноты — переход через порог
  int win = R / 500, nw = tot / win + 2, on[16], off_[16], nn = 0; double env[NMAX / 88 + 4], pk = 0;
  for (int w = 0; w < nw && (w + 1) * win <= NMAX; w++) { double e = 0; for (int i = 0; i < win; i++) { double v = all[w * win + i]; e += v * v; } env[w] = sqrt(e / win); if (env[w] > pk) pk = env[w]; }
  int in = 0; for (int w = 0; w < nw && (w + 1) * win <= NMAX; w++) {
    if (!in && env[w] > pk * .1) { in = 1; if (nn < 16) on[nn] = w * 2; }
    else if (in && env[w] < pk * .1) { in = 0; if (nn < 16) off_[nn] = w * 2; nn++; } }
  static const int want[4] = {1024, 1194, 1344, 1544};
  int okOn = nn == 4; for (int k = 0; k < 4 && k < nn; k++) okOn &= abs(on[k] - want[k]) <= 10;
  printf("нот %d (ждём 4), начала, мс:", nn); for (int k = 0; k < nn && k < 8; k++) printf(" %d", on[k]); printf(" (ждём 1024 1194 1344 1544 ±10): %s\n", okOn ? "да" : "нет"); fails += !okOn;
  // 3) высота и мягкость каждой ноты; 4) длительности и провалы
  int okPitch = nn >= 1, okDur = nn == 4, okGap = 1;
  for (int k = 0; k < nn && k < 4; k++) {
    int a = R / 1000 * (on[k] + 10), n = R / 1000 * 50; double best = 0, bh = 0;
    for (double hz = 300; hz <= 1200; hz += 1) { double m = goertzel(all, a, n, hz); if (m > best) { best = m; bh = hz; } }
    double h3 = goertzel(all, a, n, bh * 3) / (best > 0 ? best : 1); int len = off_[k] - on[k];
    int ok = fabs(bh - 554.4) <= 554.4 * .02 && h3 < .2; okPitch &= ok;
    printf("нота %d: %.0f Гц (ждём 554 ±2 %%), 3-я/1-я %.2f, звучит %d мс\n", k + 1, bh, h3, len);
    if (k < 3) { okDur &= len >= 60 && len <= 200; if (k + 1 < nn) { double lo = 1e9; for (int w = off_[k] / 2; w < on[k + 1] / 2; w++) if (env[w] < lo) lo = env[w]; okGap &= lo < pk * .1; } }
    else okDur &= len >= 300;
  }
  printf("высота и мягкость: %s; три коротких и длинная последняя: %s; между нотами провал: %s\n", okPitch ? "да" : "нет", okDur ? "да" : "нет", okGap ? "да" : "нет");
  fails += !okPitch + !okDur + !okGap;
  // 5) громкость и длина
  int peak = 0, clip = 0; for (int i = 0; i < NMAX; i++) { int v = abs(all[i]); if (v > peak) peak = v; if (v >= 32767) clip++; }
  double lenMs = tot * 1000.0 / R; int tailQuiet = 1; for (int i = tot; i < NMAX; i++) if (all[i]) { tailQuiet = 0; break; }
  int okLen = peak <= 4000 && lenMs <= 4320 && !clip && tailQuiet;
  printf("пик %d (≤ 4000), звучит %.0f мс (≤ 4320), обрезано %d, в конце тишина %s: %s\n", peak, lenMs, clip, tailQuiet ? "да" : "нет", okLen ? "да" : "нет"); fails += !okLen;
  // 6) куски, START, скорость
  int okChunk = BOOT_SND_CHUNK % 64 == 0 && BOOT_SND_CHUNK <= 65472; render(part, 64, -1); int seam = memcmp(all, part, sizeof all) != 0;
  printf("кусок %d (кратно 64): %s; кусками = одним куском: %s\n", BOOT_SND_CHUNK, okChunk ? "да" : "нет", seam ? "НЕТ" : "да"); fails += !okChunk + seam;
  int st = R / 1000 * 1200 / BOOT_SND_CHUNK * BOOT_SND_CHUNK; render(part, BOOT_SND_CHUNK, st);
  int same = !memcmp(all, part, st * 2), jump = 0, after = 1; for (int i = st; i < st + BOOT_SND_CHUNK; i++) { int d = abs(part[i] - part[i - 1]); if (d > jump) jump = d; }
  for (int i = st + BOOT_SND_CHUNK; i < NMAX; i++) if (part[i]) { after = 0; break; }
  int okStop = same && after && jump <= 4000; printf("START на 1,2 с: до остановки то же %s, скачок %d, после куска тишина %s: %s\n", same ? "да" : "нет", jump, after ? "да" : "нет", okStop ? "да" : "нет"); fails += !okStop;
  { BootSnd s; bootSndInit(&s, BOOT_SND_VARIANT); s.i = R / 1000 * 1550; clock_t c0 = clock(); for (int r = 0; r < 200; r++) { s.i = R / 1000 * 1550; bootSndFill(&s, part, BOOT_SND_CHUNK); }
    double us = (double)(clock() - c0) / CLOCKS_PER_SEC * 1e6 / 200, real = BOOT_SND_CHUNK * 1e6 / R; int okSpeed = us * 50 < real;
    printf("скорость: кусок %.0f мкс при длительности %.0f мкс: %s\n", us, real, okSpeed ? "да" : "нет"); fails += !okSpeed; }
  wav("build/bootsnd1011.wav", all, tot + R / 10);
  printf("файл build/bootsnd1011.wav\nRESULT: %s (%d failures)\n", fails ? "FAIL" : "OK", fails);
  return fails ? 1 : 0;
}
