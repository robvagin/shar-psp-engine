// №1067: проходима ли стартовая точка машины игрока на старте миссии. startcheck STARTS.tsv SET_DIR [уровень]
// По каждой строке starts.py (вид in/out): уровень L из SET_DIR (level.shr; L1 — корень, остальные lN/) и traffic.shr (точки полос TRF1: Tr Pt).
//  1) земля под точкой: разница с y локатора; контактов кузова с миром (colProbeCar) при постановке;
//  2) прямой выезд: 16 направлений, газ 6 с — сколько направлений уезжают на >= 25 м и макс. расстояние;
//  3) на дорогу: до 8 ближайших точек полос (<= 150 м), погоня с рулём и задним ходом при застревании, 35 с: достигнута точка полосы на расстоянии <= 4 м.
// Вывод: строка на старт и итог. Код выхода 0 всегда (приёмка читает таблицу).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "car.h"
static unsigned char *rdf(const char *p, unsigned *n) { FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); *n = ftell(f); rewind(f); unsigned char *b = malloc(*n); if (fread(b, 1, *n, f) != *n) return NULL; fclose(f); return b; }
static float wrapa(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
typedef struct { ColWorld w; unsigned char *lv; float *pt; unsigned npt; int ok; } Lvl;
static int loadLevel(Lvl *L, const char *set, int lv) {
  char p[512]; unsigned n; snprintf(p, sizeof p, lv == 1 ? "%s/level.shr" : "%s/l%d/level.shr", set, lv); L->lv = rdf(p, &n); if (!L->lv) return -1;
  unsigned co = *(unsigned *)(L->lv + 92), po = *(unsigned *)(L->lv + 96), pv = *(unsigned *)(L->lv + 88), end = n; if (po > co && po < end) end = po; if (pv > co && pv < end) end = pv;
  if (colInit(&L->w, L->lv + co, end - co)) return -2;
  snprintf(p, sizeof p, lv == 1 ? "%s/traffic.shr" : "%s/l%d/traffic.shr", set, lv); unsigned tn; unsigned char *t = rdf(p, &tn); L->npt = 0; L->pt = NULL;
  if (t && !memcmp(t, "TRF1", 4)) { unsigned nPt = *(unsigned *)(t + 28), off = *(unsigned *)(t + 32); L->pt = malloc(nPt * 12); for (unsigned i = 0; i < nPt; i++) { memcpy(L->pt + i * 3, t + off + i * 16, 12); } L->npt = nPt; }
  L->ok = 1; return 0;
}
static float steerK;   // знак руля: +1 если положительный руль увеличивает курс
static float calib(Lvl *L, float x, float y, float z, float h) {
  Car c; carInit(&c, NULL, &L->w, x, y, z, h); for (int i = 0; i < 20; i++) carUpdate(&c, &L->w, &(CarInput){0}, 1.f / 60); carSetSpeed(&c, 6.f);
  float h0 = c.h; for (int i = 0; i < 40; i++) carUpdate(&c, &L->w, &(CarInput){.throttle = .5f, .steer = 1.f}, 1.f / 60); return wrapa(c.h - h0) > 0 ? 1.f : -1.f;
}
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "r"); char line[1024]; int only = argc > 3 ? atoi(argv[3]) : 0; Lvl cur[8]; memset(cur, 0, sizeof cur); fgets(line, sizeof line, f);
  printf("%-3s %-6s %-4s %-20s %-8s %-8s %-6s %-5s %-6s %-7s %-6s %s\n", "L", "мисс", "вид", "локатор", "x", "z", "dy", "конт", "выезд", "макс_м", "дорога", "лучшая_точка");
  while (fgets(line, sizeof line, f)) {
    char *t[16]; int nt = 0; for (char *s = strtok(line, "\t\n"); s && nt < 16; s = strtok(NULL, "\t\n")) t[nt++] = s;
    if (nt < 9) continue; int lv = atoi(t[0]); if (only && lv != only) continue; if (!*t[5]) continue;
    float x = atof(t[5]), y = atof(t[6]), z = atof(t[7]), h = atof(t[8]);
    if (!cur[lv].ok && loadLevel(&cur[lv], argv[2], lv)) { printf("L%d: нет данных\n", lv); continue; }
    Lvl *L = &cur[lv]; float g;
    if (!colGround(&L->w, x, z, y + 3.f, y - 8.f, &g, 0)) { printf("%-3d %-6s %-4s %-20s %-8.1f %-8.1f нет земли\n", lv, t[2], t[3], t[4], x, z); continue; }
    Car c0; carInit(&c0, NULL, &L->w, x, g, z, h); for (int i = 0; i < 20; i++) carUpdate(&c0, &L->w, &(CarInput){0}, 1.f / 60); CarBox bx; carBox(&c0, &bx); int contacts = colProbeCar(&L->w, &bx);
    // прямой выезд
    int ex = 0; float mx = 0; for (int k = 0; k < 16; k++) { Car c; carInit(&c, NULL, &L->w, x, g, z, h + k * 3.14159265f / 8.f); for (int i = 0; i < 20; i++) carUpdate(&c, &L->w, &(CarInput){0}, 1.f / 60); carSetSpeed(&c, 3.f);
      float md = 0; for (int fr = 0; fr < 360; fr++) { CarInput in = {0}; in.throttle = c.v < 10.f ? 1.f : 0.f; carUpdate(&c, &L->w, &in, 1.f / 60); float d = hypotf(c.x - x, c.z - z); if (d > md) md = d; } if (md >= 25.f) ex++; if (md > mx) mx = md; }
    // на дорогу: ближайшие точки полос
    if (steerK == 0.f) steerK = calib(L, x, g, z, h);
    int tg[8]; float td[8]; int nt2 = 0;
    for (unsigned i = 0; i < L->npt; i++) { float d = hypotf(L->pt[i * 3] - x, L->pt[i * 3 + 2] - z); if (d > 150.f || fabsf(L->pt[i * 3 + 1] - g) > 6.f) continue;
      int pos = nt2 < 8 ? nt2++ : 7; if (nt2 == 8 && d >= td[7] && pos == 7 && i > 0) { if (d >= td[7]) continue; }
      int k = pos; while (k > 0 && td[k - 1] > d) { td[k] = td[k - 1]; tg[k] = tg[k - 1]; k--; } td[k] = d; tg[k] = (int)i; }
    int reached = 0; float bd = 1e9f; int bi = -1; char note[64] = "";
    for (int q = 0; q < nt2 && !reached; q++) { float tx = L->pt[tg[q] * 3], tz = L->pt[tg[q] * 3 + 2]; Car c; carInit(&c, NULL, &L->w, x, g, z, h); for (int i = 0; i < 20; i++) carUpdate(&c, &L->w, &(CarInput){0}, 1.f / 60);
      int rev = 0; float stuck = 0, best = 1e9f;
      for (int fr = 0; fr < 35 * 60; fr++) { float e = wrapa(atan2f(tx - c.x, tz - c.z) - c.h), dd = hypotf(tx - c.x, tz - c.z); if (dd < best) best = dd; if (dd <= 4.f) { reached = 1; break; }
        CarInput in = {0};
        if (rev > 0) { rev--; in.brake = 1.f; in.steer = (e > 0 ? -1.f : 1.f) * steerK; }
        else { in.steer = fmaxf(-1.f, fminf(1.f, e * 2.5f)) * steerK; in.throttle = c.v < (fabsf(e) > .6f ? 4.f : 9.f) ? 1.f : 0.f; }
        carUpdate(&c, &L->w, &in, 1.f / 60); if (fabsf(c.v) < .4f && rev == 0) { stuck += 1.f / 60; if (stuck > 1.2f) { rev = 90; stuck = 0; } } else if (rev == 0) stuck = 0; }
      if (best < bd) { bd = best; bi = q; } }
    printf("%-3d %-6s %-4s %-20s %-8.1f %-8.1f %-6.2f %-5d %-6d %-7.1f %-6s %s\n", lv, t[2], t[3], t[4], x, z, g - y, contacts, ex, mx, nt2 == 0 ? "нет полос" : reached ? "да" : "НЕТ", nt2 ? (snprintf(note, sizeof note, "до ближайшей %.0f м, остаток %.0f м", td[0], bd), note) : "");
  }
  return 0; }
