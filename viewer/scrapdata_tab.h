// СГЕНЕРИРОВАНО tools/scrap_tab.c (№1105): структура уровней для альбома. Не править руками.
#ifndef SCRAPDATA_TAB_H
#define SCRAPDATA_TAB_H
typedef struct { signed char story[7]; signed char bonus; unsigned nrew, totalGags; unsigned races;
  struct { unsigned char car, quest; char name[12]; } rew[12]; } ScrapLvTab;
// quest: 0 defaultcar/defaultskin, 1 bonusmission, 2 streetrace, 3 forsale
static const ScrapLvTab scrapTab[8] = {
  {{-1,-1,-1,-1,-1,-1,-1}, -1, 0, 0, 0, {{0, 0, ""}}},
  {{1,2,3,4,5,6,7}, 12, 10, 15, 0x700, {{1, 0, "famil_v"}, {1, 1, "cletu_v"}, {1, 2, "elect_v"}, {1, 3, "plowk_v"}, {1, 3, "cDuff"}, {1, 3, "cVan"}, {0, 0, "homer"}, {0, 3, "h_undrwr"}, {0, 3, "h_fat"}, {0, 3, "h_stcrobe"}}},   // L1: миссий в файле 13
  {{0,1,2,3,4,5,6}, 12, 10, 11, 0x700, {{1, 0, "honor_v"}, {1, 1, "gramp_v"}, {1, 2, "moe_v"}, {1, 3, "mrplo_v"}, {1, 3, "cLimo"}, {1, 3, "cFire_v"}, {0, 0, "bart"}, {0, 3, "b_tall"}, {0, 3, "b_football"}, {0, 3, "b_ninja"}}},   // L2: миссий в файле 13
  {{0,1,2,3,4,5,6}, 11, 10, 11, 0x380, {{1, 0, "lisa_v"}, {1, 1, "skinn_v"}, {1, 2, "bookb_v"}, {1, 3, "otto_v"}, {1, 3, "cDonut"}, {1, 3, "cNerd"}, {0, 0, "lisa"}, {0, 3, "l_cool"}, {0, 3, "l_florida"}, {0, 3, "l_jersey"}}},   // L3: миссий в файле 13
  {{0,1,2,3,4,5,6}, 11, 10, 15, 0x380, {{1, 0, "marge_v"}, {1, 1, "comic_v"}, {1, 2, "krust_v"}, {1, 3, "willi_v"}, {1, 3, "cKlimo"}, {1, 3, "cCurator"}, {0, 0, "marge"}, {0, 3, "m_prison"}, {0, 3, "m_pink"}, {0, 3, "m_police"}}},   // L4: миссий в файле 12
  {{0,1,2,3,4,5,6}, 12, 10, 6, 0x700, {{1, 0, "apu_v"}, {1, 1, "frink_v"}, {1, 2, "bbman_v"}, {1, 3, "carhom_v"}, {1, 3, "wiggu_v"}, {1, 3, "cCola"}, {0, 0, "apu"}, {0, 3, "a_army"}, {0, 3, "a_american"}, {0, 3, "a_besharp"}}},   // L5: миссий в файле 13
  {{0,1,2,3,4,5,6}, 12, 10, 11, 0x700, {{1, 0, "bart_v"}, {1, 1, "snake_v"}, {1, 2, "burns_v"}, {1, 3, "scorp_v"}, {1, 3, "cArmor"}, {1, 3, "cSedan"}, {0, 0, "bart"}, {0, 3, "b_hugo"}, {0, 3, "b_military"}, {0, 3, "b_man"}}},   // L6: миссий в файле 13
  {{0,1,2,3,4,5,6}, 12, 10, 15, 0x700, {{1, 0, "homer_v"}, {1, 1, "smith_v"}, {1, 2, "fone_v"}, {1, 3, "zombi_v"}, {1, 3, "hbike_v"}, {1, 3, "cHears"}, {0, 0, "homer"}, {0, 3, "h_scuzzy"}, {0, 3, "h_evil"}, {0, 3, "h_donut"}}},   // L7: миссий в файле 13
};
#endif
