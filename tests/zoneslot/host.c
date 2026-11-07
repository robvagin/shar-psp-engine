// TEST91 1А шаг 1: постоянный слот (viewer/zoneslot.h) — пакет следующего уровня ложится в тот же адрес, лишний кусок не портит соседей, слот с cap 0 (ZONE_*_OLD) всегда отдаёт 0.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../viewer/zoneslot.h"
static int fails;
#define CHK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); fails++; } } while (0)
int main(void) {
  ZSlot z = {0, 1152u << 10, 0, 0, 0};
  // «уровень 1»: пакет тремя кусками (как ui.shr при чтении кусками)
  void *a1 = zsAlloc(&z, 400000), *a2 = zsAlloc(&z, 400000), *a3 = zsAlloc(&z, 286592);
  CHK(a1 && a2 && a3, "три куска пакета 1 087 КБ влезают в слот 1152 КБ");
  CHK(zsOwns(&z, a1) && zsOwns(&z, a3), "куски принадлежат слоту");
  unsigned char *base = z.base;
  CHK(zsAlloc(&z, 200000) == 0, "слот полон: четвёртый кусок — в кучу (0)");
  int o = zsFree(&z, a1) + zsFree(&z, a2) + zsFree(&z, a3);
  CHK(o == 3, "все три куска вернулись в слот");
  CHK(z.used == 0 && z.live == 0, "слот пуст после возврата всех кусков");
  // «уровень 2»: тот же пакет — те же адреса
  void *b1 = zsAlloc(&z, 400000), *b2 = zsAlloc(&z, 400000), *b3 = zsAlloc(&z, 286592);
  CHK(b1 == a1 && b2 == a2 && b3 == a3, "пакет следующего уровня — по тем же адресам");
  CHK(z.base == base, "база слота не менялась");
  zsFree(&z, b1); zsFree(&z, b2); zsFree(&z, b3);
  // один кусок целиком, потом ещё раз
  void *c1 = zsAlloc(&z, 1100032); CHK(c1 == base, "пакет одним куском (газета 1 100 032 Б) — в начало слота");
  CHK(zsAlloc(&z, 2000000) == 0, "кусок больше слота — 0");
  void *h = malloc(64); CHK(!zsFree(&z, h), "чужой указатель — не из слота"); free(h);
  zsFree(&z, c1); void *c2 = zsAlloc(&z, 1100032); CHK(c2 == c1, "газета следующего уровня — по тому же адресу");
  // выключенный слот (ZONE_*_OLD): cap 0
  ZSlot off = {0, 0, 0, 0, 0}; CHK(zsAlloc(&off, 1000) == 0 && !off.base, "cap 0 — слот не заводится, всегда куча");
  if (fails) { printf("FAIL: %d\n", fails); return 1; }
  printf("OK\n"); return 0;
}
