// Тур Роба на 102, пункт Р3 («наезды где-то проходят сквозь стены»): глаз наезда с диска не заходит за стену. Если между головой игрока и глазом камеры
// стоит стена (луч коллизии), глаз сдвигается к игроку вдоль этого луча и останавливается за MCWALL_MARGIN до стены: путь тот же, только короче в тех местах, где он шёл сквозь здание.
// st — удержанная доля пути «игрок -> глаз» между кадрами: к стене прижимается сразу, обратно отпускается плавно (без рывка). MSNCAMWALL_OLD=1 / файл msncamwallold / flag msncamwall 0 — как было (без сдвига).
// Чистый C99 + col.h: тот же код проверяет tests/camera/msncam_wall_host.c на Маке.
#ifndef MSNCAMWALL_H
#define MSNCAMWALL_H
#include <math.h>
#include "col.h"
#ifndef MSNCAMWALL_OLD
#define MSNCAMWALL_OLD 0
#endif
#define MCWALL_MARGIN 0.6f
static inline int mcWallPull(const ColWorld *w, const float head[3], float eye[3], float *st, float dt) {
  float d[3] = {eye[0] - head[0], eye[1] - head[1], eye[2] - head[2]}, len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
  if (len < 1.f) { *st = 1.f; return 0; }
  float fr = colRayFrac(w, head, eye, 0), want = fr >= .999f ? 1.f : fmaxf(0.f, fr - MCWALL_MARGIN / len);
  if (*st < 0.f || want < *st) *st = want; else { *st += (want - *st) * fminf(1.f, dt * 4.f); if (*st > want) *st = want; }
  eye[0] = head[0] + d[0] * *st; eye[1] = head[1] + d[1] * *st; eye[2] = head[2] + d[2] * *st;
  return *st < .999f;
}
#endif
