// Модели-96: запекание поправки стоп (skinFootLift) в записи клипов CHR1. Хост-инструмент: считает ТОЙ ЖЕ функцией, что игра (viewer/skin.c skinFootLiftCompute),
// и пишет число в pad[0] клипа, метку 'FLFT' в pad[1]. Игра читает число и не обходит вершины (viewer/skin.c skinFootLift).
//   footbake FILE [...]   правка на месте: файл CHR1 (.chr, .shr с моделью) или пакет traffic.shr (пешеходы читаются настоящим читателем игры, viewer/traffic.c);
//                         повторный запуск даёт те же байты. Выход 0 — все файлы запечены
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "traffic.h"
#define MAGIC 0x54464C46u
int (*sceneReadyHook)(const ColWorld *w, float x, float z, int *failed);
// baseOff — где в буфере файла начинается память модели (смещения заголовка считаются от неё), hdrOff — где лежит заголовок модели
static int bake(const SkinModel *m, unsigned char *b, size_t baseOff, size_t hdrOff, int *n) {
  const unsigned o = m->h->pad[1];
  if ((o & 3) || o < sizeof(SkinHeader) || o + 4u * m->h->nClips > m->h->size) return -1;   // старый экспорт: места под таблицу нет — пере-экспортировать
  for (unsigned c = 0; c < m->h->nClips; c++) { float v = skinFootLiftCompute(m, (int)c); memcpy(b + baseOff + o + 4u * c, &v, 4); (*n)++; }
  unsigned mg = MAGIC; memcpy(b + hdrOff + offsetof(SkinHeader, pad[2]), &mg, 4);
  return 0;
}
int main(int argc, char **argv) {
  int bad = 0;
  for (int a = 1; a < argc; a++) {
    FILE *f = fopen(argv[a], "rb"); if (!f) { fprintf(stderr, "footbake: %s: не открыть\n", argv[a]); bad++; continue; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); unsigned char *b = aligned_alloc(64, ((size_t)n + 63) & ~(size_t)63);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fprintf(stderr, "footbake: %s: чтение\n", argv[a]); bad++; fclose(f); continue; } fclose(f);
    int baked = 0, models = 0;
    if (!memcmp(b, "CHR1", 4)) {
      SkinModel *m = skinLoad(argv[a]);   // свой разбор файла (память модели отдельная); число пишем в наш буфер по тому же смещению
      if (m) { if (bake(m, b, 0, 0, &baked)) { fprintf(stderr, "footbake: %s: нет места под таблицу (старый экспорт)\n", argv[a]); bad++; } else models = 1; skinFree(m); }
    } else {
      static Traffic T; if (trafficInitMem(&T, b, (unsigned)n, 0)) { fprintf(stderr, "footbake: %s: не CHR1 и не traffic.shr\n", argv[a]); bad++; free(b); continue; }
      for (int i = 0; i < T.npeds; i++) { if (bake(&T.peds[i], b, (size_t)(T.peds[i].base - b), (size_t)((const unsigned char *)T.peds[i].h - b), &baked)) { fprintf(stderr, "footbake: %s: нет места под таблицу (старый экспорт)\n", argv[a]); bad++; break; } models++; }
    }
    if (!models) { fprintf(stderr, "footbake: %s: моделей нет\n", argv[a]); bad++; free(b); continue; }
    FILE *g = fopen(argv[a], "wb"); if (!g || fwrite(b, 1, (size_t)n, g) != (size_t)n) { fprintf(stderr, "footbake: %s: запись\n", argv[a]); bad++; }
    if (g) fclose(g);
    printf("footbake: %s: моделей %d, клипов %d\n", argv[a], models, baked); free(b);
  }
  return bad ? 1 : 0;
}
