// Ход загрузочного экрана GONE Makers по кадрам (Роб 28.09 вечер: «сделать ещё более системной»): как POST старого ПК —
// писк в первый кадр, знак собирается в углу, а слева тем временем идут настоящие проверки железа строка за строкой
// (частота процессора, счёт памяти, видео, звук — канал того самого писка, питание, носитель). Ровная сетка bootui.h
// 4×7, колонки ключ / значение / размер / статус, один акцент для OK. Второй проход — волна по готовому знаку.
// Чистый C без PSP: один и тот же ход рисуют PSP (viewer/bootscreen.h) и превью на Маке (tools/bootscreen_preview.c).
// bootSeqDraw(n, ...) рисует состояние кадра n целиком (строки перерисовываются, знак — кадр анимации), поэтому пропуск
// START — это просто bootSeqDraw(bootSeqLen(), ...): тот же итоговый вид без прыжков.
#ifndef BOOTSEQ_H
#define BOOTSEQ_H
#include <stdio.h>
#include <string.h>
#include "bootui.h"

typedef struct {
  int mhz, busMhz;       // частота процессора и шины
  int memK;              // свободная память пользователя, КБ
  int audioCh;           // канал писка (>= 0 — занят и звучит), -1 — нет звука, -2 — ещё не знаем
  int beepHz;            // частота писка
  int batt;              // заряд, % (-1 — батареи нет: питание от сети)
  char media[8];        // устройство, откуда запущена игра (MS0:, HOST0:, ...)
  // правка Роба 203 (03.10): «загрузка того, что мы оптимизировали под PSP, с реальными данными» — числа из заголовков файлов
  // рядом с EBOOT, прочитанные при старте (bootDataScan): 0 — файла нет (строка не выводится)
  int vramK;                              // eDRAM видео (sceGeEdramGetSize)
  int nFiles; unsigned mediaMB;           // файлы игры на карте и их объём (без _archive)
  int ntex, nchunk;                       // level.shr: текстуры CLUT8 (свизл, мипы), куски города (ближние + дальние LOD)
  int pvsW, pvsH, pvsCell;                // PVS1: сетка видимости, клетка, м
  int colTri, colBox, colCyl, colSeg;     // COL1: коллизия уровня
  int trfModel, trfPed, trfRoad, trfPark; // traffic.shr
  int sfxRes, musTrack;                   // audio/sfx.sab, audio/music.sap
  int nMovie; unsigned movieMB;           // movies/*.smv
} BootSys;
typedef int (*BootRd)(void *u, const char *name, unsigned off, void *buf, int n);            // прочитать n байт с off: прочитано
typedef void (*BootDirFn)(void *u, const char *dir, const char *ext, int *count, unsigned *mb);   // файлы с окончанием ext ("" — все)
static unsigned bsqU32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24; }
static void bootDataScan(BootSys *s, BootRd rd, BootDirFn dr, void *u) {
  unsigned char h[104], q[48]; unsigned char t[148];
  if (rd(u, "level.shr", 0, h, 104) == 104 && !memcmp(h, "SHR7", 4)) {
    s->ntex = (int)bsqU32(h + 4); s->nchunk = (int)bsqU32(h + 8);
    unsigned pv = bsqU32(h + 88), co = bsqU32(h + 92);
    if (pv && rd(u, "level.shr", pv, q, 32) == 32 && !memcmp(q, "PVS1", 4)) {
      float cell; memcpy(&cell, q + 12, 4); s->pvsCell = (int)(cell + .5f); s->pvsW = (int)bsqU32(q + 16); s->pvsH = (int)bsqU32(q + 20); }
    if (co && rd(u, "level.shr", co, q, 48) == 48 && !memcmp(q, "COL1", 4)) {
      s->colTri = (int)bsqU32(q + 24); s->colBox = (int)bsqU32(q + 28); s->colCyl = (int)bsqU32(q + 32); s->colSeg = (int)bsqU32(q + 36); }
  }
  if (rd(u, "traffic.shr", 0, t, 148) == 148 && !memcmp(t, "TRF1", 4)) {
    s->trfRoad = (int)bsqU32(t + 20); s->trfModel = (int)bsqU32(t + 44); s->trfPed = (int)bsqU32(t + 120); s->trfPark = (int)bsqU32(t + 140); }
  if (rd(u, "audio/sfx.sab", 0, q, 16) == 16 && !memcmp(q, "SAB1", 4)) s->sfxRes = (int)bsqU32(q + 8);
  if (rd(u, "audio/music.sap", 0, q, 16) == 16 && !memcmp(q, "SAP1", 4)) s->musTrack = (int)bsqU32(q + 8);
  if (dr) { dr(u, "movies", ".smv", &s->nMovie, &s->movieMB); dr(u, "", "", &s->nFiles, &s->mediaMB); }
}

#define BSQ_P0 ((int)((bootAnimSpan() + 2 * BOOTANIM_WIDTH) / BOOTANIM_SPEED) + 1)   // кадров первого прохода
#define BSQ_PAUSE 12
#define BSQ_TAIL 8
// кадры строк проверок (16 мс кадр): всё успевает, пока знак собирается
enum { BSQ_CPU = 10, BSQ_MEM0 = 16, BSQ_MEM1 = 64, BSQ_VIDEO = 68, BSQ_MEDIA = 74, BSQ_AUDIO = 80, BSQ_POWER = 86,
       BSQ_DATA0 = 94, BSQ_DATA_STEP = 6, BSQ_RULE = 140 };
#define BSQ_ROW0 3   // правка 203: знак GONE справа — проверки слева сразу под заголовком (строки 0–1)
#define BSQ_HDR_COL ((BUI_LOGO_X - BUI_X0 + BOOTANIM_PW * BOOTANIM_SCALE + BUI_CW - 1) / BUI_CW + 1)   // заголовок — справа от знака
static int bootSeqLen(void);
// правка 127: пиксельные знаки причастных (viewer/bootmarks.h) под проверками; знак k появляется с кадра BSQ_MARK0 + 16·k
// за BMK_LEN кадров (последний кончается к кадру 224 из 270 — заставка не удлиняется). Нет знаков — bootSeqMarks = 0
#include "bootmarks.h"
#define BSQ_MARK0 (BSQ_DATA0 + 6)   // правка 203: логотипы под знаком справа появляются, пока слева идут строки данных
#define BSQ_MARK_STEP 16   // правка 148: 6 мелких значков, по одному каждые 16 кадров (одновременно ≤ 2 — бюджет кадра)
static const BootMarks *bootSeqMarks;
static int bootSeqLen(void) {
  int len=2*BSQ_P0+BSQ_PAUSE+BSQ_TAIL,nreg=0,tag=0;
  if(!bootSeqMarks)return len;
  for(int k=0;k<bootSeqMarks->n;k++) { int md=bootSeqMarks->m[k].mode; if(md==6)tag=1;else if(md<7)nreg++; }   // 397: кадры покоя — не знаки
  int end=BSQ_MARK0+BSQ_MARK_STEP*(nreg?nreg-1:0)+BMK_LEN;
  if(tag)end+=4+BMK_TAG_LEN; // №379: added Codex must not cut off the final El Barto reveal
  return end>len?end:len;
}
static void bootSeqMarksDraw(int n, BuiPut put, void *u) {
  if (!bootSeqMarks) return;
  bmkSW = bootSeqMarks->slotW ? bootSeqMarks->slotW : BMK1_SLOT_W; bmkSH = bootSeqMarks->slotH ? bootSeqMarks->slotH : BMK1_SLOT_H;
  bmkV = bootSeqMarks->v ? bootSeqMarks->v : 1; bmkSc = bootSeqMarks->scale > 1 ? bootSeqMarks->scale : 1; bmkN = bootSeqMarks->n;
  int nreg = 0; for (int k = 0; k < bootSeqMarks->n; k++) nreg += bootSeqMarks->m[k].mode < 6;   // правка 264: тег — не в потоке; 397: кадры покоя тоже
#if !defined(BMK_257_OLD) && !defined(BMK_254_OLD)
  if (bmkV >= 2 && !bmkFlow) {   // правка 257: спонсорский поток — места по ширине каждого знака, один раз на набор
    int w[BMK_MAX], h[BMK_MAX]; for (int k = 0; k < nreg; k++) { w[k] = bootSeqMarks->m[k].w * bmkSc; h[k] = bootSeqMarks->m[k].h * bmkSc; }
    bmkFlowLayout(w, h, nreg); bmkFlow = 1;
  }
#endif
  for (int k = 0; k < bootSeqMarks->n; k++) {
    const BootMark *m = &bootSeqMarks->m[k]; int t = n - (BSQ_MARK0 + BSQ_MARK_STEP * k);
    if (m->mode >= 7) continue;   // 397: кадр покоя рисует свой знак
    if (m->mode == 6) {   // правка 264: тег El Barto — после последнего знака, в правом нижнем углу правой части, «распылением»
      { int has6 = 0; for (int j = 0; j < bootSeqMarks->n; j++) has6 |= bootSeqMarks->m[j].mode == 6 && !strcmp(bootSeqMarks->m[j].name, BMK_TAG6_NAME);
        int is6 = !strcmp(m->name, BMK_TAG6_NAME), use6 = has6 && !bootElbarto1010Old;   // №1010: один тег из двух — родной L6, если он есть
        if (is6 != use6) continue; }
      int t0 = BSQ_MARK0 + BSQ_MARK_STEP * (nreg ? nreg - 1 : 0) + BMK_LEN + 4; t = n - t0;
      if (n < bootSeqLen() && (t < 0 || t >= BMK_TAG_LEN)) continue;
      if (n >= bootSeqLen() || t >= BMK_TAG_LEN) t = BMK_TAG_LEN - 1;
#ifndef BMK_397_OLD
      bootTagFrame(m, t, BUI_LOGO_X + (BOOTANIM_PW * BOOTANIM_SCALE - m->w * BMK_TAG_PX) / 2 + 4, BMK_BOTTOM - m->h * BMK_TAG_PX - 4, put, u);   // Роб 08.10: чуть выше и правее (4 px)
#else
      bootTagFrame(m, t, BUI_LOGO_X + (BOOTANIM_PW * BOOTANIM_SCALE - m->w * BMK_TAG_PX) / 2, BMK_BOTTOM - m->h * BMK_TAG_PX, put, u);   // по центру правой части, к низу
#endif
      continue;
    }
#ifndef BMK_397_OLD
    { const BootMark *fr[BMK_MAX]; int nf = 0, step = 0, len = bootSeqLen();
      for (int j = 0; j < bootSeqMarks->n; j++) { const BootMark *f = &bootSeqMarks->m[j];
        if (f->mode >= 7 && !strcmp(f->name, m->name) && nf < BMK_MAX) { fr[nf++] = f; step = bmkIdleStep(f->mode); } }
      if (nf && step && t >= BMK_LEN) {   // появление закончено: кадр покоя от конца появления; итоговый — как последний кадр хода
        int s0 = BSQ_MARK0 + BSQ_MARK_STEP * k + BMK_LEN, at = n >= len ? len - 1 : n;
        int cur = ((at - s0) / step) % (nf + 1), prv = at > s0 ? ((at - 1 - s0) / step) % (nf + 1) : 0;
        int ox = bmkSlotX(k), oy = bmkSlotY(k);
        const BootMark *now = cur ? fr[cur - 1] : m, *was = prv ? fr[prv - 1] : m;
        if (n >= len) bmkIdleDraw(0, now, ox, oy, BUI_BG, put, u); else if (cur != prv) bmkIdleDraw(was, now, ox, oy, BUI_BG, put, u);
        continue;
      } }
#endif
#ifdef BOOTMARK_ALWAYS_REDRAW   // tests/bootlogo: отрицательный контроль бюджета — все знаки каждый кадр -> FAIL
    if (t < 0) continue;
#else
    if (n < bootSeqLen() && (t < 0 || t >= BMK_LEN)) continue;   // только окно появления и итоговый кадр
#endif
    if (n >= bootSeqLen() || t >= BMK_LEN) t = BMK_LEN - 1;
    int ox = bmkFlow ? bmkSlotX(k) : bmkSlotX(k) + (bmkSW - m->w * bmkSc) / 2, oy = bmkFlow ? bmkSlotY(k) : bmkSlotY(k) + (bmkSH - m->h * bmkSc) / 2;
    bootMarkFrame(m, m->mode, t, ox, oy, BUI_BG, put, u);
  }
}

// Правка Роба 148 (01.10 10:5x): «всё мелко, реалистично, как будто настоящая системная штука загружается». Экран — текстовый
// POST старого BIOS (Award/Phoenix): производитель и копирайт, процессор, счёт памяти, устройства, «Loading…», ряд мелких 1-битных
// значков (bootmarks.h), подсказка START внизу и строка-идентификатор BIOS. Шрифт — системный PSPLink 8×8, строка 12 px.
// Правка 169 (01.10 ~14:00: «зачем убрал мой логотип GONE Makers? Он большой, больше всех, остальные под ним мелкие»): знак снова
// на экране — слева вверху 1:1 (115×39, самый крупный), заголовок справа от него, POST и значки ниже (BOOT_GONE_MARK 0 — без знака)
#ifndef BOOT_GONE_MARK
#define BOOT_GONE_MARK 1
#endif
static void bsqLine(BuiPut put, void *u, int row, unsigned c, const char *s) {   // строка слева, хвост прежней — фоном
  int y = BUI_Y0 + row * BUI_CH, x = buiTextX(put, u, BUI_X0, y, c, s); buiClear(put, u, x, BUI_TEXT_R, y);
}
// строка проверки: ключ (серый) | значение (светлое) | статус справа у края колонки (OK белым, NONE красным)
#define BSQ_VAL_X (BUI_X0 + 58)
static void bsqCheck(BuiPut put, void *u, int row, const char *key, const char *val, const char *st, unsigned sc) {
  int y = BUI_Y0 + row * BUI_CH, x = buiTextX(put, u, BUI_X0, y, BUI_KEY, key);
  buiClear(put, u, x, BSQ_VAL_X, y); x = buiTextX(put, u, BSQ_VAL_X, y, BUI_VAL, val);
  int sw = st ? buiTextW(st) : 0, sx = BUI_TEXT_R - sw; if (sx < x + 4) sx = x + 4;
  buiClear(put, u, x, sx, y); if (st) x = buiTextX(put, u, sx, y, sc, st); buiClear(put, u, x, BUI_TEXT_R, y);
}
static void bsqK(char *b, int n, const char *pre, int v, const char *post) {   // 31234 -> 31.2k
  if (v >= 10000) snprintf(b, n, "%s%d.%dk%s", pre, v / 1000, v % 1000 / 100, post); else snprintf(b, n, "%s%d%s", pre, v, post);
}
// Роб 17.10 вечер: «зачем там PSP 2000? нужна информация, сколько часов на тесты, сколько конвертнули, что пропатчили, свет, прорисовка
// дальности — как будто реально загружаются вещи, с которыми мы работали». Верхний блок — наша работа, числа настоящие (пересчитаны 20.10 по просьбе Роба
// «чтобы на загрузочном экране счётчик был актуален»; 17.10: 2077 прогонов, 24 ч, 741 правка, сборка 31):
//   DISC  18754 — файлов распакованного PS2-диска Роба (hit-and-run-web/source/game), из них собраны пакеты PSP
//   TESTS 2575 — прогонов: 1004 в каталоге близнецов (knowledge/twins_library/КАТАЛОГ_ПРОГОНОВ.tsv) + 1170 с журналом в build/smoke
//         всех 76 рабочих копий + 349 в build/emu_runs + 52 туров на PSP (build/psp-tour); 130 записей PS2 (ЗАПИСИ_PS2) — эталоны (пересчёт 22.10 вечер)
//   HOURS 38 — игровых часов в журналах прогонов, что ещё лежат на диске (последнее время t= журнала: smoke 26,5 ч, эмулятор 6,1 ч, туры PSP 5,9 ч;
//         нижняя граница: старые прогоны удалены)
//   FIXES 862 — правок Роба (docs/handoff/ПРАВКИ_РОБА.md, номер последней), за 46 тестовых сборок (TEST1–TEST46, номер сборки — в нижней строке)
//   LIGHT / DRAW — закон света (конус к камере, пятно на полу, тени 20 %) и даль (дальние LOD, HLOD, горизонт)
// Строки железа (CPU, MEMORY…) и «PSP-2000» — BOOT_WORK_OLD=1
#ifndef BOOT_WORK_OLD
#define BOOT_WORK_OLD 0
#endif
#define BOOT_W_DISC 18754
#define BOOT_W_RUNS 4434   // TEST78: 4394 (TEST77) + 38 журналов smoke.txt после сборки TEST77 (28.10 14:15, №1065–1067, ревизия) + 2 тура на PSP (TEST76, TEST77); счёт — нижняя граница
#define BOOT_W_PS2REC 130
#define BOOT_W_HOURS 51   // TEST70 (№988): пересчёт 26.10 — сумма последнего « t=» по 2287 журналам smoke.txt на диске (без копий): smoke 43,3 ч, туры PSP 7,5 ч, эмулятор 0,5 ч; нижняя граница
#define BOOT_W_FIXES 1067   // TEST78: номер последней строки ПРАВКИ_РОБА.md
#define BOOT_W_BUILDS 78
// №988 (Роб 26.10: «вся инфа, сколько мы сожгли токенов»): журналы Claude Code этого проекта 26.09–26.10 (~/.claude/projects/*shar*, ответы без повторов):
// 66 сессий, 51 882 ответа, 18,69 млрд токенов (из них 18,48 млрд — чтение кэша, написано 34,5 млн); журналы старше 30 дней стёрты — нижняя граница.
// Строка встаёт после FIXES; чтобы «Loading…» осталась над «Press START», убран пустой ряд перед ней. BOOT_AI988_OLD=1 — как TEST69
#ifndef BOOT_AI988_OLD
#define BOOT_AI988_OLD 0
#endif
// №1027 (Роб 26.10 ночь: «вначале написано BIOS, GONE Makers, но наш движок называется SHAR PSP; пусть там не BIOS, а именно SHAR PSP, и ссылка на GitHub — связать все детали»):
// первая строка — «SHAR PSP engine, build N», нижняя — github.com/robvagin/shar-psp-engine (вместо кода сборки). Копирайт GONE Makers остаётся второй строкой. BOOTNAME1027_OLD=1 — как в TEST71
#ifndef BOOTNAME1027_OLD
#define BOOTNAME1027_OLD 0
#endif
#define BOOT_W_TOKENS_DM 187   // десятые доли миллиарда: 18,7 млрд
#define BOOT_W_SESSIONS 66
static void bootSeqDraw(int n, const BootSys *s, BuiPut put, void *u) {
  char b[80], c[40];
  if (n <= 0) {   // первый кадр: производитель и копирайт слева вверху, подсказка и идентификатор внизу — как у BIOS сразу при включении
    if (BOOTNAME1027_OLD) bsqLine(put, u, 0, BUI_HI, "GONE Makers BIOS v1.02");
    else { char t_[48]; snprintf(t_, sizeof t_, "SHAR PSP engine, build %d", BOOT_W_BUILDS); bsqLine(put, u, 0, BUI_HI, t_); }   // №1027: не BIOS, а наш движок
    bsqLine(put, u, 1, BUI_KEY, BOOT_WORK_OLD ? "(C) 2026 GONE Makers Co. PSP-2000" : BOOTNAME1027_OLD ? "(C) 2026 GONE Makers Co." : "PSP port of Simpsons Hit & Run");   // №1027: вторая строка тоже про SHAR PSP (знак GONE Makers справа)
    bsqLine(put, u, BUI_ROW_FOOT - 1, BUI_KEY, "Press START to skip");
    bsqLine(put, u, BUI_ROW_FOOT, BUI_DIM, BOOT_WORK_OLD ? "10/03/26-PSP2000-GONEMAKERS-SHAR-01" : BOOTNAME1027_OLD ? "10/26/26-GONEMAKERS-SHAR-PORT-72" : "github.com/robvagin/shar-psp-engine");   // №1027: внизу — где лежит код
  }
  if (BOOT_GONE_MARK) {   // знак справа вверху: первый проход, пауза, второй проход, покой
    int p1 = BSQ_P0 + BSQ_PAUSE;
    if (n < BSQ_P0) bootAnimFrame(0, -BOOTANIM_WIDTH + n * BOOTANIM_SPEED, BUI_LOGO_X, BUI_LOGO_Y, BUI_BG, put, u);
    else if (n < p1) bootAnimFrame(0, 1e9f, BUI_LOGO_X, BUI_LOGO_Y, BUI_BG, put, u);
    else if (n < p1 + BSQ_P0) bootAnimFrame(1, -BOOTANIM_WIDTH + (n - p1) * BOOTANIM_SPEED, BUI_LOGO_X, BUI_LOGO_Y, BUI_BG, put, u);
    else bootAnimFrame(1, 1e9f, BUI_LOGO_X, BUI_LOGO_Y, BUI_BG, put, u);
  }
  int r = BSQ_ROW0;
  if (!BOOT_WORK_OLD) {   // Роб 17.10: вместо железа — наша работа над портом, тем же ходом строк (счёт прогонов набегает, как память)
    if (n >= BSQ_CPU) { snprintf(b, sizeof b, "%d PS2 files converted", BOOT_W_DISC); bsqCheck(put, u, r, "DISC", b, "OK", BUI_ACC); }
    r++;
    if (n >= BSQ_MEM0) {
      int k = n >= BSQ_MEM1 ? BOOT_W_RUNS : (int)((long)BOOT_W_RUNS * (n - BSQ_MEM0) / (BSQ_MEM1 - BSQ_MEM0));
      snprintf(b, sizeof b, "%d runs, %d PS2 refs", k, BOOT_W_PS2REC); bsqCheck(put, u, r, "TESTS", b, n >= BSQ_MEM1 ? "OK" : 0, BUI_ACC);
    }
    r++;
    if (n >= BSQ_VIDEO) { snprintf(b, sizeof b, "%dh+ of logged play", BOOT_W_HOURS); bsqCheck(put, u, r, "HOURS", b, "OK", BUI_ACC); }
    r++;
    if (n >= BSQ_MEDIA) { snprintf(b, sizeof b, "%d notes, %d builds", BOOT_W_FIXES, BOOT_W_BUILDS); bsqCheck(put, u, r, "FIXES", b, "OK", BUI_ACC); }
    r++;
    if (!BOOT_AI988_OLD) {   // №988: токены — между FIXES и LIGHT, LIGHT и DRAW сдвигаются на 6 и 4 кадра (до строк данных BSQ_DATA0)
      if (n >= BSQ_AUDIO) { snprintf(b, sizeof b, "%d.%dB tokens, %d sessions", BOOT_W_TOKENS_DM / 10, BOOT_W_TOKENS_DM % 10, BOOT_W_SESSIONS); bsqCheck(put, u, r, "AI", b, "OK", BUI_ACC); }
      r++;
    }
    if (n >= (BOOT_AI988_OLD ? BSQ_AUDIO : BSQ_POWER)) bsqCheck(put, u, r, "LIGHT", "cones, floor pools, shade", "OK", BUI_ACC);
    r++;
    if (n >= (BOOT_AI988_OLD ? BSQ_POWER : BSQ_POWER + 4)) bsqCheck(put, u, r, "DRAW", "far LOD, HLOD, horizon", "OK", BUI_ACC);
    r += 2;
  } else {
  // железо: настоящие значения PSP
  if (n >= BSQ_CPU) { snprintf(b, sizeof b, "ALLEGREX %dMHz bus %d", s->mhz, s->busMhz); bsqCheck(put, u, r, "CPU", b, "OK", BUI_ACC); }
  r++;
  if (n >= BSQ_MEM0) {
    int k = n >= BSQ_MEM1 ? s->memK : (int)((long)s->memK * (n - BSQ_MEM0) / (BSQ_MEM1 - BSQ_MEM0)) / 64 * 64;
    snprintf(b, sizeof b, "%dK free", k); bsqCheck(put, u, r, "MEMORY", b, n >= BSQ_MEM1 ? "OK" : 0, BUI_ACC);
  }
  r++;
  if (n >= BSQ_VIDEO) { snprintf(b, sizeof b, "GE eDRAM %dK 480x272", s->vramK ? s->vramK : 2048); bsqCheck(put, u, r, "VIDEO", b, "OK", BUI_ACC); }
  r++;
  if (n >= BSQ_MEDIA) {
    if (s->nFiles) snprintf(b, sizeof b, "%s %d files %uMB", s->media, s->nFiles, s->mediaMB); else snprintf(b, sizeof b, "%s Memory Stick", s->media);
    bsqCheck(put, u, r, "STORAGE", b, "OK", BUI_ACC);
  }
  r++;
  if (n >= BSQ_AUDIO) {
    snprintf(b, sizeof b, "speaker %dHz", s->beepHz);
    bsqCheck(put, u, r, "AUDIO", b, s->audioCh >= 0 ? "OK" : s->audioCh == -1 ? "NONE" : "..", s->audioCh == -1 ? BUI_ERR : BUI_ACC);
  }
  r++;
  if (n >= BSQ_POWER) { if (s->batt >= 0) snprintf(b, sizeof b, "battery %d%%", s->batt); else snprintf(b, sizeof b, "AC adapter");
                        bsqCheck(put, u, r, "POWER", b, 0, BUI_ACC); }
  r += 2;
  }
  // данные игры: что сделано под PSP, числа из файлов (bootDataScan); нет файла — строки нет
  int d = BSQ_DATA0;
  if (n >= d) bsqLine(put, u, r - 1, BUI_DIM, "SHAR data, built for PSP:");
  if (s->ntex && n >= d) { snprintf(b, sizeof b, "%d CLUT8 swizzle+mip", s->ntex); bsqCheck(put, u, r, "TEXTURES", b, "OK", BUI_ACC); }
  if (s->ntex) r++;
  d += BSQ_DATA_STEP;
  if (s->nchunk && n >= d) { snprintf(b, sizeof b, "%d chunks near+far LOD", s->nchunk); bsqCheck(put, u, r, "GEOMETRY", b, "OK", BUI_ACC); }
  if (s->nchunk) r++;
  d += BSQ_DATA_STEP;
  if (s->pvsW && n >= d) { snprintf(b, sizeof b, "PVS %dx%d cells %dm", s->pvsW, s->pvsH, s->pvsCell); bsqCheck(put, u, r, "VISIBILITY", b, "OK", BUI_ACC); }
  if (s->pvsW) r++;
  d += BSQ_DATA_STEP;
  if (s->colTri && n >= d) { bsqK(c, sizeof c, "", s->colTri, " tri "); snprintf(b, sizeof b, "%s%d box %d cap", c, s->colBox, s->colCyl);
                             bsqCheck(put, u, r, "COLLISION", b, "OK", BUI_ACC); }
  if (s->colTri) r++;
  d += BSQ_DATA_STEP;
  if (s->trfModel && n >= d) { snprintf(b, sizeof b, "%d cars %d peds %d roads", s->trfModel, s->trfPed, s->trfRoad); bsqCheck(put, u, r, "TRAFFIC", b, "OK", BUI_ACC); }
  if (s->trfModel) r++;
  d += BSQ_DATA_STEP;
  if (s->sfxRes && n >= d) { snprintf(b, sizeof b, "%d fx %d streams VAG 1:1", s->sfxRes, s->musTrack); bsqCheck(put, u, r, "SOUND", b, "OK", BUI_ACC); }   // PS-ADPCM диска бит в бит (audio.c)
  if (s->sfxRes) r++;
  d += BSQ_DATA_STEP;
  if (s->nMovie && n >= d) { snprintf(b, sizeof b, "%d FMV %uMB JPEG on ME", s->nMovie, s->movieMB); bsqCheck(put, u, r, "MOVIES", b, "OK", BUI_ACC); }
  if (s->nMovie) r++;
  if (BOOT_WORK_OLD || BOOT_AI988_OLD) r++;   // №988: в строках работы на одну больше — без пустого ряда перед «Loading…»
  if (n >= BSQ_RULE && r < BUI_ROW_FOOT - 1) bsqLine(put, u, r, BUI_KEY, "Loading The Simpsons Hit & Run ...");
  bootSeqMarksDraw(n, put, u);
}
#endif
