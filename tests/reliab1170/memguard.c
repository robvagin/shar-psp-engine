// №1170: mgCallocN (viewer/memguard.h) при отказе любого выделения отдаёт уже взятое и называет номер отказавшей таблицы. Подменный calloc отказывает на заданном вызове.
#include <stdio.h>
#include <stdlib.h>
static int nth, calls, live;
static void *fake_calloc(size_t n, size_t s) { if (++calls == nth) return 0; void *p = calloc(n, s); if (p) live++; return p; }
static void fake_free(void *p) { if (p) live--; free(p); }
#define calloc fake_calloc
#define free fake_free
#include "memguard.h"
#undef calloc
#undef free
int main(void) {
  int fails = 0;
  size_t cnt[7] = {10, 0, 5, 3, 8, 1, 2}, sz[7] = {64, 1, 16, 4, 8, 32, 2};
  for (int n = 3; n <= 7; n += 4) {
    for (int k = 0; k <= n; k++) {   // k == 0: без отказа; k = 1..n: отказывает k-я таблица
      void *o[7]; calls = 0; live = 0; nth = k; int r = mgCallocN(n, o, cnt, sz);
      int ok = k == 0 ? (r == 0 && live == n) : (r == k && live == 0);
      for (int i = 0; ok && i < n; i++) ok = k == 0 ? o[i] != 0 : o[i] == 0 || i >= k;
      if (!ok) { fails++; printf("FAIL n=%d отказ=%d: вернул %d, живых %d\n", n, k, r, live); }
      if (k == 0) for (int i = 0; i < n; i++) fake_free(o[i]);
    }
  }
  // прежний порядок (красный контроль): таблицы берутся и пишутся без проверки — при отказе первая запись идёт в NULL
  { void *t[3]; calls = 0; live = 0; nth = 2; for (int i = 0; i < 3; i++) t[i] = fake_calloc(4, 4); int wrote_null = t[1] == 0; if (LEGACY_MUST_FAIL && wrote_null) { fails++; printf("FAIL (красный): прежний порядок пишет в NULL при отказе\n"); } }
  printf("RESULT: %s — провалов %d\n", fails ? "FAIL" : "OK", fails);
  return fails ? 1 : 0;
}
