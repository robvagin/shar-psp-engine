// genest.h — №1186/№1188: защита от вложенного списка видеочипа. Свой sceGuStart(GU_DIRECT)+sceGuSync при открытом списке кадра (main_loop.h: sceGuStart кадра .. sceGuFinish)
// ставит список в очередь ЗА незаконченным списком кадра: очередь видеочипа не опустеет, главный поток висит на SceGeQueue (STALL phase=ui). Каждое место со своим списком
// начинается с GE_NEST_GUARD("имя"): при открытом списке кадра — отказ (возврат), строка «GE NESTED why=…» в ящик и smoke.txt, счётчик geNestedN для гейта.
// GENEST_OLD=1 — защита выключена (прежнее поведение; красный контроль гейта)
#ifndef GENEST_H
#define GENEST_H
#ifndef GENEST_OLD
#define GENEST_OLD 0
#endif
extern volatile int geListOpen;   // 1 между sceGuStart кадра и его sceGuFinish (main_loop.h)
extern unsigned geNestedN;        // отказов за запуск; гейт: 0 по всему туру
void geNestedNote(const char *why);   // main_frontend.h
#define GE_NEST_GUARD(why) do { if (!GENEST_OLD && geListOpen) { geNestedNote(why); return; } } while (0)
#define GE_NEST_GUARD_RET(why, r) do { if (!GENEST_OLD && geListOpen) { geNestedNote(why); return (r); } } while (0)
#endif
