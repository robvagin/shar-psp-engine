// TEST90 MEMMAP: счётчики классов по всем блокам и таблица блоков ≥ 16 КБ (viewer/memmap.h) против эталонной модели на случайной последовательности добавлений и удалений.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../viewer/memmap.h"
#define NREF 6000
static struct { uint32_t off; unsigned sz, ra; int live; } ref[NREF];
int main(void) {
  srand(12345); int fails = 0; static char buf[2048];
  uint32_t ras[8]; for (int i = 0; i < 8; i++) ras[i] = MM_BASE + 0x100000u + (unsigned)i * 0x4000u;
  for (int step = 0; step < 300000; step++) {
    int i = rand() % NREF;
    if (!ref[i].live) {
      ref[i].off = (uint32_t)i * 0x2000u + 0x1000u;   // уникальные адреса с шагом 8 КБ в первых 49 МБ, 16-выровнены (размер в модели не обязан помещаться между ними: это только ключ)
      unsigned r = (unsigned)rand() % 100; ref[i].sz = r < 70 ? 16u + (unsigned)rand() % 2000u : r < 95 ? 2000u + (unsigned)rand() % 60000u : 100000u + (unsigned)rand() % 900000u;
      ref[i].ra = ras[rand() % 8]; ref[i].live = 1; mmAdd((void *)(uintptr_t)(MM_BASE + ref[i].off), ref[i].sz, ref[i].ra);
    } else { ref[i].live = 0; mmDel((void *)(uintptr_t)(MM_BASE + ref[i].off), ref[i].sz); }
    if (step % 50000 == 49999) {
      mmScan(); mmFormat(buf, sizeof buf, 1.f, 0, 0, 0, 0, 0);
      unsigned live = 0, cn[MM_NCLS] = {0}, cb[MM_NCLS] = {0}, big = 0, bigKB = 0;
      for (int k = 0; k < NREF; k++) if (ref[k].live) { unsigned s = ref[k].sz; int c = 0; while (c < MM_NCLS - 1 && s > mmClsMax[c]) c++; cn[c]++; cb[c] += s; live++; if (s >= MM_MIN) { big++; bigKB += (s + 15u) & ~15u; } }
      unsigned got = 0; sscanf(strstr(buf, "live=") + 5, "%u", &got);
      printf("шаг %d: живых модель %u таблица-счётчик %u; блоков ≥16 КБ модель %u в таблице %u (КБ %u/%u), переполнений %u\n", step + 1, live, got, big, mmTabN, (bigKB + 512u) >> 10, mmTabKB, mmOver);
      if (got != live) { printf("FAIL: число живых не сошлось\n"); fails++; }
      if (mmTabN != big || mmTabKB != ((bigKB + 512u) >> 10)) { printf("FAIL: таблица блоков ≥ 16 КБ не сошлась\n"); fails++; }
      for (int c = 0; c < MM_NCLS; c++) { char key[16]; snprintf(key, sizeof key, " %s:", mmClsName[c]); const char *q = strstr(buf, key); unsigned a = 0, b = 0;
        if (!q || sscanf(q + strlen(key), "%u/%u", &a, &b) != 2 || a != cn[c] || b != ((cb[c] + 512u) >> 10)) { printf("FAIL: класс %s: строка %u/%u модель %u/%u\n", mmClsName[c], a, b, cn[c], (cb[c] + 512u) >> 10); fails++; } }
    } }
  if (mmOver) { printf("FAIL: переполнение таблицы %u\n", mmOver); fails++; }
  printf("размер статики: таблица %zu Б, группы %zu Б, счётчики %zu Б\n", sizeof mmTab, sizeof mmG, sizeof mmCCnt + sizeof mmCByt);
  if (fails) { printf("FAIL: %d\n", fails); return 1; }
  printf("OK\n"); return 0;
}
