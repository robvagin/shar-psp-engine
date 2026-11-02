// Круг 7 (сверка g10-flow): правила потока игры, вынесенные в чистый C для хост-теста (tests/flow). Без PSP.
#ifndef SV_FLOWRULES_H
#define SV_FLOWRULES_H
#ifndef CREDITS_SPEED_OLD
#define CREDITS_SPEED_OLD 0
#endif
// Титры: скорость прокрутки относительно обычной. ↓ добавляет к обычному ходу ещё четыре (итого ×5), ↑ вычитает шесть (итого −5); оба сразу: 1 + 4 − 6 = −1
// (в оригинале ↑ и ↓ проверяются по очереди, оба срабатывают). Было: ↓ побеждала. CREDITS_BOTH_OLD=1 — так
#ifndef CREDITS_BOTH_OLD
#define CREDITS_BOTH_OLD 0
#endif
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
