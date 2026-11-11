// Fable шаг 14 / дирижёр 08.11 (клинья 1236): аудит кэша данных процессора PSP (16 КБ, обратная запись). Видеочип читает вершины, индексы, палитры и текселы из ОЗУ, а процессор пишет их через кэш.
// Два правила: (1) после ЛЮБОЙ записи процессора в память, которую читает видеочип, — sceKernelDcacheWritebackRange; запись ПОСЛЕ прежнего writeback (правка вершин на месте) требует ещё одного;
// (2) перед чтением с карты (DMA) в блок, который раньше использовался и мог остаться с грязными строками кэша (переиспользуемые блоки клеток, комнат, моделей, персонажа), — sceKernelDcacheWritebackInvalidateRange:
// иначе старая грязная строка при вытеснении ложится ПОВЕРХ только что прочитанных данных (порча вершин: тонкие длинные треугольники-«клинья»). Эмулятор кэш не моделирует — только железо.
// DCACHE96_OLD=1 (сборка) / flag dcache96 0 / файл dcache96old — как было. dcacheall96 (файл рядом с EBOOT, только тур-сборка) — диагностика: sceKernelDcacheWritebackAll в начале кадра и перед каждым sceGuFinish листа/кадра.
#ifndef SHAR_DCACHE96_H
#define SHAR_DCACHE96_H
#if defined(PSP) || defined(__psp__) || defined(_PSP)
#include <pspkernel.h>
extern int dcache96, dcAll96On; extern unsigned dc96Calls, dc96Bytes, dc96PostCalls, dc96PostBytes;   // счётчики для строки DCACHE96 (цена в байтах за кадр)
#define DCPRE96(p, n) do { if (dcache96 && (p) && (n)) { dc96Calls++; dc96Bytes += (unsigned)(n); sceKernelDcacheWritebackInvalidateRange((p), (n)); } } while (0)   // перед чтением с карты в блок
#define DCPOST96(p, n) do { if (dcache96 && (p) && (n)) { dc96PostCalls++; dc96PostBytes += (unsigned)(n); sceKernelDcacheWritebackRange((p), (n)); } } while (0)            // после правки данных видеочипа на месте
#define DCALL96() do { if (dcAll96On) sceKernelDcacheWritebackAll(); } while (0)                                         // диагностика dcacheall96
#else   // хост (боты, гейты на Маке): кэша нет
#define DCPRE96(p, n) do { } while (0)
#define DCPOST96(p, n) do { } while (0)
#define DCALL96() do { } while (0)
#endif
#endif
