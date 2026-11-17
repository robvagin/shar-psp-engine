// Маркеры цели и значки заданий из msnart.shr (см. msnart.h, формат — tools/msnart_export.py).
#include "dcache96.h"
#include "gevb.h"
#ifndef GEVB_MSNART_OLD
#define GEVB_MSNART_OLD 0   // -DGEVB_MSNART_OLD=1 — значки/кольца миссий в список GE, как раньше
#endif
#include "camnear.h"
#include <pspkernel.h>
#include <pspgu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include "zone3_api.h"   // П5 Зоны-95: картинки миссий — область «миссия»
#include "msnart.h"
#include "msnres.h"
#include "power.h"   // 107б P0: pwrEpoch(), pwrSimFault(), pwrKillHook()
#include "lvcheck.h"
#include "glintshape941.h"   // №944: общий шейп блика (GLINT944_OLD)
#include "ui.h"
#include "blackbox.h"   // правка 103: bbOp — самописец чёрного ящика (main.c)
#include "uidiag.h"   // №1186: проверка вызовов рисования интерфейса (после include pspgu)

#ifndef MSNART_BBDONE_OLD
#define MSNART_BBDONE_OLD 0   // =1 — как раньше: в чёрном ящике только начало чтения записи
#endif
typedef struct { char path[80]; unsigned off, size, pad[2]; } MsaRow;                               // 96 Б
typedef struct { unsigned ntex, ndraw, nimg, offTex, offDraw, offImg, pad[2]; } MsaHdr;             // 32 Б
typedef struct { unsigned short w, h; unsigned off, psm, pad; } MsaTex;                             // 16 Б
typedef struct { char name[32]; unsigned tex, w, h, pad; } MsaImg;                                  // 48 Б
// MSA2: объект 64 Б — + суставы и кадры позы (PTRN, цикл из данных), квад и меш — в осях своего сустава
typedef struct { char name[32]; unsigned nbill, offBill, nmesh, offMesh, njoint, nfr; float fps; unsigned offJoint; } MsaDraw;   // 64 Б
typedef struct { unsigned tex, blend, mode, zwrite; float c[3], ax[3], ay[3], w, h; unsigned argb, nfr, offTrack; float fps, dist; } MsaBill;   // 80 Б; mode: 0-7 NOAX/AAX, 8-15 сустав
typedef struct { unsigned tex, flags, nvert, offVert; } MsaMesh;   // 16 Б; flags: BLMD 0-7, режим 8-15 (0 непрозрачный 1 вырезка 2 полупрозр. 3 сложение 4 вычитание), сустав 16-31
_Static_assert(sizeof(MsaDraw) == 64 && sizeof(MsaBill) == 80 && sizeof(MsaMesh) == 16, "MSA2 (tools/msnart_export.py)");
typedef struct { float u, v; unsigned c; float x, y, z; } MsaVert;                                  // 24 Б
int (*msnartVecBeam)(const float p[3], const float X[3], const float Y[3], unsigned abgr);
int (*msnartBeam995)(const float ufo[3], float ex, float ey, float ez);   // №995 (lightsprite.h beam995Draw)   // №805 (lightsprite.h vecBeamHook)
int (*msnartGlowGlint)(const char *, const float *, float, unsigned, unsigned, float, float, float, float);   // №839 (spritelight.h glowGlintHook)
// №1025 (Роб 26.10: «зелёные стрелки на полу: убрать блики, пусть будут обычные зелёные стрелки на дороге и следуют подъёму и спаду дороги»): набор стрелок пути (darrow/warrow: три квада-картинки через 4 м вдоль курса
// и три квада-свечения со сложением тех же суставов) — свечения не рисуются совсем (ни картинкой, ни векторным бликом), каждая стрелка кладётся на коллизию своей точки: высота и нормаль под центром квада (как тень
// машины №973), оси квада проецируются на плоскость дороги (длина та же). Прежнее — набор на самую высокую точку коллизии под ним, ровно по горизонту (msn_view.c drawRouteArrow). ARROWS1025_OLD=1 / «flag arrows1025 0» /
// файл arrows1025old рядом с EBOOT — как было
#ifndef ARROWS1025_OLD
#define ARROWS1025_OLD 0
#endif
int msnartArrow1025 = !ARROWS1025_OLD;
// №1052 (Роб 27.10: «в будке внизу осталось размытое мигание, у нас только векторное свечение»; дирижёр: найти по данным и не рисовать): у значка телефона (phone_icon) есть
// пятно <рисунок>_shadow с blend 2 (сложение, текстура-размытое пятно): по правилу №793 «_shadow» из блика исключён, и оно рисовалось картинкой — сиреневое пятно на тротуаре под будкой
// миссии (на L1 0,3 с мигания). Правило по рисунку и режиму, не по координатам: квад «_shadow» со сложением не рисуется; вычитающие тени (blend 3) остаются. BOOTHDECAL1052_OLD=1 / файл boothdecal1052old — как было
#ifndef BOOTHDECAL1052_OLD
#define BOOTHDECAL1052_OLD 0
#endif
int msnartDecal1052 = !BOOTHDECAL1052_OLD;
int (*msnartGroundN)(float x, float z, float yHi, float yLo, float *y, float *n3);   // высота и нормаль коллизии под точкой (main_render.h mvGroundNHook); NULL — стрелки как раньше
static int msaIsRoadArrow(const char *n) { return !strcasecmp(n, "darrow") || !strcasecmp(n, "warrow"); }
static int msaIsShadow(const char *n) { size_t l = strlen(n); return l > 7 && !strcasecmp(n + l - 7, "_shadow"); }
#ifndef GLOW_GLINT_OLD
#define GLOW_GLINT_OLD 0
#endif
static int msaIsBeam(const char *n) { for (; *n; n++) if ((n[0] | 32) == 'b' && (n[1] | 32) == 'e' && (n[2] | 32) == 'a' && (n[3] | 32) == 'm') return 1; return 0; }

static char file[320]; static MsaRow *rows; static unsigned nrows;
// 109 (Fable-2 по LOADLIST109): карточка теперь заказывает ВСЕ записи миссии сразу, а мест было 24; по спискам msnres-l1..l7 худшая миссия (L1 m2) занимает 17 своих + 6 уровневых = 23, запас 1, а при переполнении
// молча вытеснялась самая ранняя запись миссии (значок первого этапа). Мест теперь 32 (хранится только указатель; память занимают сами записи, их сумма та же, что и раньше: msnResPeak). MSNARTCAP109_OLD=1 / flag msnartcap109old 1 — прежние 24.
// Служебно flag msnartcap109 N — предел N мест (стенд переполнения). Вытеснение пишет BBOP art- (самописец и журнал смоука) и WATCHDOG msnart evict
#ifndef MSNARTCAP109_OLD
#define MSNARTCAP109_OLD 0
#endif
int msnartCap109Old = MSNARTCAP109_OLD, msnartCapSet109;
#define MAXE_ARR 32
static inline int entLim(void) { return msnartCap109Old ? 24 : (msnartCapSet109 > 0 && msnartCapSet109 < MAXE_ARR ? msnartCapSet109 : MAXE_ARR); }
#define MAXE entLim()
// №512 (11.10, железо): картинка брифинга миссии (art/frontend/dynaload/images/mis*) терялась молча в двух местах: очередь чтений была в MAXE мест и
// отбрасывала новое при заполнении, а дочитанная запись без свободной ячейки ent[] освобождалась. Теперь картинка встаёт в очередь первой, очередь
// длиннее, при нехватке ячеек вытесняется запись миссии, не картинка, сбой чтения повторяется один раз. MSNART_PIC_OLD=1 — как раньше
#ifndef MSNART_PIC_OLD
#define MSNART_PIC_OLD 0
#endif
#define MAXQ 64
static struct { int row, level; unsigned char *b; unsigned char shared, used; } ent[MAXE_ARR];   // 1363: shared — общая запись миссий уровня (переживает конец миссии, уходит при тесноте), used — просили в этой миссии
static unsigned char retried[256];
// Кольцо-96 (Fable, п.14, находка longgame L4 с fragblk 0.6): картинка брифинга при отказе памяти получала ОДИН мгновенный повтор и тихо пропадала (msnartLostPics++), а карточка 8 с (BRIEF_PIC_MAX)
// стояла на чёрном LOADING. Теперь картинка ждёт память по ступеням лестницы (msnartRoom на каждой попытке), повтор не чаще раза в секунду, до MSNART_PIC_TRIES попыток; отказ пишет
// WATCHDOG msnart pic-no-mem / pic-give-up и сразу даёт карточке показаться без картинки (msnartPicGaveUp). MSNART_PICWAIT_OLD=1 / файл msnartpicwaitold — как было
#ifndef MSNART_PICWAIT_OLD
#define MSNART_PICWAIT_OLD 0
#endif
#ifndef MSNART_PIC_TRIES
#define MSNART_PIC_TRIES 3
#endif
static unsigned char picTries[256], picGaveUp[256];
static int picMaxTries(void) { return BRIEFPIC1269_OLD ? MSNART_PIC_TRIES : MSNART_PICHARD1269; }   // №1269
static void picFinal(int r) { if (!BRIEFPIC1269_OLD && r >= 0 && r < (int)sizeof picGaveUp) picGaveUp[r] = 1; }   // №1269: картинка потеряна окончательно (чтение не удалось) — карточка не ждёт предел
int msnartPicWaitOld = MSNART_PICWAIT_OLD; int msnartFailMissgen;
void (*msnartReq)(const char *path);   // 109 З9: полный путь записи, которую начали читать (строка MSNREQ журнала смоука; на поведение не влияет)
int msnartPicGaveUp(void) { for (int r = 0; r < nrows && r < (int)sizeof picGaveUp; r++) if (picGaveUp[r]) return 1; return 0; }
unsigned msnartLostPics;   // сколько картинок брифинга так и не доехало (служебный счётчик для лога)
static unsigned bytesUsed;
// чтение записей — асинхронное (sceIoReadAsync): старт миссии не останавливает кадр чтением с карты памяти;
// объект/значок появляются, когда запись дочитана (несколько кадров), значок задания применяется тогда же
static SceUID afd = -1; static int qrow[MAXQ], qlev[MAXQ], nq, busyRow = -1, busyLev; static unsigned char *busyBuf;
static int isPic(int r) { return strstr(rows[r].path, "/images/mis") != 0; }
static char wantIcon[32];
float msnartAvDelayS;   // №1247 (служебно): задержка значка задания, секунды
// №1363 (Роб; аудит 1359 п.5: cards/wrench/missgen ×35, значки msnic ×222 за тур, ≈ 0,3–0,6 с на старт миссии): общие записи миссий уровня (список ресурсов msnres: уровневая группа «L» или записи
// двух и более миссий) читались заново на каждом старте — на конце миссии снимались все записи без «уровня». Теперь общие остаются в области «миссия» до смены уровня, но НЕ «уровня»: при тесноте
// уходят первыми (сначала те, что эта миссия ещё не просила, потом по прежним правилам: отказ памяти / нет ячейки — как до 1363), поэтому область они не занимают сверх того, что миссия и так читала.
// Картинки карточек в кэш не входят (снимаются, как раньше). Нет файла списка (msnResShared = 0) — как было. MSNARTSHARE1363_OLD=1 / «flag msnartshare1363 0» / файл msnartshare1363old — как было
#ifndef MSNARTSHARE1363_OLD
#define MSNARTSHARE1363_OLD 0
#endif
static void drop(int i);
int msnartShare1363 = !MSNARTSHARE1363_OLD; unsigned msnartShareHits1363, msnartShareEvict1363;
static int shareRow1363(int row) { return msnartShare1363 && row >= 0 && !isPic(row) && msnResShared(rows[row].path); }
static int evictShared1363(void) {   // уйти первой общей записи, которую эта миссия ещё не просила; сколько снято
  int n = 0; for (int i = 0; i < MAXE; i++) if (ent[i].b && ent[i].shared && !ent[i].used && !ent[i].level) { bbOp("art-", rows[ent[i].row].path, 0); drop(i); n++; msnartShareEvict1363++; }
  return n;
}
void msnartShutdown(void);

static void msnartPwrSync107(void);   // 107в: эпоха питания на момент открытия файла уровня
int msnartInit(const char *dir) {
  if (rows || afd >= 0) msnartShutdown();   // повторное открытие (другой уровень) — прежняя таблица не течёт
  snprintf(file, sizeof file, "%smsnart.shr", dir);
  msnartPwrSync107();   // 107в
  SceUID f = sceIoOpen(file, PSP_O_RDONLY, 0); if (f < 0) return -1;
  unsigned h[4]; if (sceIoRead(f, h, 16) != 16 || memcmp(h, "MSA2", 4)) { sceIoClose(f); return -2; }
#ifndef MSAROWS1195_OLD
#define MSAROWS1195_OLD 0   // №1195: 1 — число строк и смещение таблицы из заголовка msnart.shr без проверки, как в TEST89
#endif
  nrows = h[1];
  if (!MSAROWS1195_OLD) { int fsz_ = sceIoLseek32(f, 0, PSP_SEEK_END); if (!lvMsaHdrOk(h, fsz_, sizeof(MsaRow))) { sceIoClose(f); nrows = 0; return -4; } }   // таблица строк целиком внутри файла: иначе счётчик из заголовка давал строки за концом выделенного (повреждённый пакет рисунков)
  rows = malloc(sizeof(MsaRow) * (nrows ? nrows : 1));
  sceIoLseek32(f, (int)h[2], PSP_SEEK_SET);
  if (!rows || sceIoRead(f, rows, (SceSize)(sizeof(MsaRow) * nrows)) != (int)(sizeof(MsaRow) * nrows)) { free(rows); rows = 0; nrows = 0; }
  if (rows) afd = f; else sceIoClose(f);   // файл остаётся открытым для асинхронных чтений
  return rows ? 0 : -3;
}
extern void geDeferFree(void *p);   // main_state.h: освободить после sceGuSync кадра (GE мог получить в список картинку из этого буфера)
// №849 (тур TEST39 на PSP: «маркеры» L3 2,2 мс, L6 1,5 мс кадра; эмулятор L6: трубка над будкой 0,17 мс, значки 0,28 мс): у рисунков со
// спрайтовым путём (трубка, «!») каждый кадр обходились ВСЕ вершины меша 0 ради центра и нижней точки — они постоянны в осях сустава.
// Считаются один раз на рисунок (кэш по адресу рисунка; сброс, когда пакет рисунков выгружается). MSA_BOUNDS849_OLD=1 — каждый кадр
#ifndef MSA_BOUNDS849_OLD
#define MSA_BOUNDS849_OLD 0
#endif
#ifndef MSA_DROP852_OLD
#define MSA_DROP852_OLD 0
#endif
typedef struct { const void *d; float c0[3], b0[3]; } MsaBnd;
static MsaBnd msaBnd[24]; static unsigned msaBndN;
typedef struct { const void *d; unsigned nfr; float *bb; unsigned char *ok; } MsaNpcBnd;   // №849: рамка позы значка по кадрам клипа (msnartDrawNpcMarker)
static MsaNpcBnd msaNB[8]; static unsigned msaNBn;
static void msaNBReset(void) { for (unsigned i = 0; i < msaNBn; i++) { free(msaNB[i].bb); free(msaNB[i].ok); } msaNBn = 0; }
// №852 (тур TEST41 на PSP, L3: значки 0,5–1 мс кадра): записи кэшей рамок — только того блока, что выгружается (раньше любой drop — подгрузка
// чужого рисунка на ходу — сбрасывал все кэши, и рамки считались заново). MSA_DROP852_OLD=1 — сброс всех
static void msaCacheDrop(const unsigned char *b, unsigned size) {
  if (MSA_DROP852_OLD || !b) { msaBndN = 0; msaNBReset(); return; }
  for (unsigned i = 0; i < msaBndN; ) { const unsigned char *d = (const unsigned char *)msaBnd[i].d; if (d >= b && d < b + size) msaBnd[i] = msaBnd[--msaBndN]; else i++; }
  for (unsigned i = 0; i < msaNBn; ) { const unsigned char *d = (const unsigned char *)msaNB[i].d;
    if (d >= b && d < b + size) { free(msaNB[i].bb); free(msaNB[i].ok); msaNB[i] = msaNB[--msaNBn]; } else i++; }
}
static unsigned entGen108 = 1;   // 108 О17: растёт при любом изменении таблицы записей ent (кэш поиска рисунка по имени)
int msaFind108Red;   // красный контроль: выгрузка записи не сбрасывает кэш
static int mBndOwnerIn(const unsigned char *b, unsigned sz);   // 1298: рабочая память отсечения принадлежит этой записи
static void cullMemFree(void);
static void drop(int i) { if (!msaFind108Red) entGen108++; const unsigned char *b_ = ent[i].b; unsigned sz_ = b_ ? rows[ent[i].row].size : 0;
  if (ent[i].b) { geDeferFree(ent[i].b); bytesUsed -= rows[ent[i].row].size; } ent[i].b = 0; msaCacheDrop(b_, sz_);
  if (mBndOwnerIn(b_, sz_)) cullMemFree(); }
// №1156 (TEST84 на PSP: STALL под интерфейсом, последним действием стояло «art=» картинки брифинга): никакое чтение под интерфейсом не ждёт вечно. Чтение в полёте старше
// MSNART_READ_MS (3 с) бросается: буфер не освобождается (его всё ещё может писать ядро — теряем его намеренно), дескриптор не закрывается, файл открывается заново, картинка брифинга
// получает свой один повтор, дальше — «брифинг без картинки» (№512). Отказ памяти при чтении: сначала просим клетки (msnartRoom), потом сбрасываем кэш записей миссий и пробуем один раз.
// MSNARTSTALL1156_OLD=1 — как TEST84 (вечное ожидание в waitBusy)
#ifndef MSNARTSTALL1156_OLD
#define MSNARTSTALL1156_OLD 0
#endif
#define MSNART_READ_MS 3000u
static unsigned busyT0; unsigned msnartAbandoned;   // начало чтения в полёте, мкс; сколько чтений брошено (служебный счётчик)
static void quarPush107(unsigned char *b, unsigned sz, SceUID fd);   // 107б P0: карантин буфера и дескриптора брошенного чтения (ниже)
static void abandonBusy(void) {   // чтение не кончилось за MSNART_READ_MS: бросить буфер и файл, открыть заново
  bbOp("art!", busyRow >= 0 ? rows[busyRow].path : "-", 0); msnartAbandoned++;
  if (isPic(busyRow) && busyRow < (int)sizeof retried && !retried[busyRow] && nq < MAXQ) { retried[busyRow] = 1; memmove(qrow + 1, qrow, sizeof(int) * nq); memmove(qlev + 1, qlev, sizeof(int) * nq); qrow[0] = busyRow; qlev[0] = busyLev; nq++; }
  else if (isPic(busyRow)) { msnartLostPics++; picFinal(busyRow); }
  quarPush107(busyBuf, busyRow >= 0 ? rows[busyRow].size : 0, afd);   // 107б P0: буфер и старый дескриптор — в карантин (ядро могло ещё писать), а не в утечку насовсем
  busyBuf = 0; busyRow = -1;
  afd = sceIoOpen(file, PSP_O_RDONLY, 0);
}
static void waitBusy(void) {   // дождаться чтения в полёте (буфер нельзя освобождать, пока его пишет ядро)
  if (busyRow < 0) return;
  if (!MSNARTSTALL1156_OLD) {
    unsigned t0 = sceKernelGetSystemTimeLow(); SceInt64 r;
    while (sceIoPollAsync(afd, &r) == 1) { if ((unsigned)(sceKernelGetSystemTimeLow() - t0) > MSNART_READ_MS * 1000u) { abandonBusy(); return; } sceKernelDelayThread(1000); }
    free(busyBuf); busyBuf = 0; busyRow = -1; return;
  }
  SceInt64 r; sceIoWaitAsync(afd, &r); free(busyBuf); busyBuf = 0; busyRow = -1;
}
static void tries107Reset(void);   // 107б P0: tries94 (ниже) сбрасывается со сменой уровня, миссии и пробуждения
void msnartReset(void) { waitBusy(); tries107Reset(); nq = 0; memset(retried, 0, sizeof retried); memset(picTries, 0, sizeof picTries); memset(picGaveUp, 0, sizeof picGaveUp); for (int i = 0; i < MAXE; i++) drop(i); wantIcon[0] = 0; uiSetObjIconImage(0, 0, 0, 0, 0); }
void msnartShutdown(void) {   // смена уровня: записи, таблица и открытый файл msnart.shr — уровень N+1 откроет свой (l<N>/)
  msnartReset(); if (afd >= 0) sceIoClose(afd); afd = -1; free(rows); rows = 0; nrows = 0; bytesUsed = 0;
}
void msnartMissionStart(void) { cullMemFree(); tries107Reset(); memset(picTries, 0, sizeof picTries); memset(picGaveUp, 0, sizeof picGaveUp); for (int i = 0; i < MAXE; i++) if (ent[i].b && !ent[i].level) { if (ent[i].shared && msnartShare1363) { ent[i].used = 0; continue; } drop(i); } wantIcon[0] = 0; uiSetObjIconImage(0, 0, 0, 0, 0); }
// 109 (Роб «внедряем», Fable ПОРЯДОК_ЗАГРУЗКИ §4 п.1, 6; закон 1276б): (а) очередь msnart пуста — условие готовности карточки миссии (msn_view.c briefReady): читается всё, что заказали load-скрипт и картинка;
// (е) записи миссии (предметы, значки, картинки прошлых карточек) снимаются на её КОНЦЕ (cleanup без повтора), а не на старте следующей: у оригинала ресурсы миссии живут от карточки до конца миссии. Записи уровня остаются.
// Картинка следующей карточки (isPic), если уже заказана или лежит, не снимается. LOADEND109_OLD=1 / «flag loadend109old 1» / rollback.txt — как было (снимает старт следующей миссии)
#ifndef LOADEND109_OLD
#define LOADEND109_OLD 0
#endif
int msnartEnd109Old = LOADEND109_OLD;
int msnartIdle(void) { return busyRow < 0 && nq == 0; }
// 1337 (живой ход Роба, L2 m5: картинки миссии читались 4,2 с, персонаж 4,0 с — в очереди за догрузкой города): ресурсы миссии на старте этапа первые в очереди карты памяти (канон 17).
// msnartIoBusy1337 — запись читается или ждёт чтения; загрузчик клеток (main_stream.h msnIoHold1337) при этом не начинает новых клеток, не дольше MSNIOHOLD1337_US подряд. Служебное: ioSerial1337 / ioDevBusy1337 —
// стенд-имитация «одна карта памяти»: пока загрузчик читает клетку, фоновое чтение записи не стартует (эмулятор такого ожидания не знает).
volatile int ioSerial1337, ioDevBusy1337;
int msnartIoBusy1337(void) { return busyRow >= 0 || nq > 0; }
// 109: состояние записи по пути (тот же вид, что у msnartLoad): 1 — в памяти, 0 — в очереди или читается (повторы после отказов тоже здесь), -1 — в таблице нет или к ней не пришла (отказ, снята)
int msnartPathState109(const char *p3d) {
  if (!rows || !p3d) return -1;
  char key[80]; unsigned n = 0;
  for (; p3d[n] && n < sizeof key - 1; n++) { char c = p3d[n] == '\\' ? '/' : p3d[n]; key[n] = (c >= 'A' && c <= 'Z') ? c + 32 : c; }
  key[n] = 0;
  for (unsigned r = 0; r < nrows; r++) {
    if (strcmp(rows[r].path, key)) continue;
    for (int i = 0; i < MAXE; i++) if (ent[i].b && ent[i].row == (int)r) return 1;
    if (busyRow == (int)r) return 0;
    for (int i = 0; i < nq; i++) if (qrow[i] == (int)r) return 0;
    return -1;
  }
  return -1;
}
void msnartMissionEnd109(void) {
  if (msnartEnd109Old) return;
  int n = 0; for (int i = 0; i < MAXE; i++) if (ent[i].b && !ent[i].level && !isPic(ent[i].row)) { if (ent[i].shared && msnartShare1363) { ent[i].used = 0; continue; } drop(i); n++; }
  wantIcon[0] = 0; uiSetObjIconImage(0, 0, 0, 0, 0);   // значок указывал в буфер, который сняли
  { char a_[32]; snprintf(a_, sizeof a_, "mission n=%d", n); bbOp("art-end", a_, 0); }
}
const char *msnartWantIcon(void) { return wantIcon; }   // №958б: диагностика
unsigned msnartBytes(void) { return bytesUsed; }
void msnartLoad(const char *p3d, int level) {
  if (!rows) return;
  char key[80]; unsigned n = 0;
  for (; p3d[n] && n < sizeof key - 1; n++) { char c = p3d[n] == '\\' ? '/' : p3d[n]; key[n] = (c >= 'A' && c <= 'Z') ? c + 32 : c; }
  key[n] = 0;
  for (unsigned r = 0; r < nrows; r++) {
    if (strcmp(rows[r].path, key)) continue;
    for (int i = 0; i < MAXE; i++) if (ent[i].b && ent[i].row == (int)r) { if (level) ent[i].level = 1; if (ent[i].shared && !ent[i].used) { ent[i].used = 1; msnartShareHits1363++; } return; }   // уже есть (1363: общая — из кэша, чтения нет)
    if (busyRow == (int)r) { busyLev |= level; return; }
    for (int i = 0; i < nq; i++) if (qrow[i] == (int)r) { qlev[i] |= level; return; }
    if (!MSNART_PIC_OLD && isPic((int)r)) {   // картинка брифинга — в начало очереди, место есть всегда (вытесняется последняя запись)
      if (nq >= MAXQ) nq = MAXQ - 1;
      memmove(qrow + 1, qrow, sizeof(int) * nq); memmove(qlev + 1, qlev, sizeof(int) * nq); qrow[0] = (int)r; qlev[0] = level; nq++;
    } else if (nq < (MSNART_PIC_OLD ? MAXE : MAXQ)) { qrow[nq] = (int)r; qlev[nq] = level; nq++; }
    return;
  }
}
// №637/TEST23: отказ памяти у будки при 10 МБ свободно (дробление кучи: самый большой кусок 2,3 МБ, запись art 6 МБ). Перед отказом просим
// движок освободить далёкие клетки города, пока не появится цельный кусок (main_stream.h cellsMakeRoom). MSNART_ROOM_OLD=1 — прежнее.
#ifndef MSNART_ROOM_OLD
#define MSNART_ROOM_OLD 0
#endif
int (*msnartRoom)(unsigned);
// Build 94 (Фабль П3): запись предмета миссии (не картинка), которую не удалось прочитать (нет памяти, сбой чтения), раньше молча выпадала из очереди — предмет не появлялся.
// Теперь: строка WATCHDOG, запись встаёт в конец очереди и пробуется снова через 1 с (до MSNART_TRIES94 раз). MSNARTRETRY94_OLD=1 / flag msnartretry94 0 / файл msnartretry94old — как было
#ifndef MSNARTRETRY94_OLD
#define MSNARTRETRY94_OLD 0
#endif
#define MSNART_TRIES94 60
int msnartRetry94 = !MSNARTRETRY94_OLD;
int msnartFailSim94;   // служебное (flag msnartfail N): N первых запусков чтения «нет памяти»
void (*msnartWatch)(const char *note, const char *path, int tries);
// 108 (дирижёр, чёрный ящик PSP 13.11, сборка 106, L1 m5: 64 раза подряд «op art+ …/images/msnic» через ~1,03 с, ни одного «art=», mem 0): запись значка задания (msnicons, 64 КБ) не доходит до чтения — повтор раз
// в секунду по Build 94 (failRetry94: нет памяти или отказ запуска чтения) до сдачи через 60 попыток, значок не встаёт, бабл ждёт. Причина на железе по ящику не видна (в ящик не писалось, КАКОЙ отказ), поэтому:
// (1) отказ теперь пишется в ящик отдельной строкой: «art-mem» (нет памяти: нужно / куча), «art-io» (коды Lseek и sceIoReadAsync); (2) при отказе запуска чтения дескриптор msnart.shr закрывается и открывается заново
// (залипший асинхронный дескриптор лечится только так), а с третьей попытки запись читается СИНХРОННО (sceIoRead, 64 КБ ≈ 5 мс) в тот же буфер, мимо асинхронного пути. MSNARTIO108_OLD=1 / «flag msnartio108old 1» — как было.
// Служебно «flag msnartiostuck 1»: sceIoReadAsync отказывает, пока дескриптор не открыт заново (модель залипшего дескриптора; синхронное чтение не затронуто)
#ifndef MSNARTIO108_OLD
#define MSNARTIO108_OLD 0
#endif
int msnartIo108Old = MSNARTIO108_OLD, msnartIoStuck108; unsigned msnartIoReopen108, msnartIoSync108;
// 107б (генплан Fable 13.11, сценарий «мёртвый файл»): правка 108 лечила только значок задания (!isPic), картинка карточки сдавалась сразу (LOST), а неудачное переоткрытие оставляло afd = −1 и модуль молчал до смены
// уровня (msnartPump: afd < 0 -> return). Теперь картинки идут тем же путём (дескриптор заново, повтор раз в секунду по picTries до picMaxTries, с 3-й попытки синхронное чтение), а afd < 0 при непустой очереди
// пробуется открыть заново раз в секунду. MSNARTIO107B_OLD=1 / «flag msnartio107bold 1» — как после 108 (картинки сдаются сразу, afd −1 молчит). Служебно «flag msnartreopenfail N»: N первых переоткрытий отказывают
#ifndef MSNARTIO107B_OLD
#define MSNARTIO107B_OLD 0
#endif
int msnartIo107bOld = MSNARTIO107B_OLD, msnartReopenFail107; static unsigned reopenAt107;
static int ioReopen107(void) {   // 1 — дескриптор открыт заново
  if (afd >= 0) { sceIoClose(afd); afd = -1; }
  if (msnartReopenFail107 > 0) { msnartReopenFail107--; msnartIoReopen108++; return 0; }
  afd = sceIoOpen(file, PSP_O_RDONLY, 0); msnartIoReopen108++; msnartIoStuck108 = 0;
  return afd >= 0;
}
static unsigned char tries94[256]; static unsigned retryAt94;
static int failRetry94(int r, int lev, const char *why) {   // 1 — запись поставлена на повтор
  if (!msnartRetry94 || isPic(r) || r < 0 || r >= (int)sizeof tries94 || nq >= MAXQ) return 0;
  if (tries94[r] >= MSNART_TRIES94) { if (tries94[r] == MSNART_TRIES94) { tries94[r]++; if (msnartWatch) msnartWatch("give-up", rows[r].path, MSNART_TRIES94); } return 0; }
  if (++tries94[r] == 1 && msnartWatch) msnartWatch(why, rows[r].path, 1);   // одна строка на событие, не на каждый повтор
  qrow[nq] = r; qlev[nq] = lev; nq++;
  retryAt94 = sceKernelGetSystemTimeLow() + 1000000u; if (!retryAt94) retryAt94 = 1;
  return 1;
}
static void pumpOnce(void) {
  if (afd < 0) {
    if (msnartIo107bOld || msnartIo108Old || !rows || !file[0] || (busyRow < 0 && !nq)) return;   // 107б: не открыт, а читать нечего — молчим
    unsigned now_ = sceKernelGetSystemTimeLow(); if (reopenAt107 && (int)(now_ - reopenAt107) < 0) return;
    reopenAt107 = now_ + 1000000u; if (!reopenAt107) reopenAt107 = 1;
    int ok_ = ioReopen107(); bbOp("art-reopen", ok_ ? "ok" : "fail", 0); if (msnartWatch) msnartWatch(ok_ ? "reopen-ok" : "reopen-fail", file, 1);
    if (!ok_) return;
  }
  if (msnartAvDelayS > 0.f && wantIcon[0]) msnartSetObjIcon(wantIcon);   // №1247 (служебно): отложенный значок встаёт, когда срок вышел
  if (busyRow >= 0) {
    SceInt64 res = 0;
    if (msnartStall107 || sceIoPollAsync(afd, &res) == 1) {   // ещё читается (msnartStall107 — служебно: чтение «зависло», как при сне)
      if (!MSNARTSTALL1156_OLD && (unsigned)(sceKernelGetSystemTimeLow() - busyT0) > MSNART_READ_MS * 1000u) abandonBusy();   // №1156
      return;
    }
    int slot = -1; for (int i = 0; i < MAXE; i++) if (!ent[i].b) { slot = i; break; }
    if (slot < 0 && msnartShare1363 && res == (SceInt64)rows[busyRow].size && evictShared1363()) for (int i = 0; i < MAXE; i++) if (!ent[i].b) { slot = i; break; }   // 1363: общая, не нужная этой миссии, уходит первой
    if (slot < 0 && !MSNART_PIC_OLD && res == (SceInt64)rows[busyRow].size) {   // нет ячейки: вытеснить запись миссии (не уровня, не картинку брифинга)
      for (int i = 0; i < MAXE; i++) if (ent[i].b && !ent[i].level && !isPic(ent[i].row)) { bbOp("art-", rows[ent[i].row].path, 0); if (msnartWatch) msnartWatch("evict", rows[ent[i].row].path, 1); drop(i); slot = i; break; }
      if (slot < 0 && isPic(busyRow)) for (int i = 0; i < MAXE; i++) if (ent[i].b && !isPic(ent[i].row)) { bbOp("art-", rows[ent[i].row].path, 0); drop(i); slot = i; break; }
    }
    if (res == (SceInt64)rows[busyRow].size && slot >= 0) {
      sceKernelDcacheWritebackRange(busyBuf, rows[busyRow].size);   // текстуры GE читает из RAM
      entGen108++; ent[slot].b = busyBuf; ent[slot].row = busyRow; ent[slot].level = busyLev; ent[slot].shared = (unsigned char)shareRow1363(busyRow); ent[slot].used = 1; bytesUsed += rows[busyRow].size;
      if (busyRow < (int)sizeof tries94 && tries94[busyRow]) { if (msnartWatch) msnartWatch("recovered", rows[busyRow].path, tries94[busyRow]); tries94[busyRow] = 0; }
      if (busyRow < (int)sizeof picTries) { if (picTries[busyRow] && msnartWatch) msnartWatch("pic-recovered", rows[busyRow].path, picTries[busyRow]); picTries[busyRow] = 0; picGaveUp[busyRow] = 0; }
    } else {
      free(busyBuf);
      failRetry94(busyRow, busyLev, "read-fail");
      if (!MSNART_PIC_OLD && isPic(busyRow)) {   // картинка не дочиталась: один повтор в начало очереди, иначе в счётчик потерь
        if (busyRow < (int)sizeof retried && !retried[busyRow] && nq < MAXQ) { retried[busyRow] = 1; memmove(qrow + 1, qrow, sizeof(int) * nq); memmove(qlev + 1, qlev, sizeof(int) * nq); qrow[0] = busyRow; qlev[0] = busyLev; nq++; }
        else { msnartLostPics++; picFinal(busyRow); }
      }
    }
    busyBuf = 0; if (!MSNART_BBDONE_OLD) bbOp("art=", rows[busyRow].path, 0);   // тур 10.10 (L3): строка «op art+ …» в чёрном ящике оставалась последней и «зависшей», хотя запись давно дочитана — теперь окончание тоже действие
    busyRow = -1;
    if (wantIcon[0]) msnartSetObjIcon(wantIcon);                 // значок задания мог ждать эту запись
  }
  if (busyRow < 0 && nq) {
    if (ioSerial1337 && ioDevBusy1337) return;   // 1337: стенд «одна карта» — читать нельзя, пока загрузчик в клетке
    if (retryAt94) { if ((int)(sceKernelGetSystemTimeLow() - retryAt94) < 0) return; retryAt94 = 0; }   // Build 94: пауза 1 с между повторами
    int r = qrow[0], lev = qlev[0]; nq--; memmove(qrow, qrow + 1, sizeof(int) * nq); memmove(qlev, qlev + 1, sizeof(int) * nq);
    if (msnartReq) msnartReq(rows[r].path);
    bbOp("art+", rows[r].path, 0);   // правка 103: самописец — что читается с карты
    int sim94 = msnartFailSim94 > 0; if (sim94) msnartFailSim94--;
    if (!sim94 && msnartFailMissgen > 0 && strstr(rows[r].path, "missgen")) { sim94 = 1; msnartFailMissgen--; }   // Кольцо-96, служебно (flag msnartfailmissgen N): N отказов памяти только у missgen.p3d — воспроизводит L4 fragblk: 0,65 МБ не находится, остальное грузится   // служебный отказ: все три попытки памяти этой записи мимо
    busyBuf = sim94 ? 0 : zone3AllocX(Z3A_MISSION, "art", rows[r].size, 16);
    if (!busyBuf && msnartShare1363 && !sim94 && evictShared1363()) busyBuf = zone3AllocX(Z3A_MISSION, "art", rows[r].size, 16);   // 1363: теснота — сначала общие записи, которых эта миссия не просила
    if (!busyBuf && !MSNART_ROOM_OLD && msnartRoom && msnartRoom(rows[r].size) && !sim94) busyBuf = zone3AllocX(Z3A_MISSION, "art", rows[r].size, 16);
    // Кольцо-96 (корень чёрной карточки, longgame L4 fragblk 0.6): отказ памяти у missgen.p3d (0,65 МБ) сбрасывал ВСЕ записи миссии, в том числе только что дочитанный пакет картинки брифинга,
    // каждую секунду (60 повторов): картинка не держалась, карточка 8 с ждала на чёрном LOADING. Картинки брифинга при этом не сбрасываются (MSNART_PICWAIT_OLD=1 / flag msnartpicwaitold 1 — как было)
    if (!busyBuf && !MSNARTSTALL1156_OLD) { int dropped_ = 0; for (int i = 0; i < MAXE; i++) if (ent[i].b && !ent[i].level && (msnartPicWaitOld || !isPic(ent[i].row))) { drop(i); dropped_++; }   // №1156: отказ памяти — кэш записей миссий долой и один повтор
      if (dropped_) { bbOp("art~", rows[r].path, 0); busyBuf = sim94 ? 0 : zone3AllocX(Z3A_MISSION, "art", rows[r].size, 16); } }
    if (!busyBuf && !msnartPicWaitOld && !MSNART_PIC_OLD && isPic(r) && r < (int)sizeof picTries) {   // Кольцо-96: картинка брифинга ждёт память, раз в секунду, до MSNART_PIC_TRIES раз
      if (picTries[r] < picMaxTries() && nq < MAXQ) { if (++picTries[r] == 1 && msnartWatch) msnartWatch("pic-no-mem", rows[r].path, 1);
        qrow[nq] = r; qlev[nq] = lev; nq++; retryAt94 = sceKernelGetSystemTimeLow() + 1000000u; if (!retryAt94) retryAt94 = 1; return; }
      picGaveUp[r] = 1; msnartLostPics++; if (msnartWatch) msnartWatch("pic-give-up", rows[r].path, picMaxTries()); return;
    }
    if (!busyBuf && !msnartIo108Old) { char a_[48]; snprintf(a_, sizeof a_, "need=%u r=%d", rows[r].size, r); bbOp("art-mem", a_, 0); }
    if (!busyBuf) { if (failRetry94(r, lev, "no-mem")) return; if (!MSNART_PIC_OLD && isPic(r)) { if (r < (int)sizeof retried && !retried[r] && nq < MAXQ) { retried[r] = 1; memmove(qrow + 1, qrow, sizeof(int) * nq); memmove(qlev + 1, qlev, sizeof(int) * nq); qrow[0] = r; qlev[0] = lev; nq++; } else { msnartLostPics++; picFinal(r); } } return; }
    DCPRE96(busyBuf, rows[r].size);
    int rcS_ = sceIoLseek32(afd, (int)rows[r].off, PSP_SEEK_SET), rcR_ = rcS_ < 0 ? 0 : ((msnartIoStuck108 || pwrSimFault("msnart")) ? (int)0x80020329 : sceIoReadAsync(afd, busyBuf, rows[r].size));
    if (rcS_ >= 0 && rcR_ >= 0) { /* чтение запущено */ }
    else if (!msnartIo108Old && (!isPic(r) || !msnartIo107bOld) && r >= 0 && r < (int)sizeof tries94) {   // 108: отказ запуска чтения — в ящик, дескриптор заново, с 3-й попытки синхронно; 107б: и у картинок карточки
      int tr_ = isPic(r) ? picTries[r] : tries94[r]; char a_[48]; snprintf(a_, sizeof a_, "s=%08x r=%08x t=%u", (unsigned)rcS_, (unsigned)rcR_, tr_); bbOp("art-io", a_, 0);
      if (msnartIo107bOld) { if (afd >= 0) sceIoClose(afd); afd = sceIoOpen(file, PSP_O_RDONLY, 0); msnartIoReopen108++; msnartIoStuck108 = 0; } else ioReopen107();   // 107б: неудачное переоткрытие — afd −1, дальше повтор в начале msnartPump
      if (afd >= 0 && tr_ >= 2 && sceIoLseek32(afd, (int)rows[r].off, PSP_SEEK_SET) >= 0 && sceIoRead(afd, busyBuf, rows[r].size) == (int)rows[r].size) {
        int slot_ = -1; for (int i = 0; i < MAXE; i++) if (!ent[i].b) { slot_ = i; break; }
        if (slot_ < 0 && !msnartIo107bOld && isPic(r)) for (int i = 0; i < MAXE; i++) if (ent[i].b && !ent[i].level && !isPic(ent[i].row)) { bbOp("art-", rows[ent[i].row].path, 0); drop(i); slot_ = i; break; }   // 107б: как в msnartPump, место картинке
        if (slot_ >= 0) { sceKernelDcacheWritebackRange(busyBuf, rows[r].size); entGen108++; ent[slot_].b = busyBuf; ent[slot_].row = r; ent[slot_].level = lev; bytesUsed += rows[r].size; busyBuf = 0; msnartIoSync108++;
          bbOp("art=", rows[r].path, 0); if (tries94[r]) { if (msnartWatch) msnartWatch("recovered", rows[r].path, tries94[r]); tries94[r] = 0; }
          if (isPic(r)) { if (picTries[r] && msnartWatch) msnartWatch("pic-recovered", rows[r].path, picTries[r]); picTries[r] = 0; picGaveUp[r] = 0; } if (wantIcon[0]) msnartSetObjIcon(wantIcon); return; } }
      free(busyBuf); busyBuf = 0;
      if (isPic(r)) {   // 107б: картинка карточки — повтор раз в секунду до picMaxTries (как «нет памяти»), потом сдача с LOST
        if (picTries[r] < picMaxTries() && nq < MAXQ) { if (++picTries[r] == 1 && msnartWatch) msnartWatch("pic-io", rows[r].path, 1);
          qrow[nq] = r; qlev[nq] = lev; nq++; retryAt94 = sceKernelGetSystemTimeLow() + 1000000u; if (!retryAt94) retryAt94 = 1; return; }
        picGaveUp[r] = 1; msnartLostPics++; if (msnartWatch) msnartWatch("pic-give-up", rows[r].path, picMaxTries()); return;
      }
      if (failRetry94(r, lev, "io-fail")) return; return;
    } else { free(busyBuf); busyBuf = 0; if (failRetry94(r, lev, "io-fail")) return; if (!MSNART_PIC_OLD && isPic(r)) { msnartLostPics++; picFinal(r); } return; }
    busyRow = r; busyLev = lev; busyT0 = sceKernelGetSystemTimeLow();
  }
}
// 1359 (оценка Fable п.6б, Роб: «мелкий файл миссии стоит 0,1 с не из-за карты, а из-за опроса раз в кадр»): msnartPump звали раз в кадр, а чтение записи (значок 64 КБ ≈ 5 мс) кончалось
// через несколько миллисекунд — следующая запись очереди ждала кадра (33 мс): 10 значков подряд = 10 кадров. Теперь после запуска чтения насос в том же кадре коротко опрашивает его
// (sceIoPollAsync, паузы по 150 мкс), и как только оно кончилось — тут же берёт следующее из очереди; общий предел MSNART_CHAIN_US за кадр (крупные записи не задерживают кадр больше). MSNARTCHAIN1359_OLD=1 /
// «flag msnartchain1359old 1» / файл — как было (одно действие за кадр)
#ifndef MSNARTCHAIN1359_OLD
#define MSNARTCHAIN1359_OLD 0
#endif
unsigned msnartChainUs1359 = 8000u;   // предел за кадр; msn_view.c ставит 24 мс, пока мир скрыт (карточка, ирис): там кадр ничего не рисует, ждать чтение можно дольше
int msnartChain1359Old = MSNARTCHAIN1359_OLD;
static unsigned ch1359T0, ch1359N, ch1359Pumps, ch1359DoneUs, ch1359DonePumps;   // служебное (гейт): время и число кадров, за которые очередь из msnartService1359 опустела
void msnartPump(void) {
  if (ch1359T0) ch1359Pumps++;
  unsigned t0 = sceKernelGetSystemTimeLow();
  pumpOnce();
  if (!msnartChain1359Old && !msnartStall107) {
    while ((busyRow >= 0 || nq) && afd >= 0 && (unsigned)(sceKernelGetSystemTimeLow() - t0) < msnartChainUs1359) {
      int b0 = busyRow, q0 = nq;
      if (busyRow >= 0) sceKernelDelayThread(150);   // чтение в полёте: даём ему закончиться (результат опроса снимает pumpOnce сам — sceIoPollAsync отдаёт его один раз)
      pumpOnce();
      if (busyRow < 0 && b0 < 0 && nq == q0) break;   // читать нечего или ждём повтор/память (стенд одной карты): до следующего кадра
    }
  }
  if (ch1359T0 && busyRow < 0 && !nq) { ch1359DoneUs = sceKernelGetSystemTimeLow() - ch1359T0; ch1359DonePumps = ch1359Pumps; ch1359T0 = 0; }
}
// служебное (flag msnartchain1359 N): поставить в очередь N разных небольших записей (не картинок, ещё не загруженных) и замерить, за сколько кадров и мкс очередь опустеет
void msnartService1359(int n) {
  if (!rows || n <= 0) return;
  int got = 0;
  for (unsigned r = 0; r < nrows && got < n; r++) {
    if (isPic((int)r) || rows[r].size > 200000u) continue;
    int have = 0; for (int i = 0; i < MAXE; i++) if (ent[i].b && ent[i].row == (int)r) have = 1;
    if (have || busyRow == (int)r) continue; for (int i = 0; i < nq; i++) if (qrow[i] == (int)r) have = 1; if (have) continue;
    msnartLoad(rows[r].path, 0); got++;
  }
  ch1359T0 = sceKernelGetSystemTimeLow() | 1u; ch1359N = (unsigned)got; ch1359Pumps = 0; ch1359DoneUs = 0;
}
int msnartChainResult1359(unsigned *n, unsigned *pumps, unsigned *us) { if (!ch1359DoneUs) return 0; *n = ch1359N; *pumps = ch1359DonePumps; *us = ch1359DoneUs; return 1; }
static const MsaImg *findImg(const char *name, const unsigned char **base) {
  if (!name) return 0;
  for (int i = MAXE - 1; i >= 0; i--) {
    if (!ent[i].b) continue;
    const MsaHdr *h = (const MsaHdr *)ent[i].b;
    for (unsigned k = 0; k < h->nimg; k++) {
      const MsaImg *m = (const MsaImg *)(ent[i].b + h->offImg) + k;
      if (!strcasecmp(m->name, name)) { *base = ent[i].b; return m; }
    }
  }
  return 0;
}
// №1247 (служебно, flag avdelay S): значок задания «дочитывается» через S секунд после первого запроса — медленная карта PSP в эмуляторе (стенд пузыря: до правки текст стартует с места пакета и прыгает)
static char avDName[32]; static unsigned avDT0;
static int avHeld(const char *name) {
  if (msnartAvDelayS <= 0.f || !name || !*name) return 0;
  if (strcmp(avDName, name)) { snprintf(avDName, sizeof avDName, "%s", name); avDT0 = sceKernelGetSystemTimeLow(); }
  return (sceKernelGetSystemTimeLow() - avDT0) * 1e-6f < msnartAvDelayS;
}
int msnartImage(const char *name) { const unsigned char *b; return findImg(name, &b) != 0 && !avHeld(name); }
int msnartReading(void) { return busyRow >= 0 || nq > 0 || (msnartAvDelayS > 0.f && wantIcon[0] && avHeld(wantIcon)); }   // №1247: 1 — запись с картинками ещё читается или стоит в очереди (значок может прийти)
int msnartImageData(const char *name, const void **pix, int *tw, int *th, int *w, int *h) {
  const unsigned char *b; const MsaImg *m = findImg(name, &b);
  if (!m) return 0;
  const MsaTex *t = (const MsaTex *)(b + ((const MsaHdr *)b)->offTex) + m->tex;
  *pix = b + t->off; *tw = t->w; *th = t->h; *w = (int)m->w; *h = (int)m->h; return 1;
}
void msnartSetObjIcon(const char *name) {
  if (name != wantIcon) { strncpy(wantIcon, name ? name : "", sizeof wantIcon - 1); wantIcon[sizeof wantIcon - 1] = 0; }
  const unsigned char *b; const MsaImg *m = findImg(name, &b);
  if (!m || avHeld(name)) { uiSetObjIconImage(0, 0, 0, 0, 0); return; }
  const MsaTex *t = (const MsaTex *)(b + ((const MsaHdr *)b)->offTex) + m->tex;
  uiSetObjIconImage(b + t->off, t->w, t->h, (int)m->w, (int)m->h);
  uiSetObjIconTop((int)m->pad - 1);   // свой верх значка из экспорта (tools/msnart_export.py icon_top); -1 — нет
}

// №993: 1 — в углу пиксели того значка, что просит миссия (wantIcon); 0 — чужие или оборванные (запись выгружена); -1 — значка нет. Диагностика (flag msnhud)
int msnartIconPixOk(void) {
  const void *p = uiObjPix(); if (!wantIcon[0]) return p ? 0 : -1;
  const unsigned char *b; const MsaImg *m = findImg(wantIcon, &b);
  if (!m) return p ? 0 : -1;
  const MsaTex *t = (const MsaTex *)(b + ((const MsaHdr *)b)->offTex) + m->tex;
  return p == (const void *)(b + t->off);
}

static void bindTex(const unsigned char *b, unsigned ti, int wrap) {
  const MsaHdr *h = (const MsaHdr *)b;
  if (ti >= h->ntex) { sceGuDisable(GU_TEXTURE_2D); return; }
  const MsaTex *t = (const MsaTex *)(b + h->offTex) + ti;
  sceGuEnable(GU_TEXTURE_2D);
  sceGuTexMode(GU_PSM_8888, 0, 0, 0); sceGuTexImage(0, t->w, t->h, t->w, b + t->off);
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); sceGuTexFilter(GU_LINEAR, GU_LINEAR);
  sceGuTexWrap(wrap ? GU_REPEAT : GU_CLAMP, wrap ? GU_REPEAT : GU_CLAMP);
}
static void blendMode(unsigned blm) {   // BLMD шейдера Pure3D: 1 прозрачность, 2 сложение, 3 вычитание (тень), 0 нет
  if (blm == 0) { sceGuDisable(GU_BLEND); return; }
  sceGuEnable(GU_BLEND);
  if (blm == 2) sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
  else if (blm == 3) sceGuBlendFunc(GU_REVERSE_SUBTRACT, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
  else sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
}
static unsigned abgr(float r, float g, float b, float a) {
  #define Q(x) ((unsigned)((x) < 0.f ? 0 : (x) > 1.f ? 255 : (int)((x) * 255.f + .5f)))
  return Q(a) << 24 | Q(b) << 16 | Q(g) << 8 | Q(r);
}
// поворот всего рисунка вокруг вертикали (msnartDrawRot, стрелки навигации на дороге): курс как в main.c — вперёд (sin h, 0, cos h)
static int rotOn; static float rotC = 1.f, rotS = 0.f;
static void rotY(float *v) { if (!rotOn) return; float x = v[0], z = v[2]; v[0] = x * rotC + z * rotS; v[2] = -x * rotS + z * rotC; }
// Правка Роба 103 (повтор 01.10): каждый меш и квад — только пока в списке GE есть место (msnartGeRoom ставит main.c: предел
// GE_HARD бюджета, как у кусков города). Пропы уличных гонок — сотни мешей (l1_sr2p: 505), ~130 Б списка каждый: раньше рисовались
// без проверки и после остановленного на 90 % города писали за конец массива list (на железе встаёт вся PSP)
#define MSA_GE_MESH 160u   // байт на меш: матрица 13 слов, текстура и режим ~16, рисование 4 (+ запас)
#define MSA_GE_BILL 256u   // квад: 4 вершины в списке (96 Б) + команды
int (*msnartGeRoom)(unsigned bytes);
unsigned msnartGeSkips;
int msnartMarkerLodOld;
int msnartGlowBias;   // TEST19 (flag glowbias N): сдвиг глубины N единиц к камере для квадов-пятен под персонажами и маркерами; 0 — как есть
static void markerBound(float *lo, float *hi, const float *p) {
  for(int k=0;k<3;k++) { if(p[k]<lo[k])lo[k]=p[k]; if(p[k]>hi[k])hi[k]=p[k]; }
}
void msnartDrawNpcMarker(const char *name, int radarType, const float *pos, float ex, float ey, float ez, float t) {
  if (pos && camNearPoint(ex, ey, ez, pos[0], pos[1], pos[2], CAM_HIDE_ICON)) return;   // №502: значок у самого глаза не рисуется
  if (!msnartMarkerLodOld) for(int i=MAXE-1;i>=0;i--) {
    const unsigned char *b=ent[i].b; if(!b)continue;
    const MsaHdr *h=(const MsaHdr *)b;
    const MsaDraw *ds=(const MsaDraw *)(b+h->offDraw);
    for(unsigned k=0;k<h->ndraw;k++) {
      const MsaDraw *d=&ds[k]; if(strcasecmp(d->name,name))continue;
      unsigned fr=d->nfr>1?(unsigned)(t*(d->fps>0.f?d->fps:30.f))%d->nfr:0;
      const float *JM=d->njoint?(const float *)(b+d->offJoint)+(size_t)fr*d->njoint*12:0;
      float lo[3]={1e30f,1e30f,1e30f},hi[3]={-1e30f,-1e30f,-1e30f}; int count=0;
      const MsaMesh *ms=(const MsaMesh *)(b+d->offMesh);
      // №849: рамка позы меша зависит только от кадра клипа — считается один раз на кадр (кэш по рисунку); MSA_BOUNDS849_OLD=1 — каждый кадр
      MsaNpcBnd *nb_ = 0;
      if (!MSA_BOUNDS849_OLD && d->nmesh) { for (unsigned c_ = 0; c_ < msaNBn; c_++) if (msaNB[c_].d == (const void *)d) { nb_ = &msaNB[c_]; break; }
        if (!nb_ && msaNBn < 8) { unsigned nf_ = d->nfr > 1 ? d->nfr : 1; float *bb_ = (float *)malloc(nf_ * 6 * sizeof(float)); unsigned char *ok_ = (unsigned char *)calloc(nf_, 1);
          if (bb_ && ok_) { nb_ = &msaNB[msaNBn++]; nb_->d = d; nb_->nfr = nf_; nb_->bb = bb_; nb_->ok = ok_; } else { free(bb_); free(ok_); } }
        if (nb_ && fr < nb_->nfr && nb_->ok[fr]) { memcpy(lo, nb_->bb + fr * 6, 12); memcpy(hi, nb_->bb + fr * 6 + 3, 12); count = 1; } }
      // Bounds of the actual posed symbol; exclude decorative glow if a solid mesh exists
      if (!count) for(unsigned m=0;m<d->nmesh;m++) {
        unsigned j=ms[m].flags>>16; const float *J=JM?JM+(j<d->njoint?j:0)*12:0;
        const MsaVert *vs=(const MsaVert *)(b+ms[m].offVert);
        for(unsigned v=0;v<ms[m].nvert;v++) {
          float p[3]={vs[v].x,vs[v].y,vs[v].z};
          if(J) for(int a=0;a<3;a++)p[a]=J[a]*vs[v].x+J[3+a]*vs[v].y+J[6+a]*vs[v].z+J[9+a];
          markerBound(lo,hi,p); count++;
        }
      }
      if (nb_ && count && fr < nb_->nfr && !nb_->ok[fr]) { memcpy(nb_->bb + fr * 6, lo, 12); memcpy(nb_->bb + fr * 6 + 3, hi, 12); nb_->ok[fr] = 1; }   // №849
      if(!count) {
        const MsaBill *bs=(const MsaBill *)(b+d->offBill);
        for(unsigned q=0;q<d->nbill;q++) {
          const MsaBill *bb=&bs[q]; unsigned j=bb->mode>>8;
          if(bb->blend==2 || bb->blend==3)continue; // additive aura / subtractive spot are not the readable symbol
          const float *J=JM?JM+(j<d->njoint?j:0)*12:0;
          unsigned f=bb->nfr?(unsigned)(t*bb->fps)%bb->nfr:0;
          const float *tr=(const float *)(b+bb->offTrack)+f*6; if(tr[5]<=.004f)continue;
          float c[3]; for(int a=0;a<3;a++)c[a]=J?J[a]*bb->c[0]+J[3+a]*bb->c[1]+J[6+a]*bb->c[2]+J[9+a]:bb->c[a];
          // Camera-facing quads: their authored width/height, not a world-axis sphere
          float p[3]={c[0]-tr[0]*.5f,c[1]-tr[1]*.5f,c[2]}; markerBound(lo,hi,p);
          p[0]+=tr[0];p[1]+=tr[1];markerBound(lo,hi,p);count++;
        }
      }
      if(count) {
        float center[3],half[3];for(int a=0;a<3;a++){center[a]=(lo[a]+hi[a])*.5f+pos[a];half[a]=(hi[a]-lo[a])*.5f;}
        if(msnartGeRoom && !msnartGeRoom(MSA_GE_BILL)) { msnartGeSkips++; return; }
        if(uiMarkerSmall(radarType,center,half))return;
      }
      goto full;
    }
  }
full:
  msnartDraw(name,pos,ex,ey,ez,t);
}
static float cullD;   // msnartDrawCull: меши дальше cullD от глаза (по земле) не рисуются — пропы гонки, сотни экземпляров
// 1298 (Роб: «в гонке с Милхаусом на L2 очень сильно лагает у покрышек вдоль дороги»; реквизит l2_sr1p: 622 меша на суставах, в круге 70 м в среднем 210 мешей, у каждого своя матрица и своя привязка текстуры):
// (1) меш, чья сфера (центр из бокса вершин через матрицу сустава, радиус — полудиагональ × масштаб) целиком снаружи боковых плоскостей кадра, не рисуется (rfSphereOut, как у пропов: GE его всё равно отсёк бы);
// (2) непрозрачные меши (режим 0) рисуются группами по текстуре — одна привязка на текстуру (4020 → 480 за 60 кадров); порядок рисования при этом другой, по краям покрышек единицы-десятки пикселей (3–22 при ровном ходе); худший кадр L2 sr1 18,1 → 15,7 мс. Включено словом Роба 14.11; MSNARTGROUP1298_OLD=1 / файл msnartgroup1298old / «flag msnartgroup1298 0» — прежний порядок.
// Выключатель MSNARTCULL1298_OLD=1 / «flag msnartcull1298 0» / файл msnartcull1298old — как было. «flag msnartcull1298 3» — сверка: у отсечённого меша ни одна вершина не должна лежать внутри четырёх плоскостей кадра (msnartRaceViol);
// 4 — красный контроль: радиус ×0,3 со сверкой (viol > 0)
#ifndef MSNARTCULL1298_OLD
#define MSNARTCULL1298_OLD 0
#endif
// msnartGroup1298 (flag msnartgroup1298 0|1, файл msnartgroup1298old, MSNARTGROUP1298_OLD): непрозрачные по текстурам; с 14.11 по умолчанию 1 (слово Роба); 0 — порядок рисования прежний, картинка бит в бит
#ifndef MSNARTGROUP1298_OLD
#define MSNARTGROUP1298_OLD 0   // слово Роба 14.11 «что ты ждёшь»: группировка включена по умолчанию
#endif
int msnartCull1298 = !MSNARTCULL1298_OLD, msnartGroup1298 = !MSNARTGROUP1298_OLD; unsigned msnartRaceFrustum, msnartRaceViol, msnartRaceDraws;
int (*msnartSphereOut)(const float *c, float r); int (*msnartPtIn)(const float *p);   // main_render.h: плоскости кадра
typedef struct { float c[3], r; unsigned char ok; } MsaBndM; static const void *mBndOwner;
typedef struct { ScePspFMatrix4 M; unsigned tex, nv, off; } MsaDefer;
// 1298: сетка 32 м по положению суставов мешей реквизита (строится один раз на рисунок): в круге cullD берутся мешы только соседних ячеек, а не все 622 дважды
#define MSA_GRID_CELL 32.f
#define MSA_GRID_MAXC 1024
#define MSA_DEFER_N 512
// Рабочая память отсечения (границы мешей, отложенные меши, сетка, выбор) ~76 КБ: раньше статически (143 КБ BSS на ВСЕ запуски, канон 13), теперь берётся при первом кадре реквизита гонки из области «миссия» (zone3AllocX, как записи msnart)
// и возвращается вместе с записью реквизита (drop) / при старте миссии / при выгрузке. Не нашлась (даже после msnartRoom) — прежний путь без отсечения, повтор не чаще раза в 2 с. Строка WATCHDOG на отказ
typedef struct { MsaBndM bnd[1024]; MsaDefer defer[MSA_DEFER_N]; unsigned short kbuf[2048];
  struct { const void *d; float x0, z0; int nx, nz; unsigned start[MSA_GRID_MAXC + 1]; unsigned short idx[2048]; unsigned fill[MSA_GRID_MAXC]; unsigned char anyTr; } grid; } MsaCullMem;
static int mBndOwnerIn(const unsigned char *b, unsigned sz) { return b && mBndOwner && (const unsigned char *)mBndOwner >= b && (const unsigned char *)mBndOwner < b + sz; }
static MsaCullMem *mc; static unsigned mcFailT; unsigned msnartCullMemBytes; int msnartCullMemFail;   // msnartCullMemFail (flag msnartcullmemfail 1) — служебно: память отсечения «не нашлась», проверка прежнего пути
static void cullMemFree(void) { if (mc) free(mc); mc = 0; mBndOwner = 0; msnartCullMemBytes = 0; }
static int cullMemGet(void) {
  if (mc) return 1; unsigned now = sceKernelGetSystemTimeLow(); if (mcFailT && now - mcFailT < 2000000u) return 0;
  mc = msnartCullMemFail ? 0 : (MsaCullMem *)zone3AllocX(Z3A_MISSION, "mcull", sizeof *mc, 16);
  if (!mc && !msnartCullMemFail && !MSNART_ROOM_OLD && msnartRoom && msnartRoom(sizeof *mc)) mc = (MsaCullMem *)zone3AllocX(Z3A_MISSION, "mcull", sizeof *mc, 16);
  if (!mc) { mcFailT = now | 1u; if (msnartWatch) msnartWatch("race cull memory refused: old path", "mcull", 1); return 0; }
  memset(mc, 0, sizeof *mc); mBndOwner = 0; mcFailT = 0; msnartCullMemBytes = (unsigned)sizeof *mc; return 1;
}
unsigned msnartRaceT[3]; unsigned msnartRaceMeshUs, msnartRaceBnd, msnartRaceUs, msnartRaceMesh, msnartRaceBind, msnartRaceCullN, msnartRaceCalls; int msnartRaceOff;   // 1298 замер: реквизит гонки (мкс на рисование, мешей, привязок текстур, отсечённых по расстоянию); msnartRaceOff — служебный флаг «не рисовать»
void msnartDrawCull(const char *name, const float *pos, float ex, float ey, float ez, float t, float dist) {
  if (msnartRaceOff) return;
  unsigned rt0_ = sceKernelGetSystemTimeLow(); msnartRaceCalls++;
  cullD = dist; msnartDraw(name, pos, ex, ey, ez, t); cullD = 0.f;
  msnartRaceUs += sceKernelGetSystemTimeLow() - rt0_;
}
void msnartDrawRot(const char *name, const float *pos, float heading, float ex, float ey, float ez, float t) {
  rotOn = 1; rotC = cosf(heading); rotS = sinf(heading); msnartDraw(name, pos, ex, ey, ez, t); rotOn = 0;
}
#ifndef MSNART_SPRITE_OLD
#define MSNART_SPRITE_OLD 0
#endif
int (*msnartSprite)(const char *name, const float *c, const float *cr, const float *bt, const float *br, const float *pv, float tilt, float cf, const float *g, float gw, float gd, float ex, float ey, float ez);
// №834 (Роб 18.10: «вход в дом и в Kwik-E-Mart к Апу — там вход подсвечивается голубым; сделай его тоже большим, из наших векторных, только не с острыми
// концами, а с большими скруглениями — впадины между пиками округлые»): звезда входа (рисунки *doorstar*: sim_door, kwk_door, sch_door, l1_doors) — вместо
// колючей картинки звезды (квад 0, прозрачность) векторная: кольцо с дыркой в середине (как у картинки: прозрачно до 0,36, плотно с 0,60) и краем
// r(θ) = DS_K × (1 − DS_DEPTH × (0,5 − 0,5 cos 12θ)) — 12 скруглённых пиков и округлых впадин, край мягкий (6 %); цвет и пульс (размер, альфа) — дорожки
// квада данных; размер — DS_K от квада (1,25: «большим»). Мягкое свечение (квад 1, сложение) — как было. DOORSTAR_VEC_OLD=1 — картинкой
#ifndef DOORSTAR_VEC_OLD
#define DOORSTAR_VEC_OLD 0
#endif
#define DS_K 1.25f
#define DS_DEPTH .2f
#define DS_A .55f   // альфа пояса — доля альфы дорожки (у картинки звезды пояс не сплошной: ~0,5–0,6)
#define DS_PEAKS 12
#define DS_SEG 72   // 6 отсчётов на период: скругления гладкие (вершин 72 × 18, 21 КБ кольца вершин на звезду)
typedef struct { unsigned c; float x, y, z; } DsV;
static float dsCs[DS_SEG + 1][2], dsO[DS_SEG + 1]; static int dsOk;   // косинусы, синусы и край звезды по отсчётам — один раз (в кадре без синусов)
static void doorStarVec(const float *p, const float *X, const float *Y, unsigned col) {   // X, Y — полуоси квада в мире
  if (!dsOk) { for (int k = 0; k <= DS_SEG; k++) { float th = (float)k * (6.2831853f / DS_SEG); dsCs[k][0] = cosf(th); dsCs[k][1] = sinf(th);
      dsO[k] = DS_K * (1.f - DS_DEPTH * (.5f - .5f * cosf(DS_PEAKS * th))); } dsOk = 1; }
  unsigned a0 = col & 0x00FFFFFFu; DsV *v = (DsV *)geVbAlloc(DS_SEG * 18u * sizeof(DsV), GEVB_MSNART_OLD); unsigned n = 0;
  col = a0 | (unsigned)((float)(col >> 24) * DS_A + .5f) << 24;   // пояс прозрачный, как у картинки (белая звезда с мягкой альфой)
  #define DP(k_, r_, c_) do { float cs_ = dsCs[k_][0], sn_ = dsCs[k_][1]; \
    v[n].c = (c_); v[n].x = p[0] + (X[0] * cs_ + Y[0] * sn_) * (r_); v[n].y = p[1] + (X[1] * cs_ + Y[1] * sn_) * (r_); v[n].z = p[2] + (X[2] * cs_ + Y[2] * sn_) * (r_); n++; } while (0)
  for (int k = 0; k < DS_SEG; k++) {
    float o0 = dsO[k], o1 = dsO[k + 1];
    float r[4][2] = {{.36f * DS_K, .36f * DS_K}, {.60f * DS_K, .60f * DS_K}, {o0, o1}, {o0 * 1.06f, o1 * 1.06f}}; unsigned c[4] = {a0, col, col, a0};
    for (int g = 0; g < 3; g++) {   // три пояса: дырка -> плотное, плотное -> край, край -> мягкий спад
      DP(k, r[g][0], c[g]); DP(k + 1, r[g][1], c[g]); DP(k, r[g + 1][0], c[g + 1]);
      DP(k + 1, r[g][1], c[g]); DP(k + 1, r[g + 1][1], c[g + 1]); DP(k, r[g + 1][0], c[g + 1]);
    }
  }
  #undef DP
  sceGuDisable(GU_TEXTURE_2D);
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)n, 0, v);
  sceGuEnable(GU_TEXTURE_2D);
}
// №944 (Роб 24.10: «светяшки на домах — тот же блинг-блинг формой бубны»): звезда входа (рисунки *doorstar*: sim_door, kwk_door, sch_door, l1_doors …) — не кольцо с 12 пиками №834,
// а общий шейп блика №941 (glintshape941.h: звезда A — четыре луча, B — пять, меньше, повёрнута и плывёт; ромб с вогнутыми гладкими боками, острые кончики, без контура),
// в плоскости квада двери. Размер — кончик большого луча на DS_K от полуоси квада (как пики кольца), цвет и пульс (размер, альфа) — дорожки квада данных, альфа — DS_A
// (как у кольца), малая звезда светлее (цвет к белому) и плывёт по кругу на 12 % радиуса, пульс обеих — как у блика spritelight.h gvAdd. Вершин 108 (кольцо — 1296). GLINT944_OLD=1 — кольцо №834
#define DS944_SEG 4   // отрезков на сторону луча (как GV_SEG_RAY)
unsigned g944DoorOld, g944DoorNew; void (*msnartNote944)(const char *site, const char *what, const float *p, int a, int b);   // служебно: звёзды входа за кадры — строка GLINT944 (spritelight.h)
static float ds9Sin(float turns) {   // sin(2π · turns) по таблице 256 точек (sinf на PSP медленный; как gvSinT в spritelight.h)
  static float tab[256]; static int ok; if (!ok) { for (int i = 0; i < 256; i++) tab[i] = sinf(6.2831853f * (float)i / 256.f); ok = 1; }
  float f = turns - floorf(turns); return tab[(int)(f * 256.f) & 255];
}
static void doorStarGlint944(const float *p, const float *X, const float *Y, unsigned col, float tsec) {   // X, Y — полуоси квада в мире
  static float opx[2][GS_MAXPTS], opy[2][GS_MAXPTS]; static int om[2], ok;
  if (!ok) { for (int s = 0; s < 2; s++) om[s] = gsOutline(s ? &GS_GLINT_B : &GS_GLINT_A, DS944_SEG, 1.f, opx[s], opy[s]); ok = 1; }
  float kA = 1.f + .06f * ds9Sin(tsec / 1.3f), kB = 1.f + .12f * ds9Sin(tsec / .85f + .3342254f);
  float ox = .12f * ds9Sin(tsec / 1.1f), oy = .10f * ds9Sin(tsec / 1.1f + .25f);
  unsigned a = (unsigned)((float)(col >> 24) * DS_A + .5f), rgb = col & 0xFFFFFFu;
  unsigned r2 = (((rgb & 255u) + 255u) / 2u) | ((((rgb >> 8 & 255u) + 255u) / 2u) << 8) | ((((rgb >> 16 & 255u) + 255u) / 2u) << 16);
  unsigned cA = a << 24 | rgb, cB = (unsigned)((float)a * .9f + .5f) << 24 | r2;
  DsV *v = (DsV *)geVbAlloc((unsigned)(om[0] + om[1]) * 3u * sizeof(DsV), GEVB_MSNART_OLD); if (!v) return; unsigned n = 0;
  for (int s = 0; s < 2; s++) {
    float k = (s ? .62f * kB : kA) * DS_K, cx = s ? ox : 0.f, cy = s ? oy : 0.f; unsigned c = s ? cB : cA;
    #define DP9(x_, y_) do { float xx_ = cx + (x_) * k, yy_ = cy + (y_) * k; v[n].c = c; v[n].x = p[0] + X[0] * xx_ + Y[0] * yy_; v[n].y = p[1] + X[1] * xx_ + Y[1] * yy_; v[n].z = p[2] + X[2] * xx_ + Y[2] * yy_; n++; } while (0)
    for (int i = 0; i < om[s]; i++) { int j = (i + 1) % om[s]; DP9(0.f, 0.f); DP9(opx[s][i], opy[s][i]); DP9(opx[s][j], opy[s][j]); }
    #undef DP9
  }
  sceGuDisable(GU_TEXTURE_2D);
  sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)n, 0, v);
  sceGuEnable(GU_TEXTURE_2D);
}
void msnartList944(void (*out)(const char *name, int nbill, int nmesh)) {   // №944 служебно: рисунки загруженных записей
  for (int i = 0; i < MAXE; i++) { if (!ent[i].b) continue; const MsaHdr *h = (const MsaHdr *)ent[i].b;
    for (unsigned k = 0; k < h->ndraw; k++) { const MsaDraw *x = (const MsaDraw *)(ent[i].b + h->offDraw) + k; out(x->name, (int)x->nbill, (int)x->nmesh); } }
}
static void doorStarDraw944(const float *p, const float *X, const float *Y, unsigned col, float tsec) {   // выбор формы: общий шейп (GLINT944_OLD=1 — кольцо №834)
  if (GLINT944_OLD) { g944DoorOld++; doorStarVec(p, X, Y, col); } else { g944DoorNew++; doorStarGlint944(p, X, Y, col, tsec); }
}
void msnartDoorLab944(const float *p, const float *X, const float *Y, unsigned col, float tsec) {   // служебно (flag glintlab 2): лист на чёрном — тот же выбор формы, что у входа в игре
  sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_DEPTH_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); sceGuDepthMask(GU_TRUE);
  doorStarDraw944(p, X, Y, col, tsec);
}
int msnartHas(const char *name) {
  for (int i = MAXE - 1; i >= 0; i--) {
    if (!ent[i].b) continue;
    const MsaHdr *h = (const MsaHdr *)ent[i].b;
    for (unsigned k = 0; k < h->ndraw; k++) if (!strcasecmp(((const MsaDraw *)(ent[i].b + h->offDraw) + k)->name, name)) return 1;
  }
  return 0;
}
int msaFind108Old, msaFind108Chk; unsigned msaFind108Bad;
unsigned msaCallN, msaFindUs, msaDrawUs;   // №849 служебно (msnart.h): вызовы, мкс поиска по имени и всего рисунка
void msnartDraw(const char *name, const float *pos, float ex, float ey, float ez, float tsec) {
  unsigned t0_ = sceKernelGetSystemTimeLow(); msaCallN++;
  const unsigned char *b = 0; const MsaDraw *d = 0;
  // 108 О17 (Бабл; «поиск рисунка по имени каждый кадр, msa_find»): итог поиска — чистая функция (имя без учёта регистра, таблица записей); запоминается по хэшу имени, пока таблица не менялась (entGen108). Хит проверяется
  // сравнением строки без учёта регистра, как поиск. MSAFIND108_OLD=1 / «flag msafind108old 1» — поиск каждый раз; «flag msafind108chk 1» — каждый хит сверяется с поиском (msaFind108Bad, строка PBCHK msa_bad)
  static struct { unsigned gen; char name[40]; const MsaDraw *d; const unsigned char *b; } fc[32]; unsigned fh = 2166136261u, fx = 0; int fok = 0;
  if (!msaFind108Old) { size_t nl = strlen(name); if (nl < sizeof fc[0].name) { for (size_t q = 0; q < nl; q++) { unsigned char ch = (unsigned char)name[q]; if (ch >= 'A' && ch <= 'Z') ch += 32; fh ^= ch; fh *= 16777619u; } fx = fh & 31; fok = 1; } }
  int hit = 0;
  if (fok && fc[fx].gen == entGen108 && !strcasecmp(fc[fx].name, name)) { d = msaFind108Red == 2 ? 0 : fc[fx].d; b = fc[fx].b; hit = 1; }   // msaFind108Red 2 — красный контроль: хит теряет рисунок
  if (!hit || msaFind108Chk) {
    const MsaDraw *d2 = 0; const unsigned char *b2 = 0;
    for (int i = MAXE - 1; i >= 0 && !d2; i--) {
      if (!ent[i].b) continue;
      const MsaHdr *h = (const MsaHdr *)ent[i].b;
      for (unsigned k = 0; k < h->ndraw; k++) {
        const MsaDraw *x = (const MsaDraw *)(ent[i].b + h->offDraw) + k;
        if (!strcasecmp(x->name, name)) { d2 = x; b2 = ent[i].b; break; }
      }
    }
    if (hit) { if (d2 != d || b2 != b) msaFind108Bad++; }
    else { d = d2; b = b2; if (fok) { fc[fx].gen = entGen108; snprintf(fc[fx].name, sizeof fc[fx].name, "%s", name); fc[fx].d = d; fc[fx].b = b; } }
  }
  msaFindUs += sceKernelGetSystemTimeLow() - t0_;
  if (!d) return;
  if (pos && camNearPoint(ex, ey, ez, pos[0], pos[1], pos[2], CAM_HIDE_ICON)) return;   // №502: значок/знак у самого глаза не рисуется
  float ox = pos ? pos[0] : 0.f, oy = pos ? pos[1] : 0.f, oz = pos ? pos[2] : 0.f;
  // кадр позы: клип по кругу (tsec — время маркера), матрицы суставов кадра: R по столбцам 9 + t 3
  unsigned fr = d->nfr > 1 ? (unsigned)(tsec * (d->fps > 0.f ? d->fps : 30.f)) % d->nfr : 0;
  const float *JM = d->njoint ? (const float *)(b + d->offJoint) + (size_t)fr * d->njoint * 12 : 0;
  if (!MSNART_SPRITE_OLD && msnartSprite && !rotOn && d->nmesh && d->njoint && d->nbill) {   // добавка Роба 18.10: «!» и трубка — спрайтом (msnart.h)
    const MsaMesh *m0 = (const MsaMesh *)(b + d->offMesh); unsigned j = m0->flags >> 16; const float *J = JM + (j < d->njoint ? j : 0) * 12;
    float c0[3], b0[3]; const MsaBnd *bc_ = 0;
    if (!MSA_BOUNDS849_OLD) for (unsigned i_ = 0; i_ < msaBndN; i_++) if (msaBnd[i_].d == (const void *)d) { bc_ = &msaBnd[i_]; break; }
    if (bc_) { memcpy(c0, bc_->c0, sizeof c0); memcpy(b0, bc_->b0, sizeof b0); }
    else {
    const MsaVert *vs = (const MsaVert *)(b + m0->offVert); float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f}; b0[0] = 0.f; b0[1] = 1e30f; b0[2] = 0.f;
    for (unsigned v = 0; v < m0->nvert; v++) { float p[3] = {vs[v].x, vs[v].y, vs[v].z}; markerBound(lo, hi, p); if (p[1] < b0[1]) memcpy(b0, p, sizeof b0); }   // b0 — нижняя точка меша
    c0[0] = (lo[0] + hi[0]) * .5f; c0[1] = (lo[1] + hi[1]) * .5f; c0[2] = (lo[2] + hi[2]) * .5f;
    if (!MSA_BOUNDS849_OLD && msaBndN < 24) { MsaBnd *e_ = &msaBnd[msaBndN++]; e_->d = d; memcpy(e_->c0, c0, sizeof c0); memcpy(e_->b0, b0, sizeof b0); } }
    float c[3] = {ox + J[0] * c0[0] + J[3] * c0[1] + J[6] * c0[2] + J[9], oy + J[1] * c0[0] + J[4] * c0[1] + J[7] * c0[2] + J[10], oz + J[2] * c0[0] + J[5] * c0[1] + J[8] * c0[2] + J[11]};
    const float *R = (const float *)(b + d->offJoint) + (j < d->njoint ? j : 0) * 12;   // кадр 0 клипа — поза покоя (№814: «!» без бокового хода)
    float cr[3] = {ox + R[0] * c0[0] + R[3] * c0[1] + R[6] * c0[2] + R[9], oy + R[1] * c0[0] + R[4] * c0[1] + R[7] * c0[2] + R[10], oz + R[2] * c0[0] + R[5] * c0[1] + R[8] * c0[2] + R[11]};
    // №818: нижняя точка меша — сейчас (bt) и в позе покоя (br), и её место в кадре 0 ленты (вид спереди: x, y модели от центра рамки)
    float bt[3] = {ox + J[0] * b0[0] + J[3] * b0[1] + J[6] * b0[2] + J[9], oy + J[1] * b0[0] + J[4] * b0[1] + J[7] * b0[2] + J[10], oz + J[2] * b0[0] + J[5] * b0[1] + J[8] * b0[2] + J[11]};
    float br[3] = {ox + R[0] * b0[0] + R[3] * b0[1] + R[6] * b0[2] + R[9], oy + R[1] * b0[0] + R[4] * b0[1] + R[7] * b0[2] + R[10], oz + R[2] * b0[0] + R[5] * b0[1] + R[8] * b0[2] + R[11]};
    float pv[2] = {b0[0] - c0[0], b0[1] - c0[1]};
    const MsaBill *q = (const MsaBill *)(b + d->offBill); unsigned qj = q->mode >> 8; const float *Q = JM + (qj < d->njoint ? qj : 0) * 12;
    float g[3] = {ox + Q[0] * q->c[0] + Q[3] * q->c[1] + Q[6] * q->c[2] + Q[9], oy + Q[1] * q->c[0] + Q[4] * q->c[1] + Q[7] * q->c[2] + Q[10], oz + Q[2] * q->c[0] + Q[5] * q->c[1] + Q[8] * q->c[2] + Q[11]};
    float cf = fmodf(tsec * (d->fps > 0.f ? d->fps : 30.f), (float)(d->nfr ? d->nfr : 1u));   // №819: место в клипе без ступенек по кадрам
    if (msnartSprite(d->name, c, cr, bt, br, pv, atan2f(J[1], J[0]), cf, g, q->w, -q->dist, ex, ey, ez)) return;
  }
  sceGuDisable(GU_FOG); sceGuDisable(GU_CULL_FACE);
  sceGuTexScale(1.f, 1.f); sceGuTexOffset(0.f, 0.f);
  const MsaMesh *ms = (const MsaMesh *)(b + d->offMesh);
  unsigned meshT0_ = sceKernelGetSystemTimeLow(); unsigned nDefer = 0; unsigned nk = d->nmesh; const unsigned short *klist = 0; int skipTr = 0;
  const int cull98 = cullD > 0.f && msnartCull1298 && msnartSphereOut && d->njoint && !rotOn && cullMemGet();   // + сетка 32 м и отсечение по кадру; msnartGroup1298 — группы по текстуре
  if (cull98 && mBndOwner != (const void *)d) { memset(mc->bnd, 0, sizeof mc->bnd); mBndOwner = d; }
  if (cull98 && d->nmesh <= 2048) {   // сетка по суставам (один раз), выбор мешей круга
    if (mc->grid.d != (const void *)d) {
      float lx = 1e30f, hx = -1e30f, lz = 1e30f, hz = -1e30f; mc->grid.anyTr = 0;
      for (unsigned k = 0; k < d->nmesh; k++) { const MsaMesh *m = &ms[k]; if (!m->nvert) continue; unsigned j = m->flags >> 16; const float *J = JM + (j < d->njoint ? j : 0) * 12; float x = ox + J[9], z = oz + J[11];
        if (x < lx) lx = x; if (x > hx) hx = x; if (z < lz) lz = z; if (z > hz) hz = z; if (((m->flags >> 8) & 0xFF) >= 2) mc->grid.anyTr = 1; }
      mc->grid.x0 = lx; mc->grid.z0 = lz; mc->grid.nx = (int)((hx - lx) / MSA_GRID_CELL) + 1; mc->grid.nz = (int)((hz - lz) / MSA_GRID_CELL) + 1;
      if (mc->grid.nx * mc->grid.nz > MSA_GRID_MAXC) { mc->grid.d = 0; }
      else { memset(mc->grid.start, 0, sizeof(unsigned) * (unsigned)(mc->grid.nx * mc->grid.nz + 1));
        for (int pass2 = 0; pass2 < 2; pass2++) { if (pass2) memcpy(mc->grid.fill, mc->grid.start, sizeof(unsigned) * (unsigned)(mc->grid.nx * mc->grid.nz));
          for (unsigned k = 0; k < d->nmesh; k++) { const MsaMesh *m = &ms[k]; if (!m->nvert) continue; unsigned j = m->flags >> 16; const float *J = JM + (j < d->njoint ? j : 0) * 12;
            int cx = (int)((ox + J[9] - mc->grid.x0) / MSA_GRID_CELL), cz = (int)((oz + J[11] - mc->grid.z0) / MSA_GRID_CELL); int c = cz * mc->grid.nx + cx;
            if (!pass2) mc->grid.start[c + 1]++; else mc->grid.idx[mc->grid.fill[c]++] = (unsigned short)k; }
          if (!pass2) for (int c = 0; c < mc->grid.nx * mc->grid.nz; c++) mc->grid.start[c + 1] += mc->grid.start[c]; }
        mc->grid.d = d; } }
    if (mc->grid.d == (const void *)d) {
      int cx0 = (int)floorf((ex - cullD - mc->grid.x0) / MSA_GRID_CELL), cx1 = (int)floorf((ex + cullD - mc->grid.x0) / MSA_GRID_CELL), cz0 = (int)floorf((ez - cullD - mc->grid.z0) / MSA_GRID_CELL), cz1 = (int)floorf((ez + cullD - mc->grid.z0) / MSA_GRID_CELL);
      if (cx0 < 0) cx0 = 0; if (cz0 < 0) cz0 = 0; if (cx1 >= mc->grid.nx) cx1 = mc->grid.nx - 1; if (cz1 >= mc->grid.nz) cz1 = mc->grid.nz - 1;
      unsigned n = 0; unsigned bits[64]; unsigned nw = (d->nmesh + 31u) >> 5; memset(bits, 0, nw * 4u);
      for (int cz = cz0; cz <= cz1; cz++) for (int cx = cx0; cx <= cx1; cx++) { int c = cz * mc->grid.nx + cx; for (unsigned q = mc->grid.start[c]; q < mc->grid.start[c + 1]; q++) { unsigned k_ = mc->grid.idx[q]; bits[k_ >> 5] |= 1u << (k_ & 31); } }
      for (unsigned w = 0; w < nw; w++) for (unsigned v = bits[w]; v; v &= v - 1) mc->kbuf[n++] = (unsigned short)((w << 5) + (unsigned)__builtin_ctz(v));   // по возрастанию номера, как прежний обход, без сортировки
      klist = mc->kbuf; nk = n; skipTr = !mc->grid.anyTr; } }
  msnartRaceT[0] += sceKernelGetSystemTimeLow() - meshT0_;   // границы мешей — только у реквизита с отсечением: прочие рисунки кадра кэш не сбрасывают
  for (int pass = 0; pass < 2; pass++) {   // сначала непрозрачное и вырезки (пишут глубину), потом полупрозрачное и свечения
  if (pass && skipTr) break;   // 1298: полупрозрачных мешей у рисунка нет
  for (unsigned kk = 0; kk < nk; kk++) {   // меши (ведёрко, «!», доллар): вершины в осях сустава, матрица кадра
    unsigned k = klist ? klist[kk] : kk;
    const MsaMesh *m = &ms[k]; if (!m->nvert) continue;
    unsigned mode = (m->flags >> 8) & 0xFF, j = m->flags >> 16; if ((mode >= 2) != pass) continue;
    ScePspFMatrix4 M; memset(&M, 0, sizeof M); M.x.x = M.y.y = M.z.z = M.w.w = 1.f; M.w.x = ox; M.w.y = oy; M.w.z = oz;
    if (cullD > 0.f && d->njoint) { const float *J = JM + (j < d->njoint ? j : 0) * 12;
      float dx = ox + J[9] - ex, dz = oz + J[11] - ez; if (dx * dx + dz * dz > cullD * cullD) { if (!pass) msnartRaceCullN++; continue; } }
    if (d->njoint) { const float *J = JM + (j < d->njoint ? j : 0) * 12;
      float cx_[3] = {J[0], J[1], J[2]}, cy_[3] = {J[3], J[4], J[5]}, cz_[3] = {J[6], J[7], J[8]}, t_[3] = {J[9], J[10], J[11]};
      rotY(cx_); rotY(cy_); rotY(cz_); rotY(t_);
      M.x.x = cx_[0]; M.x.y = cx_[1]; M.x.z = cx_[2]; M.y.x = cy_[0]; M.y.y = cy_[1]; M.y.z = cy_[2]; M.z.x = cz_[0]; M.z.y = cz_[1]; M.z.z = cz_[2];
      M.w.x += t_[0]; M.w.y += t_[1]; M.w.z += t_[2]; }
    else if (rotOn) { M.x.x = rotC; M.x.z = -rotS; M.z.x = rotS; M.z.z = rotC; }
    if (cull98 && k < 1024) {   // 1298: сфера меша целиком вне боковых плоскостей кадра — не рисуем
      MsaBndM *bm = &mc->bnd[k];
      if (!bm->ok) { msnartRaceBnd++; const MsaVert *vs = (const MsaVert *)(b + m->offVert); float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
        for (unsigned v = 0; v < m->nvert; v++) { float p[3] = {vs[v].x, vs[v].y, vs[v].z}; for (int a_ = 0; a_ < 3; a_++) { if (p[a_] < lo[a_]) lo[a_] = p[a_]; if (p[a_] > hi[a_]) hi[a_] = p[a_]; } }
        for (int a_ = 0; a_ < 3; a_++) bm->c[a_] = (lo[a_] + hi[a_]) * .5f;
        bm->r = sqrtf((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) + (hi[2] - lo[2]) * (hi[2] - lo[2])) * .5f; bm->ok = 1; }
      float wc[3] = {M.x.x * bm->c[0] + M.y.x * bm->c[1] + M.z.x * bm->c[2] + M.w.x, M.x.y * bm->c[0] + M.y.y * bm->c[1] + M.z.y * bm->c[2] + M.w.y, M.x.z * bm->c[0] + M.y.z * bm->c[1] + M.z.z * bm->c[2] + M.w.z};
      float sx = sqrtf(M.x.x * M.x.x + M.x.y * M.x.y + M.x.z * M.x.z), sy = sqrtf(M.y.x * M.y.x + M.y.y * M.y.y + M.y.z * M.y.z), sz = sqrtf(M.z.x * M.z.x + M.z.y * M.z.y + M.z.z * M.z.z), sc = sx > sy ? sx : sy; if (sz > sc) sc = sz;
      if (msnartSphereOut(wc, bm->r * sc * (msnartCull1298 >= 4 ? .3f : 1.f))) {
        msnartRaceFrustum++;
        if (msnartCull1298 >= 3 && msnartPtIn) { const MsaVert *vs = (const MsaVert *)(b + m->offVert); int bad = 0;   // сверка: у отсечённого меша внутри кадра ни одной вершины
          for (unsigned v = 0; v < m->nvert && !bad; v++) { float p[3] = {M.x.x * vs[v].x + M.y.x * vs[v].y + M.z.x * vs[v].z + M.w.x, M.x.y * vs[v].x + M.y.y * vs[v].y + M.z.y * vs[v].z + M.w.y, M.x.z * vs[v].x + M.y.z * vs[v].y + M.z.z * vs[v].z + M.w.z}; if (msnartPtIn(p)) bad = 1; }
          msnartRaceViol += (unsigned)bad; }
        continue; }
    }
    if (cull98 && msnartGroup1298 && mode == 0 && nDefer < MSA_DEFER_N) { MsaDefer *df = &mc->defer[nDefer++]; df->M = M; df->tex = m->tex; df->nv = m->nvert; df->off = m->offVert; continue; }   // 1298: непрозрачные — после цикла, по текстурам
    if (msnartGeRoom && !msnartGeRoom(MSA_GE_MESH)) { msnartGeSkips++; continue; }   // правка 103: список GE полон
    sceGuSetMatrix(GU_MODEL, &M);
    if (cullD > 0.f) { msnartRaceMesh++; msnartRaceBind++; }
    bindTex(b, m->tex, 1);
    if (mode == 0) { sceGuDisable(GU_BLEND); sceGuDisable(GU_ALPHA_TEST); sceGuDepthMask(GU_FALSE); }
    else if (mode == 1) { blendMode(1); sceGuEnable(GU_ALPHA_TEST); sceGuDepthMask(GU_FALSE); }   // вырезка: альфа-тест (GREATER 0x40) + глубина
    else { blendMode(mode == 3 ? 2 : mode == 4 ? 3 : 1); sceGuDisable(GU_ALPHA_TEST); sceGuDepthMask(GU_TRUE); }
    sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)m->nvert, 0, b + m->offVert);
  }
  if (pass == 0) msnartRaceT[1] += sceKernelGetSystemTimeLow() - meshT0_;
  if (pass == 0 && nDefer) {   // 1298: непрозрачные меши группами по текстуре: привязка и состояние один раз на текстуру
    const MsaHdr *hh = (const MsaHdr *)b; unsigned nt = hh->ntex; unsigned char did[1024]; memset(did, 0, nDefer);
    for (unsigned t = 0; t <= nt; t++) { int first = 1;   // t == nt: меши с текстурой вне таблицы (bindTex их выключает)
      for (unsigned i = 0; i < nDefer; i++) { if (did[i]) continue; unsigned tx = mc->defer[i].tex; if (t < nt ? tx != t : tx < nt) continue; did[i] = 1;
        if (msnartGeRoom && !msnartGeRoom(MSA_GE_MESH)) { msnartGeSkips++; continue; }
        if (first) { bindTex(b, tx, 1); sceGuDisable(GU_BLEND); sceGuDisable(GU_ALPHA_TEST); sceGuDepthMask(GU_FALSE); first = 0; msnartRaceBind++; }
        sceGuSetMatrix(GU_MODEL, &mc->defer[i].M); msnartRaceMesh++; msnartRaceDraws++;
        sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (int)mc->defer[i].nv, 0, b + mc->defer[i].off); } }
    nDefer = 0; }
  }
  if (cullD > 0.f) { msnartRaceMeshUs += sceKernelGetSystemTimeLow() - meshT0_; msnartRaceT[2] += sceKernelGetSystemTimeLow() - meshT0_; }
  sceGuDisable(GU_ALPHA_TEST);
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f;
  sceGuSetMatrix(GU_MODEL, &I);
  const MsaBill *bl = (const MsaBill *)(b + d->offBill);
  // Правка Роба 289 (04.10: «у Ральфа над головой флажки перевёрнуты»; запись PS2 16:56 — клетчатые флажки сверху, древки сходятся
  // внизу): v квада BillboardQuad — 0 внизу, 1 вверху (картинка пакета — строка 0 сверху); свечения симметричны, ошибку было видно
  // только на флажках checkered. -DMSA_BILL_V_OLD=1 — прежнее
#if !MSA_BILL_V_OLD
  static const float uv[8] = {0, 0, 1, 0, 0, 1, 1, 1};
#else
  static const float uv[8] = {0, 1, 1, 1, 0, 0, 1, 0};
#endif
  if (msnartGlowBias) sceGuDepthOffset(msnartGlowBias);
  // №839 (Роб 19.10: «мигает размытое свечение у восклицательного знака — все блюрные свечения замени нашим векторным бликом»): квады сложением
  // (свечения значков «!», $, кубиков, флажков, стрелок, предметов) не рисуются картинкой — вместо них один векторный блик на рисунок: центр и размер
  // самого крупного квада кадра, цвета двух крупных. Луч UfoBeam (№805) и пятна <рисунок>_shadow (№793) — своим путём. GLOW_GLINT_OLD=1 — картинкой
  int gg_ = !GLOW_GLINT_OLD && msnartGlowGlint && !msaIsBeam(d->name) && !msaIsShadow(d->name); float ggP[3] = {0, 0, 0}, ggW = 0.f, ggW2 = 0.f, ggA = 0.f; unsigned ggC1 = 0, ggC2 = 0;
  for (unsigned k = 0; k < d->nbill; k++) {   // квады: к камере (AAX) или в плоскости сустава (NOAX); ширина/высота/цвет по кадрам
    const MsaBill *q = &bl[k];
    const float *tr = (const float *)(b + q->offTrack);
    unsigned bf = q->nfr ? (unsigned)(tsec * q->fps) % q->nfr : 0; tr += bf * 6;
    if (tr[5] <= 0.004f) continue;
    unsigned qj = q->mode >> 8, qm = q->mode & 0xFF;
    const float *J = d->njoint ? JM + (qj < d->njoint ? qj : 0) * 12 : 0; float c[3], a[3], bb[3];
    #define RV(v, o) do { o[0] = J[0] * v[0] + J[3] * v[1] + J[6] * v[2]; o[1] = J[1] * v[0] + J[4] * v[1] + J[7] * v[2]; o[2] = J[2] * v[0] + J[5] * v[1] + J[8] * v[2]; } while (0)
    if (d->njoint) { RV(q->c, c); c[0] += J[9]; c[1] += J[10]; c[2] += J[11]; RV(q->ax, a); RV(q->ay, bb); }
    else { memcpy(c, q->c, sizeof c); memcpy(a, q->ax, sizeof a); memcpy(bb, q->ay, sizeof bb); }
    #undef RV
    rotY(c); rotY(a); rotY(bb);   // msnartDrawRot: весь рисунок повёрнут по курсу
    float p[3] = {c[0] + ox, c[1] + oy, c[2] + oz}, X[3], Y[3], w = tr[0] * .5f, h = tr[1] * .5f;
    if (q->dist != 0.f) {   // dist квада: вдоль луча взгляда (минус — от камеры: свечение за предметом)
      float vx = p[0] - ex, vy = p[1] - ey, vz = p[2] - ez, l = sqrtf(vx * vx + vy * vy + vz * vz);
      if (l > 1e-3f) { float k = -q->dist / l; p[0] += vx * k; p[1] += vy * k; p[2] += vz * k; } }
    if (qm == 0) {
      float la = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]), lb = sqrtf(bb[0] * bb[0] + bb[1] * bb[1] + bb[2] * bb[2]);
      la = la > 1e-6f ? 1.f / la : 0.f; lb = lb > 1e-6f ? 1.f / lb : 0.f;
      for (int i = 0; i < 3; i++) { X[i] = a[i] * la * w; Y[i] = bb[i] * lb * h; } }
    else {
      float dx = p[0] - ex, dy = p[1] - ey, dz = p[2] - ez, l = sqrtf(dx * dx + dz * dz), dl = sqrtf(dx * dx + dy * dy + dz * dz);
      if (l < 1e-3f) continue;
      X[0] = dz / l * w; X[1] = 0; X[2] = -dx / l * w;
      Y[0] = -(dy / dl) * (dx / l) * h; Y[1] = l / dl * h; Y[2] = -(dy / dl) * (dz / l) * h;
    }
    if (msnartArrow1025 && msnartGroundN && msaIsRoadArrow(d->name)) {   // №1025: стрелка пути — без бликов, по поверхности дороги
      if (q->blend == 2) continue;
      if (qm == 0) {   // результат (высота центра, оси квада) кэшируется по положению и осям: набор стоит на месте, пока путь не пересчитан (раз в ROUTE_DT)
        static struct { unsigned k[4]; float y, X[3], Y[3]; int ok, used; } ac[12]; static int an;
        unsigned k4[4]; memcpy(&k4[0], &p[0], 4); memcpy(&k4[1], &p[2], 4); memcpy(&k4[2], &X[0], 4); memcpy(&k4[3], &Y[2], 4); int hit = -1;
        for (int e = 0; e < 12; e++) if (ac[e].used && !memcmp(ac[e].k, k4, sizeof k4)) { hit = e; break; }
        if (hit < 0) {
          int e = an++ % 12; memcpy(ac[e].k, k4, sizeof k4); ac[e].used = 1; ac[e].ok = 0; hit = e;
          float yv, n3[3];
          if (msnartGroundN(p[0], p[2], p[1] + 4.f, p[1] - 8.f, &yv, n3) && n3[1] > .5f) {
            float nl = sqrtf(n3[0] * n3[0] + n3[1] * n3[1] + n3[2] * n3[2]); if (nl > 1e-4f) { n3[0] /= nl; n3[1] /= nl; n3[2] /= nl; }
            float XX[3] = {X[0], X[1], X[2]}, YY[3] = {Y[0], Y[1], Y[2]}, *AX[2] = {XX, YY};
            for (int e2 = 0; e2 < 2; e2++) { float *V = AX[e2], l0 = sqrtf(V[0] * V[0] + V[1] * V[1] + V[2] * V[2]), dn = V[0] * n3[0] + V[1] * n3[1] + V[2] * n3[2];
              for (int i = 0; i < 3; i++) V[i] -= dn * n3[i];
              float l1 = sqrtf(V[0] * V[0] + V[1] * V[1] + V[2] * V[2]); if (l1 > 1e-5f) for (int i = 0; i < 3; i++) V[i] *= l0 / l1; }
            float lift = yv + .05f;   // плоскость через землю под центром; ни один угол не тонет: поднимаем на недостачу самого низкого угла (бордюр, гребень)
            for (int c = 0; c < 4; c++) { float sx = (c & 1) ? 1.f : -1.f, sy = (c & 2) ? 1.f : -1.f, cg, qx = p[0] + sx * XX[0] + sy * YY[0], qz = p[2] + sx * XX[2] + sy * YY[2];
              if (msnartGroundN(qx, qz, yv + 3.f, yv - 3.f, &cg, n3)) { float need = cg + .03f - (yv + sx * XX[1] + sy * YY[1]); if (need > lift - yv) lift = yv + need; } }
            ac[e].y = lift; memcpy(ac[e].X, XX, sizeof XX); memcpy(ac[e].Y, YY, sizeof YY); ac[e].ok = 1; } }
        if (ac[hit].ok) { p[1] = ac[hit].y; memcpy(X, ac[hit].X, sizeof X); memcpy(Y, ac[hit].Y, sizeof Y); } } }
    if (msnartDecal1052 && q->blend == 2 && msaIsShadow(d->name)) continue;   // №1052: размытое пятно сложением под значком телефона
    if (gg_ && q->blend == 2) {   // №839: свечение — в векторный блик (рисуется один раз после квадов)
      float w_ = tr[0] > tr[1] ? tr[0] : tr[1]; unsigned c_ = abgr(tr[2], tr[3], tr[4], 1.f);
      if (w_ > ggW) { ggW2 = ggW; ggC2 = ggC1; ggW = w_; ggC1 = c_; ggA = tr[5]; memcpy(ggP, p, sizeof ggP); } else if (w_ > ggW2) { ggW2 = w_; ggC2 = c_; }
      continue; }
    if (msnartGeRoom && !msnartGeRoom(MSA_GE_BILL)) { msnartGeSkips++; continue; }   // правка 103: список GE полон
    if (qm != 0 && q->blend == 2 && msnartVecBeam && msaIsBeam(d->name) && msnartVecBeam(p, X, Y, abgr(tr[2], tr[3], tr[4], tr[5]))) {   // №805: луч — векторно
      if (msnartGlowBias) sceGuDepthOffset(msnartGlowBias); sceGuTexScale(1.f, 1.f); sceGuTexOffset(0.f, 0.f); continue; }
    MsaVert *v = geVbAlloc(4 * sizeof(MsaVert), GEVB_MSNART_OLD); unsigned col = abgr(tr[2], tr[3], tr[4], tr[5]);
    for (int i = 0; i < 4; i++) {
      float sx = (i & 1) ? 1.f : -1.f, sy = (i & 2) ? 1.f : -1.f;
      v[i].u = uv[2 * i]; v[i].v = uv[2 * i + 1]; v[i].c = col;
      v[i].x = p[0] + sx * X[0] + sy * Y[0]; v[i].y = p[1] + sx * X[1] + sy * Y[1]; v[i].z = p[2] + sx * X[2] + sy * Y[2];
    }
    // №944: звезда входа — КАЖДЫЙ квад звезды рисунка (у l1_doorstars их три: дом, Kwik-E-Mart, школа — были картинкой, вектор был только у первого); GLINT944_OLD=1 — как №834 (k == 0)
    if (!DOORSTAR_VEC_OLD && (GLINT944_OLD ? k == 0 : 1) && qm == 0 && q->blend == 1 && strstr(d->name, "doorstar")) {   // №834: звезда входа — векторная
      blendMode(1); sceGuDepthMask(q->zwrite ? GU_FALSE : GU_TRUE);
      if (msnartNote944) msnartNote944("doorstar", d->name, p, (int)q->blend, (int)k);
      doorStarDraw944(p, X, Y, col, tsec); continue; }
    if (msnartNote944 && q->blend != 3) msnartNote944("msnart_img", d->name, p, (int)q->blend, (int)k);   // №944 (служебно): картинка со сложением/прозрачностью остаётся картинкой
    bindTex(b, q->tex, 0); blendMode(q->blend); sceGuDepthMask(q->zwrite ? GU_FALSE : GU_TRUE);
    sceGuDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, v);
  }
  if (gg_ && ggW > .01f && ggA > .004f) msnartGlowGlint(d->name, ggP, ggW, ggC1, ggC2 ? ggC2 : ggC1, ggA, ex, ey, ez);   // №839
  sceGuTexScale(128.f, 128.f); sceGuTexWrap(GU_REPEAT, GU_REPEAT);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); sceGuDepthMask(GU_FALSE);
  if (msnartGlowBias) sceGuDepthOffset(0);
}
// №793: замер пятна «<рисунок>_shadow» для векторного рисования (msnart.h MsnartFlat). Билборд-квадрат лежит на земле, картинка —
// радиальное свечение (диск) или кольцо: яркость RGB·A по 10 кольцам от центра; ступень 1 — кольца ≥ 50 % пика, ступень 2 — ≥ 12 %
// (вне ступени 1). Цвет — цвет билборда (argb данных), альфа — средняя яркость ступени. Запоминается по имени (данные missgen.p3d
// одни на все уровни): картинку 64×64 не перебирать каждый кадр
int msnartFlat(const char *name, MsnartFlat *f) {
  static struct { char name[32]; int ok; MsnartFlat f; } cache[16]; static int ncache;
  for (int i = 0; i < ncache; i++) if (!strcasecmp(cache[i].name, name)) { if (cache[i].ok) *f = cache[i].f; return cache[i].ok; }
  const unsigned char *b = 0; const MsaDraw *d = 0;
  for (int i = MAXE - 1; i >= 0 && !d; i--) {
    if (!ent[i].b) continue;
    const MsaHdr *h = (const MsaHdr *)ent[i].b;
    for (unsigned k = 0; k < h->ndraw; k++) { const MsaDraw *x = (const MsaDraw *)(ent[i].b + h->offDraw) + k; if (!strcasecmp(x->name, name)) { d = x; b = ent[i].b; break; } }
  }
  if (!d) return 0;   // ещё не загружен — не запоминать
  int ok = 0; MsnartFlat r; memset(&r, 0, sizeof r);
  const MsaHdr *h = (const MsaHdr *)b;
  if (d->nbill && !d->nmesh) {
    const MsaBill *bl = (const MsaBill *)(b + d->offBill);
    if (bl->blend == 2 && bl->tex < h->ntex) {   // сложение (свечение); вычитание (тень предмета) — не здесь
      const MsaTex *t = (const MsaTex *)(b + h->offTex) + bl->tex; const unsigned char *px = b + t->off;
      float sum[10] = {0}, cnt[10] = {0}, mx = 0.f; int tw = t->w, th = t->h;
      for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) {
        float dx = x + .5f - tw * .5f, dy = y + .5f - th * .5f, rr = sqrtf(dx * dx + dy * dy) / (tw * .5f); if (rr >= 1.f) continue;
        const unsigned char *q = px + 4 * (y * tw + x); int k = (int)(rr * 10.f); sum[k] += (q[0] + q[1] + q[2]) / 3.f * q[3] / 255.f; cnt[k] += 1.f; }
      for (int k = 0; k < 10; k++) { sum[k] = cnt[k] > 0.f ? sum[k] / cnt[k] : 0.f; if (sum[k] > mx) mx = sum[k]; }
      int a0 = -1, a1 = -1, b0 = -1, b1 = -1; float sa = 0.f, na = 0.f, sb = 0.f, nb = 0.f;
      for (int k = 0; k < 10; k++) { if (sum[k] >= .5f * mx) { if (a0 < 0) a0 = k; a1 = k; } if (sum[k] >= .12f * mx) { if (b0 < 0) b0 = k; b1 = k; } }
      for (int k = 0; k < 10; k++) { if (k >= a0 && k <= a1) { sa += sum[k]; na += 1.f; } else if (k >= b0 && k <= b1) { sb += sum[k]; nb += 1.f; } }
      if (mx > 8.f && a0 >= 0) {
        unsigned rgb = ((bl->argb & 0xFFu) << 16) | (bl->argb & 0xFF00u) | ((bl->argb >> 16) & 0xFFu);   // argb -> bgr
        unsigned ai = (unsigned)(na > 0.f ? sa / na : 0.f), ao = (unsigned)(nb > 0.f ? sb / nb : 0.f);
        r.rad = bl->w * .5f; r.in0 = a0 * .1f; r.in1 = (a1 + 1) * .1f; r.out0 = b0 * .1f; r.out1 = (b1 + 1) * .1f;
        r.colIn = (ai > 255u ? 255u : ai) << 24 | rgb; r.colOut = (ao > 255u ? 255u : ao) << 24 | rgb; ok = 1;
      }
    }
  }
  if (ncache < 16) { strncpy(cache[ncache].name, name, 31); cache[ncache].name[31] = 0; cache[ncache].ok = ok; cache[ncache].f = r; ncache++; }
  if (ok) *f = r;
  return ok;
}

// 107б P0 (Роб 1273 «сон и пробуждение», генплан Fable §P0): пометки отказов msnart не живут дольше повторного чтения.
// (1) tries94 — счёт неудач записи миссии (до сдачи через 60) — НИКОГДА не сбрасывался: номер строки переходил на следующий уровень, а после сдачи запись не просили до конца игры. Теперь его сбрасывают смена уровня
// (msnartReset), старт миссии (msnartMissionStart) и пробуждение. (2) pwrEpoch() (счётчик Будки: растёт на пробуждении и смене уровня): при смене — счёт неудач и пауза повтора обнулены, картинки и пометки «потеряна» тоже;
// чтение В ПОЛЁТЕ (его дескриптор после сна мёртв) возвращается в начало очереди, буфер намеренно в утечку (ядро могло ещё его писать, как в abandonBusy), дескриптор msnart.shr открывается заново (не вышло — afd = −1,
// дальше решает msnartPump). PWR107_OLD=1 / «flag pwr107old 1» — как было. pwrEpoch и убийца дескриптора — power.h (Будка, flag simsleep N).
#ifndef PWR107_OLD
#define PWR107_OLD 0
#endif
int msnartPwr107Old = PWR107_OLD; unsigned msnartPwr107N;
// Карантин (дирижёр, 13.11: Роб усыпляет PSP постоянно, утечка на каждом сне копилась бы): буфер брошенного чтения (сон, тайм-аут) и его дескриптор не бросаются насовсем, а держатся в одной из QUAR_N ячеек; ядро
// могло ещё писать в буфер, поэтому освобождаются они только через QUAR_MS после постановки (после сна чтение с мёртвого дескриптора кончается ошибкой, на тайм-ауте — давно дочитано), дескриптор — отменой и закрытием.
// Все ячейки заняты: освобождается самая старая, если ей больше QUAR_MIN_MS, иначе новый буфер уходит в утечку (три сна за секунду). Утечка и держание — счётчики msnartLeakBytes и msnartQuarBytes (строка PBCHK).
// QUAR107_OLD=1 / «flag quar107old 1» — как было (всё в утечку)
#ifndef QUAR107_OLD
#define QUAR107_OLD 0
#endif
#define QUAR_N 2
#define QUAR_MS 10000u
#define QUAR_MIN_MS 1500u
int msnartQuar107Old = QUAR107_OLD, msnartStall107; unsigned msnartLeakBytes, msnartQuarBytes;
static struct { unsigned char *b; unsigned sz, t; SceUID fd; } quar[QUAR_N];
static void quarFree(int i) { if (quar[i].fd >= 0) { sceIoCancel(quar[i].fd); sceIoClose(quar[i].fd); } free(quar[i].b); msnartQuarBytes -= quar[i].sz; quar[i].b = 0; quar[i].fd = -1; quar[i].sz = 0; }
static void quarTick107(void) { unsigned now = sceKernelGetSystemTimeLow() / 1000u; for (int i = 0; i < QUAR_N; i++) if (quar[i].b && (unsigned)(now - quar[i].t) > QUAR_MS) quarFree(i); }
static void quarPush107(unsigned char *b, unsigned sz, SceUID fd) {
  if (!b) { if (fd >= 0 && fd != afd) { sceIoCancel(fd); sceIoClose(fd); } return; }
  if (msnartQuar107Old) { msnartLeakBytes += sz; return; }
  quarTick107(); unsigned now = sceKernelGetSystemTimeLow() / 1000u; int slot = -1, old = -1;
  for (int i = 0; i < QUAR_N; i++) { if (!quar[i].b) { slot = i; break; } if (old < 0 || (unsigned)(now - quar[i].t) > (unsigned)(now - quar[old].t)) old = i; }
  if (slot < 0 && old >= 0 && (unsigned)(now - quar[old].t) > QUAR_MIN_MS) { quarFree(old); slot = old; }
  if (slot < 0) { msnartLeakBytes += sz; return; }
  quar[slot].b = b; quar[slot].sz = sz; quar[slot].t = now; quar[slot].fd = fd >= 0 ? fd : -1; msnartQuarBytes += sz;
}
static unsigned epoch107, reopenAt107b; static int hook107;
static void msnartPwrSync107(void) { epoch107 = pwrEpoch(); reopenAt107b = 0; }   // 107в (ревью Fable): без этого первый msnartPwrPoll107 после каждой загрузки уровня видел «эпоха сменилась» (pwrBump на смене уровня) и закрывал здоровый msnart.shr
static void msnartKill107(void) { if (afd >= 0) sceIoClose(afd); afd = -1; }   // убийца дескриптора для simsleep (power.h): закрыть и записать −1
static void tries107Reset(void) { if (!msnartPwr107Old) { memset(tries94, 0, sizeof tries94); retryAt94 = 0; } }
static int reopen107b(void) {   // открыть msnart.shr заново; simsleep N портит и открытия
  if (afd >= 0) sceIoClose(afd);
  afd = (file[0] && !pwrSimFault("msnart")) ? sceIoOpen(file, PSP_O_RDONLY, 0) : -1; return afd >= 0;
}
void msnartPwrPoll107(void) {
  quarTick107();   // карантин: ячейки старше QUAR_MS освобождаются
  if (!hook107) { hook107 = 1; pwrKillHook("msnart", msnartKill107); epoch107 = pwrEpoch(); }
  if (!msnartPwr107Old && afd < 0 && rows && file[0]) { unsigned now_ = sceKernelGetSystemTimeLow(); if (!reopenAt107b || (int)(now_ - reopenAt107b) >= 0) { reopenAt107b = now_ + 1000000u; if (!reopenAt107b) reopenAt107b = 1; if (reopen107b()) bbOp("art-reopen", "pwr", 0); } }   // после сна дескриптор не открылся — раз в секунду заново
  unsigned e = pwrEpoch(); if (e == epoch107) return;
  epoch107 = e; if (msnartPwr107Old) return;
  msnartPwr107N++; bbOp("art-pwr", "epoch", 0);
  memset(tries94, 0, sizeof tries94); retryAt94 = 0; memset(picTries, 0, sizeof picTries); memset(picGaveUp, 0, sizeof picGaveUp);
  if (busyRow >= 0) {   // чтение в полёте: дескриптор мёртв — в очередь заново, буфер в утечку
    if (nq < MAXQ) { memmove(qrow + 1, qrow, sizeof(int) * nq); memmove(qlev + 1, qlev, sizeof(int) * nq); qrow[0] = busyRow; qlev[0] = busyLev; nq++; }
    quarPush107(busyBuf, rows[busyRow].size, -1); busyBuf = 0; busyRow = -1;   // буфер в карантин (ограниченный), не в утечку
  }
  if (rows) reopen107b();
}
