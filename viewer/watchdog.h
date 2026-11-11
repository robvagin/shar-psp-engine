// Сторож (канон Роба 06.11, CLAUDE.md п.12): один общий модуль инвариантов игры. Каждый кадр wdStep(dt) зовёт проверки всех видов;
// проверка сама чинит нарушение и возвращает 1 (нарушение было) и заполняет info («pos=… fix=…»). Каждое срабатывание пишет в журнал строку
//   WATCHDOG <вид> actor=<кто> t=<время> <info>
// Игроку молча, ботам громко: журнал читают tools/night/msnflow_report.py и night_report.py (графа «сторож»), любое срабатывание = дыра, которую чинят в корне.
// Новый вид (окна «Меню» — msnsel, «Погоня» — chase и другие):
//   static int myCheck(void *user, float dt, char *info, int cap) { ...если нарушено: починить, snprintf(info, cap, "pos=%.1f,%.1f,%.1f fix=reset", ...); return 1; ...return 0; }
//   wdRegister("msnsel", "menu", myCheck, &ctx);      // один раз при старте уровня/игры; повторная регистрация того же вида заменяет проверку
//   wdStep(dt);                                       // зовёт главный цикл (main_loop.h) и боты (tools/bots) — свой вызов не нужен
// Если срабатывание рождается вне кадровой проверки (событие), вызови wdFire("msnsel", "menu", "state=… fix=…") — тот же журнал и счётчики.
// wdSetLog(fn): приёмник строк журнала (движок — smkWrite при smkOn, боты — stdout). wdCount(kind) / wdTotal() — счётчики для гейтов; wdReset() — обнулить (смена уровня не обнуляет).
// Выключатель: WATCHDOG_OFF=1 (сборка) / wdEnable(0) — проверки не зовутся (красный контроль: нарушение остаётся нечинёным).
#ifndef SHAR_WATCHDOG_H
#define SHAR_WATCHDOG_H
#ifdef __cplusplus
extern "C" {
#endif
#ifndef WATCHDOG_OFF
#define WATCHDOG_OFF 0
#endif
#define WD_KINDS 16
typedef int (*WdCheckFn)(void *user, float dt, char *info, int cap);
int wdRegister(const char *kind, const char *actor, WdCheckFn fn, void *user);   // 0 — ок, -1 — нет места
void wdUnregister(const char *kind);
void wdStep(float dt);
void wdFire(const char *kind, const char *actor, const char *info);
void wdSetLog(void (*fn)(const char *line));
void wdSetClock(float (*now)(void));   // время в строке t= (движок — smkT); без часов — собственные секунды по сумме dt
void wdEnable(int on);
void wdNote(const char *kind, const char *actor, const char *info);   // Build 96: сведение (WDNOTE), не нарушение: боты и туры не считают его дырой
unsigned wdCount(const char *kind);
unsigned wdTotal(void);
void wdReset(void);
#ifdef __cplusplus
}
#endif
#endif
