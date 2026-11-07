#ifndef GAG_SKIN_H
#define GAG_SKIN_H
#include "collect.h"
typedef int (*CollectRead)(void *user,unsigned off,void *data,unsigned size);
int gagSkinInit(const Collect *c);
int gagSkinInitSource(const Collect *c,CollectRead read,void *user,unsigned fileSize);
int gagSkinPrepare(const Collect *c,const unsigned char *wanted);
// ТЕСТ90 П1: те же гэг-скины по частям без чтения в кадре. Trim выгружает ненужные; Next даёт следующий нужный, но не загруженный (запись, смещение, размер в файле);
// Adopt принимает прочитанный в фоне буфер (память принадлежит гэгам), 0 — принят, иначе буфер остаётся у вызывающего
void gagSkinTrim(const unsigned char *wanted);
int gagSkinNext(const unsigned char *wanted,unsigned *idx,unsigned *off,unsigned *size);
int gagSkinAdopt(unsigned idx,unsigned char *buf);
int gagSkinGagOf(unsigned idx);        // номер гэга записи (для проверки «ещё нужен»)
void gagSkinMarkBad(unsigned idx);     // чтение не вышло (тайм-аут, ошибка): до выгрузки уровня не повторять — гэг просто не играет
extern unsigned gagSkinResidentBytes;
void gagSkinFree(void);
unsigned gagSkinDraw(const Collect *c, int room, float ex, float ez, float distance);
#endif
