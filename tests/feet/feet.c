// Модели-96, посадка стоп (Роб, L4: «у olady3 и joger1 ноги проваливаются в асфальт»). Хост-замер НАСТОЯЩИМ читателем (viewer/skin.c, traffic.c).
//   feet traffic traffic.shr names.txt   пакет пешеходов (имена по порядку, по одному в строке)
//   feet skin file.shr|file.chr          одна модель
// Для каждой модели и каждого клипа ходьбы/бега/стойки и всех клипов общего набора npd_*: поза в 24 отсчётах цикла, все вершины скинованы палитрой (как у GE), берём самую низкую.
// low   = самая низкая вершина по циклу (м, в системе модели; корень персонажа на земле = 0)
// lift  = поправка skinFootLift (то, что порт прибавляет к y при расстановке)
// sink  = low + lift: <0 — стопа под землёй, >0 — висит (в цикле хотя бы одна стопа должна касаться земли ⇒ sink≈0)
// Порог: sink в пределах [-FEET_SINK_MAX, +FEET_HANG_MAX] (по умолчанию −1 см / +2 см). Красный контроль: FEET_RED=1 — lift не прибавляется (модель стоит на корне).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "traffic.h"
#include "homer.h"
#include <time.h>
int (*sceneReadyHook)(const ColWorld *w, float x, float z, int *failed);
static unsigned char *rd(const char *p, unsigned *sz) {
  FILE *g = fopen(p, "rb"); if (!g) return 0; fseek(g, 0, SEEK_END); *sz = (unsigned)ftell(g); fseek(g, 0, SEEK_SET);
  unsigned char *b = aligned_alloc(64, (*sz + 63) & ~63u); if (fread(b, 1, *sz, g) != *sz) return 0; fclose(g); return b;
}
static int al(int o, int a) { return (o + a - 1) & ~(a - 1); }
// самая низкая скинованная вершина (y) и самая низкая вершина ниже 25 см над ней — «подошва»: возвращаем только минимум
static float lowest(const SkinModel *m, const ScePspFMatrix4 *pal) {
  float lo = 1e9f;
  for (unsigned bi = 0; bi < m->h->nBatches; bi++) {
    const SkinBatch *b = &m->b[bi]; int pf = (b->vtype >> 7) & 3, nf = (b->vtype >> 5) & 3, nb = b->nb, uvo = al(nb, 2), nro = uvo + 4, nsz = nf == 2 ? 6 : 3;
    int poo = pf == 2 ? al(nro + nsz, 2) : al(nro + nsz, 4);
    for (unsigned i = 0; i < b->nverts; i++) {
      unsigned k = (b->flags & 4) ? ((const unsigned short *)(m->base + b->pad[0]))[i] : i;
      const unsigned char *v = m->base + b->offVerts + (size_t)k * b->stride; float p[3];
      if (pf == 2) { short q[3]; memcpy(q, v + poo, 6); for (int c = 0; c < 3; c++) p[c] = q[c] / 32768.f; } else memcpy(p, v + poo, 12);
      double Y = 0; for (int w = 0; w < nb; w++) { float wt = v[w] / 128.f; const float *M = (const float *)&pal[b->bones[w]]; Y += wt * (M[1] * p[0] + M[5] * p[1] + M[9] * p[2] + M[13]); }
      if (Y < lo) lo = (float)Y;
    }
  }
  return lo;
}
static double sinkMax = 0.01, hangMax = 0.02; static int bad, rows, red;
static void one(const SkinModel *m, const char *tag) {
  ScePspFMatrix4 pal[SKIN_MAXJ] __attribute__((aligned(16))), wd[SKIN_MAXJ] __attribute__((aligned(16))); SkinXf xf[SKIN_MAXJ];
  for (unsigned c = 0; c < m->h->nClips; c++) {
    const char *n = skinClipName(m, (int)c);
    if (!(strstr(n, "loco_walk") || strstr(n, "loco_run") || strstr(n, "loco_idle_rest") || !strncmp(n, "npd_", 4))) continue;   /* ходьба, бег, стойка всех моделей + все клипы общего набора прохожих (их ставит skinFootLift); акробатические idle-гэги игроков через skinFootLift не ставятся */
    if (m->c[c].length <= 0.f) continue;
    float tmin = 0; float lo = 1e9f; int NS = (int)(m->c[c].length * 240.f); if (NS < 120) NS = 120;   /* в 4 раза чаще, чем у игры (кадр 60 Гц), с началом и концом клипа и со сдвигом */
    for (int f = 0; f <= NS + (int)(m->c[c].frames + .5f); f++) { float tt = f <= NS ? m->c[c].length * (f == NS ? 1.f : (f + .37f) / NS) : (f - NS - 1) / m->c[c].fps;   /* + все кадры исходной анимации */
      skinSample(m, (int)c, tt, xf); skinPalette(m, xf, pal, wd); float l = lowest(m, pal); if (l < lo) { lo = l; tmin = tt; } }
    float lift = red ? 0.f : skinFootLift(m, (int)c); float sink = lo + lift; int fail = sink < -sinkMax || sink > hangMax; rows++; bad += fail;
    if (getenv("FEET_DEBUG")) printf("[длина %.3f с, минимум в %.3f с] ", m->c[c].length, tmin);
    printf("%s %-26s low %+.3f lift %+.3f -> %+.3f м %s\n", tag, n, lo, lift, sink, fail ? (sink < 0 ? "FAIL(под землёй)" : "FAIL(висит)") : "ok");
  }
}
int main(int argc, char **argv) {
  if (argc < 3) return 2; red = getenv("FEET_RED") ? atoi(getenv("FEET_RED")) : 0;
  if (getenv("FEET_SINK")) sinkMax = atof(getenv("FEET_SINK")); if (getenv("FEET_HANG")) hangMax = atof(getenv("FEET_HANG"));
  if (!strcmp(argv[1], "skin")) { SkinModel *m = skinLoad(argv[2]); if (!m) { printf("FAIL skinLoad %s\n", argv[2]); return 1; } const char *b = strrchr(argv[2], '/'); one(m, b ? b + 1 : argv[2]); }
  else if (!strcmp(argv[1], "bake")) {   // feet bake traffic.shr | feet bake skin FILE: у каждого клипа есть запечённое число (метка FLFT), skinFootLift его и отдаёт (= счёту на месте), вершины не обходятся (счётчик skinFootComputed = 0)
    static Traffic T; SkinModel *one_[1]; int n = 0; const SkinModel *ms[64];
    if (!strcmp(argv[2], "skin")) { one_[0] = skinLoad(argv[3]); if (!one_[0]) return 1; ms[n++] = one_[0]; }
    else { unsigned sz; unsigned char *x = rd(argv[2], &sz); if (!x || trafficInitMem(&T, x, sz, 0)) return 1; for (int i = 0; i < T.npeds && i < 64; i++) ms[n++] = &T.peds[i]; }
    int miss = 0, diff = 0, clips = 0; double mx = 0;
    for (int i = 0; i < n; i++) for (unsigned c = 0; c < ms[i]->h->nClips; c++) {
      clips++; if (ms[i]->h->pad[2] != 0x54464C46u) { miss++; }
      struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a); float v = skinFootLift(ms[i], (int)c); clock_gettime(CLOCK_MONOTONIC, &b);
      double us = (b.tv_sec - a.tv_sec) * 1e6 + (b.tv_nsec - a.tv_nsec) / 1e3; if (us > mx) mx = us;
      if (ms[i]->c[c].length > 0.f && fabsf(v - skinFootLiftCompute(ms[i], (int)c)) > 1e-6f) diff++;
    }
    printf("BAKE моделей %d клипов %d: без числа %d, расхождений со счётом на месте %d, считано на месте %u, взято запечённых %u, самый долгий вызов %.1f мкс -> %s\n", n, clips, miss, diff, skinFootComputed, skinFootBakedHits, mx, (miss || diff || skinFootComputed) ? "FAIL" : "PASS");
    return (miss || diff || skinFootComputed) ? 1 : 0;
  }
  else if (!strcmp(argv[1], "time")) {   // feet time traffic.shr | feet time skin FILE: цена ПЕРВОГО (холодного) вызова skinFootLift по каждому клипу, мс хоста (на PSP ×5,8) и повторного (из кеша)
    static Traffic T; SkinModel *one_[1]; int n = 0; const SkinModel *ms[64];
    if (!strcmp(argv[2], "skin")) { one_[0] = skinLoad(argv[3]); if (!one_[0]) return 1; ms[n++] = one_[0]; }
    else { unsigned sz; unsigned char *x = rd(argv[2], &sz); if (!x || trafficInitMem(&T, x, sz, 0)) return 1; for (int i = 0; i < T.npeds && i < 64; i++) ms[n++] = &T.peds[i]; }
    double sum = 0, mx = 0, warm = 0; int calls = 0, over1 = 0; const char *mxn = "";
    for (int i = 0; i < n; i++) for (unsigned c = 0; c < ms[i]->h->nClips; c++) {
      const char *cn = skinClipName(ms[i], (int)c); if (!(strstr(cn, "loco_") || !strncmp(cn, "npd_", 4))) continue;
      struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a); (void)skinFootLift(ms[i], (int)c); clock_gettime(CLOCK_MONOTONIC, &b);
      double ms_ = (b.tv_sec - a.tv_sec) * 1e3 + (b.tv_nsec - a.tv_nsec) / 1e6; sum += ms_; calls++; if (ms_ > mx) { mx = ms_; mxn = cn; } if (ms_ > 1.0) over1++;
      clock_gettime(CLOCK_MONOTONIC, &a); (void)skinFootLift(ms[i], (int)c); clock_gettime(CLOCK_MONOTONIC, &b); warm += (b.tv_sec - a.tv_sec) * 1e3 + (b.tv_nsec - a.tv_nsec) / 1e6;
    }
    printf("TIME моделей %d клипов %d: холодный вызов среднее %.3f мс, максимум %.3f мс (%s), всего %.1f мс, дольше 1 мс: %d; повторный (кеш) среднее %.5f мс\n", n, calls, sum / (calls ? calls : 1), mx, mxn, sum, over1, warm / (calls ? calls : 1)); return 0;
  }
  else if (!strcmp(argv[1], "stale")) {   // feet stale A.shr B.shr namesB: уровень A, затем уровень B в той же памяти (адреса моделей те же) — поправки B не должны взяться из кеша A
    static Traffic T; unsigned sz; unsigned char *x = rd(argv[2], &sz); if (!x || trafficInitMem(&T, x, sz, 0)) { puts("FAIL trafficInitMem A"); return 1; }
    FILE *devnull = fopen("/dev/null", "w"); FILE *so = stdout; (void)so; for (int i = 0; i < T.npeds; i++) { for (unsigned c = 0; c < T.peds[i].h->nClips; c++) (void)skinFootLift(&T.peds[i], (int)c); } if (devnull) fclose(devnull);
    unsigned sz2; unsigned char *y = rd(argv[3], &sz2); if (!y || trafficInitMem(&T, y, sz2, 0)) { puts("FAIL trafficInitMem B"); return 1; }
    char names[64][32]; int nn = 0; if (argc > 4) { FILE *f = fopen(argv[4], "r"); while (f && nn < 64 && fscanf(f, "%31s", names[nn]) == 1) nn++; if (f) fclose(f); }
    for (int i = 0; i < T.npeds; i++) { char tag[48]; snprintf(tag, sizeof tag, "%-10s", i < nn ? names[i] : "ped?"); one(&T.peds[i], tag); }
  }
  else {
    unsigned sz; unsigned char *x = rd(argv[2], &sz); static Traffic T; if (!x || trafficInitMem(&T, x, sz, 0)) { puts("FAIL trafficInitMem"); return 1; }
    char names[64][32]; int nn = 0; if (argc > 3) { FILE *f = fopen(argv[3], "r"); while (f && nn < 64 && fscanf(f, "%31s", names[nn]) == 1) nn++; if (f) fclose(f); }
    for (int i = 0; i < T.npeds; i++) { char tag[48]; snprintf(tag, sizeof tag, "%-10s", i < nn ? names[i] : "ped?"); one(&T.peds[i], tag); }
  }
  printf("строк %d, нарушений %d\n%s\n", rows, bad, bad ? "FAIL" : "PASS"); return bad ? 1 : 0;
}
