// Ворота подгрузки города, как у оригинала (правка Роба 181, слово Роба 02.10: «нам нужно это сделать — ближе к оригиналу»).
// [диск] Оригинал держит в памяти 3–4 файла-региона уровня из 13 и меняет их воротами — объёмами-триггерами ZoneEvent (локаторы
// вида 5) поперёк дорог: въехал в объём — регионы из «выгрузить» уходят, из «загрузить» приходят (S = (S & ~выгр) | загр).
// Телепорт (AddTeleportDest) ставит набор места целиком. [PS2] Сверено ездой бота в своём PCSX2: 22 из 22 чтений файлов зон —
// при въезде в их ворота (tools/twins/loadmap.py, knowledge/facts/ps2-streaming-gates.yaml). Данные — блок GAT1 секции REG1
// (tools/regions_export.py, версия 2). Чистый C: один и тот же код в EBOOT (viewer/main.c) и в сверке близнеца на Маке
// (tests/zonegate/host.c: путь бота оригинала через эти ворота -> те же файлы у тех же ворот).
#ifndef ZONEGATE_H
#define ZONEGATE_H
#include <math.h>
#include <string.h>

#define ZG_MAX 64
#define ZG_PLACES 32
typedef struct { float c[3], half[3], ax[9]; unsigned short load, unload; unsigned pad; } ZoneGate;   // 68 Б, как в файле
typedef struct { float x, z; unsigned short mask, pad; } ZonePlace;                                   // 12 Б

// точка в объёме ворот (ax — строки: оси объёма в осях порта; r — запас, м: кузов касается объёма раньше центра машины)
static inline int zoneGateIn(const ZoneGate *g, float x, float y, float z, float r) {
  float d0 = x - g->c[0], d1 = y - g->c[1], d2 = z - g->c[2];
  for (int k = 0; k < 3; k++) {
    float p = d0 * g->ax[3 * k] + d1 * g->ax[3 * k + 1] + d2 * g->ax[3 * k + 2];
    if (fabsf(p) > g->half[k] + r) return 0;
  }
  return 1;
}
// 108 Б.10 (Fable): у машины «касание» ворот — кузовом, а не точкой с запасом 1,5 м (запас был заменой четырёх углов: у узких ворот срабатывал рано, у косых поздно). Объём считается занятым, если в нём центр
// кузова или любой из четырёх его углов в плане (fx, fz — единичная ось «вперёд», hw/hl — полуширина/полудлина). ZONE_BODY_OLD=1 / файл zonebodyold / flag zonebody 0 — прежнее (точка + 1,5 м)
static inline int zoneGateInBody(const ZoneGate *g, float x, float y, float z, float fx, float fz, float hw, float hl) {
  if (zoneGateIn(g, x, y, z, 0.f)) return 1;
  for (int sl = -1; sl <= 1; sl += 2) for (int sw = -1; sw <= 1; sw += 2) {
    float cx = x + fx * hl * (float)sl - fz * hw * (float)sw, cz = z + fz * hl * (float)sl + fx * hw * (float)sw;
#ifdef ZG_BODY_BREAK
    (void)cx; (void)cz; continue;   // красный контроль: углы не проверяются, только центр
#endif
    if (zoneGateIn(g, cx, y, cz, 0.f)) return 1; }
  return 0;
}
static inline unsigned zoneGatesStepBody(const ZoneGate *g, int n, unsigned char *was, unsigned res, float x, float y, float z, float fx, float fz, float hw, float hl, int *fired) {
  for (int i = 0; i < n; i++) {
    int in = zoneGateInBody(&g[i], x, y, z, fx, fz, hw, hl);
    if (in && !was[i]) { res = (res & ~(unsigned)g[i].unload) | g[i].load; if (fired) *fired = i; }
    was[i] = (unsigned char)in;
  }
  return res;
}
// кадр: въезд в объём (фронт) меняет набор; was[] — был ли игрок в объёме кадром раньше. -> набор; *fired — последние сработавшие
static inline unsigned zoneGatesStep(const ZoneGate *g, int n, unsigned char *was, unsigned res, float x, float y, float z, float r, int *fired) {
  for (int i = 0; i < n; i++) {
    int in = zoneGateIn(&g[i], x, y, z, r);
    if (in && !was[i]) { res = (res & ~(unsigned)g[i].unload) | g[i].load; if (fired) *fired = i; }
    was[i] = (unsigned char)in;
  }
  return res;
}
// набор места: ближайшая точка AddTeleportDest (оригинал ставит её набор при телепорте); нет мест — все регионы
static inline unsigned zonePlaceNear(const ZonePlace *p, int n, float x, float z, unsigned all) {
  int best = -1; float bd = 1e30f;
  for (int i = 0; i < n; i++) { float dx = p[i].x - x, dz = p[i].z - z, d = dx * dx + dz * dz; if (d < bd) { bd = d; best = i; } }
  return best >= 0 ? p[best].mask : all;
}
#endif
