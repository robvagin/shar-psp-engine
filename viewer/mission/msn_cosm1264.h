// Бабл 107, добивка A1.8 / A1.11 / A1.12 (сверка Fable §4 по исходнику оригинала): чистые функции без зависимостей, чтобы хост-тест tests/cosm1264 гонял те же условия, что msn_run.c и msn_view.c.
// old = 1 — прежнее поведение (красный контроль). Выключатель в движке: msnCosm1264Old (маска 1 — A1.11, 2 — A1.12, 4 — A1.8), flag msncosm1264old N, или -DMSN_COSM1264_OLD=7.
#ifndef MSN_COSM1264_H
#define MSN_COSM1264_H
#include <math.h>
#ifndef MSN_COSM1264_OLD
#define MSN_COSM1264_OLD 0
#endif
extern int msnCosm1264Old;   // msn_run.c
// A1.11: время цели timer (SetDurationTime) оригинал читает целым числом секунд (0,5 → 0, этап кончается на первом тике; в данных так L5 m2 и L7 m1)
static inline float mvTimerDur1264(float s, int old) { return old ? s : floorf(s); }
// A1.12: «дальше предела» оригинал считает по дробному расстоянию (150,5 > 150), порт сравнивал целые части (срабатывало с 151 м)
static inline int mvBeyond1264(float d, float lim, int old) { return old ? (int)d > (int)lim : d > lim; }
// A1.8: стрелка над целью dump: с привязками предметов к точкам пути «следуй» (arrow_chase, как у follow), без привязок «разбей» (arrow_destroy). Прочие — как были (вызывающий)
static inline const char *mvDumpArrow1264(int nbind, int old) { return (!old && nbind > 0) ? "arrow_chase" : "arrow_destroy"; }
#endif
