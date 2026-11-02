// №1105: SCRAP BOOK главного меню — данные альбома из сохранения (чистый C99, без PSP, общий с хост-тестом tests/collect/scrap1105.c).
// Что показывает альбом оригинала (по сверке P14 №10, M3 №2/Д4–Д10 и чтению экранов оригинала; здесь только числа и условия):
//   на уровне (L / R листают 7 уровней по кругу): сюжетные миссии из 8 (пройденные сюжетные + бонусная), костюмы из 3 (купленные, стартовый не считается),
//   машины из 6 (купленные + машина за бонусную миссию + машина за уличные гонки + стартовая машина), карточки из 7;
//   Game Stats: сюжетные из 49, бонусные из 7, уличные гонки из 21, карточки из 49, костюмы из 21, машины из 35 (стартовые не считаются), осы и гэги — сумма по уровням и сумма всего,
//   ролики из 7, процент игры: среднее по 7 уровням (на уровне — среднее из восьми долей: сюжет/7, бонус/1, гонки/3, костюмы/3, машины/5, карточки/7, осы/всего, гэги/всего) x 0,99 плюс 1,0 при открытом ролике L3.
// Наклейка Красти — процент выше 99,999.
// Данные собранного — из нашей записи (SaveGame): карточки, осы, гэги, купленное (owned), маски пройденных миссий (svLvGet), ролики — маска cfg.movies (ext[71..72]).
// Пропущенные миссии (мисия «пропущена», в галерее с прозрачностью 0,5): в порте пропуска нет (MISSION SKIP не вводился) — не показываются.
// SCRAPBOOK1105_OLD=1 — прежний альбом (только карточки, main_save.h scrapRun).
#ifndef SCRAPDATA_H
#define SCRAPDATA_H
#include <stdint.h>
#include <string.h>
#include "savegame.h"
#include "scrapdata_tab.h"

#ifndef SCRAPBOOK1105_OLD
#define SCRAPBOOK1105_OLD 0
#endif

#define SCRAP_LEVELS 7
#define SCRAP_WASPS_TOTAL 20     // всего ос на уровне (у оригинала по умолчанию 20; в пакетах всех 7 уровней нашего экспорта 20)
#define SCRAP_MOVIE_L3 8         // бит ролика L3 («fmv8», MOVIES[8] main_collect.h): открывается гэгом с кинотеатром L3 / миссией; даёт +1,0 к проценту игры

typedef struct {
  int story[8], bonus[8], races[8], cards[8], skins[8], cars[8];   // по уровням 1..7: сделано (cars — без стартовой машины)
  int carsOwned[8];                                                // машины для строки уровня «из 6»: cars + стартовая машина уровня (когда уровень достигнут)
  int wasps[8], gags[8], gagsTotal[8];
  int movies[8];                                                   // ролик уровня открыт (1/0)
  int reached;                                                     // старший уровень игры (1..7)
  int pctLevel[8];                                                 // процент уровня, десятые доли процента (0..1000)
  int pctGame;                                                     // процент игры, десятые (0..1000)
  unsigned mask[8]; int known[8]; unsigned owned[8];                // маска пройденных миссий уровня (svLvGet), признак (2 записано, 1 пройден без маски, 0 нет), купленное (owned) — для галерей
  int sum[10];                                                     // Game Stats: сюжет, бонус, гонки, карточки, костюмы, машины, осы, гэги, ролики; [9] — всего гэгов
} ScrapData;

// номер бита ролика уровня L в cfg.movies (MOVIES[k]: 2 fmv2 .. 8 fmv8): L1 fmv2, L2 fmv3, L3 fmv8 (ролик «I & S»), L4 fmv4, L5 fmv5, L6 fmv6, L7 fmv7
static inline int scrapMovieBit(int L) { static const int b[8] = {0, 2, 3, 8, 4, 5, 6, 7}; return b[L < 1 ? 0 : L > 7 ? 7 : L]; }

static inline int scrapPop(unsigned long long v) { int n = 0; while (v) { n += (int)(v & 1u); v >>= 1; } return n; }

// owned-бит k награды уровня L (порядок BindReward = scrapTab[L].rew[k]) куплен/получен
static inline int scrapRewOwned(const SaveGame *s, int L, int k, int bonusDone, int racesDone) {
  const ScrapLvTab *t = &scrapTab[L];
  switch (t->rew[k].quest) {
    case 0: return 1;                    // стартовая машина / стартовый костюм: есть с начала
    case 1: return bonusDone;            // машина за бонусную миссию
    case 2: return racesDone;            // машина за уличные гонки
#ifdef SCRAP_RED_OWNEDBIT   // красный контроль хост-теста: сдвиг номера бита награды на единицу
    default: return (s->owned[L] >> (k + 1) & 1u) != 0;
#else
    default: return (s->owned[L] >> k & 1u) != 0;
#endif
  }
}

static inline void scrapFill(ScrapData *d, const SaveGame *s, unsigned moviesMask) {
  memset(d, 0, sizeof *d);
  d->reached = svLvReached(s); if (d->reached < 1) d->reached = 1; if (d->reached > 7) d->reached = 7;
  for (int L = 1; L <= SCRAP_LEVELS; L++) {
    const ScrapLvTab *t = &scrapTab[L];
    uint32_t mask = 0; unsigned rows = 0; int known = svLvGet(s, L, &mask, &rows);   // 2 записано, 1 пройден (маска неизвестна), 0 нет
    d->mask[L] = known == 2 ? mask : 0; d->known[L] = known; d->owned[L] = s->owned[L];
    int story = 0;
    for (int k = 0; k < 7; k++) { int b = t->story[k]; if (b >= 0 && (known == 1 || (known == 2 && (mask >> b & 1u)))) story++; }
    int bonus = known == 2 && t->bonus >= 0 && (mask >> t->bonus & 1u);
    int races = 0; if (known == 2) races = scrapPop(mask & t->races);
    int nraces = scrapPop(t->races);
    d->story[L] = story; d->bonus[L] = bonus; d->races[L] = races;
    d->cards[L] = scrapPop(s->cards[L] & 0x7f);
    int bonusDone = bonus, racesDone = nraces > 0 && races >= nraces;
    int skins = 0, cars = 0;
    for (int k = 0; k < t->nrew; k++) {
      if (t->rew[k].quest == 0) continue;                              // стартовые в «из 3» и «из 5» не считаются
      int own = scrapRewOwned(s, L, k, bonusDone, racesDone);
      if (t->rew[k].car) cars += own; else skins += own;
    }
    d->skins[L] = skins; d->cars[L] = cars; d->carsOwned[L] = cars + (L <= d->reached ? 1 : 0);
    d->wasps[L] = scrapPop(s->wasps[L]); d->gags[L] = scrapPop(s->gags[L]); d->gagsTotal[L] = t->totalGags;
    d->movies[L] = (moviesMask >> scrapMovieBit(L) & 1u) != 0;
    // процент уровня: среднее из восьми долей, десятые доли процента
    int w = d->wasps[L] > SCRAP_WASPS_TOTAL ? SCRAP_WASPS_TOTAL : d->wasps[L], g = d->gags[L] > t->totalGags ? t->totalGags : d->gags[L];
    long num = 0;   // сумма долей в тысячных, потом /8
    num += (long)story * 1000 / 7; num += (long)bonus * 1000; num += nraces ? (long)races * 1000 / nraces : 0; num += (long)skins * 1000 / 3;
    num += (long)cars * 1000 / 5; num += (long)d->cards[L] * 1000 / 7; num += (long)w * 1000 / SCRAP_WASPS_TOTAL; num += t->totalGags ? (long)g * 1000 / t->totalGags : 0;
    d->pctLevel[L] = (int)(num / 8);
    d->sum[0] += story; d->sum[1] += bonus; d->sum[2] += races; d->sum[3] += d->cards[L]; d->sum[4] += skins; d->sum[5] += cars;
    d->sum[6] += d->wasps[L]; d->sum[7] += d->gags[L]; d->sum[8] += d->movies[L]; d->sum[9] += t->totalGags;
  }
  long tot = 0; for (int L = 1; L <= SCRAP_LEVELS; L++) tot += d->pctLevel[L];
  long pct = tot * 99 / 7 / 100;   // среднее по 7 уровням x 0,99
  if (d->movies[3]) pct += 10;     // +1,0 за открытый ролик L3
  d->pctGame = (int)(pct > 1000 ? 1000 : pct);
}

// значения для экранов (поля UI: пары «сделано, всего»): уровень L, раздел 0 миссии, 1 костюмы, 2 машины, 3 карточки
static inline void scrapContents(const ScrapData *d, int L, int out[4][2]) {
  out[0][0] = d->story[L] + d->bonus[L]; out[0][1] = 8;
  out[1][0] = d->skins[L];               out[1][1] = 3;
  out[2][0] = d->carsOwned[L];           out[2][1] = 6;
  out[3][0] = d->cards[L];               out[3][1] = 7;
}
// Game Stats: девять пар и процент игры
static inline void scrapStats(const ScrapData *d, int out[9][2], int *pct) {
  static const int tot[9] = {49, 7, 21, 49, 21, 35, 0, 0, 7};
  for (int k = 0; k < 9; k++) { out[k][0] = d->sum[k]; out[k][1] = tot[k]; }
  out[6][1] = SCRAP_WASPS_TOTAL * SCRAP_LEVELS; out[7][1] = d->sum[9];
  *pct = d->pctGame;
}
// наклейка Красти: игра пройдена на 100 процентов (выше 99,999)
static inline int scrapKrusty(const ScrapData *d) { return d->pctGame >= 1000; }


// ---- галереи (этап 2 №1105) ----
// миссии: слоты 0..6 — сюжетные уровня по порядку, слот 7 — бонусная; открыт — пройдена (в старой записи без маски — все сюжетные пройдены)
static inline int scrapMissionSlot(const ScrapData *d, int L, int i) {
  const ScrapLvTab *t = &scrapTab[L];
  if (i < 7) { int b = t->story[i]; return b >= 0 && (d->known[L] == 1 || (d->known[L] == 2 && (d->mask[L] >> b & 1u))); }
  return d->bonus[L];
}
// машины и костюмы: номера строк таблицы наград уровня по слотам галереи (машины — все шесть по порядку BindReward, костюмы — три на продажу); -> сколько слотов
static inline int scrapRewSlots(int L, int car, int idx[8]) {
  const ScrapLvTab *t = &scrapTab[L]; int n = 0;
  for (int k = 0; k < t->nrew && n < 8; k++) if ((t->rew[k].car != 0) == (car != 0) && (car || t->rew[k].quest == 3)) idx[n++] = k;
  return n;
}
static inline int scrapRewOwnedAt(const ScrapData *d, int L, int k) {
  const ScrapLvTab *t = &scrapTab[L];
  switch (t->rew[k].quest) {
    case 0: return d->known[L] != 0 || L <= d->reached;   // стартовая: есть, когда уровень достигнут
    case 1: return d->bonus[L];
    case 2: { int nr = scrapPop(t->races); return nr > 0 && d->races[L] >= nr; }
#ifdef SCRAP_RED_OWNEDBIT
    default: return (d->owned[L] >> (k + 1) & 1u) != 0;
#else
    default: return (d->owned[L] >> k & 1u) != 0;
#endif
  }
}

#endif
