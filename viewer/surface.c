// Тип поверхности земли под точкой (сверка g12-data, строка P18-04, 02.11.2026). Настоящее определение svSurfaceAt из
// viewer/sverka_shared.h (там слабая заглушка «везде дорога»): данные — блок SRF1 секции COL1 (tools/psp_export.py col_section),
// байт на треугольник земли в порядке массива ColTri. Чистый C99, без PSP-заголовков и malloc: собирается и в EBOOT, и в ботах на Маке.
// Заголовок sverka_shared.h здесь НЕ включается (иначе слабое и сильное определение в одном файле): номера типов повторены ниже и
// обязаны совпадать с enum SURF_* там (дорога 0, трава 1, песок 2, гравий 3, вода 4, настил 5, металл 6, грунт 7).
// SURFACE_OLD=1 — как до правки: везде дорога, помещения нет.
#include <stdint.h>
#include <string.h>
#include "col.h"

#ifndef SURFACE_OLD
#define SURFACE_OLD 0
#endif

static const ColWorld *sfW;        // мир, по которому ищем опору (colGroundMatId)
static const uint8_t *sfByte;      // байты поверхности (по числу треугольников земли), NULL — в уровне блока нет
static uint32_t sfN;
static const uint8_t *sfNib;       // круг 4 (02.11.2026): тип поверхности из ОТДЕЛЬНОГО файла surf-l<N>.shr, по полубайту на треугольник (бит 3 = помещение, 0..2 тип)
static const void *sfSec; static unsigned sfSecSz;   // секция COL1, к которой привязано (для проверки файла рядом)
static uint32_t sfRingFp, sfRingNtri;   // Коллизия-96: привязка к кольцу (COL1 целиком в памяти нет): отпечаток и число треугольников из заголовка colring.shr

// Привязка к уровню: вызывается там же, где colInit (main_levels.h levelColAdopt), section — тот же буфер, что отдан colInit.
// Старые level.shr (без SRF1) и битый блок: sfByte = NULL, svSurfaceAt отвечает «дорога».
void svSurfaceBind(const ColWorld *w, const void *section, unsigned size) {
  sfW = w; sfByte = 0; sfN = 0; sfNib = 0; sfSec = section; sfSecSz = size;
  if (SURFACE_OLD || !w || !section || size < 64 || memcmp(section, "COL1", 4)) return;
  const ColHdr *h = (const ColHdr *)section;
  uint64_t need = 48 + (uint64_t)h->ntri * 48 + (uint64_t)h->nbox * 64 + (uint64_t)h->ncyl * 48 + (uint64_t)h->nseg * 32 +
                  ((uint64_t)h->W * h->H + 1) * 4 + (uint64_t)h->nref * 4;   // конец ссылок (как в colInit)
  need = (need + 15u) & ~(uint64_t)15u;                                       // блок SRF1 — со следующего кратного 16
  if (need + 16 > size) return;
  const uint8_t *b = (const uint8_t *)section + need; uint32_t v[3];
  if (memcmp(b, "SRF1", 4)) return;
  memcpy(v, b + 4, 12);
  if (v[0] != 1 || v[1] != h->ntri || need + 16 + v[1] > size) return;
  sfByte = b + 16; sfN = v[1];
}

void svSurfaceFree(void) { sfW = 0; sfByte = 0; sfN = 0; sfNib = 0; sfSec = 0; sfSecSz = 0; sfRingFp = sfRingNtri = 0; }
// Коллизия-96: то же, что svSurfaceBind, но COL1 в памяти нет — блок SRF1 (если был) лежит резидентно в файле кольца, отпечаток посчитан сборщиком по тому же алгоритму
void svSurfaceBindRing(const ColWorld *w, const void *srf, unsigned srfLen, uint32_t fp, uint32_t ntri) {
  sfW = w; sfByte = 0; sfN = 0; sfNib = 0; sfSec = 0; sfSecSz = 0; sfRingFp = fp; sfRingNtri = ntri;
  if (SURFACE_OLD || !w || !srf || srfLen < 16 + ntri || memcmp(srf, "SRF1", 4)) return;
  uint32_t v[3]; memcpy(v, (const uint8_t *)srf + 4, 12);
  if (v[0] != 1 || v[1] != ntri) return;
  sfByte = (const uint8_t *)srf + 16; sfN = v[1];
}

// Круг 4: уровни TEST81 собраны другим рецептом, блока SRF1 в их COL1 нет, а менять COL1 нельзя (хеш проб машин миссий, mcpr.shr). Поэтому
// те же данные лежат рядом с уровнем, в файле surf-l<N>.shr (tools/sv_overlay.py): 'SRFS', u32 версия 1, u32 число треугольников, u32 отпечаток
// COL1, дальше (ntri+1)/2 байт, у чётного треугольника младший полубайт. Полубайт: биты 0..2 тип (как в SRF1), бит 3 помещение.
// Отпечаток: FNV-1a по первым 44 Б заголовка COL1 (поле flags, где смещение JMP1, не берём) и по 16 Б от 256 строк треугольников, равномерно.
uint32_t svSurfaceFingerprint(const void *section, unsigned size) {
  const ColHdr *h = (const ColHdr *)section; uint32_t f = 2166136261u;
  if (!section || size < 48 + 16 || memcmp(section, "COL1", 4)) return 0;
  const uint8_t *b = (const uint8_t *)section;
  for (int i = 0; i < 44; i++) f = (f ^ b[i]) * 16777619u;
  for (uint32_t k = 0; k < 256 && h->ntri; k++) {
    uint64_t row = (uint64_t)k * h->ntri / 256u, o = 48 + row * 48;
    if (o + 16 > size) return 0;
    for (int i = 0; i < 16; i++) f = (f ^ b[o + i]) * 16777619u;
  }
  return f ? f : 1u;
}
// side — прочитанный файл рядом с уровнем (вызывающий держит его до svSurfaceFree). 1 — принят, 0 — не подходит (чужой уровень, битый) -> «дорога»
int svSurfaceSide(const void *side, unsigned n) {
  if (SURFACE_OLD || !sfW || !(sfSec || sfRingFp) || sfByte || !side || n < 16 || memcmp(side, "SRFS", 4)) return 0;
  uint32_t v[3]; memcpy(v, (const uint8_t *)side + 4, 12);
  if (sfSec) {
    const ColHdr *h = (const ColHdr *)sfSec;
    if (v[0] != 1 || v[1] != h->ntri || v[2] != svSurfaceFingerprint(sfSec, sfSecSz) || 16 + ((uint64_t)v[1] + 1) / 2 > n) return 0;
  } else if (v[0] != 1 || v[1] != sfRingNtri || v[2] != sfRingFp || 16 + ((uint64_t)v[1] + 1) / 2 > n) return 0;   // кольцо: отпечаток из заголовка файла кольца
  sfNib = (const uint8_t *)side + 16; sfN = v[1];
  return 1;
}
int svSurfaceHave(void) { return sfByte || sfNib; }

// x, y, z — точка в мире (y — высота над землёй под ней, выше самой высокой опоры не берём); *indoor (может быть NULL) — 1, если опора
// помечена «помещение» (бит 0x80). Нет мира, нет блока или под точкой не треугольник земли (ящик, цилиндр) — дорога, не помещение.
int svSurfaceAt(float x, float y, float z, int *indoor) {
  if (indoor) *indoor = 0;
  if (SURFACE_OLD || !sfW || !(sfByte || sfNib)) return 0;
  float gy, n3[3]; int mat = 0; uint32_t ref = ~0u;
  // опора = самая высокая проходимая поверхность не выше y + 0,5 м (машина стоит колёсами на земле) и не ниже 4 м под точкой
  if (!colGroundMatId(sfW, x, z, y + 0.5f, y - 4.0f, &gy, n3, &mat, &ref)) return 0;
  if ((ref >> 29) != COL_TRI) return 0;
  uint32_t i = ref & 0x1fffffffu;
  if (i >= sfN) return 0;
  uint8_t t;
  if (sfByte) t = sfByte[i];
  else { t = (uint8_t)((sfNib[i >> 1] >> ((i & 1) * 4)) & 15); t = (uint8_t)((t & 7) | ((t & 8) ? 0x80 : 0)); }   // полубайт -> байт SRF1
  if (indoor) *indoor = (t & 0x80) ? 1 : 0;
  t &= 0x7f;
  return t < 8 ? (int)t : 0;
}
