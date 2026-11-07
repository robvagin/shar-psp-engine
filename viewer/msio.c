// См. viewer/msio.h: обращения к карте по одному и сброс кэша ARK-4 после каждого куска чтения (правки 252/259)
#undef sceIoRead    // -include msio.h уже подменил имена — здесь нужны настоящие вызовы драйвера
#undef sceIoWrite
#define MSIO_IMPL
#ifndef MSIO_HOST_STUBS   // tests/msio/host.c подставляет свою модель драйвера карты
#include <pspkernel.h>
#include <pspiofilemgr.h>
#endif
#include "msio.h"
#define MSIO_CHUNK (64u * 1024u)   // кусок чтения: между кусками семафор свободен — звук не ждёт загрузку целиком
#define MSIO_NOP_IOCTL 0x7f5a0001u  // неизвестная драйверу команда: обработчик ARK сбрасывает кэш, драйвер отвечает ошибкой
static SceUID msioSema = -1;
int (*volatile msioReadWait)(int fd, void *data, unsigned int size);
volatile unsigned msioReads, msioWaits, msioMaxWaitUs;
volatile unsigned msioLockLimitMs; void (*volatile msioLockTimeout)(unsigned ms);
volatile unsigned msioBusySince; volatile int msioBusyFd, msioHangTest, msioHangLoader;
void msioInit(void) { if (msioSema < 0) msioSema = sceKernelCreateSema("msio", 0, 1, 1, NULL); }
// №741/№700 (смена уровня висела намертво на экране загрузки — эмулятор 2 из 2, L6 на PSP): msioRead держит замок и, пока ждёт кусок, крутит кадры
// экрана загрузки (msioReadWait); в кадре самописец пишет журнал (msioWrite) — тот же поток второй раз берёт тот же неповторный замок и ждёт сам себя.
// Теперь замок знает владельца: вложенная запись из потока-владельца пропускается (теряется одна строка журнала, msioNestSkips), вложенное чтение идёт
// без замка (msioNestReads). MSIO_NEST_OLD — прежнее
#ifndef MSIO_NEST_OLD
#define MSIO_NEST_OLD 0
#endif
static volatile SceUID owner = -1;
volatile unsigned msioNestSkips, msioNestReads;
static int nested(void) { return !MSIO_NEST_OLD && msioSema >= 0 && owner >= 0 && owner == sceKernelGetThreadId(); }
int msioOwned(void) { return nested(); }   // №775: поток сейчас внутри своего чтения с замком (журнал смоука не сбрасывать — запись была бы пропущена)
static void lock(void) {
  if (msioSema < 0) return;
  unsigned t0 = sceKernelGetSystemTimeLow();
  if (sceKernelPollSema(msioSema, 1) < 0) { msioWaits++;
#ifndef MSIO_HOST_STUBS
    for (;;) {   // ТЕСТ90: замок ждёт не дольше предела за раз; владелец завис в чтении — главный поток получает экран ошибки, а не вечное ожидание
      unsigned to = (msioLockLimitMs ? msioLockLimitMs : 6000u) * 1000u;
      if (sceKernelWaitSema(msioSema, 1, &to) >= 0) break;
      void (*cb)(unsigned) = msioLockTimeout; if (cb) cb((sceKernelGetSystemTimeLow() - t0) / 1000u);
    }
#else
    sceKernelWaitSema(msioSema, 1, NULL);
#endif
    unsigned w = sceKernelGetSystemTimeLow() - t0; if (w > msioMaxWaitUs) msioMaxWaitUs = w; }
  owner = sceKernelGetThreadId();
}
static void unlock(void) { owner = -1; if (msioSema >= 0) sceKernelSignalSema(msioSema, 1); }
int msioRead(int fd, void *data, unsigned int size) {
#ifdef MSIO_OLD
  return sceIoRead(fd, data, size);
#else
  if (nested()) { msioNestReads++; return sceIoRead(fd, data, size); }   // №741: изнутри своего же чтения — без замка
  int total = 0; char *p = (char *)data;
  while (size > 0) {
    unsigned int n = size > MSIO_CHUNK ? MSIO_CHUNK : size;
    lock();
    int (*waitRead)(int, void *, unsigned) = msioReadWait;
#ifndef MSIO_HOST_STUBS
    if (msioHangLoader > 0 && sceKernelGetThreadCurrentPriority() == 0x18) { msioHangLoader--; msioBusyFd = fd; msioBusySince = sceKernelGetSystemTimeLow() | 1u; sceKernelSleepThread(); }
    if (msioHangTest > 0) { msioHangTest--; msioBusyFd = fd; msioBusySince = sceKernelGetSystemTimeLow() | 1u; sceKernelSleepThread(); }   // тест: чтение не возвращается
#endif
    msioBusyFd = fd; msioBusySince = sceKernelGetSystemTimeLow() | 1u;
    int r = waitRead ? waitRead(fd, p, n) : sceIoRead(fd, p, n);
    msioBusySince = 0;
    sceIoIoctl(fd, MSIO_NOP_IOCTL, NULL, 0, NULL, 0);   // сброс общего кэша ARK: следующее чтение (любой файл, любой поток) — с карты
    unlock(); msioReads++;
    if (r < 0) return total ? total : r;
    total += r; p += r; size -= (unsigned int)r;
    if ((unsigned int)r < n) break;   // конец файла
  }
  return total;
#endif
}
int msioWrite(int fd, const void *data, unsigned int size) {
#ifdef MSIO_OLD
  return sceIoWrite(fd, data, size);
#else
  if (nested()) { msioNestSkips++; return (int)size; }   // №741: запись из кадра внутри своего чтения (журнал самописца) — пропуск, не самоблокировка
  lock(); int r = sceIoWrite(fd, data, size); sceIoIoctl(fd, MSIO_NOP_IOCTL, NULL, 0, NULL, 0); unlock(); return r;
#endif
}

int msioWriteRaw(int fd, const void *data, unsigned int size) { return sceIoWrite(fd, data, size); }
