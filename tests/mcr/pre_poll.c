// 1362: предчтение машины миссии не должно опрашивать итог дважды (viewer/mcrpre.h mcrPreReadyC / mcrPreTakeC, main_render.h mcrPreReady / mcrPreTake).
// sceIoPollAsync отдаёт итог один раз: после «готово» повторный опрос возвращает ошибку «нет асинхронной операции» (на PSP 0x80020324). Имитация ниже ведёт себя так же.
//   cc -I viewer tests/mcr/pre_poll.c -o /tmp/pre_poll && /tmp/pre_poll          PASS
//   cc -I viewer -DMCRPREPOLL_OLD=1 tests/mcr/pre_poll.c -o /tmp/pre_poll_old && /tmp/pre_poll_old     красный: готовое предчтение выбрасывается (FAIL)
#include <stdio.h>
#include "mcrpre.h"
typedef struct { int left, consumed, polls, waits; long long res; } Fake;
static int fPoll(void *u, long long *res) {
  Fake *f = u; f->polls++;
  if (f->consumed) return (int)0x80020324;                    // итог уже отдан
  if (f->left > 0) { f->left--; return 1; }                    // ещё читается
  f->consumed = 1; *res = f->res; return 0; }
static int fWait(void *u, long long *res) {                    // как ioWaitBounded: опрос до конца; 1 — завершилось, иначе ошибка
  Fake *f = u; f->waits++;
  for (int k = 0; k < 1000; k++) { int r = fPoll(u, res); if (r <= 0) return r == 0 ? 1 : -1; }
  return 0; }
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main(void) {
  const unsigned SZ = 203712;
  // 1. читается 3 опроса, потом готово; «взять» сразу после готового опроса (как mcrGet) — блок обязан быть принят, ожидания (повторного опроса) нет
  { Fake f = {3, 0, 0, 0, SZ}; McrPollC c = {0}; int ready = 0, calls = 0;
    while (!ready && calls < 10) { ready = mcrPreReadyC(&c, fPoll, &f); calls++; }
    CHECK(ready && calls == 4, "готово на опросе 4, было %d", calls);
    int got = mcrPreTakeC(&c, SZ, fWait, &f);
    CHECK(got == 1, "готовое предчтение выброшено (повторный опрос после «готово» вернул ошибку)");
    CHECK(f.waits == 0, "«взять» готовое не должно ждать/опрашивать заново (waits=%d)", f.waits); }
  // 2. повторные «готово» без нового опроса (mcrGet зовётся дважды за кадр)
  { Fake f = {0, 0, 0, 0, SZ}; McrPollC c = {0};
    int r1 = mcrPreReadyC(&c, fPoll, &f), r2 = mcrPreReadyC(&c, fPoll, &f);
    CHECK(r1 == 1 && r2 == 1, "готовность держится (r1=%d r2=%d)", r1, r2);
    CHECK(f.polls == 1 || MCRPREPOLL_OLD, "итог опрошен один раз (polls=%d)", f.polls); }
  // 3. «взять» ещё читающийся блок (машина игрока): ждёт остаток и принимает
  { Fake f = {5, 0, 0, 0, SZ}; McrPollC c = {0};
    CHECK(mcrPreTakeC(&c, SZ, fWait, &f) == 1, "не готовый блок: ожидание остатка должно принять"); }
  // 4. ошибка чтения: готово с ошибкой, «взять» отказывает (синхронный откат в mcrGet)
  { Fake f = {0, 0, 0, 0, -1}; McrPollC c = {0};
    CHECK(mcrPreReadyC(&c, fPoll, &f) == 1, "ошибочное чтение — окончено");
    CHECK(mcrPreTakeC(&c, SZ, fWait, &f) == 0, "ошибочное чтение принято"); }
  // 5. прочитано не то количество байт
  { Fake f = {0, 0, 0, 0, SZ - 16}; McrPollC c = {0};
    mcrPreReadyC(&c, fPoll, &f);
    CHECK(mcrPreTakeC(&c, SZ, fWait, &f) == 0, "неполное чтение принято"); }
  printf(fails ? "FAIL (%d)\n" : "PASS\n", fails);
  return fails != 0;
}
