// №1195 (g01-cars): страховки надёжности — замок с пределом, потокобезопасная строка в ящик, закраска и высота заполненности стека. Сборка и прогон: tests/g01cars/reliab.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "reliab.h"
static int fails;
#define CHECK(name, cond) do { int ok_ = !!(cond); printf("%s  %s\n", ok_ ? "OK  " : "FAIL", name); if (!ok_) fails++; } while (0)
static unsigned delays, delayCap; static volatile int *releaseLock; static unsigned releaseAt;
static void delayFn(unsigned us) { (void)us; delays++; if (releaseLock && delays == releaseAt) *releaseLock = 0; if (delays > delayCap) { printf("ЗАВИСАНИЕ: %u ожиданий без выхода\n", delays); exit(3); } }
static unsigned drained; static int badLine;
static void sink(const char *l) { drained++; if (strncmp(l, "T", 1) && strncmp(l, "LOCK", 4)) badLine++; }
static void *writer(void *a) { int id = (int)(long)a; for (int i = 0; i < 2000; i++) relNote("T%d n=%d", id, i); return 0; }
int main(void) {
  relStackOn = 1;   // закраска по умолчанию выключена (reliab.h); тест включает её сам
  // 1. замок, который никто не отпустит: выход по пределу (при LOCKLIM1195_OLD=1 — зависание, тест прерывает после 2 млн ожиданий)
  { volatile int lock = 1; delays = 0; delayCap = 2000000; releaseLock = 0; unsigned n0 = relNoteCount();
    int r = relSpinAcquire(&lock, 100000u, 1000u, delayFn, "test"); CHECK("замок, который не отпускают: выход по пределу, не взят", r == 0 && lock == 1 && delays >= 100 && delays <= 101 && relNoteCount() == n0 + 1); }
  // 2. замок отпущен вовремя (на 20-м ожидании из 100 допустимых): взят, строки LOCK нет
  { volatile int lock = 1; delays = 0; delayCap = 2000000; releaseLock = &lock; releaseAt = 20; unsigned n0 = relNoteCount();
    int r = relSpinAcquire(&lock, 100000u, 1000u, delayFn, "test2"); CHECK("замок отпущен до предела: взят, без строки LOCK", r == 1 && lock == 1 && relNoteCount() == n0); releaseLock = 0; }
  // 3. свободный замок
  { volatile int lock = 0; delays = 0; int r = relSpinAcquire(&lock, 100000u, 1000u, delayFn, "free"); CHECK("свободный замок берётся сразу", r == 1 && delays == 0); }
  // 4. кольцо строк: четыре потока пишут, главный читает без порчи
  { pthread_t t[4]; unsigned stop = 0; for (int i = 0; i < 4; i++) pthread_create(&t[i], 0, writer, (void *)(long)i);
    for (int k = 0; k < 4000; k++) relFrame(sink, 0); for (int i = 0; i < 4; i++) pthread_join(t[i], 0); relFrame(sink, 0); (void)stop;
    CHECK("строки потоков в кольце целы (ни одной порченой)", badLine == 0 && drained > 0); }
  // 5. стек: закраска и высота заполненности; по умолчанию (relStackOn = 0) закраска ничего не пишет
  { unsigned char t[64]; memset(t, 0, sizeof t); relStackOn = 0; relStackPaintRange(t, sizeof t); int clean = t[0] == 0 && t[63] == 0; relStackOn = 1; CHECK("по умолчанию закраска не пишет в память", clean); }
  { enum { SZ = 16384 }; static unsigned char stk[SZ]; memset(stk, 0, sizeof stk); relStackPaintRange(stk, SZ - 1024);
    memset(stk + SZ - 1024, 0x11, 1024);   // «использованы» верхние 1 КБ
    unsigned h = relStackHeadroom(stk, SZ); CHECK("неглубокий стек: запас 15 КБ из 16, без предупреждения", h == SZ - 1024 && !relStackWarn(h, SZ));
    memset(stk + SZ - 15000, 0x22, 15000 - 1024);   // глубокий: использовано 15000 из 16384
    h = relStackHeadroom(stk, SZ); CHECK("искусственно глубокий стек: запас 1384 < 10 % — предупреждение", h == SZ - 15000 && relStackWarn(h, SZ));
    CHECK("граница: ровно 10 % запаса — ещё без предупреждения", !relStackWarn(1640u, 16400u) && relStackWarn(1639u, 16400u)); }
  printf(fails ? "ПРОВАЛ: %d\n" : "RESULT ok\n", fails); return fails != 0;
}
