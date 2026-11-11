// Круг 7 (сверка g10-flow): правила потока игры, вынесенные в чистый C для хост-теста (tests/flow). Без PSP.
#ifndef SV_FLOWRULES_H
#define SV_FLOWRULES_H
#include <stdio.h>
#include <string.h>
#ifndef CREDITS_SPEED_OLD
#define CREDITS_SPEED_OLD 0
#endif
// Титры: скорость прокрутки относительно обычной. ↓ добавляет к обычному ходу ещё четыре (итого ×5), ↑ вычитает шесть (итого −5); оба сразу: 1 + 4 − 6 = −1
// (в оригинале ↑ и ↓ проверяются по очереди, оба срабатывают). Было: ↓ побеждала. CREDITS_BOTH_OLD=1 — так
#ifndef CREDITS_BOTH_OLD
#define CREDITS_BOTH_OLD 0
#endif
// Скорость титров, px PSP/с: оригинал 20 px Scrooby/с (запись PS2, Options → View Credits: 21,5 px/с на 512 строках = 20,2 на 480). Титры после финала (CreditsPostFMV) в оригинале
// наследуют тот же экран, значит едут с той же скоростью; у порта они шли 35 (×1,75 быстрее). ENDCRED_SPEED_OLD=1 — так (только для титров конца игры)
#ifndef ENDCRED_SPEED_OLD
#define ENDCRED_SPEED_OLD 0
#endif
static inline float flowCreditsSpeed(int ending) {
  return ((CREDITS_SPEED_OLD || (ending && ENDCRED_SPEED_OLD)) ? 35.f : 20.f) * 272.f / 480.f;
}
// Подпись △ «Talk To <Имя>» (правило порта, правка 206): имя берётся по кадру Механика 09.11 не из кода данных («NRIVIERA», «BURNS», «FRINK», «GRANDPA», у brn_unf вообще «UNF»), а как в
// тексте задания этого же собеседника (строки уровней: «Talk To Dr. Nick», «Mr. Burns», «Professor Frink», «Grampa», «Comic Book Guy», «Chief Wiggum»). ACTIONNAME_OLD=1 — прежнее имя из кода данных
#ifndef ACTIONNAME_OLD
#define ACTIONNAME_OLD 0
#endif
static inline const char *flowTalkName(const char *id, char *buf, int n) {
  static const char *const T[][2] = { {"burns", "Mr. Burns"}, {"brn_unf", "Mr. Burns"}, {"nriviera", "Dr. Nick"}, {"cbg", "Comic Book Guy"}, {"wiggum", "Chief Wiggum"},
    {"grandpa", "Grampa"}, {"frink", "Professor Frink"}, {"hibbert", "Dr. Hibbert"}, {"skinner", "Principal Skinner"}, {"captain", "Sea Captain"}, {"moleman", "Hans Moleman"}, {"teen", "Teen"} };
  const char *nm = id;
  if (!ACTIONNAME_OLD) { for (unsigned i = 0; i < sizeof T / sizeof *T; i++) { if (!strcmp(id, T[i][0])) return T[i][1]; } }
  { const char *u = strrchr(id, '_'); nm = u && u[1] ? u + 1 : id; }   // без префикса миссии m3_ и т.п.
  if (!ACTIONNAME_OLD) { for (unsigned i = 0; i < sizeof T / sizeof *T; i++) { if (!strcmp(nm, T[i][0])) return T[i][1]; } }
  snprintf(buf, (size_t)n, "%s", nm); if (buf[0] >= 'a' && buf[0] <= 'z') buf[0] = (char)(buf[0] - 32);
  return buf;
}
// Выход из титров (запись PS2: Options → View Credits и титры конца игры): закрывает только ✕, △ ○ Start Select не закрывают, подсказки Back на экране нет (Роб 09.11).
// CREDITSX_OLD=1 — выход △ / ○ и подсказка Back, как было. Кнопки PSP: △ 0x1000, ○ 0x2000, ✕ 0x4000 (ui.c сверяет с pspctrl.h)
#ifndef CREDITSX_OLD
#define CREDITSX_OLD 0
#endif
#define FLOW_PAD_TRIANGLE 0x1000u
#define FLOW_PAD_CIRCLE 0x2000u
#define FLOW_PAD_CROSS 0x4000u
static inline int flowCreditsExit(unsigned hit) {
  return CREDITSX_OLD ? (hit & (FLOW_PAD_TRIANGLE | FLOW_PAD_CIRCLE)) != 0 : (hit & FLOW_PAD_CROSS) != 0;
}
static inline float flowCreditsRate(int up, int dn) {
  if (CREDITS_SPEED_OLD) return 1.f;
  if (!CREDITS_BOTH_OLD && up && dn) return -1.f;
  return dn ? 5.f : up ? -5.f : 1.f;
}
// Титры, реплики Кэнга и Кодос (№1104): очередной номер строки. last — сколько строк уже отсчитано; каждый кадр, если прокрутка (px PSP) минус 80 px (по Scrooby, ×272/480)
// не меньше last × шаг строки (px PSP), счётчик растёт на 1 и это номер строки; иначе 0. Одна строка за кадр, как в оригинале
#ifndef CREDVO_OLD
#define CREDVO_OLD 0
#endif
static inline int flowCreditsLine(float scrollPsp, int lhPsp, int *last) {
  if (CREDVO_OLD || lhPsp <= 0) return 0;
  if (scrollPsp - 80.f * 272.f / 480.f >= (float)*last * (float)lhPsp) return ++*last;
  return 0;
}
// №1112 (против оригинала): надпись LOADING на закрытом экране ириса. Появляется, если чёрный висит дольше 3 с; дальше мигает с периодом 0,25 с, и первый переключатель
// (надпись скрыта по умолчанию) — через 0,25 с после порога: видна на 3,25–3,50 с, 3,75–4,00 с и так далее. Стоит по центру внизу экрана (шрифт 16, верх строки на 397 из 480 по Scrooby, 225 px PSP).
// Было: надпись горела на 3,00–3,25, 3,50–3,75 (фаза наоборот) и стояла посередине экрана (128 px). IRISLOAD1112_OLD=1 — так
#ifndef IRISLOAD1112_OLD
#define IRISLOAD1112_OLD 0
#endif
static inline int flowIrisLoadingOn(float blackT) {
  if (blackT <= 3.f) return 0;
  int odd = ((int)((blackT - 3.f) / .25f)) & 1;
  return IRISLOAD1112_OLD ? !odd : odd;
}
#define FLOW_IRIS_LOADING_Y (IRISLOAD1112_OLD ? 128 : 225)
#endif
