// Круг 6 (g11b-collect): гэги. Каждая проверка падает на своём выключателе _OLD=1.
//   P16-14 GAG_SOUNDWAIT_OLD  гэг «играет», пока идёт его звук (gagHold), конец только после звука и анимации   (ложное: конец по длине анимации)
//   P16-Д5 GAG_COINS_AUTO_OLD монеты гэга на улице после первого касания земли сами летят к игроку             (ложное: лежат до подбора)
//   P05:9  CARD_EXACT_OLD      карточка берётся точкой строго внутри сферы локатора (центр игрока = ступни + 0,9 м)  (ложное: запас 0,6 м пешком)
//   P16-Д7 GAG_MISSION_OLD    гэги с роликом (gag_tele, loot) в миссии не срабатывают; обычные гэги срабатывают   (ложное: срабатывают все)
//   круг 7 P15:Д2 GAG_OUTRO_OLD / GAG_INTRO_OLD  гэг с окончанием по кругу (gag_melt: intro 1, outro 60) никогда не кончается и не даёт монет «после конца»; ждущий гэг крутит вступление   (ложное: конец и монета по длительности)
// Пакет build/collect-l1.shr, без мира и Гомера. См. tests/collect/sverka_g11b.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "collect.h"
#include "coins.h"
static Collect c; static unsigned char *blob; static int fail; static const char *PATH;
#define CHECK(ok, ...) do { int o_ = (ok); printf("%s ", o_ ? "PASS" : "FAIL"); printf(__VA_ARGS__); printf("\n"); fail += !o_; } while (0)
static void fresh(void) {
  static int have; if (have) { collectFree(&c); free(blob); } have = 1;
  FILE *f = fopen(PATH, "rb"); fseek(f, 0, SEEK_END); unsigned n = ftell(f); rewind(f);
  blob = aligned_alloc(16, (n + 15u) & ~15u); if (fread(blob, 1, n, f) != n) exit(2); fclose(f);
  if (collectInit(&c, blob, n)) exit(2);
}
static int gagBy(const char *nm) { for (unsigned i = 0; i < c.ngag; i++) if (!strcmp(c.gag[i].name, nm)) return (int)i; exit(2); }
static int cnt(int type, int idx) { int n = 0; CollEvent e; while (collectPop(&c, &e)) n += e.type == type && (idx < 0 || e.idx == idx); return n; }
int main(int argc, char **argv) {
  if (argc != 2) return 2; PATH = argv[1];
  Coins cs; float far3[3] = {9000.f, 0.f, 9000.f};
  // ---- P16-14: звук длиннее анимации (gag_bbq, анимация 5,7 с) удерживает гэг «играет»
  fresh(); int g = gagBy("gag_bbq"); const CollGag *G = &c.gag[g]; CollPlayer p = {G->x + 30.f, G->y, G->z, 1, -1, 0};
  collectGagPlay(&c, (unsigned)g); c.st.gagHold = g + 1;
  for (int f = 0; f < 60 * 9; f++) collectStep(&c, &p, 0, 0, 0, 0, 1.f / 60.f);
  int endedHeld = cnt(CE_GAG_END, g), stillHeld = c.st.gagSt[g] == 1;
  c.st.gagHold = 0; for (int f = 0; f < 10; f++) collectStep(&c, &p, 0, 0, 0, 0, 1.f / 60.f);
  int endedFree = cnt(CE_GAG_END, g);
  CHECK(stillHeld && endedHeld == 0 && endedFree == 1, "P16-14 звук ещё идёт на 9 с (анимация 5,7 с): гэг играет (%d), конца нет (%d); звук кончился: конец (%d)", stillHeld, endedHeld, endedFree);
  // ---- P16-Д5: монеты гэга на улице: после падения сами летят к игроку
  fresh(); g = gagBy("gag_bbq"); G = &c.gag[g]; memset(&cs, 0, sizeof cs); coinsInit(&cs, far3, 1); p = (CollPlayer){G->x + 30.f, G->y, G->z, 1, -1, 0};
  collectGagPlay(&c, (unsigned)g);
  for (int f = 0; f < 60 * 7; f++) collectStep(&c, &p, 0, 0, 0, &cs, 1.f / 60.f);
  unsigned autoN = 0, poolN = 0; for (unsigned i = cs.nworld; i < cs.n; i++) if (cs.c[i].active) { poolN++; autoN += cs.c[i].autoCollect; }
  CHECK(poolN == (unsigned)G->coins && autoN == poolN, "P16-Д5 монет гэга в воздухе %u из %u, сами летят к игроку после касания земли: %u", poolN, G->coins, autoN);
  coinsFree(&cs);
  // ---- круг 7 (P15:Д2): gag_melt (outro 60): играет 60 с — не кончается (конца нет, монет нет, стоит «играет»); ждущий — круг вступления включён
  fresh(); g = gagBy("gag_melt"); G = &c.gag[g]; memset(&cs, 0, sizeof cs); coinsInit(&cs, far3, 1); p = (CollPlayer){G->x + 30.f, G->y, G->z, 1, -1, 0};
  for (int f = 0; f < 3; f++) collectStep(&c, &p, 0, 0, 0, &cs, 1.f / 60.f);
  int introOn = c.sp.run[G->inst].loopOn;
  collectGagPlay(&c, (unsigned)g);
  for (int f = 0; f < 60 * 60; f++) collectStep(&c, &p, 0, 0, 0, &cs, 1.f / 60.f);
  unsigned coinN = 0; for (unsigned i = cs.nworld; i < cs.n; i++) coinN += cs.c[i].active;
  int endsMelt = cnt(CE_GAG_END, g), stillMelt = c.st.gagSt[g] == 1, loopMelt = c.sp.run[G->inst].loopOn;
  CHECK(introOn && stillMelt && endsMelt == 0 && coinN == 0 && c.st.gagFound[g] && loopMelt, "круг 7 gag_melt: вступление идёт (%d); за 60 с гэг играет (%d), конца %d, монет %u (ждём 0), засчитан найденным (%d), окончание по кругу (%d)", introOn, stillMelt, endsMelt, coinN, c.st.gagFound[g], loopMelt);
  coinsFree(&cs);
  // ---- P16-Д7: в миссии гэг с роликом (gag_tele) не срабатывает, обычный (gag_alm2 той же комнаты) срабатывает
  for (int k = 0; k < 2; k++) {
    const char *nm = k ? "gag_alm2" : "gag_tele";
    for (int mis = 0; mis < 2; mis++) {
      fresh(); g = gagBy(nm); G = &c.gag[g]; c.st.inMission = mis;
      p = (CollPlayer){G->tx + 5.f, G->ty, G->tz, 1, G->room, 0}; collectStep(&c, &p, 0, 0, 0, 0, 1.f / 30.f);   // вошёл в комнату (снаружи сферы)
      p.x = G->tx; p.action = 1; for (int f = 0; f < 4; f++) collectStep(&c, &p, 0, 0, 0, 0, 1.f / 30.f);
      int started = cnt(CE_GAG_START, g);
      int want = k ? 1 : !mis;   // обычный — всегда; с роликом — только вне миссии
      CHECK(started == want, "P16-Д7 %s %s: стартов %d (ждём %d)", nm, mis ? "в миссии" : "в свободной езде", started, want);
    }
  }
  // ---- P05:9 (круг 2): карточка 0, пешком; центр игрока = ступни + 0,9 м; снаружи сферы на 0,3 м — не берётся (запас 0,6 м был), внутри на 0,2 м — берётся
  for (int k = 0; k < 2; k++) {
    fresh(); const CollCard *cd = &c.card[0]; float d = cd->r + (k ? -0.2f : 0.3f);
    p = (CollPlayer){cd->x + d, cd->y - .9f, cd->z, 1, -1, 0}; for (int f = 0; f < 3; f++) collectStep(&c, &p, 0, 0, 0, 0, 1.f / 30.f);
    CHECK(c.st.cardGot[0] == k, "P05:9 карточка, игрок на %.2f м от центра сферы (радиус %.2f): взята %d (ждём %d)", d, cd->r, c.st.cardGot[0], k);
  }
  // ---- круг 7, P05:9: из машины запас 1,5 м (как в оригинале: сфера 1,5 м вокруг центра машины — сферы касаются при расстоянии до радиус + 1,5): на радиус + 1,2 берётся, на радиус + 1,8 нет
  for (int k = 0; k < 2; k++) {
    fresh(); const CollCard *cd = &c.card[0]; float d = cd->r + (k ? 1.2f : 1.8f);
    p = (CollPlayer){cd->x + d, cd->y - .9f, cd->z, 0, -1, 0}; for (int f = 0; f < 3; f++) collectStep(&c, &p, 0, 0, 0, 0, 1.f / 30.f);
    CHECK(c.st.cardGot[0] == k, "круг 7 P05:9 машина на %.2f м от центра сферы (радиус %.2f, запас 1,5): взята %d (ждём %d)", d, cd->r, c.st.cardGot[0], k);
  }
  // ---- круг 7: гэг-касание на улице, въезд на машине: сфера 1,5 м вокруг машины касается триггера — срабатывает на радиус + 1,2, не на радиус + 1,8
  { int gi = -1; fresh(); for (unsigned i = 0; i < c.ngag; i++) if (c.gag[i].trig == COLL_TOUCH && !c.gag[i].pad && !c.gag[i].fmv[0] && c.gag[i].tr > 0.5f && c.gag[i].tr < 8.f) { gi = (int)i; break; }   // в L1 касания только в комнатах: берём один и выносим на улицу (пакет в нашей памяти)
    for (int k = 0; gi >= 0 && k < 2; k++) {
      fresh(); ((CollGag *)&c.gag[gi])->room = -1; ((CollGag *)&c.gag[gi])->random = 0; G = &c.gag[gi]; float d = G->tr + (k ? 1.2f : 1.8f);
      p = (CollPlayer){G->tx + 40.f, G->ty, G->tz, 0, -1, 0}; collectStep(&c, &p, 0, 0, 0, 0, 1.f / 30.f);   // снаружи
      p.x = G->tx + d; for (int f = 0; f < 3; f++) collectStep(&c, &p, 0, 0, 0, 0, 1.f / 30.f);
      int st = cnt(CE_GAG_START, gi);
      CHECK(st == k, "круг 7 гэг %s касанием, машина на %.2f м от центра триггера (радиус %.2f, запас 1,5): стартов %d (ждём %d)", G->name, d, G->tr, st, k);
    }
    CHECK(gi >= 0, "круг 7 в пакете есть уличный гэг-касание (%d)", gi); }
  printf("RESULT %s\n", fail ? "FAIL" : "PASS"); return !!fail;
}
