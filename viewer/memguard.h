// №1170 (разбор надёжности 04.11, §3): выделения при загрузке уровня без проверки писали по пустому адресу (красный экран CRASH). mgCallocN берёт несколько таблиц сразу,
// при отказе любой отдаёт уже взятые обратно и возвращает её номер (1..n); 0 — все получены. Вызывающий отвечает честно (fail("Not enough memory …") или уровень без
// части), а не записью в NULL+i. LOADMEM1170_OLD=1 — прежние выделения без проверки (viewer/main_levels.h, main_collect.h).
#ifndef SHAR_MEMGUARD_H
#define SHAR_MEMGUARD_H
#include <stdlib.h>
#ifndef LOADMEM1170_OLD
#define LOADMEM1170_OLD 0
#endif
static inline int mgCallocN(int n, void **out, const size_t *cnt, const size_t *sz) {
  for (int i = 0; i < n; i++) {
    out[i] = calloc(cnt[i] ? cnt[i] : 1, sz[i]);
    if (!out[i]) { for (int j = 0; j < i; j++) { free(out[j]); out[j] = 0; } return i + 1; }
  }
  return 0;
}
#endif
