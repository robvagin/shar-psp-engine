// №1105: SCRAP BOOK — данные альбома из сохранения (viewer/scrapdata.h). Сохранение с набором собранного → экраны показывают ровно его.
//   cc -std=c11 -DSKIN_HOST -I viewer tests/collect/scrap1105.c viewer/savegame.c viewer/stateprops.c viewer/coins.c -lm   (см. tests/collect/sverka_g11b.sh)
// Красные контроли: -DSCRAPBOOK1105_OLD=1 (прежний альбом: счётчиков нет), -DSCRAP_RED_OWNEDBIT (сдвиг бита награды), -DSCRAP_RED_PCT (процент не умножается на 0,99).
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "scrapdata.h"
static int fail;
#define CHECK(ok, ...) do { int o_ = (ok); printf("%s ", o_ ? "PASS" : "FAIL"); printf(__VA_ARGS__); printf("\n"); fail += !o_; } while (0)
int main(void) {
#if SCRAPBOOK1105_OLD
  printf("FAIL прежний альбом (SCRAPBOOK1105_OLD=1): счётчиков, Game Stats и галерей нет\nRESULT FAIL\n"); return 1;
#endif
  SaveGame s; svClear(&s);
  // уровень 1: сюжетные миссии файла 1, 2, 3 (биты 1..3), бонусная (бит 12), одна уличная гонка (бит 8); куплено: plowk_v (бит 3), h_undrwr (бит 7), h_stcrobe (бит 9); карточки 0 и 2; 5 ос; 3 гэга
  svLvPut(&s, 1, (1u << 1) | (1u << 2) | (1u << 3) | (1u << 12) | (1u << 8), 0);
  s.owned[1] = (1u << 3) | (1u << 7) | (1u << 9); s.cards[1] = 0x05; s.wasps[1] = 0x1f; s.gags[1] = 0x7;
  // уровень 2: все семь сюжетных (биты 0..6), три гонки (8, 9, 10), бонусной нет; куплено 3 машины и ничего из одежды; 7 карточек
  svLvPut(&s, 2, 0x7fu | (7u << 8), 0); s.owned[2] = (1u << 3) | (1u << 4) | (1u << 5); s.cards[2] = 0x7f;
  svLvPut(&s, 3, 0, 0); s.level = 3; s.msnLevel = 0;   // третий уровень достигнут (старший = 3), ничего в нём не пройдено; голова не содержит прогресса уровней
  ScrapData d; scrapFill(&d, &s, (1u << 2) | (1u << 8));   // открыты ролики fmv2 (уровень 1) и fmv8 (уровень 3)
  int c[4][2]; scrapContents(&d, 1, c);
  CHECK(c[0][0] == 4 && c[0][1] == 8, "L1 миссии %d/%d (ждём 4/8: три сюжетные + бонусная)", c[0][0], c[0][1]);
  CHECK(c[1][0] == 2 && c[1][1] == 3, "L1 костюмы %d/%d (ждём 2/3; стартовый не считается)", c[1][0], c[1][1]);
  CHECK(c[2][0] == 3 && c[2][1] == 6, "L1 машины %d/%d (ждём 3/6: куплена plowk_v, бонусная машина, стартовая)", c[2][0], c[2][1]);
  CHECK(c[3][0] == 2 && c[3][1] == 7, "L1 карточки %d/%d (ждём 2/7)", c[3][0], c[3][1]);
  scrapContents(&d, 2, c);
  CHECK(c[0][0] == 7 && c[0][1] == 8 && c[1][0] == 0 && c[2][0] == 5 && c[3][0] == 7, "L2 миссии %d/8 (7), костюмы %d (0), машины %d/6 (5: три купленные + машина за гонки + стартовая), карточки %d (7)", c[0][0], c[1][0], c[2][0], c[3][0]);
  scrapContents(&d, 4, c);
  CHECK(c[0][0] == 0 && c[2][0] == 0, "L4 уровень не достигнут: миссии %d, машины %d (стартовая не считается, ждём 0)", c[0][0], c[2][0]);
  // галереи уровня 1: миссии (сюжетные слоты 0..2 открыты: биты файла 1, 2, 3; бонусная — слот 7), машины (стартовая, бонусная, plowk_v открыты; гонки неполные, cDuff и cVan закрыты), костюмы (h_undrwr, h_stcrobe)
  { int m[8], mw[8] = {1, 1, 1, 0, 0, 0, 0, 1}; for (int i = 0; i < 8; i++) m[i] = scrapMissionSlot(&d, 1, i);
    CHECK(!memcmp(m, mw, sizeof m), "галерея миссий L1: слоты %d%d%d%d%d%d%d%d (ждём 11100001)", m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7]);
    int ix[8], n = scrapRewSlots(1, 1, ix), cw[6] = {1, 1, 0, 1, 0, 0}, ok = n == 6;
    for (int k = 0; k < 6 && ok; k++) ok = scrapRewOwnedAt(&d, 1, ix[k]) == cw[k];
    CHECK(ok, "галерея машин L1: %d слотов, открыты стартовая, бонусная, plowk_v (ждём 110100)", n);
    n = scrapRewSlots(1, 0, ix); int sw[3] = {1, 0, 1}; ok = n == 3;
    for (int k = 0; k < 3 && ok; k++) ok = scrapRewOwnedAt(&d, 1, ix[k]) == sw[k];
    CHECK(ok, "галерея костюмов L1: %d слотов, открыты h_undrwr и h_stcrobe (ждём 101)", n);
    scrapRewSlots(4, 1, ix); CHECK(!scrapRewOwnedAt(&d, 4, ix[0]), "уровень 4 не достигнут: стартовая машина закрыта");
    CHECK(scrapMissionSlot(&d, 2, 6) && !scrapMissionSlot(&d, 2, 7), "галерея миссий L2: седьмая сюжетная открыта, бонусной нет"); }
  int st[9][2], pct; scrapStats(&d, st, &pct);
  CHECK(st[0][0] == 10 && st[0][1] == 49, "Game Stats сюжетные %d/49 (ждём 10 = 3 + 7)", st[0][0]);
  CHECK(st[1][0] == 1 && st[1][1] == 7, "Game Stats бонусные %d/7 (ждём 1)", st[1][0]);
  CHECK(st[2][0] == 4 && st[2][1] == 21, "Game Stats гонки %d/21 (ждём 4 = 1 + 3)", st[2][0]);
  CHECK(st[3][0] == 9 && st[3][1] == 49, "Game Stats карточки %d/49 (ждём 9)", st[3][0]);
  CHECK(st[4][0] == 2 && st[4][1] == 21, "Game Stats костюмы %d/21 (ждём 2)", st[4][0]);
  CHECK(st[5][0] == 6 && st[5][1] == 35, "Game Stats машины %d/35 (ждём 6: L1 бонусная и plowk_v, L2 три купленные и машина за гонки)", st[5][0]);
  CHECK(st[6][0] == 5 && st[6][1] == 140, "Game Stats осы %d/%d (ждём 5/140)", st[6][0], st[6][1]);
  CHECK(st[7][0] == 3 && st[7][1] == 15 + 11 + 11 + 15 + 6 + 11 + 15, "Game Stats гэги %d/%d (ждём 3/84)", st[7][0], st[7][1]);
  CHECK(st[8][0] == 2 && st[8][1] == 7, "Game Stats ролики %d/7 (ждём 2: fmv2 и fmv8)", st[8][0]);
  // процент: независимый расчёт в double
  double l1 = (3 / 7.0 + 1 + 1 / 3.0 + 2 / 3.0 + 2 / 5.0 + 2 / 7.0 + 5 / 20.0 + 3 / 15.0) / 8, l2 = (1 + 0 + 1 + 0 + 4 / 5.0 + 1 + 0 + 0) / 8;
  double want = (l1 + l2) / 7 * 100 * 0.99 + 1.0;
#ifdef SCRAP_RED_PCT
  want = (l1 + l2) / 7 * 100 + 1.0;
#endif
  CHECK(fabs(pct / 10.0 - want) < 0.15, "процент игры %.1f %% (ждём %.1f: среднее по 7 уровням x 0,99 + 1,0 за ролик L3)", pct / 10.0, want);
  CHECK(!scrapKrusty(&d), "наклейка Красти не показывается при %.1f %%", pct / 10.0);
  // 100 процентов: всё собрано
  SaveGame f; svClear(&f); f.msnLevel = 0; f.level = 7; int gt[8] = {0, 15, 11, 11, 15, 6, 11, 15};
  for (int L = 1; L <= 7; L++) {
    uint32_t m = 0; for (int k = 0; k < 7; k++) if (scrapTab[L].story[k] >= 0) m |= 1u << scrapTab[L].story[k];
    m |= scrapTab[L].races; if (scrapTab[L].bonus >= 0) m |= 1u << scrapTab[L].bonus; svLvPut(&f, L, m, 0);
    f.cards[L] = 0x7f; f.wasps[L] = (1u << 20) - 1; f.gags[L] = (1ull << gt[L]) - 1;
    for (int k = 0; k < scrapTab[L].nrew; k++) if (scrapTab[L].rew[k].quest == 3) f.owned[L] |= 1u << k;
  }
  scrapFill(&d, &f, 0x1fcu);   // fmv2..fmv8
  scrapStats(&d, st, &pct);
  CHECK(pct == 1000 && scrapKrusty(&d), "всё собрано: процент %.1f %%, наклейка Красти есть", pct / 10.0);
  CHECK(st[0][0] == 49 && st[1][0] == 7 && st[2][0] == 21 && st[3][0] == 49 && st[4][0] == 21 && st[5][0] == 35 && st[8][0] == 7, "всё собрано: 49/49, 7/7, 21/21, 49/49, 21/21, 35/35, ролики 7/7");
  printf("RESULT %s\n", fail ? "FAIL" : "PASS"); return !!fail;
}
