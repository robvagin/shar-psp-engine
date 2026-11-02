// Сильное определение поверхности для проверок g01-cars: тип задаёт тест через gSurf (в чек-файле слабое из sverka_shared.h затеняется этим при линковке).
int gSurf;
int svSurfaceAt(float x, float y, float z, int *indoor) { (void)x; (void)y; (void)z; if (indoor) *indoor = 0; return gSurf; }
