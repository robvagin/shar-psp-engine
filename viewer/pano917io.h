// №917: кольцо-панорама зоны 250 м — загрузка (рисует pano917.h, чистая часть pano917core.h). Файл pano917.bin уровня ('PNO1', строит tools/pano917/, окно «Контур»):
// заголовок 32 Б ('PNO1', n, SW=128, H 64/128, R, шаг), n записей по 32 Б (центр клетки, высота глаза съёмки, углы места, смещение и размер blob), blob = CLUT 16×RGBA8888 (индекс 0 прозрачен)
// и атлас T4 512×(4·H) в swizzle: 16 сегментов по 128 текселей (поле в 1 тексель). В памяти до PNO_SLOTS панорам (64 КБ при H=64, 128 КБ при H=128).
// Читает загрузчик клеток (pano917Serve из loaderThread, как мелочь разметки №912): главный поток только просит слот (pnoReq) и забирает готовый. PANO917_OLD=1 / «flag pano917 0» / файл pano917old — прежнее:
// зоны 250 м рисуют город до 250 м, кольца нет
#pragma once
#include "pano917core.h"
#ifndef PANO917_OLD
#define PANO917_OLD 0
#endif
static int pano917On = !PANO917_OLD;   // 0 — прежнее, 1 — кольцо, 2 — служебное: город 150 м без кольца (flag pano917 2, красный контроль гейта)
typedef struct { volatile int id; unsigned char *buf; volatile int st; } PnoSlot;   // st: 0 пусто, 1 грузится (слот не трогать), 2 готово
static PnoE *pnoE; static unsigned pnoN, pnoH, pnoSz, pnoPsm, pnoFlags;   // pnoFlags (заголовок, смещение 28): бит 0 — в картинках есть индекс пробивки (T4 15 / T8 255, альфа в CLUT 1)
static float pnoR; static char pnoPath[256]; static PnoSlot pnoS[PNO_SLOTS];
static volatile int pnoReq = -1;   // слот, который надо прочитать (пишет главный поток, сбрасывает загрузчик)
static unsigned pnoLoads, pnoLoadUs, pnoLoadMax, pnoFails;
static int pano917Active(void) { return pano917On && pnoN > 0; }   // у уровня есть панорамы и они включены: город держит 150 м
static void pano917Free(void) {
  for (int w = 0; w < 200 && pnoReq >= 0; w++) sceKernelDelayThread(1000);   // загрузчик дочитывает слот (не дольше 0,2 с)
  for (int i = 0; i < PNO_SLOTS; i++) { free(pnoS[i].buf); pnoS[i].buf = 0; pnoS[i].id = -1; pnoS[i].st = 0; }
  free(pnoE); pnoE = 0; pnoN = 0; pnoReq = -1;
}
static void pano917Load(void) {
  pano917Free();
  char fn[256]; levelFile(fn, "pano917.bin"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  int fsz = (int)sceIoLseek(f, 0, PSP_SEEK_END); sceIoLseek(f, 0, PSP_SEEK_SET);
  unsigned char hd[32]; unsigned n, sw, h, psm; float r;
  if (sceIoRead(f, hd, 32) != 32 || memcmp(hd, "PNO1", 4)) { sceIoClose(f); return; }
  memcpy(&n, hd + 4, 4); memcpy(&sw, hd + 8, 4); memcpy(&h, hd + 12, 4); memcpy(&r, hd + 16, 4); memcpy(&psm, hd + 24, 4); if (!psm) psm = 4; unsigned fl; memcpy(&fl, hd + 28, 4);   // смещение 24: 4 или 8 бит на тексель (в первых файлах 0 = 4)
  if (!n || n > 256 || sw != 128 || (h != 64 && h != 128) || (psm != 4 && psm != 8) || !(r >= 50.f && r <= 400.f)) { sceIoClose(f); return; }
  unsigned sz = psm == 8 ? 1024 + 512 * 4 * h : 64 + 512 * 4 * h / 2; PnoE *e = (PnoE *)malloc(n * sizeof(PnoE));
  if (!e || sceIoRead(f, e, (SceSize)(n * sizeof(PnoE))) != (int)(n * sizeof(PnoE))) { sceIoClose(f); free(e); return; }
  sceIoClose(f);
  for (unsigned i = 0; i < n; i++) if (e[i].size != sz || (e[i].off & 63u) || (unsigned long long)e[i].off + sz > (unsigned)fsz || !(e[i].hi > e[i].lo)) { free(e); return; }   // кривая запись — файла как будто нет
  snprintf(pnoPath, sizeof pnoPath, "%s", fn); pnoE = e; pnoN = n; pnoH = h; pnoSz = sz; pnoR = r; pnoPsm = psm; pnoFlags = fl & 1u;
  for (int i = 0; i < PNO_SLOTS; i++) { pnoS[i].id = -1; pnoS[i].st = 0; }
  if (smkOn) { char s[128]; snprintf(s, sizeof s, "PANO917 loaded level=%d n=%u h=%u psm=%u r=%.0f blob=%u punch=%u\n", gameLevel, n, h, psm, r, sz, fl & 1u); smkWrite(s); }
}
// загрузчик: один слот за раз, целиком (64–128 КБ, ~20–40 мс карты); ошибка чтения — слот пуст, главный попросит заново не раньше, чем через секунду (pnoFails)
static void pano917Serve(void) {
  int s = pnoReq; if (s < 0 || s >= PNO_SLOTS) { pnoReq = -1; return; }
  PnoSlot *p = &pnoS[s]; unsigned t0 = sceKernelGetSystemTimeLow(); int ok = 0;
  SceUID f = sceIoOpen(pnoPath, PSP_O_RDONLY, 0);
  if (f >= 0) { if (p->id >= 0 && p->buf && sceIoLseek32(f, pnoE[p->id].off, PSP_SEEK_SET) == (int)pnoE[p->id].off && sceIoRead(f, p->buf, (SceSize)pnoSz) == (int)pnoSz) ok = 1; sceIoClose(f); }
  if (ok) sceKernelDcacheWritebackInvalidateRange(p->buf, pnoSz);
  unsigned us = sceKernelGetSystemTimeLow() - t0; pnoLoadUs += us; if (us > pnoLoadMax) pnoLoadMax = us;
  __asm__ volatile("" ::: "memory");
  if (ok) { p->st = 2; pnoLoads++; } else { p->st = 0; p->id = -1; pnoFails++; }
  __asm__ volatile("" ::: "memory"); pnoReq = -1;
}
