// Переходы как в оригинале (пакет «переходы и выброс при взрыве», Роб 28.09): ирис — «кружочек стягивается к центру,
// виден только центр, мультяшно, потом раскрывается». Сетка, морф, ключи и камера — из данных диска (Scrooby 3dIris.pag ->
// pure3d\iris.p3d, экспорт tools/iris_export.py -> iris.shr рядом с EBOOT). Нет пакета — переходы мгновенные, как раньше.
//
// Как идёт переход: transitClose — кадр мира стоит (последний показанный кадр), ирис закрывается по ключам STE данных
// (кадры 0 -> 10 при 30 кадр/с); вызывающий меняет мир (Гомер, комната, перенос миссии), дальше transitStep/transitDraw
// держат закрытый ирис (кадры 10 -> 15) и раскрывают его (15 -> 25) уже над живой игрой.
// Правила порта (сверить с эталоном PS2): мир на время закрытия стоит; отверстие — по центру экрана (как 3dIris.pag);
// масштаб «cover» — полностью открытый ирис относится к полудиагонали 480×272 так же, как в оригинале к 640×480
// (открыт 407 px при полудиагонали 400); LoadingText.pag экрана IrisWipe.scr не рисуется.
//
// Служебное (smoke.txt, строки ключ=значение):
//   IRIS why=.. phase=close|hold|open f=кадр w=вес морфа r=радиус отверстия (px, вписанный) t=..
//   BOOM / EJECT — взрыв машины и где Гомер через 2.5 с (transitEjectWatch)
#ifndef TRANSIT_H
#define TRANSIT_H
#include "homer.h"
#include "car.h"

typedef void (*TransitLog)(const char *line);
int  transitLoad(const char *path);                       // iris.shr: 0 = ок
void transitSetup(int psm, unsigned fbBytes, TransitLog log, const float *clock);   // формат кадра, байт на буфер, журнал
int  transitReady(void);                                   // пакет есть и переходы не выключены
int  transitSkipMsn(void);   // №625: по Continue снять ирис старта миссии, не раскрывая (карточка уезжает над миром); 1 — снят
#include "genest.h"   // №1186: geListOpen, GE_NEST_GUARD
int  transitClose(void **fbp, const char *why);            // закрыть над застывшим кадром (блокирует ~1/3 с); -> показано кадров развёртки
void transitStep(float dt);                                // закрыто -> раскрытие над игрой
void transitDraw(void);                                    // внутри открытого списка GE, после HUD
int  transitBusy(void);                                    // 1 — ирис на экране
// P0.2/A1 Опора-93 (канон Роба п.11, «экран держит переход»): пока крючок возвращает не 0 (1 — под героем нет опоры, 2 — нет сцены/клеток), закрытый ирис не раскрывается:
// герой не показан, мир и камера под ним ещё не готовы. Предел — HOMER_SPAWN_WAIT_T + 1 с (потом раскрытие как раньше, дальше решает сторож). Строка SPAWNHOLD ms=.. why=ground|scene.
// SPAWNHOLD_OLD=1 — прежнее (ирис раскрывается по таймеру)
extern int (*transitHoldHook)(void);
extern int transitHoldGate;
extern void (*transitHoldNote)(const char *line);
void transitDrop(void);                                    // №663: снять закрытый ирис, не раскрывая
int  transitFullyClosed(void);                             // 1 — экран закрыт ирисом/чёрным целиком: мир под ним не виден (брифинг, main_loop.h)
// причина перехода Гомера за этот кадр: "overtop" (выход через верх), "seat" (посадка при занятой двери), "car_in" / "car_out"
// (правка 333: посадка / выход у машины с SetIrisTransition, темп ириса — данных), NULL — нет
const char *transitHomerWhy(const Homer *H);
// журнал взрыва: boomNow = кадр взрыва; через 2.5 с строка EJECT (пешком ли, насколько вне кузова)
void transitEjectWatch(const Homer *H, const Car *c, int haveHomer, int boomNow, float dt);
// Правка 349: затемнение зоны DEATH оригинала ([PS2] 05.10 pcsx2-twins: не ирис — ровное затемнение всего кадра с HUD, мир стоит,
// чёрный экран, перенос, затем картинка сразу, без проявления). Пакет iris.shr не нужен.
int  transitFadeReady(void);                               // кадр настроен (transitSetup) и переходы не выключены
int  transitFade(void **fbp, const char *why, float sec);  // гаснет над застывшим кадром за sec (блокирует); -> показано кадров
void transitHold(float sec);                               // после переноса: чёрный экран над живой игрой sec, потом картинка сразу
// служебное
void transitSlow(float k);                                 // время ириса ×1/k (снимки середины)
void transitOff(int off);                                  // отрицательный контроль: без ириса
#endif
