// Зона 3 «эпизод» — интерфейс для клиентов (реализация zone3.c, ядро zone3.h). Спецификация 08.11, раздел А.3; пакет П5 Зоны-95.
// Замена zonealloc.h (Опора, П1) на время, пока тот не влит: те же имена зон; при слиянии zoneAlloc(Z3_MISSION|Z3_PLACE, who, size) = zone3AllocX ниже.
#ifndef ZONE3_API_H
#define ZONE3_API_H
enum { Z3A_MISSION = 0, Z3A_PLACE = 1, Z3A_CITY = 2 };
void *zone3AllocX(int area, const char *who, unsigned size, unsigned align);   // блок из области, иначе (область тесна) из кучи с записью WATCHDOG mem … spill; z3 выключена — memalign как раньше
void *zone3TryX(int area, const char *who, unsigned size, unsigned align);   // то же без проливов и ступеней (предчтение): области тесно — 0; зона выключена — memalign
int zone3FreeX(void *p);                       // 1 — блок был из области и возвращён (вызывается из __wrap_free); 0 — чужой, free() его освободит
int zone3OwnsX(const void *p);
unsigned zone3MarkX(int area);                 // отметка «места» перед входом
int zone3ReleaseX(int area, unsigned mark);    // сброс до отметки; сколько блоков снято
int zone3CheckX(int area, const char *why);    // сколько живых блоков в области (журнал ZONE3 live=…); не освобождает
int zone3LeakCityX(const char *why);   // 102: город после выгрузки клеток: живые блоки - WATCHDOG zone3 leak area=city, область не стирается
int zone3ResetX(int area, const char *why);    // стирание области, если в ней нет живых блоков чужих владельцев: живые — WATCHDOG zone3 leak и область не трогаем (возвращает число живых)
int zone3InitX(unsigned missionB, unsigned placeB, unsigned cityB, unsigned budget);   // cityB = 0 — без области города; budget — сколько байт областей взять при старте (снизу вверх, пока влезает)
int zone3CityOnX(void);
int zone3WantedX(void);                         // зона включена в этой игре (размеры заданы), даже если на уровень все области отцеплены
int zone3AreaOnX(int area);                     // область прицеплена и обслуживает клиентов (отцеплена на уровень — 0)
unsigned zone3ShrinkX(unsigned want, const char *why);   // уровню не хватает кучи: отцепить пустые области (город, место, миссия) и вернуть куче не меньше want байт; сколько отдано
int zone3RegrowX(unsigned heapFree, unsigned need, unsigned ring, const char *why);   // куча с запасом: прицепить обратно отцепленные области, пока heapFree − область ≥ need; сколько прицеплено
                         // область города взята (клетки и хвосты cellpool.h идут в неё)
unsigned zone3FreeTotX(int area);               // свободно в области всего (не наибольший кусок)
unsigned zone3UsedX(int area);
int zone3OnX(void);
unsigned zone3MaxFreeX(int area);              // наибольший свободный кусок области (для проверки «влезет ли»)
void zone3SetHooks(int (*reclaim)(int area, const char *who, unsigned need, int step), void (*logFn)(const char *));
void zone3DumpX(int area, char *o, unsigned n);
void zone3StatLineX(char *o, unsigned n, float t, const char *why);
#endif
void zone3SetRegionOldX(int old);   // до zone3InitX: 1 — прежняя посадка областей по одной (файл zone3regold / ZONE3_REGION_OLD)
unsigned zone3HomeX(int area);     // домашний адрес области (для журнала)
unsigned zone3RegMissX(void);      // сколько прицепок отложено (память пришла не на домашний адрес)
int zone3RegionIsOldX(void);   // 1 — прежняя посадка областей
void zone3SetTightX(int area, unsigned bytes);   // до zone3InitX: до скольки байт ужимать область на тесном уровне вместо отцепки (0 - не ужимать)
void zone3SetFloorX(int on);   // 1 - ужатая область не отцепляется совсем (с L2); на L1 0 - можно отцепить
int zone3EnsureFloorX(const char *why);   // с L2 прицепить «место» хотя бы ужатым, если оно отцеплено
void zone3SetScribbleX(int on);   // шаг 7 Fable: заливка 0xFF освобождённых блоков областей
int zone3ScribbleOnX(void);
void zone3DangleX(int area, const char *who);   // красный контроль: канарейка и оставленный указатель
int zone3DangleHitsX(void);
void zone3DangleFreeX(void);   // освободить блок канарейки (красный контроль)
