// Надёжность (разбор arch-fable 04.11.2026, НАДЁЖНОСТЬ §4, риски 8–10): три страховки без изменения игры в обычном ходе.
//  1) релиз замка по пределу времени: спин-замок или семафор, которого никто не отпустит, не вешает поток навсегда — выход со строкой LOCK в ящик;
//  2) зонд стека потоков: стек потока при старте закрашивается, раз в 5 с главный поток меряет, сколько закраски осталось (высота заполненности), строка STACK в ящик при росте
//     на 1 КБ и предупреждение WARN при запасе меньше 10 %;
//  3) потокобезопасная строка события relNote: любой поток кладёт строку в кольцо, главный раз в кадр отдаёт кольцо в ящик и smoke.txt (relFrame).
// Выключатели: LOCKLIM1195_OLD=1 (замки без предела, как в TEST89); закраска и зонд стека — по умолчанию ВЫКЛЮЧЕНЫ, включает файл relstackon рядом с EBOOT / -DRELSTACK1195_ON=1 / «flag relstackon 1» (до старта потоков),
// RELSTACK1195_OLD=1 и файл relstackold выключают совсем.
#ifndef RELIAB_H
#define RELIAB_H
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifndef LOCKLIM1195_OLD
#define LOCKLIM1195_OLD 0
#endif
#ifndef RELSTACK1195_OLD
#define RELSTACK1195_OLD 0
#endif
#ifndef RELSTACK1195_ON
#define RELSTACK1195_ON 0   // закраска стека по умолчанию ВЫКЛЮЧЕНА до проверки на железе (TEST90 на PSP падал сразу после загрузочного экрана)
#endif
extern volatile int relLockOld, relStackOld, relStackOn;   // relStackOn: 1 — закраска и зонд стека работают (файл relstackon рядом с EBOOT, flag relstackon, -DRELSTACK1195_ON=1)
#define REL_LOCK_LIMIT_US 3000000u   // замок держат миллисекунды; 3 с — заведомо «никто не отпустит»
void relNote(const char *fmt, ...);   // любой поток; строка с «\n» в конце не нужна
// спин-замок с пределом: 1 — взят, 0 — не взят за limitUs (строка LOCK name timeout). delayFn(us) — сон между попытками (на PSP sceKernelDelayThread); при relLockOld предела нет
int relSpinAcquire(volatile int *lock, unsigned limitUs, unsigned stepUs, void (*delayFn)(unsigned), const char *name);
// закрашенный стек: сколько байт закраски осталось с нижнего края (стек растёт вниз), и условие предупреждения (запас меньше 10 % размера)
#define REL_STACK_PAINT 0xA5u
void relStackPaintRange(unsigned char *base, unsigned n);   // закрасить n байт с base (при relStackOld — ничего)
unsigned relStackHeadroom(const unsigned char *base, unsigned size);
int relStackWarn(unsigned headroom, unsigned size);
// PSP: закрасить стек текущего потока (вызывает fpuThreadInit каждого потока) и раз в 5 с проверять; relFrame зовёт главный поток каждый кадр; sink получает строки с «\n»
void relStackPaint(void);
void relFrame(void (*sink)(const char *line), unsigned nowUs);
// служебное (тест): число записанных/выданных строк и последние данные зонда
unsigned relNoteCount(void);
#ifdef __cplusplus
}
#endif
#endif
