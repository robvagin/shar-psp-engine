// Гейт В5 на Маке: карточки, осы, гэги уровня 1 — логика viewer/collect.c на пакете build/collect-l1.shr против ожиданий,
// снятых с диска независимо (tests/collect/expect.py), и та же игра без экрана (tools/bots/world.c: настоящие Гомер,
// машина, коллизия COL1, монеты). Запуск и красный/зелёный — tests/collect/host.sh.
//   host EXPECT.tsv PACK LEVEL.shr HOMER.shr [--plant card|gag|wasp|trig]
// --plant портит пакет в памяти (карточка сдвинута на 3 м, центр триггера гэга на 3 м, точка осы на 5 м, радиус триггера
// гэга x0.3) — гейт обязан упасть. Итог: строки "CHECK имя ok|FAIL подробности" и "RESULT PASS|FAIL n/m".
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "collect.h"
#ifdef GAG_OUTRO_OLD
#define GAG_OUTRO_OLD_H GAG_OUTRO_OLD
#else
#define GAG_OUTRO_OLD_H 0
#endif

static int nok, nfail;
static void check(const char *name, int ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void check(const char *name, int ok, const char *fmt, ...) {
  char b[256]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a);
  printf("CHECK %-28s %s %s\n", name, ok ? "ok  " : "FAIL", b); if (ok) nok++; else nfail++;
}
static void *readAll(const char *p, size_t *n) {
  FILE *f = fopen(p, "rb"); if (!f) return 0;
  fseek(f, 0, SEEK_END); long s = ftell(f); rewind(f);
  unsigned char *b = aligned_alloc(16, ((size_t)s + 15) & ~(size_t)15);
  if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return 0; }
  fclose(f); *n = (size_t)s; return b;
}

typedef struct { char kind[8], name[24]; int room, trig, cycle, random, coins; float x, y, z, r; char fmv[16]; } Exp;
static Exp ex[128]; static int nex;
static void loadExpect(const char *path) {
  FILE *f = fopen(path, "r"); if (!f) { fprintf(stderr, "нет %s\n", path); exit(2); }
  char line[512];
  while (fgets(line, sizeof line, f) && nex < 128) {
    Exp *e = &ex[nex]; memset(e, 0, sizeof *e);
    if (sscanf(line, "card\t%23s\t%f\t%f\t%f\t%f", e->name, &e->x, &e->y, &e->z, &e->r) == 5) strcpy(e->kind, "card");
    else if (sscanf(line, "wasp\t%23s\t%f\t%f\t%f\t%f", e->name, &e->x, &e->y, &e->z, &e->r) == 5) strcpy(e->kind, "wasp");
    else if (sscanf(line, "gag\t%23s\t%d\t%d\t%d\t%d\t%d\t%f\t%f\t%f\t%f\t%15s", e->name, &e->room, &e->trig, &e->cycle, &e->random,
                    &e->coins, &e->x, &e->y, &e->z, &e->r, e->fmv) == 11) strcpy(e->kind, "gag");
    else continue;
    nex++;
  }
  fclose(f);
}

static Collect C; static Coins coins; static float coinBuf[3];
static int drain(int type, int *idx) {   // сколько событий type; idx — номер последнего
  CollEvent e; int n = 0; while (collectPop(&C, &e)) if (e.type == type) { n++; if (idx) *idx = e.idx; } return n;
}
static int step(float x, float y, float z, int foot, int room, int action, float dt, int type, int *idx) {
  CollPlayer p = {x, y, z, foot, room, action};
  int n = 0; CollEvent e;
  collectStep(&C, &p, NULL, NULL, NULL, &coins, dt);
  while (collectPop(&C, &e)) if (e.type == type) { n++; if (idx) *idx = e.idx; }
  return n;
}
static int gagIndex(const char *name) { for (unsigned g = 0; g < C.ngag; g++) if (!strcmp(C.gag[g].name, name)) return (int)g; return -1; }

int main(int argc, char **argv) {
  if (argc < 5) { fprintf(stderr, "host EXPECT.tsv PACK LEVEL.shr HOMER.shr [--plant X]\n"); return 2; }
  const char *plant = argc > 6 && !strcmp(argv[5], "--plant") ? argv[6] : "";
  loadExpect(argv[1]);
  size_t psize; unsigned char *pak = readAll(argv[2], &psize); if (!pak) { fprintf(stderr, "нет пакета %s\n", argv[2]); return 2; }
  // ---- отрицательные контроли: испорченный пакет (гейт обязан упасть)
  if (!strcmp(plant, "card") || !strcmp(plant, "gag") || !strcmp(plant, "wasp") || !strcmp(plant, "trig")) {
    const unsigned *u = (const unsigned *)(pak + 4);
    if (!strcmp(plant, "card")) ((CollCard *)(pak + u[7]))[2].x += 3.f;
    if (!strcmp(plant, "wasp")) for (unsigned i = 0; i < u[8]; i++) ((CollWasp *)(pak + u[9]))[i].x += 5.f;
    if (!strcmp(plant, "gag")) for (unsigned i = 0; i < u[10]; i++) ((CollGag *)(pak + u[11]))[i].tx += 3.f;
    if (!strcmp(plant, "trig")) for (unsigned i = 0; i < u[10]; i++) ((CollGag *)(pak + u[11]))[i].tr *= .3f;
    printf("PLANT %s\n", plant);
  }
  int e = collectInit(&C, pak, (unsigned)psize);
  check("pack", e == 0, "%s: cards %u wasps %u gags %u alive %u", e ? collectError(e) : "ok", C.ncard, C.nwasp, C.ngag, C.maxAlive);
  if (e) { printf("RESULT FAIL\n"); return 1; }
  float nowhere[3] = {0, 0, 0}; (void)nowhere; (void)coinBuf;
  coinsInit(&coins, NULL, 0);

  // ===================================================== карточки: подбор ровно в точке данных, не дальше досягаемости
  int ncard = 0, cardsOk = 0, farOk = 0;
  for (int k = 0; k < nex; k++) {
    if (strcmp(ex[k].kind, "card")) continue; ncard++;
    int id = atoi(ex[k].name), got = -1;
    // мимо: сбоку на радиус + досягаемость + 0.4 м (пешком) — не берётся
    float off = ex[k].r + .6f + .4f;
    int n0 = step(ex[k].x + off, ex[k].y - .9f, ex[k].z, 1, -1, 0, 1 / 60.f, CE_CARD, NULL);
    // в точке — берётся ровно эта
    int n1 = step(ex[k].x, ex[k].y - .9f, ex[k].z, 1, -1, 0, 1 / 60.f, CE_CARD, &got);
    if (!n0) farOk++;
    if (n1 == 1 && got >= 0 && C.card[got].id == id) cardsOk++;
    else printf("  карточка %d: в точке событий %d (карточка %d)\n", id, n1, got >= 0 ? C.card[got].id : -1);
  }
  int tot, have = collectCount(&C, 1, COLL_CARDS, &tot);
  check("cards.pickup_at_data_point", ncard == 7 && cardsOk == ncard, "%d/%d карточек взяты в точке данных", cardsOk, ncard);
  check("cards.not_from_afar", farOk == ncard, "%d/%d не взяты с r+1 м", farOk, ncard);
  check("cards.count_api", have == 7 && tot == 7 && collectCards(&C, 1) == 0x7f, "счёт %d/%d маска %#x", have, tot, collectCards(&C, 1));
  { int again = step(ex[0].x, ex[0].y - .9f, ex[0].z, 1, -1, 0, 1 / 60.f, CE_CARD, NULL);
    check("cards.once", again == 0, "повторный подбор: %d событий", again); }

  // ===================================================== гэги: триггер из данных; touch — вход, action — △ внутри
  collectNewGame(&C);
  int ngag = 0, gagOk = 0, gagFar = 0, gagCoins = 0, ncoinGag = 0, fmvOk = 0;
  for (int k = 0; k < nex; k++) {
    if (strcmp(ex[k].kind, "gag")) continue; ngag++;
    int g = gagIndex(ex[k].name);
    if (g < 0) { printf("  гэг %s: нет в пакете\n", ex[k].name); continue; }
    int room = ex[k].room, idx = -1, n = 0, far = 0;
    float out = ex[k].r + .4f;
    for (int visit = 0; visit < 64 && !n; visit++) {   // random: входить в комнату, пока не выпадет этот гэг
      step(0, 50, 0, 1, -1, 0, 1 / 60.f, 0, NULL);                                      // город
      step(ex[k].x + out, ex[k].y, ex[k].z, 1, room, 0, 1 / 60.f, 0, NULL);             // вошёл в комнату мимо триггера
      far += step(ex[k].x + out, ex[k].y, ex[k].z, 1, room, 1, 1 / 60.f, CE_GAG_START, &idx);   // △ мимо — нет
      n = step(ex[k].x + .8f * ex[k].r, ex[k].y, ex[k].z, 1, room, ex[k].trig == 1, 1 / 60.f, CE_GAG_START, &idx);   // у края сферы (0.8 r)
      if (n && idx != g) { n = 0; }
      if (!ex[k].random) break;
    }
    if (n) gagOk++; else printf("  гэг %s: не сработал в триггере данных\n", ex[k].name);
    if (!far) gagFar++; else printf("  гэг %s: сработал мимо триггера\n", ex[k].name);
    if (ex[k].coins) {   // монеты за первый запуск — один раз
      ncoinGag++;
      int c0 = coins.collected + (int)0; unsigned nd0 = 0; for (unsigned i = coins.nworld; i < coins.n; i++) nd0 += coins.c[i].active;
      for (int t = 0; t < 60 * 12; t++) step(ex[k].x, ex[k].y, ex[k].z, 1, room, 0, 1 / 60.f, 0, NULL);
      unsigned nd1 = 0; for (unsigned i = coins.nworld; i < coins.n; i++) nd1 += coins.c[i].active;
      int got = (coins.collected - c0) + (int)(nd1 - nd0);
      int never_ = !GAG_OUTRO_OLD_H && C.gag[g].outro > 0.f && C.gag[g].coinDelay < 0.f;   // круг 7: окончание по кругу навсегда — монет «после конца» нет (gag_melt L1; оригинал)
      if (got == (never_ ? 0 : ex[k].coins)) gagCoins++; else printf("  гэг %s: монет %d, в данных %d%s\n", ex[k].name, got, ex[k].coins, never_ ? " (ждали 0: окончание по кругу)" : "");
      if (strcmp(ex[k].fmv, "-") && !strcmp(C.gag[g].fmv, ex[k].fmv)) fmvOk++;
    }
  }
  have = collectCount(&C, 1, COLL_GAGS, &tot);
  check("gags.fire_at_data_trigger", ngag == 33 && gagOk == ngag, "%d/%d гэгов сработали в 0.8 r от центра данных", gagOk, ngag);
  check("gags.not_outside_trigger", gagFar == ngag, "%d/%d не сработали в r+0.4 м", gagFar, ngag);
  check("gags.coins_once", gagCoins == ncoinGag && ncoinGag == 15, "%d/%d гэгов дали свои монеты", gagCoins, ncoinGag);
  check("gags.count_api", have == 15 && tot == 15, "засчитано %d/%d", have, tot);
  check("gags.loot_fmv", fmvOk == 1, "ролик loot у gag_tele: %d", fmvOk);
  { // single не повторяется, reset — повторяется
    int gs = gagIndex("gag_bbq"), gr = gagIndex("gag_atm"), n1 = 0, n2 = 0;
    const CollGag *S = &C.gag[gs], *R = &C.gag[gr];
    for (int t = 0; t < 3; t++) { step(S->tx + 5, S->ty, S->tz, 1, -1, 0, .1f, 0, NULL); n1 += step(S->tx, S->ty, S->tz, 1, -1, 1, .1f, CE_GAG_START, NULL);
                                  for (int w = 0; w < 100; w++) step(S->tx, S->ty, S->tz, 1, -1, 0, .1f, 0, NULL); }
    step(0, 50, 0, 1, -1, 0, .1f, 0, NULL);
    for (int t = 0; t < 3; t++) { step(R->tx + 5, R->ty, R->tz, 1, R->room, 0, .1f, 0, NULL); n2 += step(R->tx, R->ty, R->tz, 1, R->room, 1, .1f, CE_GAG_START, NULL);
                                  for (int w = 0; w < 100; w++) step(R->tx, R->ty, R->tz, 1, R->room, 0, .1f, 0, NULL); }
    check("gags.single_vs_reset", n1 == 0 && n2 == 3, "single (сыгран) запусков %d, reset %d", n1, n2);
  }

  // ===================================================== осы: появление в дистанции, предел живых, пинок, счёт
  collectNewGame(&C);
  int nwasp = 0, spawnOk = 0, killOk = 0, maxAlive = 0;
  for (int k = 0; k < nex; k++) {
    if (strcmp(ex[k].kind, "wasp")) continue; nwasp++;
    int w = -1; for (unsigned i = 0; i < C.nwasp; i++) if (!strcmp(C.wasp[i].name, ex[k].name)) w = (int)i;
    if (w < 0) { printf("  оса %s: нет в пакете\n", ex[k].name); continue; }
    // игрок в 1 м от точки данных: эта оса (ближайшая из неубитых) появляется; мир заново, убитые — из прогресса
    int idx = -1; uint32_t km = collectWasps(&C, 1); collectNewGame(&C); collectSetProgress(&C, 1, 0, km, 0);
    for (int t = 0; t < 3; t++) step(ex[k].x + 1.f, ex[k].y - 1.5f, ex[k].z, 1, -1, 0, 1 / 60.f, 0, NULL);
    unsigned a = 0; for (unsigned i = 0; i < C.nwasp; i++) a += C.st.waspSt[i] == 1; if ((int)a > maxAlive) maxAlive = (int)a;
    { const SpRun *sr = &C.sp.run[C.wasp[w].inst]; float d = hypotf(sr->x - ex[k].x, sr->z - ex[k].z) + fabsf(sr->y - ex[k].y);
      if (C.st.waspSt[w] == 1 && d < (WASP_BOB_OLD ? .5f : 1.1f)) spawnOk++;   /* сверка g11b W11: оса качается ±0,5 м по вертикали и ±0,5 м вбок вокруг точки данных */ else printf("  оса %s: не появилась в точке данных (живая %d, отклонение %.2f м)\n", ex[k].name, C.st.waspSt[w] == 1, d); }
    // пинок Гомера точно в осу: DESTROYED -> засчитана
    SpRun *r = &C.sp.run[C.wasp[w].inst]; float c3[3], rr; spBounds(&C.sp, C.wasp[w].inst, c3, &rr);
    HomerEvent he = {HOMER_EV_HIT, c3[0], c3[1], c3[2], 1.f, 0, 1};
    collectHomerHit(&C, &he); (void)r;
    int dead = 0; for (int t = 0; t < 30; t++) dead += step(ex[k].x + 1.f, ex[k].y - 1.5f, ex[k].z, 1, -1, 0, 1 / 60.f, CE_WASP_DEAD, &idx);
    if (dead == 1 && idx == w) killOk++; else printf("  оса %s: пинок не убил (%d)\n", ex[k].name, dead);
    for (int t = 0; t < 200; t++) step(ex[k].x + 1.f, ex[k].y - 1.5f, ex[k].z, 1, -1, 0, 1 / 60.f, 0, NULL);   // исчезнуть (2.4 с)
  }
  have = collectCount(&C, 1, COLL_WASPS, &tot);
  check("wasps.spawn_at_data_point", nwasp == 20 && spawnOk == nwasp, "%d/%d появились у точки данных", spawnOk, nwasp);
  check("wasps.kick_destroys", killOk == nwasp, "%d/%d убиты пинком", killOk, nwasp);
  check("wasps.alive_limit", maxAlive <= (int)C.maxAlive, "живых одновременно до %d (предел %u)", maxAlive, C.maxAlive);
  check("wasps.count_api", have == 20 && tot == 20 && collectWasps(&C, 1) == 0xFFFFFu, "счёт %d/%d маска %#x", have, tot, collectWasps(&C, 1));
  { // далеко от всех — ни одна не появляется
    collectNewGame(&C); for (int t = 0; t < 30; t++) step(0, 200, 5000, 1, -1, 0, 1 / 60.f, 0, NULL);
    unsigned a = 0; for (unsigned i = 0; i < C.nwasp; i++) a += C.st.waspSt[i] == 1;
    check("wasps.none_far_away", a == 0, "живых вдали %u", a); }

  // ===================================================== та же игра без экрана: машина сбивает осу, Гомер пинает с прыжка
  if (!strcmp(plant, "") || !strcmp(plant, "wasp")) {
    if (bwData(argv[3], argv[4], NULL)) { check("world.load", 0, "уровень/Гомер не загрузились"); }
    else {
      static BotWorld W; bwNew(&W);
      check("world.collect_loaded", W.haveColl, "пакет в мире ботов: %d", W.haveColl);
      if (W.haveColl && !strcmp(plant, "wasp")) {   // мир ботов читает пакет сам — портим и его
        for (unsigned i = 0; i < W.coll.nwasp; i++) ((CollWasp *)W.coll.wasp)[i].x += 5.f;
      }
      // машина: у осы w_simpsons (стоит на месте, 1.7 м над землёй двора) ставим машину в 12 м и едем на неё
      int w = -1; for (unsigned i = 0; W.haveColl && i < W.coll.nwasp; i++) if (!strcmp(W.coll.wasp[i].name, "w_bonuscar")) w = (int)i;
      if (w >= 0) {
        const CollWasp *cw = &W.coll.wasp[w];
        float gy = 200.f; colGround(bwCol(), cw->x, cw->z + 14.f, 200.f, -50.f, &gy, 0);
        bwPlace(&W, cw->x, gy, cw->z + 14.f, 3.14159265f);   // курс π: вперёд = −z, на осу
        int killed = 0, frames = 0;
        for (; frames < 60 * 12 && !killed; frames++) { bwStep(&W, GP_R, 0, 0); killed = W.coll.st.waspSt[w] == 2; }
        CollEvent ev; while (collectPop(&W.coll, &ev)) {}
        check("world.car_destroys_wasp", killed, "w_bonuscar сбита машиной: %s за %.1f с (кошелёк %d)", killed ? "да" : "нет", frames / 60.f, W.coins.collected);
      }
    }
  }
  printf("RESULT %s %d/%d\n", nfail ? "FAIL" : "PASS", nok, nok + nfail);
  return nfail ? 1 : 0;
}
