// 108 шаг 0 (замеры З-1…З-4 из ОПТИМИЗАЦИЯ_PSP_ПРИЁМЫ_FABLE2_2026-11-13.md). Служебное: только сборка тура (SHR_REL_SMK == 0) и только по флагам сценария; в игровой сборке пусто, байты игры прежние.
//  flag piecebench 1  — З-1: в ближайшем кадре куски своей машины идут десять раз подряд настоящим путём (drawPieceM), строка PIECEBENCH: мкс на кусок в первом проходе и в 2–10
//  flag cachebench N  — З-2: цена промаха кэша данных на трёх массивах по 16 КБ (куча, статика, при N>=2 ещё быстрая память 0x00010000); N=3 — без вытеснения кэша (красный контроль: почти ноль)
//  З-3 — после «cachebench 2» узор в быстрой памяти и сверка раз в секунду (строка SCRPADGUARD) до конца тура
//  З-4 — счёт вызовов sinf/cosf/atan2f/hypotf/floorf на кадр через линковку --wrap (строка MATHCNT раз в секунду)
#ifndef SMKDIAG_H
#define SMKDIAG_H
#include "release.h"
#if !SHR_REL_SMK
extern int pieceBench;
extern unsigned mathCnt[5];
void pbReport(const unsigned *us, const int *n, int passes, void (*w)(const char *));
void cacheBenchRun(int mode, int emu, void (*w)(const char *));
void scrGuardTick(float t, const char *scene, void (*w)(const char *));

// Измерители стратегии Fable (СТРАТЕГИЯ_ОПТИМИЗАЦИИ_FABLE_2026-11-13 §«что мерить»), только тур:
//  CELLSTAT раз в секунду — разбор клетки (cellFinish, нить загрузчика): число, среднее и наибольшее время (стенное: нить низкого приоритета, главный её прерывает — оценка сверху) и счёт чтений клетки с повтором
//    (одна и та же клетка прочитана за уровень второй раз и далее: reread — «выбросили и снова читаем»);
//  flag geonce 1 — «тот же список одной отправкой»: после обычного конца кадра список кадра (list) отправляется в видеочип ещё раз без остановки и ждётся до конца: GEONCE ge_ms — сколько видеочип реально занят этим кадром
//  (процессор не строит, не ждёт); flag geonce 0 — выключить. На кадр рисуется второй раз поверх (картинка кадра не меняется, смесь прозрачного может чуть темнеть) — только служебная сцена
void cellParseNote(int cell, int level, unsigned us);
int cellStatLine(char *buf, unsigned n, float t, const char *scene);
extern int geOnceOn;
void geOnceAdd(float geMs, unsigned words, float cpuMs, float waitMs);
int geOnceLine(char *buf, unsigned n, float t, const char *scene);
#define CELLPARSE_OPEN unsigned cpT0_ = sceKernelGetSystemTimeLow();
#define CELLPARSE_CLOSE(i, lvl) cellParseNote((i), (lvl), sceKernelGetSystemTimeLow() - cpT0_);
#define PCB_OPEN int pbReps_ = pieceBench ? 9 : 0; unsigned pbUs_[10] = {0}; int pbN_[10] = {0}; for (int pbR_ = 0; pbR_ <= pbReps_; pbR_++) { unsigned pbT_ = sceKernelGetSystemTimeLow();
#define PCB_COUNT pbN_[pbR_]++;
#define PCB_CLOSE pbUs_[pbR_] = sceKernelGetSystemTimeLow() - pbT_; } if (pbReps_) { pbReport(pbUs_, pbN_, pbReps_ + 1, smkWrite); pieceBench = 0; }
#else
#define CELLPARSE_OPEN
#define CELLPARSE_CLOSE(i, lvl)
#define PCB_OPEN
#define PCB_COUNT
#define PCB_CLOSE
#endif
#endif
