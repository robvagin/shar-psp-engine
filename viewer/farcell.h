// Лестница-96 + Модели-96 (решение Роба 09.11, в 101): дальние версии клеток. Боковой файл l{N}/farcells.shr («FAR1», пишет tools/farcells_export.py), level.shr не меняется.
// У клетки две версии: полная (level.shr) и дальняя (farcells.shr: все куски клетки, вершины без индексов, упрощённые). В ОЗУ в каждый момент одна. Версия выбирается по краю клетки от глаза:
// дальше FAR_OUT_M — дальняя, ближе FAR_IN_M — полная, между — какая есть (гистерезис ±15 м вокруг 120 м, иначе клетка «дышит»). Смена версии у загруженной клетки — make-before-break:
// загрузчик читает другую версию в НОВЫЙ блок (Cell.swSt 1 -> 2), главный поток в начале stream() перевешивает куски (cellSwapDo), старый блок отдаёт. Бюджет области города и круга считается по размеру выбранной версии.
// Выключатели: FARCELL_OLD=1 / файл farcellold / «flag farcell 0» — только полные клетки, как до правки. Нет файла farcells.shr, не сошёлся ключ — то же самое.
// Формат (Модели-96): заголовок 64 Б {'FAR1', ver=1, key, ncell, npiece, offCells, offPieces, offBlob, mode}; таблица клеток по номеру клетки уровня, 16 Б {farOff от offBlob (0xFFFFFFFF — дальней нет), farSize, nvTotal, pad};
// таблица кусков по номеру куска, 16 Б {nvFar, offFar (внутри дальнего блоба своей клетки), pad[2]}.
// Что делает отрисовка (Модели-96): cellFinishFar(i, m) — куски клетки на дальний блок m; cellRestoreFull(i) — вернуть Piece.r полной версии (перед cellFinish после дальней).
#ifndef FARCELL_OLD
#define FARCELL_OLD 1   // 103 (решение Роба 10.11): дальние клетки выключены по умолчанию, карта как в 101 (в 102 на PSP «мигание текстур»); включить сценарием «flag farcell 1» до загрузки уровня или сборкой -DFARCELL_OLD=0
#endif
#define FAR_OUT_M 135.f
#define FAR_IN_M 105.f
#define FAR_SW_MAX 4        // обменов версий одновременно в работе
#define FAR_HEAD_MS 26.f    // обмен только при запасе кадра: прошлый кадр короче FAR_HEAD_MS (FARHEAD_OLD=1 / flag farhead 0 - всегда)
#define FAR_HEAD_WAIT 8     // ...но не дольше стольких кадров ожидания у готового обмена
#define FAR_BACK_US 1500000u // после отказа обмена (нет места под новый блок) та же клетка не просится 1,5 с
#define FAR_MIN_US 3000000u  // одна клетка меняет версию не чаще раза в 3 с (дыхание у границы 105-135 м)
#define FAR_SW_FRAME 1      // разбирается за один кадр (остальные готовые ждут следующего)
static int farOn = !FARCELL_OLD;           // flag farcell 0 — выключено
static int farFake;                         // farOk (farcells.h Моделей): файл принят, у клеток есть fsize; farFake: служебная проба очереди (flag farfake 1: «дальняя» версия = та же полная из level.shr)
static char farPath[256];
static volatile unsigned lvGen;   // (то же объявление ниже в main_stream.h: поколение уровня)
static SceUID farFdLoader = -1; static unsigned farFdGen = ~0u;            // дескриптор файла у загрузчика (по поколению уровня lvGen)
static unsigned farLoads, farFulls, farSwapFail, farSwapStale, farFarBytes;   // farSwaps - в farcells.h   // счётчики для строки FARCELL
static short farSwq[FAR_SW_MAX]; static volatile int farSwn;                // очередь обменов: номера клеток
static int bonusShrink107 = 1;   // Лестница-96: flag bonusshrink 0 - бонус-трасса не отцепляет области Z3 перед чтением (красный контроль s26)
static float farLastWorkMs; static int farHeadOn = 1;   // работа прошлого кадра (main_loop.h) и выключатель запаса кадра
static unsigned farBackT[4096];   // метка последнего отказа обмена по клетке (мкс | 1)
static unsigned farSwT[4096]; static unsigned char farSwSeen[4096]; static unsigned farSwUniq, farSwRep;   // последний обмен клетки (мкс | 1), была ли обмененной, уникальных клеток, повторных обменов < 6 с
static int farMinOn = 1;   // flag farmin 0 / файл farminold - без минимального интервала обмена клетки
static unsigned farHeadWaits;   // кадров, в которые готовый обмен ждал запас кадра (FARHEAD), для строки FARCELL (hw)
