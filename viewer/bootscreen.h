// Загрузочный экран «как у системы» (Роб 27–28.09): логотип GONE Makers Co. в правом верхнем углу, как знак на экране
// старого BIOS, слева настоящие проверки железа, в первый кадр — писк включения (viewer/bootbeep.h). Единственная
// авторская часть бренда; всё остальное — как в исходной игре. START — пропустить. Рисует pspDebugScreen (до sceGuInit).
// Ход по кадрам — viewer/bootseq.h (общий с превью на Маке), знак — viewer/bootanim.h, сетка и шрифт — viewer/bootui.h.
// Служебные файлы рядом с EBOOT (только для tools/boot_gate.sh):
//   bootframes — снимки кадров появления: строка «SHOT boot_fNNN» в smoke.txt, ждём shot_boot_fNNN.ok (как smoke.h), до 5 с
//   nobeep     — отрицательный контроль гейта: звук не играет (строка BOOTSND покажет ch=-1 out=0)
//   bootslow   — отрицательный контроль гейта первого кадра: 0,5 с «ранних чтений» до первого кадра (BOOTT first_ms)
// Правка Роба 28 (видео 0:37–0:46: белый логотип PSP ~9 с до нашего экрана): первый кадр заставки (чёрный экран, заголовок)
// — bootFirst, самым первым делом в main, до смены частоты процессора, опроса батареи и чтений с карты. Строка BOOTT —
// время от входа в main до первого кадра и до конца заставки (tools/boot_check.py). Остальное до main — прошивка
// (заставка PSP при запуске и загрузка модуля), движок её не укорачивает.
// Строки BOOTSND / BOOTSEQ для smoke.txt отдаёт bootReport (main.c пишет их после smokeInit).
#ifndef BOOTSCREEN_H
#define BOOTSCREEN_H
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psppower.h>
#include <pspsysmem.h>
#include <pspiofilemgr.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stddef.h>
#include <unistd.h>
#include <malloc.h>
#include <pspaudio.h>
#include <pspge.h>

static int bootSkip;
static void bootPut(unsigned *fb, int x, int y, unsigned c) { if (x >= 0 && x < 480 && y >= 0 && y < 272) fb[y * 512 + x] = c; }
#include "bootseq.h"    // ход экрана по кадрам, сетка 8×8 и шрифт PSPSDK msx (bootui.h, правка 125), знак (bootanim.h)
#include "bootbeep.h"   // звук включения: кнопка, кулер, бип POST (правка 128)
static void bootFbPut(void *u, int x, int y, unsigned c) { (void)u; bootPut((unsigned *)0x44000000, x, y, c); }
#define BP bootFbPut, 0

static void bootDelay(int ms) {   // пауза; START — сразу дальше
  SceCtrlData p;
  for (int t = 0; t < ms && !bootSkip; t += 4) {
    sceCtrlPeekBufferPositive(&p, 1); if (p.Buttons & PSP_CTRL_START) bootSkip = 1; else sceKernelDelayThread(4000);
  }
}
// Правка Роба 170 (01.10: «дисклеймер: фанатская сборка just for fun, денег не брал, ничего не крал, не продаётся; игре 20+ лет и на
// PSP её нет; любые совпадения случайны; международно»): страница после POST тем же системным шрифтом, по центру, 5 с или до НОВОГО
// нажатия START/✕ (зажатый с POST START её не пропускает). BOOT_NOTICE 0 или файл nonotice рядом с EBOOT — без страницы
#ifndef BOOT_NOTICE
#define BOOT_NOTICE 1
#endif
#define BOOT_NOTICE_MS 5000
// Роб 17.10 вечер: под словом NOTICE — воздух (пустая строка; раньше текст шёл вплотную), и кто и где сделал: по своей инициативе, руками веб-кодеров,
// в Киеве между отключениями света и воздушными тревогами. BOOT_NOTICE_OLD=1 — прежний текст
#ifndef BOOT_NOTICE_OLD
#define BOOT_NOTICE_OLD 0
#endif
static const char *const bootNoticeOld[] = {
  "NOTICE",
  "This is an unofficial, non-commercial fan project.",
  "It is free and must never be sold.",
  "",
  "Not affiliated with or endorsed by 20th Century Studios,",
  "Disney, Gracie Films, Matt Groening, Radical",
  "Entertainment, Vivendi Universal Games, Activision",
  "or Sony Interactive Entertainment.",
  "",
  "The Simpsons and The Simpsons: Hit & Run are",
  "trademarks of their respective owners.",
  "No original source code was used: the PSP engine was",
  "written from scratch to preserve a 2003 game that",
  "never came to PSP. Please own an original copy.",
  "",
  "All characters and events are fictitious. Any",
  "resemblance to real persons, living or dead,",
  "or to Springfield, is purely coincidental.",
};
static const char *const bootNoticeNew[] = {
  "NOTICE",
  "",
  "This is an unofficial, non-commercial fan project,",
  "made on our own initiative by a few web coders",
  "in Kyiv, between blackouts and air raid alerts.",
  "It is free and must never be sold.",
  "",
  "Not affiliated with or endorsed by 20th Century Studios,",
  "Disney, Gracie Films, Matt Groening, Radical",
  "Entertainment, Vivendi Universal Games, Activision",
  "or Sony Interactive Entertainment.",
  "",
  "The Simpsons and The Simpsons: Hit & Run are",
  "trademarks of their respective owners.",
  "No original source code was used: the PSP engine was",
  "written from scratch to preserve a 2003 game that",
  "never came to PSP. Please own an original copy.",
  "",
  "All characters and events are fictitious. Any",
  "resemblance to real persons, living or dead,",
  "or to Springfield, is purely coincidental.",
};
#define bootNotice (BOOT_NOTICE_OLD ? bootNoticeOld : bootNoticeNew)
#define BOOT_NOTICE_N (BOOT_NOTICE_OLD ? (int)(sizeof bootNoticeOld / sizeof *bootNoticeOld) : (int)(sizeof bootNoticeNew / sizeof *bootNoticeNew))
// Роб 18.10: «заголовок NOTICE капельку ниже, на 2 пикселя, ближе к текстовым строкам». NOTICE_TITLE_DY_OLD=1 — как было
// №832 (Роб 18.10, повтор после TEST35: «заголовок ещё на 2 пикселя ниже, ближе к текстовым блокам»): всего на 4 px. NOTICE_TITLE_DY4_OLD=1 — 2 px, как TEST35
#ifndef NOTICE_TITLE_DY4_OLD
#define NOTICE_TITLE_DY4_OLD 0
#endif
#ifndef NOTICE_TITLE_DY_OLD
#define NOTICE_TITLE_DY_OLD 0
#endif
static void bootNoticeDraw(BuiPut put, void *u) {   // всё поле — фон, строки по центру (шаг знаков по чернилам, правка 203)
  for (int y = 0; y < BUI_H; y++) for (int x = 0; x < BUI_W; x++) put(u, x, y, BUI_BG);
  int rows = (BUI_H - 2 * BUI_Y0) / BUI_CH, r0 = (rows - BOOT_NOTICE_N) / 2;
  for (int i = 0; i < BOOT_NOTICE_N; i++) {
    if (!bootNotice[i][0]) continue;
    buiTextX(put, u, (BUI_W - buiTextW(bootNotice[i])) / 2, BUI_Y0 + (r0 + i) * BUI_CH + (i == 0 && !NOTICE_TITLE_DY_OLD ? (NOTICE_TITLE_DY4_OLD ? 2 : 4) : 0), i == 0 ? BUI_HI : BUI_VAL, bootNotice[i]);
  }
}
static char bootDir[200];   // папка EBOOT (argv[0] до последнего '/')
static void bootSib(char *fn, const char *name) {   // fn[256]
  if (SHR_REL && shrDevName(name)) { strcpy(fn, SHR_OFF_DIR); strncat(fn, name, 200); return; }   // релиз (viewer/release.h)
  size_t l = strlen(bootDir); memcpy(fn, bootDir, l); strncpy(fn + l, name, 255 - l); fn[255] = 0;
}
static int bootHas(const char *name) { char fn[256]; SceIoStat st; bootSib(fn, name); return sceIoGetstat(fn, &st) >= 0; }
static int bootNoticeWait(int ms, int held) {   // до ms или НОВОГО нажатия START/✕ (зажатый с прошлой страницы — ждать отпускания); -> зажато ли сейчас
  SceCtrlData p;
  for (int t = 0; t < ms; t += 4) {
    sceCtrlPeekBufferPositive(&p, 1); int down = p.Buttons & (PSP_CTRL_START | PSP_CTRL_CROSS);
    if (!down) held = 0; else if (!held) return 1;
    sceKernelDelayThread(4000);
  }
  return held;
}
// Страница «шрифты» (11.10, слово Роба: два пиксельных шрифта Patrick H. Lauke, лицензия CC BY 3.0 требует указания автора): файл fontcredit.txt рядом с EBOOT пишет
// сборка (tools/final-build.sh), только если какая-то грань пакетов интерфейса — шрифт Lauke; строки уже по ширине экрана (до 52 знаков). Нет файла — страницы нет.
// FONTCREDIT_OLD=1 — без страницы
// Слово Роба 11.10: отдельную страницу при запуске убрать — указание авторов шрифтов теперь в Options → View Credits (tools/menu_export.py PORT_CREDITS); страница выключена, FONTCREDIT_OLD=0 включает её обратно
#ifndef FONTCREDIT_OLD
#define FONTCREDIT_OLD 1
#endif
#define BOOT_FONTCREDIT_MS 4000
static int bootFontCreditDraw(BuiPut put, void *u) {
  static char buf[704]; char fn[256]; bootSib(fn, "fontcredit.txt");
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return 0;
  int r = sceIoRead(f, buf, (int)sizeof buf - 1); sceIoClose(f); if (r <= 0) return 0; buf[r] = 0;
  const char *ln[16]; int n = 0; ln[n++] = "FONT CREDITS"; ln[n++] = "";
  for (char *q = buf; *q && n < 16;) { ln[n++] = q; char *e = strchr(q, '\n'); if (!e) break; *e = 0; q = e + 1; }
  for (int y = 0; y < BUI_H; y++) for (int x = 0; x < BUI_W; x++) put(u, x, y, BUI_BG);
  int rows = (BUI_H - 2 * BUI_Y0) / BUI_CH, r0 = (rows - n) / 2;
  for (int i = 0; i < n; i++) if (ln[i][0]) buiTextX(put, u, (BUI_W - buiTextW(ln[i])) / 2, BUI_Y0 + (r0 + i) * BUI_CH, i == 0 ? BUI_HI : BUI_VAL, ln[i]);
  return 1;
}
// правка 203: настоящие числа для строк данных — заголовки файлов рядом с EBOOT (несколько коротких чтений) и счёт файлов
static int bootRd(void *u, const char *name, unsigned off, void *buf, int n) {
  (void)u; char fn[256]; bootSib(fn, name); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return -1;
  int r = sceIoLseek32(f, (int)off, PSP_SEEK_SET) == (int)off ? sceIoRead(f, buf, n) : -1; sceIoClose(f); return r;
}
static void bootDirScan(const char *dir, const char *ext, int depth, int *count, unsigned long long *bytes) {
  SceUID d = sceIoDopen(dir); if (d < 0) return;
  SceIoDirent e; size_t el = strlen(ext);
  for (;;) {
    memset(&e, 0, sizeof e); if (sceIoDread(d, &e) <= 0) break;
    if (e.d_name[0] == '.') continue;
    if (FIO_S_ISDIR(e.d_stat.st_mode)) {
      if (depth > 0 && strcmp(e.d_name, "_archive")) { char sub[256]; snprintf(sub, sizeof sub, "%s%s/", dir, e.d_name); bootDirScan(sub, ext, depth - 1, count, bytes); }
      continue;
    }
    size_t nl = strlen(e.d_name); if (el && (nl < el || strcasecmp(e.d_name + nl - el, ext))) continue;   // UMD/эмулятор отдают имена ЗАГЛАВНЫМИ
    (*count)++; *bytes += (unsigned long long)e.d_stat.st_size;
  }
  sceIoDclose(d);
}
static void bootDirCount(void *u, const char *dir, const char *ext, int *count, unsigned *mb) {
  (void)u; char fn[256]; unsigned long long by = 0; *count = 0;
  if (dir[0]) { bootSib(fn, dir); size_t l = strlen(fn); if (l < 250) { fn[l] = '/'; fn[l + 1] = 0; } } else bootSib(fn, "");
  bootDirScan(fn, ext, dir[0] ? 0 : 1, count, &by); *mb = (unsigned)((by + (1u << 19)) >> 20);
}

// звук включения (правка 128): фоновый поток синтезирует партитуру viewer/bootbeep.h кусками по BOOT_SND_CHUNK в два
// буфера поочерёдно (пока играет один — заполняется другой; было 2 × 16 КБ BSS на всё время игры, стало 2 × 2 КБ),
// START — затухание за один кусок; в конце канал освобождается, чтобы игре и роликам хватило каналов
static short bootSndBuf[2][BOOT_SND_CHUNK] __attribute__((aligned(64)));
static volatile int bootBeepCh = -2, bootBeepOut, bootBeepRc, bootSndStopReq, bootSndDone, bootSndStopped, bootSndUs;
static int bootSndThread(SceSize args, void *argp) {
  (void)args; (void)argp;
  BootSnd st; bootSndInit(&st, BOOT_SND_VARIANT);
  int ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, BOOT_SND_CHUNK, PSP_AUDIO_FORMAT_MONO);
  if (ch < 0) { bootBeepRc = ch; bootBeepCh = -1; bootSndDone = 1; sceKernelExitDeleteThread(0); return 0; }
  bootBeepCh = ch;
  for (int k = 0;; k ^= 1) {
    if (bootSndStopReq && st.stopAt < 0) { bootSndStop(&st); bootSndStopped = 1; }
    unsigned t0 = sceKernelGetSystemTimeLow();
    int n = bootSndFill(&st, bootSndBuf[k], BOOT_SND_CHUNK);
    unsigned us = sceKernelGetSystemTimeLow() - t0; if ((int)us > bootSndUs) bootSndUs = (int)us;   // самый долгий кусок
    if (n <= 0) break;
    int rc = sceAudioOutputBlocking(ch, PSP_AUDIO_VOLUME_MAX, bootSndBuf[k]); if (rc < 0) { bootBeepRc = rc; break; }
    bootBeepOut += n;
  }
  for (int k = 0; k < 100 && sceAudioGetChannelRestLen(ch) > 0; k++) sceKernelDelayThread(4000);   // доиграл последний кусок
  sceAudioChRelease(ch); bootSndDone = 1;
  sceKernelExitDeleteThread(0); return 0;
}
static void bootBeep(void) {
  int th = sceKernelCreateThread("bootsnd", bootSndThread, 0x14, 0x2000, 0, 0);
  if (th >= 0) sceKernelStartThread(th, 0, 0); else { bootBeepCh = -1; bootSndDone = 1; }
}

// снимки кадров появления для tools/boot_gate.sh (файл bootframes рядом с EBOOT)
static int bootShotsOn, bootFrames;
static int bootShotFrame(int n) {
  int p1 = BSQ_P0 + BSQ_PAUSE;
  return n == 20 || n == 50 || n == 80 || n == BSQ_P0 - 1 || n == p1 + 40;
}
static void bootShot(const char *name) {
  char fn[256], l[48]; snprintf(l, sizeof l, "SHOT %s\n", name);
  bootSib(fn, "smoke.txt"); SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
  if (f >= 0) { sceIoWrite(f, l, strlen(l)); sceIoClose(f); }
  snprintf(l, sizeof l, "shot_%s.ok", name);
  for (int k = 0; k < 100 && !bootHas(l); k++) sceKernelDelayThread(50000);
}

// память игры: куча libc (PSP_HEAP_SIZE_KB(-1024) в main.c забирает почти всю память при старте, поэтому
// sceKernelTotalFreeMemSize один показывает лишь остаток ~1 МБ) — сколько ещё отдаёт sbrk (проба и сразу возврат, как
// heapFreeBytes в smoke.h; других потоков с malloc ещё нет) плюс свободное внутри malloc
static unsigned bootHeapFree(void) {
  unsigned lo = 0, hi = 64u << 20;
  while (hi - lo > (64u << 10)) { unsigned m = (lo + hi) / 2; void *q = sbrk((ptrdiff_t)m);
    if (q != (void *)-1) { sbrk(-(ptrdiff_t)m); lo = m; } else hi = m; }
  return lo + (unsigned)mallinfo().fordblks;
}
static unsigned bootTMain, bootTFirst, bootTEnd; static int bootFirstDone;
static void bootFirst(const char *argv0) {   // самый первый кадр: экран включён, заголовок заставки — до всего остального
  bootTMain = sceKernelGetSystemTimeLow();
  strncpy(bootDir, argv0 ? argv0 : "", sizeof bootDir - 1); { char *sl = strrchr(bootDir, '/'); if (sl) sl[1] = 0; else bootDir[0] = 0; }
  pspDebugScreenInit(); pspDebugScreenClear();   // экран и кадр; текст рисуем своим шрифтом (bootui.h)
  BootSys s0; memset(&s0, 0, sizeof s0); s0.audioCh = -2; bootSeqDraw(0, &s0, BP);
  bootTFirst = sceKernelGetSystemTimeLow(); bootFirstDone = 1;
}
// правка 127: знаки причастных — файл рядом с EBOOT (tools/bootmarks.py), в куче только на время заставки; нет файла или
// испорчен — экран без знаков, игра идёт (строка BOOTMARKS n=0 / n=-1 в smoke.txt)
static unsigned char *bootMarkData; static BootMarks bootMarkSet; static int bootMarkN = -2, bootMarkBytes;
// Правка Роба 148 (01.10 09:1x): плашки знаков с диска на заставке — «уродство», заставка — как старые загрузочные экраны BIOS.
// 10:2x Роб: «вставь логотипы, дай пиксель на них, всё мелкое» — значки снова включены, мелкие 1-битные (tools/bootmarks.py)
#ifndef BOOT_MARKS
#define BOOT_MARKS 1   // правка Роба 148, 10:2x: «просто вставь логотипы и дай пиксель на них» — мелкие пиксельные значки 64×24
#endif
static void bootMarksLoad(const char *name) {
  bootMarkN = 0; if (!BOOT_MARKS) return;
  char fn[256]; bootSib(fn, name); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);
  if (f < 0) return;
  int len = (int)sceIoLseek(f, 0, PSP_SEEK_END); sceIoLseek(f, 0, PSP_SEEK_SET);
  if (len > 16 && len <= 65536 && (bootMarkData = (unsigned char *)malloc(len)) != 0 && sceIoRead(f, bootMarkData, len) == len) {
    bootMarkBytes = len; bootMarkN = bootMarksParse(bootMarkData, len, &bootMarkSet); }
  sceIoClose(f);
  bmkFlow = 0;   // правка 257: раскладка потока считается заново для этого набора
  if (bootMarkN > 0) bootSeqMarks = &bootMarkSet; else { free(bootMarkData); bootMarkData = 0; }
}
static void bootBegin(const char *argv0, const char *marksFile) {
  if (!bootFirstDone) bootFirst(argv0);
  if (bootHas("elbarto1010old")) bootElbarto1010Old = 1;   // №1010: прежний тег El Barto (файл Роба), прежним цветом
  bootMarksLoad(marksFile);
  if (bootHas("bootslow")) { sceKernelDelayThread(500000); bootTFirst = sceKernelGetSystemTimeLow(); }   // отрицательный контроль
  bootShotsOn = bootHas("bootframes");
  if (bootShotsOn) { char fn[256]; bootSib(fn, "smoke.txt"); SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f >= 0) sceIoClose(f); }
  BootSys s; memset(&s, 0, sizeof s);
  s.mhz = scePowerGetCpuClockFrequency(); s.busMhz = scePowerGetBusClockFrequency();
  s.vramK = (int)(sceGeEdramGetSize() / 1024);
  bootDataScan(&s, bootRd, bootDirCount, 0);   // правка 203: настоящие числа данных игры — до первой строки проверок
  s.memK = (int)((bootHeapFree() + sceKernelTotalFreeMemSize()) / 1024); s.beepHz = BOOT_BEEP_HZ;
  if (bootHas("bootsnd1011old")) bootSnd1011Old = 1;   // №1011: прежняя партитура (щелчок, кулер, бип POST)
  if (bootHas("nobeep")) { bootBeepCh = -1; bootSndDone = 1; } else bootBeep();
  s.batt = scePowerIsBatteryExist() ? scePowerGetBatteryLifePercent() : -1; if (s.batt > 100) s.batt = 100;
  { int i = 0; for (; argv0 && argv0[i] && argv0[i] != ':' && i < 6; i++) s.media[i] = (char)toupper((unsigned char)argv0[i]);
    if (argv0 && argv0[i] == ':') { s.media[i] = ':'; s.media[i + 1] = 0; } else strcpy(s.media, "MS0:"); }
  int len = bootSeqLen();
  for (int n = 0; n < len && !bootSkip; n++) {
    unsigned tf = sceKernelGetSystemTimeLow();
    s.audioCh = bootBeepCh; bootSeqDraw(n, &s, BP); bootFrames = n + 1;
    if (bootShotsOn && bootShotFrame(n)) { char nm[24]; snprintf(nm, sizeof nm, "boot_f%03d", n); bootShot(nm); tf = sceKernelGetSystemTimeLow(); }
    int left = 16 - (int)((sceKernelGetSystemTimeLow() - tf) / 1000); if (left > 0) bootDelay(left);
  }
  for (int k = 0; k < 25 && bootBeepCh == -2; k++) sceKernelDelayThread(4000);   // канал звука уже известен
  bootSndStopReq = 1;   // START (или хвост партитуры): затухание за один кусок; канал свободен до лицензии и звука игры
  for (int k = 0; k < 50 && !bootSndDone; k++) sceKernelDelayThread(4000);
  s.audioCh = bootBeepCh; bootSeqDraw(len, &s, BP);   // итоговый вид (и после START): тот же, без прыжков
  bootTEnd = sceKernelGetSystemTimeLow();
  if (bootShotsOn) bootShot("boot_end");
  if (BOOT_NOTICE && !bootHas("nonotice")) {   // правка 170: дисклеймер
    bootNoticeDraw(BP); if (bootShotsOn) bootShot("boot_notice");
    int held = bootNoticeWait(BOOT_NOTICE_MS, 1);
    if (!FONTCREDIT_OLD && bootFontCreditDraw(BP)) {   // указание автора шрифтов (CC BY 3.0): вторая страница, только если пакет интерфейса использует шрифт Lauke
      if (bootShotsOn) bootShot("boot_fonts");
      bootNoticeWait(BOOT_FONTCREDIT_MS, held);
    }
  }
  bootSeqMarks = 0; free(bootMarkData); bootMarkData = 0;   // знаки больше не нужны: куча свободна до игры
}
// строки для smoke.txt (main.c после smokeInit): звук сыгран? ch — канал, out — сэмплов партитуры отдано в звук (полная
// длина n, если не остановлен), stop — остановлен START, synth_us — самый долгий кусок синтеза
static void bootReport(char *s, int n) {
  int k = snprintf(s, n, "BOOTMARKS n=%d bytes=%d\n", bootMarkN, bootMarkBytes); if (k < 0 || k >= n) return; s += k; n -= k;
  snprintf(s, n, "BOOTSND hz=%d var=%c tune=%d ms=%d n=%d beep_at=%d ch=%d out=%d rc=%d stop=%d done=%d synth_us=%d\nBOOTSEQ frames=%d len=%d skip=%d\nBOOTT first_ms=%d end_ms=%d\n",
           bootSnd1011Old ? BOOT_BEEP_HZ : (int)(BOOT_TUNE_HZ + .5f), bootSndName[BOOT_SND_VARIANT], !bootSnd1011Old, BOOT_SND_MS, BOOT_SND_N, bootSndBeepMs(BOOT_SND_VARIANT), bootBeepCh, bootBeepOut, bootBeepRc,
           bootSndStopped, bootSndDone, bootSndUs, bootFrames, bootSeqLen(), bootSkip,
           (int)((bootTFirst - bootTMain) / 1000), (int)((bootTEnd - bootTMain) / 1000));
}
#endif
