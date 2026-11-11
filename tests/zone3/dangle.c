// Шаг 7 Fable (висячие указатели): хост-проверка заливки 0xFF и красного контроля «канарейка». tests/zone3/dangle.sh
//   зелёное: с заливкой блок области после освобождения/отката до отметки/сброса читается через оставленный указатель как 0xFF и проверка пишет dangling; без заливки молчит.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
struct _reent;
static void *D_ma(size_t al, size_t n) { n = (n + al - 1) / al * al; return aligned_alloc(al, n ? n : al); }
#define memalign(al, sz) D_ma((al), (sz))
static char logBuf[200]; static int logN;
#define Z3_LOGF(s) (logN++, snprintf(logBuf, sizeof logBuf, "%s", (s)))
#include "../../viewer/zone3.h"
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)
static unsigned char *canary(int area, unsigned char *keep) { unsigned char *p = zone3Alloc(area, "canary", 64, 64); memset(p, 0x5A, 64); z3Dangle = p; z3DangleArea = area; (void)keep; return p; }
int main(int argc, char **argv) {
  int scribble = argc > 1 && !strcmp(argv[1], "scribble");
  z3Scribble = scribble; CHECK(zone3Init(2u << 20, 2u << 20, 0, 0xFFFFFFFFu), "области не взялись");
  // 1. освобождение блока
  unsigned char *p = canary(Z3_MISSION, 0); zone3Free(p); int h1 = z3DangleHits;
  // 2. откат «места» до отметки: блок после отметки снят, указатель оставлен
  unsigned m = zone3Mark(Z3_PLACE); p = canary(Z3_PLACE, 0); zone3Release(Z3_PLACE, m); int h2 = z3DangleHits;
  // 3. сброс всей области
  p = canary(Z3_MISSION, 0); z3AreaReset(&z3A[Z3_MISSION]); int h3 = z3DangleHits;
  printf("заливка %s: зацепок free=%d release=%d reset=%d; последняя строка журнала: %s", scribble ? "ВКЛ" : "выкл", h1, h2 - h1, h3 - h2, logN ? logBuf : "(нет)\n");
  if (scribble) { CHECK(h1 == 1 && h2 - h1 == 1 && h3 - h2 == 1, "с заливкой висячий указатель обязан быть пойман в трёх местах"); CHECK(strstr(logBuf, "WATCHDOG zone3 dangling"), "нет строки WATCHDOG"); }
  else CHECK(h1 + (h2 - h1) + (h3 - h2) == 0, "без заливки проверка должна молчать (красный контроль самой заливки)");
  printf("%s\n", fails ? "FAIL" : "PASS"); return fails != 0; }
