// №1104 (Роб 02.11.2026: «коды оригинала — добавить»): ввод кодов и таблица включённых. Чистый C99, без PSP: проверяется на Маке (tests/cheats/host.c).
// Поведение сверено с оригиналом (число кодов, комбинации, порядок ввода, условие запуска); код и имена наши.
//
// Как вводится (так же, как на PS2): в верхнем экране OPTIONS главного меню и в OPTIONS паузы зажать L и R вместе, не отпуская — четыре нажатия
// ✕ ○ □ △ подряд. Отпустил L или R — ввод и набранное сбрасываются. Пока L и R зажаты, меню кнопок не принимает. После четвёртого нажатия
// код сверяется с таблицей: верный — звук успеха и код включён (повторный ввод не выключает, это сборка без отладки); неверный — звук ошибки,
// ничего не меняется; набор не сбрасывается и идёт по кругу следующей четвёркой. PS2: ✕ ○ □ △ — те же кнопки PSP, L1 и R1 — L и R.
// L2/R2 в оригинале не участвуют (раскладку PSP менять не нужно).
// Включённые коды живут до выхода в главное меню из игры (cheatsReset), в записи не хранятся — как в оригинале.
#ifndef SV_CHEATS_H
#define SV_CHEATS_H
#include <string.h>
#include "game.h"   // GP_* — кнопки в раскладке PSP

#ifndef CHEATS1104_OLD
#define CHEATS1104_OLD 0   // №1104: 1 — кодов нет (ввод не слушается, эффекты выключены)
#endif
#ifndef CHEATS_DEBUG
#define CHEATS_DEBUG 0     // 1 — ещё и отладочные коды оригинала (в выпускной сборке PS2 их нет): переключатели, а не «включил навсегда»
#endif

enum {   // номера кодов (наши); порядок = порядок строк в таблице
  CH_VEHICLES, CH_NOTOP, CH_ACCEL, CH_HORNJUMP, CH_ONETAP, CH_CAMERAS, CH_CREDITS, CH_SPEEDO, CH_REDBRICK, CH_INVINC, CH_TREE, CH_TRIPPY,
  CH_RETAIL_N,   // дальше — только отладочные
  CH_CARDS = CH_RETAIL_N, CH_SKINS, CH_LIST, CH_MISSIONS, CH_MOVIES, CH_ALLUNLOCK, CH_COINS, CH_KICKMODEL, CH_POS, CH_FULLDMG, CH_EXTRATIME, CH_DEMO,
  CH_N
};
enum { CHI_X = 0, CHI_O = 1, CHI_SQ = 2, CHI_TRI = 3 };   // ✕ ○ □ △
typedef struct { int id; unsigned char seq[4]; const char *name, *what; } CheatDef;
static const CheatDef CHEAT_TAB[] = {
  {CH_VEHICLES, {CHI_X, CHI_O, CHI_X, CHI_O}, "Vehicles", "все машины-награды в телефонной будке; только когда пройдены все 7 сюжетных миссий всех 7 уровней"},
  {CH_NOTOP, {CHI_SQ, CHI_SQ, CHI_SQ, CHI_SQ}, "No Top Speed", "у машины игрока нет сопротивления: разгон дальше предела скорости"},
  {CH_ACCEL, {CHI_TRI, CHI_TRI, CHI_TRI, CHI_TRI}, "High Acceleration", "тяга машины игрока вдвое"},
  {CH_HORNJUMP, {CHI_SQ, CHI_SQ, CHI_SQ, CHI_TRI}, "Car Jump on Horn", "прыжок машины игрока по гудку (кнопки гудка на PSP нет)"},
  {CH_ONETAP, {CHI_TRI, CHI_TRI, CHI_SQ, CHI_SQ}, "One Tap Traffic Death", "машина трафика или ИИ от касания машины игрока сразу разбита"},
  {CH_CAMERAS, {CHI_O, CHI_O, CHI_O, CHI_X}, "Unlock All Cameras", "шесть дополнительных камер езды, две пешком и широкий угол следящих камер"},
  {CH_CREDITS, {CHI_X, CHI_SQ, CHI_SQ, CHI_TRI}, "Play Credits Dialog", "в титрах звучит диалог Кэнга и Кодос без прохождения игры"},
  {CH_SPEEDO, {CHI_TRI, CHI_TRI, CHI_O, CHI_SQ}, "Show Speedometer", "на экране скорость в км/ч"},
  {CH_REDBRICK, {CHI_O, CHI_O, CHI_TRI, CHI_SQ}, "Red Brick", "бесплатная машина на улице — кирпич"},
  {CH_INVINC, {CHI_TRI, CHI_X, CHI_TRI, CHI_X}, "Invincible Car", "машина игрока не получает урона"},
  {CH_TREE, {CHI_O, CHI_X, CHI_O, CHI_TRI}, "Show Tree", "отладочная подсветка: красные коробки дерева мира и треугольник под игроком"},
  {CH_TRIPPY, {CHI_TRI, CHI_O, CHI_TRI, CHI_O}, "Trippy", "сильное размытие кадра"},
#if CHEATS_DEBUG
  {CH_CARDS, {CHI_X, CHI_X, CHI_X, CHI_X}, "Unlock All Cards", "все карточки"},
  {CH_SKINS, {CHI_SQ, CHI_TRI, CHI_SQ, CHI_TRI}, "Unlock All Clothing", "все костюмы"},
  {CH_LIST, {CHI_X, CHI_O, CHI_SQ, CHI_TRI}, "Display All Cheats", "список кодов поверх OPTIONS"},
  {CH_MISSIONS, {CHI_X, CHI_O, CHI_O, CHI_X}, "Unlock All Missions", "все миссии в MISSION SELECT"},
  {CH_MOVIES, {CHI_O, CHI_TRI, CHI_SQ, CHI_TRI}, "Unlock All Movies", "все ролики"},
  {CH_ALLUNLOCK, {CHI_X, CHI_O, CHI_O, CHI_TRI}, "Unlock All", "карточки, костюмы, миссии, ролики и машины сразу"},
  {CH_COINS, {CHI_X, CHI_SQ, CHI_O, CHI_TRI}, "Add Extra Coins", "добавка монет"},
  {CH_KICKMODEL, {CHI_O, CHI_O, CHI_O, CHI_SQ}, "Kick Swaps Character Model", "удар меняет модель персонажа"},
  {CH_POS, {CHI_O, CHI_O, CHI_O, CHI_O}, "Show Avatar Position", "координаты игрока на экране"},
  {CH_FULLDMG, {CHI_SQ, CHI_SQ, CHI_TRI, CHI_TRI}, "Full Damage", "машина игрока нарисована полностью разбитой"},
  {CH_EXTRATIME, {CHI_O, CHI_O, CHI_O, CHI_TRI}, "Extra Time", "время задания ×5"},
  {CH_DEMO, {CHI_X, CHI_X, CHI_X, CHI_O}, "Demotest", "режим демо"},
#endif
};
#define CHEAT_TAB_N ((int)(sizeof CHEAT_TAB / sizeof CHEAT_TAB[0]))

typedef struct {
  unsigned on;          // биты включённых кодов (1u << CH_*)
  int active;           // L и R зажаты
  int idx; unsigned char seq[4];   // набранные нажатия (кольцо из четырёх, индекс 0..3)
} CheatSys;
enum { CHEV_NONE = 0, CHEV_OK = 1, CHEV_FAIL = 2 };   // событие кадра: звук успеха / ошибки
static inline void cheatsReset(CheatSys *c) { memset(c, 0, sizeof *c); }
static inline int cheatOn(const CheatSys *c, int id) { return !CHEATS1104_OLD && id >= 0 && id < CH_N && (c->on >> id & 1u) != 0; }
static inline int cheatFind(const unsigned char *seq) {   // номер кода по четвёрке нажатий или -1
  for (int i = 0; i < CHEAT_TAB_N; i++) if (!memcmp(CHEAT_TAB[i].seq, seq, 4)) return CHEAT_TAB[i].id;
  return -1;
}
// Один кадр ввода. held — удерживаемые кнопки, hit — нажатые в этом кадре; allStory — пройдены ли все сюжетные миссии (условие кода машин).
// *idOut — какой код отработал. Возврат: событие кадра. Кнопки меню при cheatsActive игнорируются вызывающим.
static inline int cheatsFrame(CheatSys *c, unsigned held, unsigned hit, int allStory, int *idOut) {
  if (idOut) *idOut = -1;
  if (CHEATS1104_OLD) return CHEV_NONE;
  int was = c->active, now = (held & GP_L) && (held & GP_R);
  if (now != was) { c->active = now; c->idx = 0; memset(c->seq, 0, sizeof c->seq); }   // любое изменение L/R сбрасывает набор
  if (!c->active) return CHEV_NONE;
  static const unsigned bt[4] = {GP_CROSS, GP_CIRCLE, GP_SQUARE, GP_TRIANGLE};
  int ev = CHEV_NONE;
  for (int k = 0; k < 4; k++) {
    if (!(hit & bt[k])) continue;
    c->seq[c->idx++] = (unsigned char)k;
    if (c->idx < 4) continue;
    c->idx = 0; int id = cheatFind(c->seq);
    if (id < 0) { ev = CHEV_FAIL; continue; }
#if CHEATS_DEBUG
    int enable = !(c->on >> id & 1u);   // отладочная сборка оригинала: код переключается
#else
    int enable = 1;
#endif
    if (id == CH_VEHICLES && !allStory) { ev = CHEV_FAIL; if (idOut) *idOut = id; continue; }   // машины — только после всех сюжетных миссий
    if (enable) c->on |= 1u << id; else c->on &= ~(1u << id);
    if (id == CH_ALLUNLOCK) for (int u = CH_CARDS; u < CH_ALLUNLOCK; u++) { if (enable) c->on |= 1u << u; else c->on &= ~(1u << u); }
    ev = (c->on >> id & 1u) ? CHEV_OK : CHEV_FAIL; if (idOut) *idOut = id;
  }
  return ev;
}
static inline int cheatsActive(const CheatSys *c) { return !CHEATS1104_OLD && c->active; }   // L и R зажаты: кнопки меню игнорируются
#endif
