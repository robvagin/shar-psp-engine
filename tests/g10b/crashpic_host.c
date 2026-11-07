// Хост-тест рисунка экрана вылета (№1208): viewer/kexc/crashpic.h на синтетической картинке 240×136 (4 цвета, квадранты) в буферы 8888 и 565; красный контроль — битая шапка, обрезанный файл.
// cc -O2 -I viewer/kexc tests/g10b/crashpic_host.c -o /tmp/crashpic_host && /tmp/crashpic_host   (код возврата 0 — PASS)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "crashpic.h"
int main(void) {
  unsigned w = 240, h = 136, nc = 4; unsigned n = 12 + nc * 4 + w * h; unsigned char *p = calloc(1, n);
  memcpy(p, "SHXC", 4); p[4] = w; p[5] = 0; p[6] = h; p[7] = 0; p[8] = nc; p[9] = 0;
  unsigned char pal[16] = {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255}; memcpy(p + 12, pal, 16);
  for (unsigned y = 0; y < h; y++) for (unsigned x = 0; x < w; x++) p[28 + y * w + x] = (unsigned char)((y >= h / 2) * 2 + (x >= w / 2));
  static unsigned buf32[512 * 272]; static unsigned short buf16[512 * 272]; int bad = 0;
  if (!crashPicDraw(p, n, buf32, 512, 3)) { puts("FAIL: 8888 не нарисовано"); return 1; }
  if (buf32[10 * 512 + 10] != 0xff0000ffu) { printf("FAIL: пиксель (10,10) 8888 = %08x\n", buf32[10 * 512 + 10]); bad++; }   // красный: R=255 -> 0xff0000ff (A B G R)
  if (buf32[10 * 512 + 400] != 0xff00ff00u) { printf("FAIL: (400,10) = %08x\n", buf32[10 * 512 + 400]); bad++; }
  if (buf32[250 * 512 + 10] != 0xffff0000u) { printf("FAIL: (10,250) = %08x\n", buf32[250 * 512 + 10]); bad++; }
  if (buf32[250 * 512 + 400] != 0xffffffffu) { printf("FAIL: (400,250) = %08x\n", buf32[250 * 512 + 400]); bad++; }
  if (!crashPicDraw(p, n, buf16, 512, 0)) { puts("FAIL: 565 не нарисовано"); return 1; }
  if (buf16[10 * 512 + 10] != 0x001f) { printf("FAIL: 565 красный = %04x\n", buf16[10 * 512 + 10]); bad++; }
  unsigned char *q = malloc(n); memcpy(q, p, n); q[0] = 'X';
  if (crashPicDraw(q, n, buf32, 512, 3)) { puts("FAIL: красный контроль (битая шапка) не пойман"); bad++; }
  if (crashPicDraw(p, n - 100, buf32, 512, 3)) { puts("FAIL: красный контроль (обрезанный файл) не пойман"); bad++; }
  // сжатый вариант: те же индексы PackBits-потоком, результат обязан совпасть пиксель в пиксель
  { unsigned char *c = malloc(n * 2); unsigned cn = 0; const unsigned char *src = p + 28; unsigned total = w * h;
    for (unsigned i = 0; i < total; ) { unsigned j = i + 1; while (j < total && src[j] == src[i] && j - i < 129) j++;
      if (j - i >= 2) { c[cn++] = (unsigned char)(126 + (j - i)); c[cn++] = src[i]; i = j; }
      else { unsigned k = i; while (k < total && k - i < 128 && !(k + 1 < total && src[k] == src[k + 1])) k++; if (k == i) k++; c[cn++] = (unsigned char)(k - i - 1); for (unsigned q = i; q < k; q++) c[cn++] = src[q]; i = k; } }
    unsigned m = 28 + 4 + cn; unsigned char *z = calloc(1, m); memcpy(z, p, 12); z[10] = 1; memcpy(z + 12, pal, 16); z[28] = cn; z[29] = cn >> 8; z[30] = cn >> 16; z[31] = cn >> 24; memcpy(z + 32, c, cn);
    static unsigned bz[512 * 272]; if (!crashPicDraw(z, m, bz, 512, 3)) { puts("FAIL: сжатая не нарисована"); bad++; }
    else if (memcmp(bz, buf32, sizeof bz)) { puts("FAIL: сжатая не совпала с несжатой"); bad++; }
    printf("сжатие: %u -> %u байт\n", n, m);
    z[40] ^= 0x55; if (crashPicSize(z, m - 5)) { puts("FAIL: обрезанный сжатый не пойман"); bad++; } }
  puts(bad ? "FAIL" : "PASS"); return bad ? 1 : 0;
}
