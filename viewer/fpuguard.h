// Правка 252 (03.10, зависание всей PSP у особняка L1 F10-4/F10-5: звук пропал, сторож молчит, ловушки ещё не было):
// ловушки плавающей точки. Поток PSP стартует с регистром FPU FCR31 = 0x00000e00 (так его ставит PPSSPP в sceKernelThread.cpp,
// но сам эмулятор исключений FPU не делает; на железе значение покажет шапка ящика «fpu до>после»): включены ловушки
// «недопустимая операция» (0/0, ∞−∞, sqrt(<0), NaN в сравнении c.lt/c.le, float->int вне диапазона), «деление на ноль» и
// «переполнение». Движок FCR31 не трогал (в ELF ни одной ctc1) — одно редкое 0/0 в любом потоке на PSP = исключение
// процессора: без обработчика встаёт вся приставка (звук тоже); ловит ли их sharexc — не проверено (в турах 259 он молчал,
// проверка — служебные файлы fputrap + fpeprobe). Пример класса: путь 259 до 133b926 делил 1/0 в sceGumPerspective (FOV 0)
// и умножал 0·∞ в sceGumMultMatrix — эмулятор-проверяльщик (tools/twins/ppsspp_hwcheck_build.sh) видит это сразу.
// Правило (все уровни, все потоки движка): при старте потока ловушки FPU выключаются (fpuThreadInit), а сами случаи не
// пропадают — флаги FCR31 «было V/Z/O» поток раз в цикл переносит в общий учёт (fpuThreadPoll), главный поток раз в секунду
// пишет событие FPE в чёрный ящик (поток и флаги) и строку в smoke.txt. Игру это не меняет: в обычной игре таких случаев нет
// (замер 03.10: 0 на 3,7·10^9 операций FPU по пути F10 в проверяльщике, 10 посадок, 3 — с разбитием), меняется только исход
// редкого случая — вместо остановки всей PSP запись в ящике.
// Прежнее (ловушки как у PSP по умолчанию, для фото красного экрана sharexc с адресом): служебный файл fputrap рядом с EBOOT
// (потоки звука начаты до чтения файла — у них ловушки выключены всегда) или сборка -DFPU_TRAP_KEEP=1 (все потоки правила).
// Потоки cb (выход по HOME) и bootsnd (звук заставки) правило не трогает — как было. Только для PSP (asm MIPS): подключают
// main.c, audio.c (часть PSP), movie.c; мир ботов на Маке — нет.
#ifndef FPUGUARD_H
#define FPUGUARD_H
#define FPU_EN_SHIFT 7                    // биты разрешения ловушек: I U O Z V (7..11)
#define FPU_FLAG_SHIFT 2                  // липкие флаги «было»: I U O Z V (2..6)
#define FPU_VZO 0x1Cu                     // V Z O (младшие I U не ловим и не считаем: неточность и потеря точности — норма)
#define FPU_KEEP_MASK 0x01800003u         // FS, cc0 и режим округления — остаются как были
enum { FPU_T_MAIN, FPU_T_LOADER, FPU_T_MIX, FPU_T_AIO, FPU_T_BBW, FPU_T_DOG, FPU_T_MOVIE, FPU_T_BOOT, FPU_T_N };
#include "reliab.h"
extern volatile int fpuKeep;              // 1 — ловушки как у PSP по умолчанию (служебный файл fputrap / FPU_TRAP_KEEP)
extern volatile unsigned fpuSeenThr, fpuSeenFlg;   // какие потоки видели V/Z/O (бит FPU_T_*) и какие флаги (V=16 Z=8 O=4)
static inline unsigned fpuGet(void) { unsigned v; __asm__ volatile("cfc1 %0, $31" : "=r"(v)); return v; }
static inline void fpuSet(unsigned v) { __asm__ volatile("ctc1 %0, $31" : : "r"(v) : "memory"); }
// начало потока: ловушки выключить (если не fpuKeep), флаги и причину сбросить; -> FCR31 до правки (для шапки ящика)
static inline unsigned fpuThreadInit(void) {
  relStackPaint();   // №1195: стек потока закрасить (зонд высоты заполненности, reliab.c)
  unsigned f = fpuGet();
#if !defined(FPU_TRAP_KEEP) || !FPU_TRAP_KEEP
  if (!fpuKeep) fpuSet(f & FPU_KEEP_MASK);
#endif
  return f;
}
// цикл потока: флаги «было V/Z/O» -> общий учёт, свои флаги сбросить (дёшево: две команды и сравнение)
static inline void fpuThreadPoll(int t) {
  unsigned f = fpuGet(), g = (f >> FPU_FLAG_SHIFT) & FPU_VZO;
  if (!g) return;
  fpuSeenFlg |= g; fpuSeenThr |= 1u << t;
  fpuSet(f & ~(0x1Fu << FPU_FLAG_SHIFT) & ~(0x3Fu << 12));
}
#endif
