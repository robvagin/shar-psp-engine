// Формат записи сохранения (см. savegame.h). Чистый C99: собирается и на PSP, и на Маке (tests/savegame/host.sh).
#include "savegame.h"
#include "collect.h"
#include "mission/msn.h"
#include <string.h>
#include <stdio.h>

// SV_BROKEN_CRC — только для отрицательного контроля проверки на Маке (tests/savegame/host.sh --negative):
// сборка, которая не сверяет CRC, обязана провалить тест испорченного файла
#ifdef SV_BROKEN_CRC
#define SV_CHECK_CRC 0
#else
#define SV_CHECK_CRC 1
#endif

uint32_t svCrc32(const uint8_t *p, unsigned n) {
  uint32_t c = 0xffffffffu;
  for (unsigned i = 0; i < n; i++) {
    c ^= p[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
  }
  return ~c;
}

void svClear(SaveGame *s) {
  memset(s, 0, sizeof *s);
  s->level = 1; s->msnLevel = 1; s->msnStory = 0xff;
  s->music = s->effects = s->vehicle = s->voice = 1000; s->radar = 1;   // как main.c cfg по умолчанию
}

// ---- запись полей по порядку (little endian), без зависимости от выравнивания структуры
typedef struct { uint8_t *p; unsigned n, at; int bad; } Cur;
static void p8(Cur *c, uint32_t v) { if (c->at + 1 > c->n) { c->bad = 1; return; } c->p[c->at++] = (uint8_t)v; }
static void p16(Cur *c, uint32_t v) { p8(c, v & 0xff); p8(c, v >> 8); }
static void p32(Cur *c, uint32_t v) { p16(c, v & 0xffff); p16(c, v >> 16); }
static void p64(Cur *c, uint64_t v) { p32(c, (uint32_t)v); p32(c, (uint32_t)(v >> 32)); }
static uint32_t g8(Cur *c) { if (c->at + 1 > c->n) { c->bad = 1; return 0; } return c->p[c->at++]; }
static uint32_t g16(Cur *c) { uint32_t a = g8(c); return a | g8(c) << 8; }
static uint32_t g32(Cur *c) { uint32_t a = g16(c); return a | g16(c) << 16; }
static uint64_t g64(Cur *c) { uint64_t a = g32(c); return a | (uint64_t)g32(c) << 32; }

#define HDR 12   // "SHSV" + версия 2 + размер 2 + CRC 4 (голова; CRC — по байтам 12..255)

// Правка 334: запись = голова версии 2 (байты 0..255, ровно как до 05.10: прежние сборки читают её и не видят хвоста) + хвост
// с байта 256 со своей проверкой: "SHSX", версия хвоста, размер хвоста, CRC-32 тела хвоста, тело (разовые предметы, ext)
#define TAIL SV_BYTES_V2                    // начало хвоста
#define TAIL_HDR 12                         // "SHSX" + версия 2 + размер 2 + CRC 4
int svWriteVer(const SaveGame *s, uint8_t *out, unsigned n, int ver) {
  unsigned size = ver == 3 ? SV_BYTES_OLD : SV_BYTES_V2;
  if (ver < 1 || ver > 3 || !out || n < size) return -1;
  memset(out, 0, size);
  Cur c = {out, SV_BYTES_V2, 0, 0};
  memcpy(out, "SHSV", 4); c.at = 4; p16(&c, ver == 1 ? 1u : 2u); p16(&c, SV_BYTES_V2); p32(&c, 0);   // в голове 3-й раскладки — версия 2
  p32(&c, s->seq); p16(&c, s->year); p8(&c, s->month); p8(&c, s->day); p8(&c, s->hour); p8(&c, s->minute); p8(&c, s->level);
  p8(&c, s->msnLevel); p8(&c, s->msnStory); p8(&c, s->msnPhase); p8(&c, s->msnStage); p32(&c, s->msnCompleted);
  for (int i = 0; i < SV_LEVELS; i++) p8(&c, s->cards[i]);
  for (int i = 0; i < SV_LEVELS; i++) p32(&c, s->wasps[i]);
  for (int i = 0; i < SV_LEVELS; i++) p64(&c, s->gags[i]);
  p32(&c, (uint32_t)s->coins); p16(&c, s->nworld);
  for (unsigned i = 0; i < sizeof s->worldCoins; i++) p8(&c, s->worldCoins[i]);
  p32(&c, (uint32_t)s->music); p32(&c, (uint32_t)s->effects); p32(&c, (uint32_t)s->vehicle); p32(&c, (uint32_t)s->voice);
  p32(&c, (uint32_t)s->radar);
  if (ver >= 2) {   // v1 кончалась настройками (дальше нули)
#ifndef SV_BROKEN_OWNED
    for (int i = 0; i < SV_LEVELS; i++) p16(&c, s->owned[i]);   // v2
    p8(&c, s->skin);                                            // правка 107: костюм (до 30.09 — ноль)
#else
    for (int i = 0; i < SV_LEVELS; i++) p16(&c, 0);             // отрицательный контроль tests/savegame: купленное не пишется
#endif
  }
  int bad = c.bad;
  uint32_t crc = svCrc32(out + HDR, SV_BYTES_V2 - HDR);
  c.at = 8; p32(&c, crc);
  if (ver == 3) {   // правка 334: хвост — разовые предметы по уровням, затем запас ext
    Cur t = {out, SV_BYTES_OLD, TAIL, 0};
    memcpy(out + TAIL, "SHSX", 4); t.at = TAIL + 4; p16(&t, SV_TAIL_VERSION); p16(&t, SV_BYTES_OLD - TAIL); p32(&t, 0);
#ifndef SV_BROKEN_ONCE
    for (int L = 0; L < SV_LEVELS; L++) {
      p16(&t, s->once[L].n); p16(&t, s->once[L].sig);
      for (unsigned i = 0; i < SV_ONCE_BYTES; i++) p8(&t, s->once[L].st[i]);
    }
#else
    t.at += SV_LEVELS * (4 + SV_ONCE_BYTES);                    // отрицательный контроль tests/savegame: разовые не пишутся
#endif
    for (unsigned i = 0; i < SV_EXT_BYTES; i++) p8(&t, s->ext[i]);
    bad |= t.bad || t.at != SV_BYTES_OLD;
    uint32_t tc = svCrc32(out + TAIL + TAIL_HDR, SV_BYTES_OLD - TAIL - TAIL_HDR);
    t.at = TAIL + 8; p32(&t, tc);
  }
  return bad ? -1 : (int)size;
}
static int svWrite3(const SaveGame *s, uint8_t *out);
static int svRead3(const uint8_t *in, unsigned n, SaveGame *s);
static int svReadOld(const uint8_t *in, unsigned n, SaveGame *s);
int svWrite(const SaveGame *s, uint8_t out[SV_BYTES]) { return SAVEV3_1114_OLD ? svWriteVer(s, out, SV_BYTES_OLD, 3) : svWrite3(s, out); }

static int liveBest[SV_LEVELS][SV_MREC_N];   // №1114: живая таблица лучших времён и попыток (svBestNote)
static uint16_t liveTry[SV_LEVELS][SV_MREC_N];
static int vol(int32_t v) { return v >= 0 && v <= 1000; }

int svIsOldFormat(const uint8_t *in, unsigned n) { return in && n >= SV_BYTES_V2 && !memcmp(in, "SHSV", 4); }
// перевод: прежняя запись читается своим разбором (тот же SaveGame в памяти: настройки, прогресс уровней, прочность машин и секунды лежат в ext, как и раньше),
// лучшие времена и попытки пусты; следующая svWrite кладёт всё в SHV3. Границы и CRC прежнего формата прежние — испорченная прежняя запись по-прежнему «Damaged»
int svRead(const uint8_t *in, unsigned n, SaveGame *s) {
  if (SAVEV3_1114_OLD) return svReadOld(in, n, s);
  if (!SAVEV3CONV_OLD && svIsOldFormat(in, n)) return svReadOld(in, n, s);
  return svRead3(in, n, s);
}
static int svReadOld(const uint8_t *in, unsigned n, SaveGame *s) {
  if (!in || n < SV_BYTES_V2 || memcmp(in, "SHSV", 4)) return SV_DAMAGED;
  // правка 334: ровно 256 Б — запись до 05.10 (файл save<NN>.bin); больше, но меньше полной — оборвана; полная (и буфер утилиты
  // 1 КБ) — хвост: нули за головой — запись до 05.10, иначе хвост обязан сойтись (магия, версия, размер, CRC)
  if (n > SV_BYTES_V2 && n < SV_BYTES_OLD) return SV_DAMAGED;
  Cur c = {(uint8_t *)in, SV_BYTES_V2, 4, 0};
  int ver = g16(&c); if ((ver != SV_VERSION && ver != 1) || g16(&c) != SV_BYTES_V2) return SV_DAMAGED;   // версия 1 — без купленного
  uint32_t crc = g32(&c);
  if (SV_CHECK_CRC && crc != svCrc32(in + HDR, SV_BYTES_V2 - HDR)) return SV_DAMAGED;
  SaveGame t; memset(&t, 0, sizeof t);
  t.seq = g32(&c); t.year = (uint16_t)g16(&c); t.month = (uint8_t)g8(&c); t.day = (uint8_t)g8(&c); t.hour = (uint8_t)g8(&c);
  t.minute = (uint8_t)g8(&c); t.level = (uint8_t)g8(&c);
  t.msnLevel = (uint8_t)g8(&c); t.msnStory = (uint8_t)g8(&c); t.msnPhase = (uint8_t)g8(&c); t.msnStage = (uint8_t)g8(&c);
  t.msnCompleted = g32(&c);
  for (int i = 0; i < SV_LEVELS; i++) t.cards[i] = (uint8_t)g8(&c);
  for (int i = 0; i < SV_LEVELS; i++) t.wasps[i] = g32(&c);
  for (int i = 0; i < SV_LEVELS; i++) t.gags[i] = g64(&c);
  t.coins = (int32_t)g32(&c); t.nworld = (uint16_t)g16(&c);
  for (unsigned i = 0; i < sizeof t.worldCoins; i++) t.worldCoins[i] = (uint8_t)g8(&c);
  t.music = (int32_t)g32(&c); t.effects = (int32_t)g32(&c); t.vehicle = (int32_t)g32(&c); t.voice = (int32_t)g32(&c);
  t.radar = (int32_t)g32(&c);
  if (ver >= 2) for (int i = 0; i < SV_LEVELS; i++) t.owned[i] = (uint16_t)g16(&c);
  if (ver >= 2) t.skin = (uint8_t)g8(&c);   // правка 107: в записях до 30.09 — ноль (обычный Гомер)
  int tailBad = 0;
  if (n >= SV_BYTES_OLD) {   // правка 334: хвост
    int zero = 1; for (unsigned i = TAIL; i < SV_BYTES_OLD && zero; i++) zero = in[i] == 0;
    if (!zero) {
      Cur u = {(uint8_t *)in, SV_BYTES_OLD, TAIL + 4, 0};
      unsigned tv = g16(&u), tsz = g16(&u); uint32_t tc = g32(&u);
      if (memcmp(in + TAIL, "SHSX", 4) || tv != SV_TAIL_VERSION || tsz != SV_BYTES_OLD - TAIL ||
          (SV_CHECK_CRC && tc != svCrc32(in + TAIL + TAIL_HDR, SV_BYTES_OLD - TAIL - TAIL_HDR))) return SV_DAMAGED;
      for (int L = 0; L < SV_LEVELS; L++) {
        t.once[L].n = (uint16_t)g16(&u); t.once[L].sig = (uint16_t)g16(&u);
        for (unsigned i = 0; i < SV_ONCE_BYTES; i++) t.once[L].st[i] = (uint8_t)g8(&u);
        if (t.once[L].n > SV_ONCE_MAX) tailBad = 1;
      }
      for (unsigned i = 0; i < SV_EXT_BYTES; i++) t.ext[i] = (uint8_t)g8(&u);
      tailBad |= u.bad;
    }
  }
  // границы: испорченное, но с верным CRC (или чужая версия записи) не должно попасть в игру
  if (c.bad || tailBad || t.level < 1 || t.level >= SV_LEVELS || t.msnLevel < 1 || t.msnLevel >= SV_LEVELS ||
      (t.msnStory != 0xff && t.msnStory >= 32) || t.msnPhase > 1 || t.coins < 0 || t.coins > 999999 ||
      t.nworld > SV_WORLDCOINS || !vol(t.music) || !vol(t.effects) || !vol(t.vehicle) || !vol(t.voice) ||
      t.radar < 0 || t.radar > 1 || t.month > 12 || t.day > 31 || t.hour > 23 || t.minute > 59 || t.skin > 7)
    return SV_DAMAGED;
  *s = t;
  return SV_OK;
}

int svMissionsDone(const SaveGame *s) {
  int n = 0; for (uint32_t m = s->msnCompleted; m; m &= m - 1) n++;
  return n;
}

int svProgressText(const SaveGame *s, char *out, int n) {
  int wc = 0; for (unsigned i = 0; i < sizeof s->worldCoins; i++) for (uint8_t b = s->worldCoins[i]; b; b &= b - 1) wc++;
  char cw[SV_LEVELS * 3 + 1]; int k = 0;
  for (int i = 1; i < SV_LEVELS; i++) k += snprintf(cw + k, sizeof cw - k, i > 1 ? ",%x" : "%x", s->cards[i]);
  return snprintf(out, n, "level=%d msn=%08x story=%d phase=%d stage=%d cards=%s wasps=%x gags=%llx coins=%d wcoins=%d/%d "
                  "vol=%d,%d,%d,%d radar=%d owned=%x once=%d", s->level, (unsigned)s->msnCompleted, s->msnStory == 0xff ? -1 : s->msnStory,
                  s->msnPhase, s->msnStage, cw, (unsigned)s->wasps[s->level], (unsigned long long)s->gags[s->level], (int)s->coins, wc,
                  s->nworld, (int)s->music, (int)s->effects, (int)s->vehicle, (int)s->voice, (int)s->radar, (unsigned)s->owned[s->level < SV_LEVELS ? s->level : 0],
                  svOnceCount(&s->once[s->level < SV_LEVELS ? s->level : 0]));
}

void svCapture(SaveGame *s, const MsnProgress *p, const Collect *c, const Coins *k) {
  if (p) { s->msnLevel = p->level; s->msnStory = p->story; s->msnPhase = p->phase; s->msnStage = p->stage; s->msnCompleted = p->completed; }
  if (c) {
    if (c->level >= 1 && c->level < SV_LEVELS) s->level = (uint8_t)c->level;
    for (int L = 1; L < SV_LEVELS && L < COLL_LEVELS; L++) { s->cards[L] = (uint8_t)collectCards(c, L); s->wasps[L] = collectWasps(c, L); s->gags[L] = collectGags(c, L); }
  }
  for (int L = 1; L < SV_LEVELS; L++) for (int m = 0; m < SV_MREC_N; m++) { s->mrec[L][m].best = svBestLive(L, m); s->mrec[L][m].attempts = (uint16_t)svAttemptsLive(L, m); }   // №1114
  if (k) {
    s->coins = k->collected; s->nworld = (uint16_t)(k->nworld < SV_WORLDCOINS ? k->nworld : SV_WORLDCOINS);
    memset(s->worldCoins, 0, sizeof s->worldCoins);
    for (unsigned i = 0; i < s->nworld; i++) if (!k->c[i].active || k->c[i].collecting) s->worldCoins[i >> 3] |= (uint8_t)(1u << (i & 7));
    for (int L = 0; L < SV_LEVELS && L < 8; L++) s->owned[L] = (uint16_t)k->owned[L];
  }
}
void svApply(const SaveGame *s, Collect *c, Coins *k) {
  svBestReset();   // №1114: загрузка заменяет лучшие времена и попытки записанными
  for (int L = 1; L < SV_LEVELS; L++) for (int m = 0; m < SV_MREC_N; m++) { if (s->mrec[L][m].best > 0) svBestNote(L, m, s->mrec[L][m].best); liveTry[L][m] = s->mrec[L][m].attempts; }
  if (c) for (int L = 1; L < SV_LEVELS && L < COLL_LEVELS; L++) collectSetProgress(c, L, s->cards[L], s->wasps[L], s->gags[L]);
  if (k) {
    k->collected = s->coins;
    for (int L = 0; L < SV_LEVELS && L < 8; L++) k->owned[L] = s->owned[L];   // купленное (v1 — ничего)
    if (s->nworld == k->nworld) for (unsigned i = 0; i < k->nworld; i++) if (s->worldCoins[i >> 3] >> (i & 7) & 1) k->c[i].active = 0;   // другой уровень/пакет — монеты мира не трогаем
  }
}
void svMsnProgress(const SaveGame *s, MsnProgress *p) {
  memset(p, 0, sizeof *p);
  p->version = 1; p->level = s->msnLevel; p->story = s->msnStory; p->phase = s->msnPhase; p->stage = s->msnStage; p->completed = s->msnCompleted;
}

// ---- правка 334: разовые предметы уровня (stateprops.h spOnce, savegame.h SvOnce)
unsigned svOnceSig(const StateProps *sp, unsigned *n) {   // отпечаток: тип и место (дм) каждого разового по порядку экземпляров
  uint32_t h = 2166136261u; unsigned k = 0;
  for (unsigned i = 0; sp && i < sp->ninst; i++) {
    if (!spOnce(sp, i)) continue;
    const SpInst *I = &sp->inst[i];
    int32_t v[4] = {(int32_t)I->type, (int32_t)(I->x * 10.f), (int32_t)(I->y * 10.f), (int32_t)(I->z * 10.f)};
    for (int j = 0; j < 4; j++) for (int b = 0; b < 4; b++) { h ^= (uint8_t)((uint32_t)v[j] >> (8 * b)); h *= 16777619u; }
    k++;
  }
  if (n) *n = k;
  return (h ^ (h >> 16)) & 0xFFFFu;
}
void svOnceCapture(SvOnce *o, const StateProps *sp) {
  memset(o, 0, sizeof *o);
  unsigned n; o->sig = (uint16_t)svOnceSig(sp, &n); o->n = (uint16_t)(n < SV_ONCE_MAX ? n : SV_ONCE_MAX);
  for (unsigned i = 0, k = 0; sp && i < sp->ninst && k < o->n; i++) {
    if (!spOnce(sp, i)) continue;
    unsigned st = sp->run[i].state;                            // 0 — цел (машины состояний диска в s0 не возвращаются)
    if (st > 15) st = 15;
    o->st[k >> 1] |= (uint8_t)(st << ((k & 1) * 4)); k++;
  }
}
int svOnceApply(const SvOnce *o, StateProps *sp) {
  unsigned n; uint16_t sig = (uint16_t)svOnceSig(sp, &n);
  if (!o->n) return 0;                                        // записи нет (новая игра, v1/v2, прежняя сборка)
  if (o->n != (n < SV_ONCE_MAX ? n : SV_ONCE_MAX) || o->sig != sig) return -1;
  int got = 0;
  for (unsigned i = 0, k = 0; i < sp->ninst && k < o->n; i++) {
    if (!spOnce(sp, i)) continue;
    unsigned st = (o->st[k >> 1] >> ((k & 1) * 4)) & 15u; k++;
    if (!st) continue;
    spRestore(sp, i, (int)st); got++;
  }
  return got;
}
int svOnceCount(const SvOnce *o) {
  int c = 0; for (unsigned k = 0; k < o->n && k < SV_ONCE_MAX; k++) c += ((o->st[k >> 1] >> ((k & 1) * 4)) & 15u) != 0;
  return c;
}

// ---- облако 05.10 (карьера): прогресс миссий всех уровней в ext (savegame.h, SV_EXT_LV)
static uint8_t *lvRec(SaveGame *s, int L) { return s->ext + SV_EXT_LV_REC + (L - 1) * SV_EXT_LV_STRIDE; }
void svLvPut(SaveGame *s, int L, uint32_t mask, unsigned rows) {
#ifdef SV_BROKEN_LV
  return;   // отрицательный контроль tests/career/lvprog.sh: прогресс уровней не пишется (как до 05.10)
#endif
  if (!s || L < 1 || L >= SV_LEVELS) return;
  int top = svLvReached(s); if (L > top) top = L;
  uint8_t *r = lvRec(s, L);
  r[0] = (uint8_t)mask; r[1] = (uint8_t)(mask >> 8); r[2] = (uint8_t)(mask >> 16); r[3] = (uint8_t)(mask >> 24); r[4] = (uint8_t)rows; r[5] = 0;
  s->ext[SV_EXT_LV] = (uint8_t)(SV_EXT_LV_SET | (top & 7));
}
int svLvReached(const SaveGame *s) {
  int top = s->level >= 1 && s->level < SV_LEVELS ? s->level : 1;
  if (s->ext[SV_EXT_LV] & SV_EXT_LV_SET) { int e = s->ext[SV_EXT_LV] & 7; if (e > top) top = e; }
  return top;
}
int svLvGet(const SaveGame *s, int L, uint32_t *mask, unsigned *rows) {
  uint32_t m = 0; unsigned w = 0; int got = 0;
  if (s && L >= 1 && L < SV_LEVELS) {
    const uint8_t *r = s->ext + SV_EXT_LV_REC + (L - 1) * SV_EXT_LV_STRIDE;
    int recorded = (s->ext[SV_EXT_LV] & SV_EXT_LV_SET) && (r[0] | r[1] | r[2] | r[3] | r[4]);
    if (L == s->msnLevel) { m = s->msnCompleted; w = recorded ? r[4] : 0; got = 2; }   // голова — свежее (текущий уровень)
    else if (recorded) { m = (uint32_t)r[0] | (uint32_t)r[1] << 8 | (uint32_t)r[2] << 16 | (uint32_t)r[3] << 24; w = r[4]; got = 2; }
    else if (L < svLvReached(s)) { m = 0xFFFFFFFFu; w = SV_LV_ROWS_OK | 0x7f; got = 1; }   // запись до 05.10: уровень ниже — пройден
  }
  if (mask) *mask = m;
  if (rows) *rows = w;
  return got;
}
void svLvKeep(SaveGame *dst, const SaveGame *src) {
  memcpy(dst->ext + SV_EXT_LV, src->ext + SV_EXT_LV, SV_EXT_LV_REC - SV_EXT_LV + (SV_LEVELS - 1) * SV_EXT_LV_STRIDE);
}

void svDevCollect(struct Collect *c) {
  if (!c || c->level <= 0 || c->level >= COLL_LEVELS) return;
  unsigned cm = 0;
  for (unsigned i = 0; i < c->ncard; i++) {
    unsigned k = c->card[i].id % 10; if (k >= 1 && k <= 8) cm |= 1u << (k - 1);
  }
  uint32_t wm = c->nwasp >= 32 ? 0xffffffffu : (1u << c->nwasp) - 1u;
  uint64_t gm = 0;
#ifndef SV_DEV_GAGS_OLD
  for (unsigned g = 0; g < c->ngag; g++) if (c->gag[g].coins) gm |= 1ull << g;
#else
  gm = c->ngag >= 64 ? ~0ull : (1ull << c->ngag) - 1ull;
#endif
  collectSetProgress(c, c->level, cm, wm, gm);
}

// ---- P01 Д1 (сверка g10-flow): прочность личных машин в ext[45..69] (раскладка в savegame.h)
void svCarHealthPut(SaveGame *s, int n, const uint16_t *hash, const float *frac) {
#ifdef SV_BROKEN_CARHP   // только для отрицательного контроля tests/savegame/host.sh
  (void)s; (void)n; (void)hash; (void)frac; return;
#endif
  if (n < 0) n = 0; if (n > SV_EXT_CARHP_MAX) n = SV_EXT_CARHP_MAX;
  memset(s->ext + SV_EXT_CARHP, 0, 1 + 3 * SV_EXT_CARHP_MAX);
  s->ext[SV_EXT_CARHP] = (uint8_t)(SV_EXT_CARHP_SET | n);
  for (int i = 0; i < n; i++) {
    float f = frac[i] < 0.f ? 0.f : frac[i] > 1.f ? 1.f : frac[i];
    uint8_t *e = s->ext + SV_EXT_CARHP + 1 + 3 * i;
    e[0] = (uint8_t)(hash[i] & 0xff); e[1] = (uint8_t)(hash[i] >> 8); e[2] = (uint8_t)(f * 255.f + .5f);
  }
}
int svCarHealthGet(const SaveGame *s, uint16_t *hash, float *frac) {
  uint8_t b = s->ext[SV_EXT_CARHP];
  if (!(b & SV_EXT_CARHP_SET)) return -1;
  int n = b & 15; if (n > SV_EXT_CARHP_MAX) n = SV_EXT_CARHP_MAX;
  for (int i = 0; i < n; i++) {
    const uint8_t *e = s->ext + SV_EXT_CARHP + 1 + 3 * i;
    hash[i] = (uint16_t)(e[0] | e[1] << 8); frac[i] = e[2] / 255.f;
  }
  return n;
}

// ================================================================================================================================
// №1114: формат SHV3 (Роб 02.11.2026). Раскладка (все числа little endian, байты от начала файла; подробно — docs/handoff/СОХРАНЕНИЯ_1114.md):
//   0..31    голова: "SHV3", u16 версия 3, u16 размер головы 32, u32 размер записи SV_BYTES, u32 CRC-32 байт 16..SV_BYTES-1, u32 seq, u16 год, месяц, день, час, минута, секунда (u8 ×5),
//            u8 уровень записи 1..7, u8 старший достигнутый уровень, u8 флаги (бит0 — секунды записаны), 2 байта запаса
//   32..111  прогресс: u8 уровень миссий, сюжетная, фаза, этап; u32 маска пройденных (текущий уровень); i32 монеты; u16 монет мира; 2 б запас; 64 б собранные монеты мира
//   112..143 настройки: i16 музыка, эффекты, машина, голос; u8 радар; u8 костюм; u8 настройки (бит0 прыжковые камеры, бит1 навигация, бит7 «записано»); u8 вид за машиной (бит0..1, бит7);
//            u8 камера (бит0 инвертировать, бит7); u8 билет/допуск (бит0..2, бит7); u16 показанные подсказки; u16 показанные ролики (бит0..8); u8 (бит7 — ролики записаны); запас до 143
//   144..177 машины игрока: u8 число (бит7 «таблица записана», 0..8); 1 б запас; 8 × (u16 хеш имени, u8 прочность 0..255, 1 б запас)
//   192..1479 уровни 1..7, по 184 Б: u32 маска пройденных миссий; u8 строки MISSION SELECT (бит7 — известны); u8 флаги (бит0 — маска записана); u8 карточки; 1 б запас;
//            u32 осы; u64 гэги; u16 купленное; 13 × запись о миссии (u8 флаги, 1 б запас, u16 попытки, i32 лучшее время, с; -1 нет); u16 n + u16 sig разовых предметов; 48 б разовые; запас до 184
//   1480..1535 запас (нули)
#define V3_HDR 32
#define V3_PROG 32
#define V3_SET 112
#define V3_CARS 144
#define V3_LV 192
#define V3_LVSZ 184
#define V3_END (V3_LV + 7 * V3_LVSZ)
#define V3_BESTMAX 359999   // 99:59:59

static void putSet(Cur *c, int bit7, int v) { p8(c, (uint32_t)((bit7 ? 0x80 : 0) | v)); }

static int svWrite3(const SaveGame *s, uint8_t *out) {
  memset(out, 0, SV_BYTES);
  Cur c = {out, SV_BYTES, 0, 0};
  memcpy(out, "SHV3", 4); c.at = 4; p16(&c, 3); p16(&c, V3_HDR); p32(&c, SV_BYTES); p32(&c, 0);
  p32(&c, s->seq); p16(&c, s->year); p8(&c, s->month); p8(&c, s->day); p8(&c, s->hour); p8(&c, s->minute);
  int secOk = (s->ext[83] & 0x80) != 0; p8(&c, secOk ? s->ext[83] & 0x7f : 0);   // секунды записи (savemenu.c): ext[83] в памяти, в файле — своё поле
  p8(&c, s->level); p8(&c, (uint32_t)svLvReached(s)); p8(&c, secOk ? 1 : 0);
  c.at = V3_PROG;
  p8(&c, s->msnLevel); p8(&c, s->msnStory); p8(&c, s->msnPhase); p8(&c, s->msnStage); p32(&c, s->msnCompleted);
  p32(&c, (uint32_t)s->coins); p16(&c, s->nworld); p16(&c, 0);
  for (unsigned i = 0; i < sizeof s->worldCoins; i++) p8(&c, s->worldCoins[i]);
  c.at = V3_SET;
#ifndef SV_BROKEN_SETTINGS
  p16(&c, (uint32_t)s->music); p16(&c, (uint32_t)s->effects); p16(&c, (uint32_t)s->vehicle); p16(&c, (uint32_t)s->voice);
  p8(&c, (uint32_t)s->radar); p8(&c, s->skin);
  const uint8_t *e = s->ext;
  p8(&c, (uint32_t)((e[SV_EXT_OPTS] & SV_EXT_OPTS_SET ? 0x80 : 0) | (e[SV_EXT_OPTS] & SV_EXT_OPTS_JUMP ? 1 : 0) | (e[SV_EXT_OPTS] & SV_EXT_OPTS_NAV ? 2 : 0)));
  putSet(&c, e[SV_EXT_CHASE] & SV_EXT_CHASE_SET, e[SV_EXT_CHASE] & 3);
  putSet(&c, e[SV_EXT_CAM] & SV_EXT_CAM_SET, e[SV_EXT_CAM] & SV_EXT_CAM_INV ? 1 : 0);
  putSet(&c, e[SV_EXT_TICKET] & SV_EXT_TICKET_SET, e[SV_EXT_TICKET] & 7);
  p16(&c, (uint32_t)e[SV_EXT_MOVIES + 2] | (uint32_t)e[SV_EXT_MOVIES + 3] << 8);
  p16(&c, (uint32_t)(e[SV_EXT_MOVIES] & 1) << 8 | e[SV_EXT_MOVIES + 1]);
  p8(&c, e[SV_EXT_MOVIES] & SV_EXT_MOVIES_SET);
#endif
  c.at = V3_CARS;
#ifndef SV_BROKEN_CARHP   // отрицательный контроль tests/savegame/host.sh: прочность машин не пишется
  { uint16_t h[SV_EXT_CARHP_MAX]; float f[SV_EXT_CARHP_MAX]; int n = svCarHealthGet(s, h, f);
    p8(&c, n < 0 ? 0 : (uint32_t)(0x80 | n)); p8(&c, 0);
    for (int i = 0; i < (n < 0 ? 0 : n); i++) { p16(&c, h[i]); p8(&c, (uint32_t)(f[i] * 255.f + .5f)); p8(&c, 0); } }
#endif
  for (int L = 1; L < SV_LEVELS; L++) {
    c.at = V3_LV + (L - 1) * V3_LVSZ;
    uint32_t m = 0; unsigned rows = 0; int got = svLvGet(s, L, &m, &rows);
#ifdef SV_BROKEN_LV
    got = 0;
#endif
    if (got != 2) { m = 0; rows = 0; }   // «уровень ниже пройден» из записи до 05.10 (got 1) в SHV3 держит старший достигнутый уровень в голове (svLvReached)
    p32(&c, m); p8(&c, rows); p8(&c, got == 2 ? 1 : 0); p8(&c, s->cards[L]); p8(&c, 0);
    p32(&c, s->wasps[L]); p64(&c, s->gags[L]);
#ifndef SV_BROKEN_OWNED
    p16(&c, s->owned[L]);
#else
    p16(&c, 0);
#endif
    for (int k = 0; k < SV_MREC_N; k++) {
#ifndef SV_BROKEN_BEST
      const SvMrec *r = &s->mrec[L][k];
      p8(&c, r->flags & 6); p8(&c, 0); p16(&c, r->attempts); p32(&c, (uint32_t)(r->best > 0 ? r->best : -1));
#else
      p8(&c, 0); p8(&c, 0); p16(&c, 0); p32(&c, (uint32_t)-1);
#endif
    }
#ifndef SV_BROKEN_ONCE
    p16(&c, s->once[L].n); p16(&c, s->once[L].sig);
    for (unsigned i = 0; i < SV_ONCE_BYTES; i++) p8(&c, s->once[L].st[i]);
#endif
  }
  int bad = c.bad;
  c.at = 12; p32(&c, svCrc32(out + 16, SV_BYTES - 16));
  return bad ? -1 : SV_BYTES;
}

static int svRead3(const uint8_t *in, unsigned n, SaveGame *s) {
  if (!in || n < SV_BYTES || memcmp(in, "SHV3", 4)) return SV_DAMAGED;   // "SHSV" сюда не приходит: svRead переводит их svReadOld (SAVEV3CONV_OLD=0)
  Cur c = {(uint8_t *)in, SV_BYTES, 4, 0};
  if (g16(&c) != 3 || g16(&c) != V3_HDR || g32(&c) != SV_BYTES) return SV_DAMAGED;
  uint32_t crc = g32(&c);
  if (SV_CHECK_CRC && crc != svCrc32(in + 16, SV_BYTES - 16)) return SV_DAMAGED;
  SaveGame t; svClear(&t);
  t.seq = g32(&c); t.year = (uint16_t)g16(&c); t.month = (uint8_t)g8(&c); t.day = (uint8_t)g8(&c); t.hour = (uint8_t)g8(&c); t.minute = (uint8_t)g8(&c);
  unsigned sec = g8(&c); t.level = (uint8_t)g8(&c); unsigned reached = g8(&c), hf = g8(&c);
  if (hf & 1) t.ext[83] = (uint8_t)(0x80 | (sec < 60 ? sec : 59));
  c.at = V3_PROG;
  t.msnLevel = (uint8_t)g8(&c); t.msnStory = (uint8_t)g8(&c); t.msnPhase = (uint8_t)g8(&c); t.msnStage = (uint8_t)g8(&c); t.msnCompleted = g32(&c);
  t.coins = (int32_t)g32(&c); t.nworld = (uint16_t)g16(&c); g16(&c);
  for (unsigned i = 0; i < sizeof t.worldCoins; i++) t.worldCoins[i] = (uint8_t)g8(&c);
  c.at = V3_SET;
  t.music = (int16_t)g16(&c); t.effects = (int16_t)g16(&c); t.vehicle = (int16_t)g16(&c); t.voice = (int16_t)g16(&c);
  t.radar = (int32_t)g8(&c); t.skin = (uint8_t)g8(&c);
  unsigned opts = g8(&c), chase = g8(&c), cam = g8(&c), tick = g8(&c), tut = g16(&c), mov = g16(&c), movSet = g8(&c);
  if (opts & 0x80) t.ext[SV_EXT_OPTS] = (uint8_t)(SV_EXT_OPTS_SET | (opts & 1 ? SV_EXT_OPTS_JUMP : 0) | (opts & 2 ? SV_EXT_OPTS_NAV : 0));
  if (chase & 0x80) t.ext[SV_EXT_CHASE] = (uint8_t)(SV_EXT_CHASE_SET | (chase & 3));
  if (cam & 0x80) t.ext[SV_EXT_CAM] = (uint8_t)(SV_EXT_CAM_SET | (cam & 1 ? SV_EXT_CAM_INV : 0));
  if (tick & 0x80) t.ext[SV_EXT_TICKET] = (uint8_t)(SV_EXT_TICKET_SET | (tick & 7));
  t.ext[SV_EXT_MOVIES + 2] = (uint8_t)tut; t.ext[SV_EXT_MOVIES + 3] = (uint8_t)(tut >> 8);
  if (movSet & 0x80) { t.ext[SV_EXT_MOVIES] = (uint8_t)(SV_EXT_MOVIES_SET | (mov >> 8 & 1)); t.ext[SV_EXT_MOVIES + 1] = (uint8_t)mov; }
  c.at = V3_CARS;
  { unsigned cn = g8(&c); g8(&c); uint16_t h[SV_EXT_CARHP_MAX]; float f[SV_EXT_CARHP_MAX]; unsigned k = cn & 15;
    if (k > SV_EXT_CARHP_MAX) return SV_DAMAGED;
    for (unsigned i = 0; i < k; i++) { h[i] = (uint16_t)g16(&c); f[i] = g8(&c) / 255.f; g8(&c); }
    if (cn & 0x80) svCarHealthPut(&t, (int)k, h, f); }
  for (int L = 1; L < SV_LEVELS; L++) {
    c.at = V3_LV + (L - 1) * V3_LVSZ;
    uint32_t m = g32(&c); unsigned rows = g8(&c), fl = g8(&c);
    t.cards[L] = (uint8_t)g8(&c); g8(&c);
    t.wasps[L] = g32(&c); t.gags[L] = g64(&c); t.owned[L] = (uint16_t)g16(&c);
    for (int k = 0; k < SV_MREC_N; k++) {
      SvMrec *r = &t.mrec[L][k];
      r->flags = (uint8_t)(g8(&c) & 6); g8(&c); r->attempts = (uint16_t)g16(&c); int32_t b = (int32_t)g32(&c);
      if (b != -1 && (b < 1 || b > V3_BESTMAX)) return SV_DAMAGED;
      r->best = b == -1 ? 0 : b;
    }
    t.once[L].n = (uint16_t)g16(&c); t.once[L].sig = (uint16_t)g16(&c);
    for (unsigned i = 0; i < SV_ONCE_BYTES; i++) t.once[L].st[i] = (uint8_t)g8(&c);
    if (fl & 1) svLvPut(&t, L, m, rows);
  }
  if (reached >= 1 && reached < SV_LEVELS && (t.ext[SV_EXT_LV] & SV_EXT_LV_SET)) { if ((t.ext[SV_EXT_LV] & 7) < reached) t.ext[SV_EXT_LV] = (uint8_t)(SV_EXT_LV_SET | reached); }
  else if (reached >= 1 && reached < SV_LEVELS && reached > t.level) t.ext[SV_EXT_LV] = (uint8_t)(SV_EXT_LV_SET | reached);
  if (c.bad || t.level < 1 || t.level >= SV_LEVELS || t.nworld > SV_WORLDCOINS || t.coins < 0 || !vol(t.music) || !vol(t.effects) || !vol(t.vehicle) || !vol(t.voice) ||
      t.radar < 0 || t.radar > 1 || t.month > 12 || t.day > 31 || t.hour > 23 || t.minute > 59 || t.skin > 7 || reached < 1 || reached >= SV_LEVELS)
    return SV_DAMAGED;
  *s = t;
  return SV_OK;
}

// ---- №1114: лучшие времена и попытки (живая таблица игры; savegame.h)
static int slotOk(int L, int k) { return L >= 1 && L < SV_LEVELS && k >= 0 && k < SV_MREC_N; }
int svBestNote(int L, int k, int sec) {
  if (!slotOk(L, k) || sec < 1) return 0;
  if (sec > V3_BESTMAX) sec = V3_BESTMAX;
  if (liveBest[L][k] > 0 && liveBest[L][k] <= sec) return 0;
  liveBest[L][k] = sec; return 1;
}
int svBestLive(int L, int k) { return slotOk(L, k) && liveBest[L][k] > 0 ? liveBest[L][k] : 0; }
void svAttemptNote(int L, int k) { if (slotOk(L, k) && liveTry[L][k] < 65535) liveTry[L][k]++; }
int svAttemptsLive(int L, int k) { return slotOk(L, k) ? liveTry[L][k] : 0; }
void svBestReset(void) { memset(liveBest, 0, sizeof liveBest); memset(liveTry, 0, sizeof liveTry); }
