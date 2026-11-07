#ifndef COLL_STREAM_H
#define COLL_STREAM_H
#include "gag_skin.h"
#ifndef SYNCREAD_P1_OLD
#define SYNCREAD_P1_OLD 0   // ТЕСТ90 П1: 1 — гэги читаются синхронно в кадре, как в TEST89
#endif
#ifndef GAG_SKIN_STREAM_OLD
#define GAG_SKIN_STREAM_OLD 0
#endif
extern int gagSkinStreamOld;
extern unsigned collStreamResidentBytes,collStreamTrackBytes;
void *collStreamLoad(Collect *c,CollectRead read,void *user,unsigned size);
int collStreamPrepare(Collect *c,int room,float ex,float ez,float distance,int snap);
void collStreamFree(void);
// ТЕСТ90 П1 (SYNCREAD_P1_OLD): фоновое чтение гэгов. start — начать чтение size байт с off в data (0 — начато); poll — 1 готово и прочитано целиком, 0 идёт, -1 ошибка;
// wait — дождаться конца (при выгрузке уровня). Нет таблицы (хост, файл syncreadp1old) — как раньше, чтение синхронно в кадре
// cancel — прервать идущее чтение и дождаться конца, чтобы буфер можно было освободить (sceIoCancel + sceIoWaitAsync); nowMs — часы в миллисекундах (тайм-аут задания COLL_ASYNC_MS)
typedef struct { int (*start)(void *user,unsigned off,void *data,unsigned size); int (*poll)(void *user); void (*cancel)(void *user); unsigned (*nowMs)(void *user); void *user; } CollectAsync;
#define COLL_ASYNC_MS 3000u   // задание не пришло за 3 с — отмена, гэг не играет
void collStreamSetAsync(const CollectAsync *a);
extern unsigned collStreamAllocFails,collStreamLastFail;extern int collFailTest;   // ТЕСТ90 память: отказов выделения пакета гэгов; «flag collfail N» — следующие N выделений без памяти (проверка повтора)
extern unsigned collStreamAsyncStarts,collStreamAsyncDone,collStreamAsyncDefer,collStreamAsyncCancels,collStreamAsyncFails;   // для журнала: заданий начато / принято / ключей, отложенных на кадр
#endif
