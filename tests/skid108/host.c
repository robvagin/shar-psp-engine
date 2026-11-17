// 108 О13: плотный массив следов шин (viewer/skid.c qc). Хост на Маке, настоящие car.c, col.c, skid.c (SKID_HOST), уровень build/level.shr. Машина игрока и две «ИИ» (слот 1, 2) 4000 кадров
// тормозят, скользят и ездят по кругу — кольцо 384 квадов переполняется и затирается. Каждые 5 кадров: сверка qc с исходными массивами (skidCheck108 == 0) и выбор квадов skidSelect108 в 6 точках глаза
// новым путём РАВЕН выбору прежним (два обхода) — массив номеров целиком, хэш. Печатает расхождения, число квадов, строки кэша (модель) и время выбора с холодным кэшем.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include "car.h"
#include "skid.h"
void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
static ColWorld W;
static unsigned char *junk; static volatile unsigned char sink;
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e6 + t.tv_nsec * 1e-3; }
static void flush(void) { memset(junk, 1, 64u << 20); sink = junk[777]; }
#define DT (1 / 60.f)
static uint32_t fnv(uint32_t h, const void *d, size_t n) { const unsigned char *p = d; while (n--) { h ^= *p++; h *= 16777619u; } return h; }
static void place(Car *c, float x, float z, float h) {
  float g; if (!colGround(&W, x, z, 50, -50, &g, 0)) { printf("нет земли %.1f %.1f\n", x, z); exit(2); }
  carInit(c, NULL, &W, x, g, z, h);
  for (int k = 0; k < 30; k++) { CarInput in = {0, 0, 0, 0}; carUpdate(c, &W, &in, DT); }
}
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  junk = malloc(64u << 20);
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2; fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
  unsigned char *d = aligned_alloc(16, (n + 15) & ~15); if (fread(d, 1, n, f) != (size_t)n) return 2; fclose(f);
  unsigned co = *(unsigned *)(d + 92), po = *(unsigned *)(d + 96), pv = *(unsigned *)(d + 88), ce = (unsigned)n;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&W, d + co, ce - co)) return 2;
  skidClear(1);
  Car c[3]; for (int k = 0; k < 3; k++) place(&c[k], 190.f + 12.f * k, 160.f + 6.f * k, 1.57f);
  unsigned bad = 0, diff = 0, checks = 0, qmax = 0; uint32_t hn = 2166136261u, ho = 2166136261u;
  double tN = 0, tO = 0; unsigned long long lnN = 0, lnO = 0; unsigned nsel = 0;
  static unsigned short sa[SKID_QUADS], sb[SKID_QUADS];
  for (int fr = 0; fr < 4000; fr++) {
    for (int k = 0; k < 3; k++) {
      float t = fr * DT + k * 1.3f;
      CarInput in = {fmodf(t, 5.f) < 2.f ? 1.f : 0.f, fmodf(t, 5.f) > 2.5f && fmodf(t, 5.f) < 3.3f ? 1.f : 0.f, .6f * sinf(t * 1.7f), fmodf(t, 7.f) > 6.2f};
      carUpdate(&c[k], &W, &in, DT); skidCar(k, &c[k], &in, &W);
    }
    if (fr % 5) continue;
    bad += skidCheck108(); checks++;
    const float ex[6][3] = {{c[0].x, c[0].y + 3, c[0].z}, {c[1].x, c[1].y + 3, c[1].z}, {c[2].x + 40, c[2].y + 3, c[2].z}, {200, 10, 170}, {260, 10, 190}, {c[0].x - 85, c[0].y + 3, c[0].z}};
    for (int e = 0; e < 6; e++) {
      skidDense108Old = 0; unsigned nn = skidSelect108(ex[e][0], ex[e][1], ex[e][2], sa);
      skidDense108Old = 1; unsigned no = skidSelect108(ex[e][0], ex[e][1], ex[e][2], sb);
      if (nn != no || memcmp(sa, sb, nn * sizeof(unsigned short))) diff++;
      hn = fnv(hn, sa, nn * sizeof(unsigned short)); ho = fnv(ho, sb, no * sizeof(unsigned short)); nsel += nn;
    }
    { float p[4][3]; unsigned char a[4]; unsigned q = 0; for (int i = 0; i < SKID_QUADS; i++) q += skidQuadAt(i, p, a); if (q > qmax) qmax = q; }
    if (fr % 50 == 0) {   // время и строки кэша: холодный кэш, оба пути
      skidDense108Old = 0; flush(); double t0 = now(); skidSelect108(ex[0][0], ex[0][1], ex[0][2], sa); tN += now() - t0; lnN += skidLines108();
      skidDense108Old = 1; flush(); t0 = now(); skidSelect108(ex[0][0], ex[0][1], ex[0][2], sb); tO += now() - t0; lnO += skidLines108();
    }
  }
  unsigned tm = 4000 / 50;
  printf("checks %u\nbad %u\ndiff %u\nhashnew %08x\nhashold %08x\nquads_max %u\nselected %u\nlines_new %.1f\nlines_old %.1f\nus_new %.2f\nus_old %.2f\n", checks, bad, diff, hn, ho, qmax, nsel, (double)lnN / tm, (double)lnO / tm, tN / tm, tO / tm);
  return 0;
}
