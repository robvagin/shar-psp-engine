// Релизная сборка (docs/handoff/РЕЛИЗ_СЛУЖЕБНОЕ_2026-10-10.md): -DSHAR_RELEASE=1 (tools/final-build.sh --release, по умолчанию ВЫКЛЮЧЕНО) глушит
// служебное, ничего не удаляя из кода. Все служебные файлы-флаги и журналы рядом с EBOOT идут через sibling()/bootSib(): в релизе их имена
// отправляются в несуществующую папку, поэтому открыть их нельзя и писать некуда; код чтения/записи не меняется (он и так проверяет f < 0).
//   SHAR_RELEASE_KEEP_DIAG=1  — оставить самописец и ловушку вылета (blackbox*.txt, crash*.txt, sharexc.prx): решает Роб
//   SHAR_RELEASE_KEEP_HUD=1   — оставить игроку счётчик FPS/треугольников/RAM (R+SELECT), как просил Роб: решает Роб
//   SHAR_RELEASE_KEEP_RESET=1 — оставить RESET CAR на SELECT в машине (№484: к релизу убрать)
#ifndef SHAR_RELEASE_H
#define SHAR_RELEASE_H
#include <string.h>
#ifdef SHAR_RELEASE
#define SHR_REL 1
#else
#define SHR_REL 0
#endif
#ifdef SHAR_RELEASE_KEEP_DIAG
#define SHR_KEEP_DIAG 1
#else
#define SHR_KEEP_DIAG 0
#endif
#ifdef SHAR_RELEASE_KEEP_HUD
#define SHR_KEEP_HUD 1
#else
#define SHR_KEEP_HUD 0
#endif
#ifdef SHAR_RELEASE_KEEP_SMOKE
#define SHR_KEEP_SMOKE 1   // тур на железе в релизной конфигурации (TEST20b): остаются журнал смоука (smokelog, smoke.txt), сценарий (scenario.txt) и их строки в кадре; всё остальное служебное — как в релизе
#else
#define SHR_KEEP_SMOKE 0
#endif
#define SHR_REL_SMK (SHR_REL && !SHR_KEEP_SMOKE)   // условие «смоука нет» в viewer/smoke.h
#ifdef SHAR_RELEASE_KEEP_RESET
#define SHR_KEEP_RESET 1
#else
#define SHR_KEEP_RESET 0
#endif
// 1 — имя служебное и в релизе не открывается
static int shrDevName(const char *n) {
  static const char *dev[] = {"service", "toon", "fb32", "view60", "slowmo", "slowio", "autobench", "autodrive", "autostraight", "pvscheck", "pvscheck.txt",
    "bench.txt", "goto.txt", "opts.txt", "camang.txt", "carseq.txt", "testdmg", "testboom", "clearchk", "flypath", "fpeprobe", "fputrap",
    "gagskinstreamold", "lvlogoson", "mefxmax", "mefxpair", "mepeak", "near07", "nolayer", "toonworld", "oldfog", "oldstream", "pcold", "seamchk", "seamsold", "streamlog", "stream.txt",
    "smokelog", "scenario.txt", "ballast", "jitter", "nojitter", "smoke.txt", "intro.txt", "sfxlog", "sfxlog.txt", "trace.txt",
    "flatdump", "flatrescanold", "flatrebuildold", "acct", "acctoff", "acct.txt", "loadacc", "bisect", "groundbias", "pausecellold", "startnoloadold", "uifit.txt"};   // 16.10 аудит диагностики; файлы-выключатели игрока (hudon, mecityoff, startnewsold, newsiris, coneold, homerspotold, homerlitold, loadsilent, regprehyst) сюда не входят
  static const char *diag[] = {"blackbox.txt", "blackbox_prev.txt", "crash.txt", "crash_prev.txt", "sharexc.prx"};
  if (SHR_KEEP_SMOKE && (!strcmp(n, "smokelog") || !strcmp(n, "scenario.txt") || !strcmp(n, "smoke.txt") || !strcmp(n, "trace.txt"))) return 0;
  if (SHR_KEEP_SMOKE && !strcmp(n, "slowio")) return 0;   // ag/loadorder: имитация медленной карты нужна турам и прогонам в релизной конфигурации (без KEEP_SMOKE файл по-прежнему не открывается)
  for (unsigned i = 0; i < sizeof dev / sizeof *dev; i++) if (!strcmp(n, dev[i])) return 1;
  if (!SHR_KEEP_DIAG) for (unsigned i = 0; i < sizeof diag / sizeof *diag; i++) if (!strcmp(n, diag[i])) return 1;
  return 0;
}
#define SHR_OFF_DIR "ms0:/.shar_release_off/"   // такой папки нет: открытие на чтение и запись с созданием не удаётся
#endif
