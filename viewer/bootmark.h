// bootmark.h — шаги старта на карту (№1192/TEST90: игра умерла после экрана GONE Makers, на карте ничего). bmMark открывает bootmark.txt рядом с EBOOT,
// дописывает строку и закрывает (карта сбрасывает запись при закрытии): после смерти приставки последняя строка = последний пройденный шаг.
// Включено в турах (SHR_KEEP_SMOKE или не релиз) и в игре с файлом bootmark рядом с EBOOT. BOOTMARK_OLD=1 — шаги не пишутся. Потоки: bmThread(имя) до и после закраски стека.
#ifndef BOOTMARK_H
#define BOOTMARK_H
#ifndef BOOTMARK_OLD
#define BOOTMARK_OLD 0
#endif
#ifdef __cplusplus
extern "C" {
#endif
extern char bmLast[80];   // последний шаг (для экрана вылета)
void bmBegin(const char *argv0, int defaultOn);   // самый первый вызов main: путь, включение, строка «BEGIN»
void bmMark(const char *what);                      // «t=<мкс от начала> <what>»
void bmMarkU(const char *what, unsigned v);        // то же с числом (id потока и т. п.)
#ifdef __cplusplus
}
#endif
#endif
