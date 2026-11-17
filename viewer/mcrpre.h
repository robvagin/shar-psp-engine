// №538: решения очереди фонового чтения моделей машин миссии (viewer/main_render.h, mcrGet/mcrSync). Чистый C, без PSP: тест tests/mcr/pre_queue.c с имитацией задержки.
#ifndef MCRPRE_H
#define MCRPRE_H
enum { MCRPRE_TAKE, MCRPRE_DEFER, MCRPRE_WAIT };
#define MCRPRE_MAX_AGE 12   // вызовов mcrGet (≈ 2 за кадр), пока блок читается в фоне; дальше ждём остаток, как прежнее синхронное чтение
// Блок готов -> взять. Не готов: машина игрока (force) или ждали слишком долго -> дождаться остатка; иначе отложить (машина появится на следующем кадре)
static inline int mcrPreDecide(int ready, int age, int force) {
#ifdef PRE_BREAK   /* красный контроль теста: машина игрока откладывается как все */
  force = 0;
#endif
  if (ready) return MCRPRE_TAKE; if (force || age >= MCRPRE_MAX_AGE) return MCRPRE_WAIT; return MCRPRE_DEFER; }
// 1362 (аудит простоев чтения: двойной опрос). sceIoPollAsync отдаёт итог чтения ОДИН раз: первый вызов с готовым результатом вернул 0 и забрал его, второй (из ожидания при «взять») возвращает
// ошибку — готовое предчтение машины выбрасывалось, машина читалась синхронно в кадре (на PSP «MCR load» 47–65 мс вместо 10–25). Теперь итог запоминается в McrPollC (как у персонажей, skin.c SkinPre.done/res).
// MCRPREPOLL_OLD=1 — прежнее: опрос без памяти, «взять» опрашивает заново. Чистый C: тест tests/mcr/pre_poll.c с имитацией «итог отдаётся один раз»
#ifndef MCRPREPOLL_OLD
#define MCRPREPOLL_OLD 0
#endif
#ifndef MCRPRE_OLDVAR   /* движок подставляет переменную (rollback.txt / файл-выключатель), тест — константу */
#define MCRPRE_OLDVAR MCRPREPOLL_OLD
#endif
typedef struct { int done, err; long long res; } McrPollC;
typedef int (*McrPollFn)(void *u, long long *res);   // как sceIoPollAsync: 0 — готово (итог в *res), >0 — идёт, <0 — ошибка
static inline int mcrPreReadyC(McrPollC *c, McrPollFn poll, void *u) {
  if (MCRPRE_OLDVAR) { long long r = -1; return poll(u, &r) == 0; }
  if (c->done) return 1;
  long long r = -1; int pr = poll(u, &r);
  if (pr > 0) return 0;
  c->done = 1; c->err = pr < 0; c->res = r; return 1; }
// Взять итог: 1 — прочитано ровно size байт. Готовое берётся из памяти опроса, не готовое (или прежний путь) ждёт wait (ioWaitBounded: 1 — завершилось, итог в *res)
static inline int mcrPreTakeC(McrPollC *c, unsigned size, McrPollFn wait, void *u) {
  long long r = -1;
  if (!MCRPRE_OLDVAR && c->done) r = c->err ? -1 : c->res; else if (wait(u, &r) != 1) r = -1;
  return r == (long long)size; }
// Начать фоновое чтение: есть свободное место в очереди и под модель, куча даёт запас (memPrefetchRoom), модели нет в памяти и чтение уже не идёт
static inline int mcrPreMayStart(int queueFree, int modelSlotsFree, int heapOk, int loaded, int pending) { return queueFree > 0 && modelSlotsFree > 0 && heapOk && !loaded && !pending; }
#endif
