// №417 (Роб 09.10, PSP: «гаечный ключ при подборе по-прежнему не исчезает»): ключи всех уровней подряд в ОДНОЙ сессии, как EBOOT.
// В main.c Game G живёт всю сессию, а SPR1 уровня пересоздаётся (LEVELUP / меню: levelUnload+levelLoad; NEW GAME: spInit). Здесь
// мир ботов (tools/bots/world.c) пересоздаётся на каждый уровень, а поля ключей Game переносятся из прошлого мира — как в EBOOT.
//   tests/wrench/levels.sh [--gate]
// Проверки по каждому уровню из списка аргументов (LEVEL.shr MSN — парами):
//   hide     — машина проезжает через КАЖДЫЙ ключ уровня: ключ взят, модель (экземпляр SPR1 в точке ключа) не рисуется
//   newgame  — после NEW GAME (spInit, Game тот же) ключ 0 снова лежит и подбирается (модель прячется)
//   respawn  — взятый ключ возвращается на ту же точку: игрок далеко и вне вида; время возврата (с) — в строке
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"

static int fails;
static void check(const char *name, int ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void check(const char *name, int ok, const char *fmt, ...) {
  char d[400]; va_list a; va_start(a, fmt); vsnprintf(d, sizeof d, fmt, a); va_end(a);
  printf("CHECK %-10s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++;
}
static BotWorld W;
static void steps(int n) { for (int i = 0; i < n; i++) bwStep(&W, 0, 0, 0); }
typedef struct { uint32_t taken; float away[32]; short inst[32]; int mapped; } Keep;
static void keep(Keep *k) { Game *G = &W.G; k->taken = G->wrenchTaken; memcpy(k->away, G->wrenchAway, sizeof k->away); memcpy(k->inst, G->wrenchInst, sizeof k->inst); k->mapped = G->wrenchMapped; }
static void restore(const Keep *k) { Game *G = &W.G; G->wrenchTaken = k->taken; memcpy(G->wrenchAway, k->away, sizeof k->away); memcpy(G->wrenchInst, k->inst, sizeof k->inst); G->wrenchMapped = k->mapped; }
// экземпляр модели ключа ищется независимо от game.c: тип SPR1 cls 2 в точке ключа
static int modelAt(const float p[3]) {
  for (unsigned i = 0; i < W.sp.ninst; i++) { const SpInst *I = &W.sp.inst[i]; if (W.sp.type[I->type].cls != 2) continue;
    float dx = I->x - p[0], dy = I->y - p[1], dz = I->z - p[2]; if (dx * dx + dy * dy + dz * dz < .01f) return (int)i; }
  return -1;
}
static float groundAt(const ColWorld *C, float x, float z, float y0) { float g; return colGround(C, x, z, y0 + 3.f, y0 - 10.f, &g, 0) ? g : y0; }
// машина у ключа: на земле под ним; ключ над землёй (крыша, трамплин — в оригинале берётся прыжком) — машина в его точке в воздухе
static void carAt(const ColWrench *K) { const ColWorld *C = bwCol(); float gy = groundAt(C, K->p[0], K->p[2], K->p[1]);
  W.car.damage = .5f; bwPlace(&W, K->p[0], gy, K->p[2], 0.f); W.car.damage = .5f; steps(3);
  (void)gy; if (fabsf(K->p[1] - W.car.y) > 2.f) { W.car.x = K->p[0]; W.car.y = K->p[1] - .8f; W.car.z = K->p[2]; gameWrenchStep(&W.G); } }

int main(int argc, char **argv) {
  if (argc < 4 || (argc - 2) % 2) { fprintf(stderr, "levels HOMER.shr LEVEL.shr MSN [LEVEL.shr MSN ...]\n"); return 2; }
  setvbuf(stdout, NULL, _IONBF, 0);
  Keep K; int have = 0;
  for (int a = 2; a + 1 < argc; a += 2) {
    if (bwData(argv[a], argv[1], argv[a + 1])) { printf("FAIL: данные %s\n", argv[a]); return 1; }
    bwNew(&W); if (have) restore(&K);   // Game тот же, что на прошлом уровне (main.c: G статический)
    steps(10);
    const ColWorld *C = bwCol(); unsigned n = C->nwrench < 32 ? C->nwrench : 32; char lv[64]; snprintf(lv, sizeof lv, "%s", argv[a]);
    // ---- hide: каждый ключ уровня
    int took = 0, hid = 0, model = 0;
    for (unsigned k = 0; k < n; k++) {
      carAt(&C->wrench[k]); int i = modelAt(C->wrench[k].p);
      took += (W.G.wrenchTaken >> k & 1u) != 0; if (i >= 0) { model++; hid += !W.sp.run[i].alive; }
      if (getenv("WR_DEBUG")) printf("  k=%u p=%.1f,%.1f,%.1f r=%.2f car=%.1f,%.1f,%.1f taken=%d inst=%d/%d alive=%d\n", k, C->wrench[k].p[0], C->wrench[k].p[1], C->wrench[k].p[2], C->wrench[k].r,
        W.car.x, W.car.y, W.car.z, (int)(W.G.wrenchTaken >> k & 1u), W.G.wrenchInst[k], i, i >= 0 ? W.sp.run[i].alive : -1);
    }
    check("hide", n && took == (int)n && model == (int)n && hid == (int)n, "%s: ключей %u, взято %d, с моделью %d, модель спрятана %d", lv, n, took, model, hid);
    // ---- newgame: тот же уровень, SPR1 заново (spInit), Game тот же
    keep(&K); bwNew(&W); restore(&K); steps(5);
    { int i = modelAt(C->wrench[0].p); int lay = i >= 0 && W.sp.run[i].alive; carAt(&C->wrench[0]);
      check("newgame", lay && (W.G.wrenchTaken & 1u) && i >= 0 && !W.sp.run[i].alive, "%s: после NEW GAME ключ 0 лежит %d, подобран %d, модель спрятана %d",
            lv, lay, (int)(W.G.wrenchTaken & 1u), i >= 0 && !W.sp.run[i].alive); }
    // ---- respawn: взят ключ 0, игрок у самого дальнего ключа — сколько секунд до возврата модели в ту же точку
    { const ColWrench *Kf = &C->wrench[0]; float best = 0;
      for (unsigned k = 0; k < n; k++) { float d = hypotf(C->wrench[k].p[0] - C->wrench[0].p[0], C->wrench[k].p[2] - C->wrench[0].p[2]); if (d > best) { best = d; Kf = &C->wrench[k]; } }
      W.G.wrenchTaken = 1u;   // остальные ключи — на месте (иначе машина у дальнего ключа подберёт его)
      for (unsigned k = 1; k < n; k++) { int i = W.G.wrenchInst[k]; if (i >= 0) W.sp.run[i].alive = 1; }
      { int i = W.G.wrenchInst[0]; if (i >= 0) W.sp.run[i].alive = 0; }
      float gy = groundAt(C, Kf->p[0] + 8.f, Kf->p[2], Kf->p[1]); bwPlace(&W, Kf->p[0] + 8.f, gy, Kf->p[2], 0.f);
      int i0 = modelAt(C->wrench[0].p), f = 0; for (; f < 60 * 40 && i0 >= 0 && !W.sp.run[i0].alive; f++) steps(1);
      float T = f / 60.f; int back = i0 >= 0 && W.sp.run[i0].alive && !(W.G.wrenchTaken & 1u);
      check("respawn", back && T > 29.5f && T < 30.8f,   /* сверка g10-flow: 30 с по таймеру */ "%s: игрок за %.0f м — модель ключа 0 вернулась %d через %.2f с", lv, best, back, T); }
    keep(&K); have = 1;
  }
  printf("RESULT %s %d\n", fails ? "FAIL" : "OK", fails);
  return fails ? 1 : 0;
}
