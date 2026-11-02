// №430 (ДОБИВКА 19): арест Hit & Run по 7 арестам PS2. Те же функции, что зовёт viewer/main.c hnrPursuit (viewer/game.c:
// gameHnrChaseStart / gameHnrSpawnDue / gameHnrCatch / gameHnrBustTime / gameHnrBustTick), против записей PS2
// build/ps2ref/answers/{b3_bust,b3_bust80,b3_fine30,b3_fine200,b3_coinauth,b3_coins30,b3_coins80}/trace.tsv, свёрнутых в
// tests/hnr/arrest_ps2_ref.h (tests/hnr/arrest_ps2_ref.py, sha256 каждой записи в заголовке). Колонка coins этих рецептов
// (0x9e79c0 — не монеты) не читается; штраф min(50, монеты) — отдельный гейт (ps2-hnr-police), здесь не трогается.
// Отсчёты записей идут через кадр (rel чётные): событие лежит не дальше 1 кадра до своего отсчёта, всё ниже — с точностью ±1 кадр.
//   delay  — полиция после начала погони: по отсчётам от первого с флагом 0x100 до первого с флагом 0x101 (34–38 rel в 7 записях),
//            порт при 50 Гц ±2 кадра; в секундах 0,68–0,76 и при 60 Гц PSP. 36 кадров (0,72 с) — правило порта в диапазоне
//            факта ps2-hnr-police «0,72–0,76 с», не общее для всех записей: в b3_fine200 и b3_coins30 (34 rel, событие 33–35)
//            порт позже PS2 на 1–3 кадра и проходит на краю допуска 2 (по позиции полиции общее для 9 записей b3_* — 35 кадров)
//   replay — отсчёты записи (дистанция до полиции, скорость машины, полиция в игре) кадр за кадром при 50 Гц через gameHnrCatch:
//            арест порта не раньше и не позже ареста PS2 больше чем на 4 кадра (такт физики PS2 25 Гц + фаза отсчёта ±1 такт,
//            ps2-hnr-police limitation); после дыры в записи (b3_bust rel 1000–1030, снимок) счётчик берётся из записи
//   timer  — таймер ареста при 50 Гц равен разнице отсчётов «первый с таймером — первый с нулём» (74 rel во всех 7). Разница
//            отсчётов всегда чётная и не отличает 73, 74 и 75; по мс 0x6121ec (1500 вниз) аресты длятся ~73–75 кадров
//            (b3_coins80 ~75). 1,48 с — правило порта: 1500 мс с шагом кадра ареста; при 60 Гц — в пределах кадра PSP
//   rule   — пороги (60 Гц PSP): 9,9 м стоя — арест за 0,75 с (порог 10 м факта ps2-hnr-police); 10,07 м — нет (выше всех
//            отсчётов сброса 0x6121dc: 9,934–10,063 м, b3_fine30 rel 274, b3_coinauth rel 270, b3_fine200 rel 366 и др.);
//            31,6 км/ч — нет (порог 30 км/ч — из факта; сброса по скорости в записях нет, 31,6 км/ч — отсчёт b3_bust rel 876,
//            когда счётчик уже 0 с rel 870); 25,2 км/ч на 4,8 м — арест (счётчик PS2 растёт, b3_bust rel 1030)
// Выход: CHECK … OK|FAIL, RESULT; код 1 — есть FAIL. КРАСНЫЙ: -DARREST_PS2_OLD=1 (tests/hnr/arrest_ps2.sh)
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "arrest_ps2_ref.h"

static int fails, checks;
static void check(const char *name, int ok, const char *fmt, double a, double b, double c) {
  char d[320]; snprintf(d, sizeof d, fmt, a, b, c);
  printf("CHECK %-7s %s  %s\n", name, ok ? "OK  " : "FAIL", d); checks++; if (!ok) fails++;
}
static int delayFrames(float dt) {   // main.c: кадр защёлки (кадр 0) ставит таймер и в том же кадре делает первый шаг
  float t = 0.f; gameHnrNewLevel(0); gameHnrChaseStart(&t, dt);   /* сверка g10-flow P03 Д4: каждая запись — первая погоня уровня */
  for (int f = 0; f < 1000; f++) if (gameHnrSpawnDue(&t, dt)) return f;
  return -1;
}
static int bustFrames(float dt) {
  Game g; memset(&g, 0, sizeof g); g.hnrBusted = gameHnrBustTime();
  for (int f = 1; f < 1000; f++) if (gameHnrBustTick(&g, dt)) return f;
  return -1;
}
static int holdFrames(float dt, float d, float kmh) {   // кадров близости до ареста (-1 — нет за 3 с)
  float t = 0.f;
  for (int f = 1; f <= (int)(3.f / dt); f++) if (gameHnrCatch(&t, dt, 1, d, kmh / 3.6f, 0)) return f;
  return -1;
}
int main(void) {
  const int nrec = (int)(sizeof ARREST_RECS / sizeof ARREST_RECS[0]);
  check("ref", nrec == 7, "записей PS2 %.0f (нужно 7)", nrec, 0, 0);
  // delay
  int f50 = delayFrames(1.f / 50.f), f60 = delayFrames(1.f / 60.f);
  for (int r = 0; r < nrec; r++) {
    const ArrestRec *R = &ARREST_RECS[r]; int ps2 = R->police - R->chase;
    char n[160]; snprintf(n, sizeof n, "%s: PS2 %%.0f rel (0x100 -> 0x101, ±1 кадр), порт %%.0f (50 Гц), допуск 2", R->name);
    check("delay", ps2 >= 34 && ps2 <= 38 && f50 >= 0 && abs(f50 - ps2) <= 2, n, ps2, f50, 0);
  }
  check("delay", f50 * .02f >= .68f - 1e-4f && f50 * .02f <= .76f + 1e-4f, "50 Гц: %.3f с (PS2 0,68–0,76)", f50 * .02f, 0, 0);
  check("delay", f60 / 60.f >= .68f - 1e-4f && f60 / 60.f <= .76f + 1e-4f, "60 Гц: %.3f с (PS2 0,68–0,76)", f60 / 60.f, 0, 0);
  // replay + timer per record
  int b50 = bustFrames(1.f / 50.f), b60 = bustFrames(1.f / 60.f);
  for (int r = 0; r < nrec; r++) {
    const ArrestRec *R = &ARREST_RECS[r]; const ArrestSample *S = R->s;
    int B = -1, E = -1;
    for (int i = 0; i < R->n; i++) { if (B < 0 && S[i].bust > 0.f) B = S[i].rel; if (B >= 0 && E < 0 && S[i].rel > B && S[i].bust <= 0.f) E = S[i].rel; }
    float nearT = 0.f; int A = -1, seeded = 0;
    for (int i = 0; i + 1 < R->n && A < 0; i++) {
      const ArrestSample *p = &S[i], *q = &S[i + 1]; int gap = q->rel - p->rel;
      if (B > 0 && p->rel > B + 4) break;   // PS2 арестовала больше 4 кадров назад: порт опоздал
      if (gap > 2) {               // дыра в записи: состояние на её конце — из записи
        nearT = q->near > 0.f ? q->near / 1000.f : 0.f; seeded = 1; continue;
      }
      for (int k = 1; k <= gap && A < 0; k++) {
        float u = (float)k / gap, d = p->d + (q->d - p->d) * u, v = (p->kmh + (q->kmh - p->kmh) * u) / 3.6f;
        // полиция в игре на начале отрезка; после ареста PS2 (флаги сняты) машина полиции стоит там же — порт ещё может догнать
        if (gameHnrCatch(&nearT, .02f, p->police || p->bust > 0.f, d, v, 0)) A = p->rel + k;
      }
    }
    char n[320];
    snprintf(n, sizeof n, "%s%s: арест PS2 кадр %%.0f, порта %%.0f (разница %%.0f, допуск 4)", R->name, seeded ? " (после дыры записи — счётчик из записи)" : "");
    check("replay", B > 0 && A > 0 && abs(A - B) <= 4, n, B, A, A - B);
    snprintf(n, sizeof n, "%s: таймер ареста PS2 %%.0f rel по отсчётам через кадр (±1), порт %%.0f при 50 Гц", R->name);
    check("timer", E > B && b50 == E - B, n, E - B, b50, 0);
  }
  check("timer", b60 > 0 && fabs(b60 / 60. - 74 / 50.) < 1. / 60., "60 Гц: %.4f с, правило порта 74 кадра PAL = %.4f с, допуск кадр PSP", b60 / 60., 74 / 50., 0);
  // rule: пороги на отсчётах записей (60 Гц PSP)
  int h = holdFrames(1.f / 60.f, 9.9f, 0.f);
  check("rule", h > 0 && h / 60.f >= HNR_BUST_CAR - 1e-4f && h / 60.f <= HNR_BUST_CAR + 1.f / 60.f, "9,9 м стоя: арест через %.3f с (PS2 750 мс счётчика)", h / 60.f, 0, 0);
  check("rule", holdFrames(1.f / 60.f, 10.07f, 0.f) < 0, "10,07 м (выше сбросов PS2 9,934–10,063 м: fine30 rel 274, coinauth rel 270): ареста нет, кадров %.0f", holdFrames(1.f / 60.f, 10.07f, 0.f), 0, 0);
  check("rule", holdFrames(1.f / 60.f, 5.f, 31.6f) < 0, "31,6 км/ч на 5 м (порог 30 км/ч факта ps2-hnr-police; сброса по скорости в записях нет): ареста нет, кадров %.0f", holdFrames(1.f / 60.f, 5.f, 31.6f), 0, 0);
  check("rule", holdFrames(1.f / 60.f, 4.82f, 25.2f) > 0, "25,2 км/ч на 4,82 м (счётчик PS2 растёт, b3_bust rel 1030): арест, кадров %.0f", holdFrames(1.f / 60.f, 4.82f, 25.2f), 0, 0);
  printf("RESULT %s %d/%d\n", fails ? "FAIL" : "PASS", checks - fails, checks);
  return fails ? 1 : 0;
}
