// Подмена sceIo для хоста (№1170): файлы на обычной ФС, и «обрыв питания» на заданном шаге записи.
// psp_budget — сколько изменяющих шагов (открытие на запись, запись, удаление, переименование) ещё выполнится; шаг с нулевым остатком не выполняется
// (запись при этом ложится половиной) и питание «выключается»: все следующие вызовы возвращают ошибку и ничего не меняют до psp_reboot().
#ifndef STUB_PSPIO_H
#define STUB_PSPIO_H
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
typedef int SceUID;
#define PSP_O_RDONLY 1
#define PSP_O_WRONLY 2
#define PSP_O_CREAT 0x200
#define PSP_O_TRUNC 0x400
extern int psp_budget, psp_off, psp_steps;
static inline int psp_step(void) { if (psp_off) return 0; if (psp_budget == 0) { psp_off = 1; return 0; } if (psp_budget > 0) psp_budget--; psp_steps++; return 1; }
static inline void psp_reboot(void) { psp_off = 0; psp_budget = -1; }
static inline SceUID sceIoOpen(const char *p, int fl, int mode) {
  if (psp_off) return -1;
  if (fl & PSP_O_WRONLY) { if (!psp_step()) return -1; return open(p, O_WRONLY | ((fl & PSP_O_CREAT) ? O_CREAT : 0) | ((fl & PSP_O_TRUNC) ? O_TRUNC : 0), mode); }
  return open(p, O_RDONLY);
}
static inline int sceIoRead(SceUID f, void *b, int n) { if (psp_off) return -1; return (int)read(f, b, (size_t)n); }
static inline int sceIoWrite(SceUID f, const void *b, int n) {
  if (psp_off) return -1;
  if (!psp_step()) { int h = n / 2; if (h > 0) (void)!write(f, b, (size_t)h); return -1; }
  return (int)write(f, b, (size_t)n);
}
static inline int sceIoClose(SceUID f) { return close(f); }
static inline int sceIoRemove(const char *p) { if (!psp_step()) return -1; return unlink(p) == 0 ? 0 : (int)0x80010002u; }
static inline int sceIoRename(const char *a, const char *b) {   // как на PSP (FAT): на существующее имя не переименовывает
  if (!psp_step()) return -1; if (access(b, F_OK) == 0) return -1; return rename(a, b);
}
#endif
