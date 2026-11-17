// 107б P0 (правка Роба 1273, генплан Fable 13.11 §P0): одно место, которое знает про сон и пробуждение PSP. Все долгие файлы и пометки «битое/больше не пробовать» сверяются с эпохой.
//   unsigned pwrEpoch(void)       — растёт ОДИН раз за пробуждение (на RESUME_COMPLETE; если её нет за 2 с — запасной ход на главной нити) и на смене уровня (pwrBump("level")). Хранить свою копию; не совпало — сбросить пометки и переоткрыть дескриптор
//   unsigned pwrResumeCount(void) — пробуждений за запуск;  int pwrAsleep(void) — приставка спит (между SUSPENDING и RESUMING)
//   void pwrBump(const char *why) — ручная смена эпохи (уровень, служебное); пишет строку PWR bump
//   int pwrSimFault(const char *who) — 1: это чтение/открытие долгого файла надо ИСПОРТИТЬ (flag simsleep N даёт N таких отказов подряд); звать в точке sceIoRead/sceIoReadAsync/Open как «отказ»
//   void pwrKillHook(const char *name, void (*kill)(void)) — «убийца дескриптора» владельца: на simsleep закрывает файл, переменную с дескриптором не обнуляет (читать мёртвый, как после сна)
//   void pwrSimSleep(int n)       — flag simsleep N (только сборка тура): сон + kill-хуки + N отказов + пробуждение. Строки PWR suspend/resume n= epoch= t= в ящик и журнал
// Счётчики объявлены слабыми определениями: заголовок можно включать в любой модуль (и в хост-сборки), линковка power.c не нужна для чтения эпохи.
#ifndef POWER_H
#define POWER_H
volatile unsigned pwrEpochV __attribute__((weak));
volatile unsigned pwrResumeN __attribute__((weak));
volatile unsigned pwrSuspendN __attribute__((weak));
volatile int pwrAsleepV __attribute__((weak));
volatile int pwrSimN __attribute__((weak));
volatile unsigned pwrResumeAtUs __attribute__((weak));   // время последнего пробуждения (мкс системного таймера)
volatile unsigned pwrSimAt __attribute__((weak));   // системное время последнего flag simsleep (0 — не было): пустышка ME молчит 5 с после него
volatile unsigned pwrSimHits __attribute__((weak));
static inline unsigned pwrEpoch(void) { return pwrEpochV; }
static inline unsigned pwrResumeCount(void) { return pwrResumeN; }
static inline int pwrAsleep(void) { return pwrAsleepV; }
static inline int pwrSimFault(const char *who) { (void)who; if (pwrSimN > 0) { pwrSimN--; pwrSimHits++; return 1; } return 0; }
void pwrBump(const char *why);
void pwrNoteSuspend(void);            // из обработчика питания (нить обратного вызова): только флаги, строку в ящик пишет pwrTakeLog из главной нити
void pwrNoteResume(int flags);
int pwrTakeLog(char *buf, unsigned n);   // 1 — есть строка PWR для ящика и журнала (главная нить, раз в кадр)
void pwrKillHook(const char *name, void (*kill)(void));
void pwrSimSleep(int n);
void pwrSleepArm(int pt);      // flag simsleepat N (тур): сон наступит в точке ожидания N (1 — ожидание персонажей комнаты, 2 — загрузка уровня под газетой); без «взвода» точки ничего не делают
void pwrSleepAtPoint(int pt);
#endif
