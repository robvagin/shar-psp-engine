#ifndef COLL_STREAM_H
#define COLL_STREAM_H
#include "gag_skin.h"
#ifndef SYNCREAD_P1_OLD
#define SYNCREAD_P1_OLD 0   // ТЕСТ90 П1: 1 — гэги читаются синхронно в кадре, как в TEST89
#endif
#ifndef GAG_SKIN_STREAM_OLD
#define GAG_SKIN_STREAM_OLD 0
#endif
#ifndef COLLSPLIT_OLD
#define COLLSPLIT_OLD 0   // Пропечка-96 (Fable шаг 11): 1 - вершины гэгов в блоке пакета целиком, как в Build 95
#endif
extern int gagPlan99Off;extern unsigned collStreamPlanN,collStreamPlanKB;extern int gagSkinStreamOld,collSplitOld;extern unsigned collHoldMs;   // flag collhold N - служебное: каждое фоновое чтение гэга держится N мс
const void *collStreamVerts(unsigned type);   // база вершин типа под piece->vOff; 0 - вершин сейчас нет, тип не рисовать
int collStreamSplitOn(void),collStreamTexSplitOn(void);
extern unsigned collStreamVertBytes,collStreamVertLoads,collStreamVertSaved,collStreamTexBytes,collStreamTexSaved;
#define COLL_STREAM_TW 4   // слов маски текстур: до 128 текстур пакета, больше - текстуры остаются в блоке
// Текстуры гэгов по номеру: движок (main_collect.h) строит по записи Tex в cxTex; unbind - до освобождения буфера
extern void (*collStreamTexBind)(unsigned k,unsigned char *rec,unsigned size);extern void (*collStreamTexUnbind)(unsigned k);
void collStreamTexRebind(void);   // привязать уже загруженные (постоянные) текстуры, когда движок поставил хуки
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
extern int collGagPend1257Old;   // Будка-93 (1257): 1 - счёт ожидания гэгов как был
unsigned collStreamPending(void);   // Будка-93 (1226): чтений в пути и в очереди
extern unsigned collStreamAsyncStarts,collStreamAsyncDone,collStreamAsyncDefer,collStreamAsyncCancels,collStreamAsyncFails;   // для журнала: заданий начато / принято / ключей, отложенных на кадр
#endif
