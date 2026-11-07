// bootmark.c — см. bootmark.h. Только sceIo, целые и строки, без кучи и без printf с плавающей точкой; вызывается из любого потока.
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <string.h>
#include "bootmark.h"
char bmLast[80];   // №1208: последний пройденный шаг — читает экран вылета (sharexc) по адресу
static char bmFile[272]; static volatile int bmOn; static unsigned bmT0; static volatile unsigned bmN;   // не больше BM_MAX строк: потоки роликов и звука стартуют и в игре, файл не растёт
#define BM_MAX 90u
static char *putS(char *p, const char *e, const char *s) { while (*s && p < e) *p++ = *s++; return p; }
static char *putU(char *p, const char *e, unsigned v) { char t[12]; int n = 0; do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v); while (n && p < e) *p++ = t[--n]; return p; }
static char *putX(char *p, const char *e, unsigned v) { for (int i = 7; i >= 0 && p < e; i--) *p++ = "0123456789abcdef"[(v >> (4 * i)) & 15u]; return p; }
void bmBegin(const char *argv0, int defaultOn) {
  if (BOOTMARK_OLD) return;
  size_t l = 0; if (argv0) { l = strlen(argv0); if (l > 250) l = 250; memcpy(bmFile, argv0, l); }
  while (l > 0 && bmFile[l - 1] != '/') l--;
  strcpy(bmFile + l, "bootmark");
  SceIoStat st; int on = defaultOn;
  if (!on && sceIoGetstat(bmFile, &st) >= 0) on = 1;
  strcpy(bmFile + l, "bootmark.txt");
  bmT0 = sceKernelGetSystemTimeLow(); bmOn = on;
  if (on) { SceUID f = sceIoOpen(bmFile, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f >= 0) sceIoClose(f); bmMark("BEGIN"); }   // новый запуск — файл с нуля
}
void bmMark(const char *what) {
  { unsigned i = 0; for (; what[i] && i < sizeof bmLast - 1; i++) bmLast[i] = what[i]; bmLast[i] = 0; }   // №1208: всегда, даже без файла bootmark
  if (!bmOn || ++bmN > BM_MAX) return;
  char ln[120], *p = ln, *e = ln + sizeof ln - 2;
  p = putS(p, e, "t="); p = putU(p, e, (sceKernelGetSystemTimeLow() - bmT0) / 1000u); p = putS(p, e, "ms "); p = putS(p, e, what); *p++ = '\n';
  SceUID f = sceIoOpen(bmFile, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777); if (f < 0) return;
  sceIoWrite(f, ln, (SceSize)(p - ln)); sceIoClose(f);
}
void bmMarkU(const char *what, unsigned v) {
  if (!bmOn) return;
  char s[100], *p = s, *e = s + sizeof s - 2;
  p = putS(p, e, what); p = putS(p, e, " 0x"); p = putX(p, e, v); *p = 0; bmMark(s);
}
