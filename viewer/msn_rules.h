// Окно g07b-msnview, круг 6: правила экранов миссии, вынесенные в чистые функции, чтобы хост-тест (tests/g07b_sverka6/rules_host.c) гонял те же самые условия, что msn_view.c.
// Все числа сверены с оригиналом по поведению (см. комментарии у вызовов в msn_view.c); имена и устройство кода оригинала сюда не переносились.
#ifndef MSN_RULES_H
#define MSN_RULES_H
#include "mission/msn_format.h"

// Дорожные стрелки к цели есть только у семи типов целей: идти в точку, выйти наружу, поговорить, зайти в интерьер, гонка, доставка, сброс груза
// (в оригинале стрелки строят цели-точки и цели-предметы с перечнем точек; «подобрать предмет», сесть в машину, следить, уничтожить, оторваться, таймер, разговор, ролик, монеты, покупки — нет)
static inline int mvObjHasRoadArrows(int obj) {
  switch (obj) {
    case MSN_OBJ_GOTO: case MSN_OBJ_TALKTO: case MSN_OBJ_INTERIOR: case MSN_OBJ_GOOUTSIDE: case MSN_OBJ_RACE: case MSN_OBJ_DELIVERY: case MSN_OBJ_DUMP: return 1;
    default: return 0;
  }
}
// Решение «рисовать дорожные стрелки»: переключатель Intersect Nav System включён, режим стрелки этапа не neither и не nearest road, тип цели из списка выше.
// oldAll=1 — совсем прежнее (всегда да); whiteOld=1 — чёрный список вместо белого (стрелки у всего, кроме перечисленных); pickOld=1 — стрелки и у «подобрать предмет»
static inline int mvRouteAllowed(int obj, int nav, int intersectOn, int oldAll, int whiteOld, int pickOld) {
  if (oldAll) return 1;
  if (!intersectOn || nav == MSN_NAV_NEITHER || nav == MSN_NAV_NEAREST_ROAD) return 0;
  if (obj == MSN_OBJ_PICKUPITEM) return pickOld;
  if (!whiteOld) return mvObjHasRoadArrows(obj);
  switch (obj) {
    case MSN_OBJ_GETIN: case MSN_OBJ_FOLLOW: case MSN_OBJ_DESTROY: case MSN_OBJ_DESTROYBOSS: case MSN_OBJ_LOSETAIL: case MSN_OBJ_TIMER:
    case MSN_OBJ_DIALOGUE: case MSN_OBJ_FMV: case MSN_OBJ_COINS: case MSN_OBJ_BUYCAR: case MSN_OBJ_BUYSKIN: return 0;
    default: return 1;
  }
}
// Реплика старта миссии при принятии брифинга молчит только в уличных гонках (имя начинается на sr); у ставочных (gr) звучит. gambleToo=1 — прежнее (молчали оба вида)
static inline int mvMstartMuted(unsigned missionFlags, int gambleToo) { return (missionFlags & (MSN_MF_STREETRACE | (gambleToo ? MSN_MF_GAMBLE : 0))) != 0; }
// Skip на экране провала: нужно 7 учтённых попыток. Попытка = один запуск миссии (старт или перезапуск); оригинал считает каждый запуск дважды и делит на 2, у нас счёт сразу в запусках.
// На 7-м уровне с пятой сюжетной миссии пропуска нет. div2=1 — прежнее (делили ещё раз: нужно было 14 запусков)
#define MV_SKIP_TRIES 7
static inline int mvSkipAllowed(int tries, int level, int storyOrd, int div2) {
  if (level == 7 && storyOrd >= 5) return 0;
  return (div2 ? tries / 2 : tries) >= MV_SKIP_TRIES;
}
// Значок строки MISSION SELECT: 2 галка (пройдена), 1 красный крест (были попытки и не пропущена), 0 жёлтая черта
static inline int mvRowIcon(int completed, int tries, int skipped) { return completed ? 2 : (tries > 0 && !skipped) ? 1 : 0; }
// Мигание «вкл на чётных полупериодах»: счётчик кругов и Item Dropped (полупериод 0,25 с); LOADING на карточке — наоборот, скрыта на первом полупериоде
static inline int mvPulseOn(float t, float half) { return (int)(t / half) % 2 == 0; }
// Счётчик кругов: 8 переключений по 0,25 с; -1 — мигание кончилось (счётчик виден), иначе номер переключения
static inline int mvLapStep(float t) { int n = (int)(t / .25f); return n >= 8 ? -1 : n; }
#endif
