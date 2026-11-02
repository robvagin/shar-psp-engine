// Общие точки внедрения сверки с исходником (02.11.2026, дирижёр, правки 1077–1081). Окна правят каждое свои файлы,
// а эти две точки связывают их без правок чужих файлов. Заголовок самодостаточен: значения по умолчанию слабые
// (weak), поэтому собирается и в EBOOT, и в ботах на Маке (tools/bots, tools/cartest) без новых .c в их сборках.
#ifndef SVERKA_SHARED_H
#define SVERKA_SHARED_H

// Тип поверхности земли (данные диска: байт поверхности у каждого треугольника земли; экспорт и настоящее чтение —
// окно g12-data, своим файлом с сильным определением svSurfaceAt). Пока данных нет — везде дорога, как было.
enum { SURF_ROAD, SURF_GRASS, SURF_SAND, SURF_GRAVEL, SURF_WATER, SURF_WOOD, SURF_METAL, SURF_DIRT, SURF_COUNT };
// x, y, z — точка в мире (y — высота над землёй под ней); *indoor (может быть NULL) — 1, если поверхность помечена «помещение»
__attribute__((weak)) int svSurfaceAt(float x, float y, float z, int *indoor) {
  (void)x; (void)y; (void)z;
  if (indoor) *indoor = 0;
  return SURF_ROAD;
}

// Взрыв машины: car.c (окно g01-cars) заполняет поля и увеличивает seq; читают предметы (g11a), монеты (g11b),
// шкала Hit & Run (g10a), звук (g08). Читатель хранит свой последний seq и реагирует, когда он вырос.
typedef struct {
  float x, y, z;        // центр взрыва
  float radius, force;  // радиус действия, м, и сила (как в строке сверки P01-14)
  int byPlayer;         // 1 — машину разбил игрок
  int kind;             // 0 — машина игрока, 1 — трафик, 2 — ИИ миссии, 3 — полиция, 4 — стоящая
  unsigned seq;         // счётчик взрывов
} SvBoom;
__attribute__((weak)) SvBoom svBoom;

#endif
