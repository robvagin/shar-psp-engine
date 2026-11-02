// Крен камеры пешком на уровне 7 (сверка g06-camera, СВЕРКА_КАМЕРА P08:20; правка Роба №1084). Чистая функция, без игры вокруг: проверяется на Маке (tests/camera/sverka_g06_host.c).
// Правило (сверено дирижёром с оригиналом): только уровень 7, только камера пешком, только пока рядом (ближе 5 м) персонаж миссии — close 0..1 (доля близости), и только пока настоящая
// горизонтальная скорость Гомера (модуль скорости, не стик) меньше трети бега. Иначе цель 0, и крен гаснет. Размах 0,176 рад (10°) × close.
// №1084 (Роб, TEST82: «внутри комнат начался крен камеры, когда бегаю вправо-влево»): крен поднимался сразу на всю цель — при каждом торможении у стены/развороте бега скорость падала
// ниже трети, и кадр дёргало на 10° рывком; теперь подъём плавный (постоянная 0,6 с), спад как был (0,01 цели за 1/60 с). Где крен применяется (не поверх камеры комнаты/зоны/разговора) —
// main_loop.h при построении вида. ROLL1084_OLD=1 — как в TEST82
#ifndef CAMROLL_H
#define CAMROLL_H
#include <math.h>
#ifndef ROLL1084_OLD
#define ROLL1084_OLD 0
#endif
#define CAMROLL_MAX .176f
#define CAMROLL_RUN 8.f   // максимальная скорость бега, м/с (HOMER_RUN)
static inline float camRollStepSv(float cur, int level, float close, float spd, float dt) {
  float want = level == 7 && close > 0.f && spd < CAMROLL_RUN / 3.f ? CAMROLL_MAX * close : 0.f;
  if (ROLL1084_OLD) return cur > want ? fmaxf(want, cur - .01f * dt * 60.f * CAMROLL_MAX) : want;
  if (cur > want) return fmaxf(want, cur - .01f * dt * 60.f * CAMROLL_MAX);
  return cur + (want - cur) * (1.f - expf(-dt / .6f));
}
#endif
