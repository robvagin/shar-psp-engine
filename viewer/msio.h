// Защита от кэша карты памяти ARK-4 (правки 252/259, 03.10): настройка ARK «mscache» держит один общий кэш чтения по смещению
// внутри файла — без имени файла и без замка между потоками (knowledge/facts/psp-ark4-mscache.yaml). Порт читает несколько
// открытых файлов из разных потоков (загрузчик клеток, звук, ролики, модели) — поток может получить байты чужого файла, и испорченные
// данные вешают всю PSP. Здесь: все чтения и записи карты — по одному (семафор), большое чтение — кусками до MSIO_CHUNK, после
// каждого куска — ioctl, который ARK понимает как «сбросить кэш» (его обработчик ioctl сначала сбрасывает кэш, потом передаёт
// команду драйверу; команда неизвестна — драйвер вернёт ошибку, на данные это не влияет).
// Подключается ко всем исходникам движка флагом -include (viewer/CMakeLists.txt): sceIoRead/sceIoWrite -> msioRead/msioWrite.
// MSIO_OLD — прежнее прямое чтение (красный контроль).
#ifndef MSIO_H
#define MSIO_H
// без заголовков PSP: файл подключается ко всем исходникам (-include), в том числе к чистому C99 (свои имена u32 и т. п.);
// типы — как SceUID (int) и SceSize (unsigned int) в pspiofilemgr.h
#ifdef __cplusplus
extern "C" {
#endif
int msioRead(int fd, void *data, unsigned int size);
int msioWrite(int fd, const void *data, unsigned int size);
void msioInit(void);
extern int (*volatile msioReadWait)(int fd, void *data, unsigned int size); /* optional owner-thread loading wait */
int msioWriteRaw(int fd, const void *data, unsigned int size);
int msioOwned(void);   // №775: 1 — этот поток держит замок карты (внутри своего чтения; запись была бы пропущена)   // мимо очереди — след зависаний (trace.txt) не встаёт вместе с игрой
// ТЕСТ90 (WAITLIMIT_OLD): сторож видит, что поток стоит внутри чтения карты: msioBusySince — время начала текущего куска (0 — не читает), msioBusyFd; msioHangTest N —
// тест: следующие N чтений никогда не возвращаются (flag readhangsync N)
// ТЕСТ90: ожидание замка карты ограничено (msioLockLimitMs, 0 — 6 с): не дождались — msioLockTimeout(ms) (главный поток: экран «Memory Stick read error»), другие потоки ждут дальше
extern volatile unsigned msioLockLimitMs; extern void (*volatile msioLockTimeout)(unsigned ms);
extern volatile unsigned msioBusySince; extern volatile int msioBusyFd, msioHangTest, msioHangLoader;   // msioHangLoader N — то же, но только для потока загрузчика (приоритет 0x18)
extern volatile unsigned msioReads, msioWaits, msioMaxWaitUs;   // счётчики для чёрного ящика
#ifdef __cplusplus
}
#endif
#if !defined(MSIO_IMPL) && !defined(MSIO_OLD)
#define sceIoRead msioRead
#define sceIoWrite msioWrite
#endif
#endif
