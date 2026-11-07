// Ловушка вылета SHARVIEW (аудит ведущего 01.10 22:35, обрыв 22:26 на a95a30cd: ящик без STALL, встал весь процессор).
// Модуль ядра sharexc.prx рядом с EBOOT; main.c грузит его первым делом (kuKernelLoadModule ARK-4) и передаёт папку игры.
// Исключение процессора в любом потоке -> строка «CRASH …» в конец blackbox.txt (кольцо ящика пишется с нуля фиксированной
// длиной BB_FILE, хвост за ним не затирается) и в crash.txt: epc (адрес команды), ra, badvaddr (адрес ошибки), cause,
// база модуля SHARVIEW (EBOOT — PRX, адрес плавает) и смещения от неё, цепочка вызовов, поток. Дальше упавший поток
// спит: сторож bbdog через 5 с пишет «STALL» (как при зависании) — CRASH остаётся хвостом.
// Строка кода: tools/crash_addr.py build/crash/<папка> (psp-addr2line по ELF выпуска, build/crash/elf/<EBOOT>.elf)
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <psploadcore.h>
#include <pspthreadman_kernel.h>
#include <string.h>
#include <stdarg.h>

PSP_MODULE_INFO("sharexc", 0x1006, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

static char gDir[200];
static char gLine[640];
static volatile int gHit;
// №1208 (слово Роба: красный экран заменить картинкой). Движок за строкой папки в args кладёт расширение: u32 'SHX1', u32 адрес картинки, u32 размер, u32 адрес строки последнего шага (bootmark),
// u32 адрес строки сборки. Картинка crashpic.shx: 'SHXC', u16 w, u16 h, u16 ncolors (<=256), u16 0, палитра ncolors×4 байта (R,G,B,A), затем w×h индексов построчно сверху вниз.
// Рисует её обработчик ПРОЦЕССОРОМ в текущий кадровый буфер на весь экран (ближайший сосед), потом код ошибки мелким шрифтом в нижней полосе (строки 176–271 экрана); без sceIo.
// Нет картинки / расширения (старый движок) — прежний красный текстовый экран. SHAREXC_OLD=1 — всё как до №1192.
static volatile int *gOutTid; static char *gOutBuf; static unsigned gOutCap;   // №1192: поток записи создаёт ИГРА (в модуле его создание в module_start вешало старт: эмулятор, TEST90), адреса — в args
static const unsigned char *gPic; static unsigned gPicN; static const char *gStep, *gBuild; static unsigned gPicRect[4];   // кадр от игры (рамка с диска + картинка) и его code_rect

// своя мелкая запись строк: libc ядра без printf (pspDebugDumpException тянет vsnprintf — заглушка, экран не используем)
int vsnprintf(char *b, size_t n, const char *f, va_list a) { if (n) b[0] = 0; return 0; }
static char *ps(char *p, const char *s) { while (*s && p < gLine + sizeof gLine - 2) *p++ = *s++; *p = 0; return p; }
static char *px(char *p, unsigned v) { char t[9]; for (int i = 7; i >= 0; i--, v >>= 4) t[i] = "0123456789abcdef"[v & 15]; t[8] = 0; return ps(p, t); }
static char *pd(char *p, unsigned v) { char t[12]; int i = 11; t[i] = 0; do { t[--i] = (char)('0' + v % 10); v /= 10; } while (v); return ps(p, t + i); }

static void __attribute__((unused)) put(const char *name, int append) {   // только прежний путь SHAREXC_OLD
  char fn[256]; unsigned l = strlen(gDir); memcpy(fn, gDir, l); strcpy(fn + l, name);
  SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | (append ? PSP_O_APPEND : PSP_O_TRUNC), 0777);
  if (f >= 0) { sceIoWrite(f, gLine, strlen(gLine)); sceIoClose(f); }
}

#ifndef SHAREXC_OLD
#define SHAREXC_OLD 0   // 1 — прежний порядок (экран, запись на карту и вечный sceKernelDelayThread прямо в контексте исключения)
#endif
// №1192 (разбор arch-fable ПЕРЕХОД_УРОВНЯ: «обработчик зовёт pspDebugScreenInit, sceIo* и вечный DelayThread внутри контекста исключения; на железе ни одного CRASH не ловили»).
// Новый порядок: в контексте исключения — только строка в памяти и рисунок в кадровый буфер ПРОЦЕССОРОМ (без смены режима дисплея, без sceIo); потом будится поток sharexc_w
// (приоритет выше игровых), он пишет blackbox.txt и crash.txt в обычном контексте потока; упавший поток крутит пустой цикл (ни одного системного вызова, как DelayThread).
// Зависшая запись (замок карты у упавшего потока) прячет только поток записи, экран уже показан. SHAREXC_OLD=1 — как было.
#if !SHAREXC_OLD
static char gCode1[24], gCode2[24];   // код для картинки: две строки до 15 знаков (сетка 8x8 в code_rect): «E4 27C704 0137» (тип, смещение, сборка ЧЧММ) и последний шаг
#include "crashpic.h"
#include "crashpic_data.h"   // gEmbPic, gEmbPicN, gEmbPicTag (tools/crashpic_embed.py): картинка вшита в модуль
static void screenCpu(void) {   // картинка (если есть) или красный экран со строкой CRASH — прямо в текущий кадровый буфер, без sceDisplaySet*
  void *top = 0; int bw = 0, fmt = 0;
  if (sceDisplayGetFrameBuf(&top, &bw, &fmt, PSP_DISPLAY_SETBUF_IMMEDIATE) < 0 || !top) return;
  void *vram = (void *)(((unsigned)top & 0x0FFFFFFFu) | 0x40000000u);
  pspDebugScreenInitEx(vram, fmt, 0);
  // порядок по слову Роба (№1208в): кадр от игры (рамка с диска + картинка) -> вшитая картинка без рамки -> текстовый экран
  const unsigned char *pic = gPic && crashPicSize(gPic, gPicN) ? gPic : gEmbPic; unsigned pn = pic == gPic ? gPicN : gEmbPicN;
  int havePic = pic && crashPicSize(pic, pn);
  if (havePic) { crashPicDraw(pic, pn, vram, bw ? bw : 512, fmt);
    { const unsigned *rc = gEmbRect;
      int bpp = fmt == 3 ? 4 : 2; unsigned B = (unsigned)(bw ? bw : 512);
      if (pic != gEmbPic) rc = gPicRect[2] ? gPicRect : gEmbRect;
      pspDebugScreenEnableBackColor(0);   // фон под буквами — картинка: без заливки клетки
      for (int pass = 0; pass < 2; pass++) { pspDebugScreenSetTextColor(pass ? 0xffffffffu : 0xff000000u);
        for (int ln = 0; ln < 2; ln++) { unsigned x = rc[0] + (unsigned)(pass ^ 1), y = rc[1] + (unsigned)ln * 9u + (unsigned)(pass ^ 1);   // тень чёрным на 1 пиксель правее и ниже
          pspDebugScreenSetOffset((int)((y * B + x) * (unsigned)bpp)); pspDebugScreenSetXY(0, 0); pspDebugScreenPuts(ln ? gCode2 : gCode1); } } }
    return; }
  pspDebugScreenSetBackColor(0xff0000a0); pspDebugScreenSetTextColor(0xffffffff); pspDebugScreenClear();
  pspDebugScreenPuts("SHARVIEW CRASH - PHOTO THIS SCREEN, THEN HOLD POWER\n"); pspDebugScreenPuts(gLine);
}
#endif
static void onExc(PspDebugRegBlock *r) {
  if (!gHit++) {
    SceModule *m = sceKernelFindModuleByName("SHARVIEW");
    unsigned base = m ? m->text_addr : 0, sz = m ? m->text_size : 0;
    char tn[32] = "?"; SceKernelThreadInfo ti; memset(&ti, 0, sizeof ti); ti.size = sizeof ti;
    if (sceKernelReferThreadStatus(sceKernelGetThreadId(), &ti) >= 0) { strncpy(tn, ti.name, sizeof tn - 1); tn[sizeof tn - 1] = 0; }
    unsigned ec = (r->cause >> 2) & 31;
    char *p = ps(gLine, "\nCRASH exc="); p = pd(p, ec);
    p = ps(p, " epc="); p = px(p, r->epc); p = ps(p, " ra="); p = px(p, r->r[31]); p = ps(p, " badv="); p = px(p, r->badvaddr);
    p = ps(p, " cause="); p = px(p, r->cause); p = ps(p, " sp="); p = px(p, r->r[29]); p = ps(p, " base="); p = px(p, base);
    p = ps(p, " size="); p = px(p, sz); p = ps(p, " off_epc="); p = px(p, r->epc - base); p = ps(p, " off_ra="); p = px(p, r->r[31] - base);
    p = ps(p, " thread="); p = ps(p, tn); p = ps(p, " trace");
    PspDebugStackTrace tr[8]; int k = pspDebugGetStackTrace2(r, tr, 8);
    for (int i = 0; i < k; i++) { p = ps(p, " "); p = px(p, tr[i].call_addr - base); }
    ps(p, "\n");
#if !SHAREXC_OLD
    { char *q = gCode1; const char *t;
      *q++ = 'E'; { char tmp[12]; unsigned v = ec; int n = 0; do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *q++ = tmp[--n]; }
      *q++ = ' '; for (int i = 5; i >= 0; i--) *q++ = "0123456789abcdef"[((r->epc - base) >> (4 * i)) & 15u];   // смещение epc от базы модуля (6 знаков: текст < 16 МБ)
      *q++ = ' '; if (gBuild && gBuild[0] && strlen(gBuild) >= 17) { *q++ = gBuild[12]; *q++ = gBuild[13]; *q++ = gBuild[15]; *q++ = gBuild[16]; }   // время сборки ЧЧММ из «Nov  5 2026 01:37:55»
      *q = 0; q = gCode2; unsigned k = 0; for (t = gStep ? gStep : "-"; *t && k < 15; k++) *q++ = *t++; *q = 0;
    }
#endif
#if SHAREXC_OLD
    // экран — первым (запись на карту из исключения может не пройти): красный фон, строка CRASH — сфоткать (03.10, правка 252)
    pspDebugScreenInit(); pspDebugScreenSetBackColor(0xff0000a0); pspDebugScreenSetTextColor(0xffffffff); pspDebugScreenClear();
    pspDebugScreenPuts("SHARVIEW CRASH - PHOTO THIS SCREEN, THEN HOLD POWER\n"); pspDebugScreenPuts(gLine);
    put("blackbox.txt", 1); put("crash.txt", 0);
#else
    screenCpu();
    if (gOutTid && gOutBuf && gOutCap > 8 && *gOutTid >= 0) { unsigned k = 0; for (; gLine[k] && k < gOutCap - 1; k++) gOutBuf[k] = gLine[k]; gOutBuf[k] = 0; sceKernelWakeupThread(*gOutTid); }   // запись crash.txt и ящика — в потоке игры
#endif
  }
#if SHAREXC_OLD
  for (;;) sceKernelDelayThread(1000000);
#else
  for (;;) { __asm__ volatile("nop"); }
#endif
}

int module_start(SceSize args, void *argp) {
#ifdef SHAREXC_HANGTEST
  for (;;) sceKernelDelayThread(1000000);   // красный контроль сторожа старта в игре (tests/g10b/kexcguard.sh): модуль, который не возвращается
#endif
#if !SHAREXC_OLD
  (void)*(volatile const char *)gEmbPicTag;   // метка картинки остаётся в .rodata (гейт сборки читает её из PRX)
#endif
  if (args > 0 && argp) { unsigned l = args < sizeof gDir ? args : sizeof gDir - 1; memcpy(gDir, argp, l); gDir[l] = 0;
    unsigned dl = (unsigned)strlen(gDir) + 1, off = (dl + 3u) & ~3u;   // расширение «SHX1» за строкой папки (№1208)
    if (args >= off + 20) { const unsigned *x = (const unsigned *)((const char *)argp + off);
      if (x[0] == 0x31584853u) { gPic = (const unsigned char *)x[1]; gPicN = x[2]; gStep = (const char *)x[3]; gBuild = (const char *)x[4];
        if (args >= off + 36) { gPicRect[0] = x[5]; gPicRect[1] = x[6]; gPicRect[2] = x[7]; gPicRect[3] = x[8]; }
        if (args >= off + 48) { gOutTid = (volatile int *)x[9]; gOutBuf = (char *)x[10]; gOutCap = x[11]; } } } }
  pspDebugInstallErrorHandler(onExc);
  return 0;
}
int module_stop(SceSize args, void *argp) { return 0; }
