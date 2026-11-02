// заглушка PSP SDK для хост-теста будки: только то, что берёт viewer/phone_view.c
#ifndef STUB_PSPKERNEL_H
#define STUB_PSPKERNEL_H
static inline void sceKernelDcacheWritebackRange(const void *p, unsigned n) { (void)p; (void)n; }
static inline unsigned sceKernelGetSystemTimeLow(void) { return 0; }
#endif
