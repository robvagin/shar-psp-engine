// Правка Роба 249 (видео IMG_7707 1:55–2:02: «не остаётся тормозных следов на дороге — в оригинале оставались»): следы шин.
// Правила — исполняемый файл PS2 SLES_518.97 (разбор psp-objdump, адреса — в skid.c): уровень заноса машины (0x282208),
// следы колёс на земле (SkidMarkGenerator 0x2052f0, Skidmark 0x2061f0), текстура skid.bmp шейдера skid_m (art/l01_fx.p3d, 16×16).
// Рисование — одно кольцо квадов в постоянном буфере, один вызов GE на кадр. Чистая логика собирается и на Маке (SKID_HOST).
#ifndef SHAR_SKID_H
#define SHAR_SKID_H
#include "car.h"

#ifndef SKID_MARKS
#define SKID_MARKS 1        // 0 — прежнее поведение (следов нет): красный контроль гейтов tests/skid
#endif
#define SKID_QUADS 384      // кольцо квадов (правило порта; у оригинала 30 следов по ≤ 32 плитки 0,5 м — Init(30) 0x14b714)
#define SKID_CARS 4         // машин со своими следами (0 — машина игрока)

typedef struct {
  int quads;        // живых квадов в кольце
  int drawn;        // нарисовано в прошлом кадре
  int live;         // следов, которые сейчас тянутся за колёсами
  int made;         // квадов всего с загрузки уровня
  int madeAi;       // 249: счётчик машин миссий, race
  int skipped;      // кадров без рисования из-за полного списка GE
  float lvl;        // уровень заноса машины игрока в прошлом кадре (0..1, правило 0x282208)
  float us;         // время процессора на skidCar + skidDraw в прошлом кадре, мкс (на PSP; в эмуляторе — его часы)
} SkidStat;

// уровень загружен: следы стереть; on — включены на этом уровне (слово Роба 03.10: только уровень 1)
void skidClear(int on);
void skidEnable(int on);    // служебное «flag skidoff» (красный контроль в эмуляторе): 0 — не рисовать и не класть
// Уровень заноса по правилу оригинала (Vehicle, 0x282208): занос (slip 1/2) — |поперечная доля скорости|, не меньше ручник × 0,5;
// тормоз на ходу вперёд при 7–50 % TopSpeed — 1; ручник на скорости > 7 % — 0,5; пробуксовка (газ с места: 1 − 0,5 · доля/BurnoutRange;
// задний ход медленнее 50 км/ч: тормоз) — не меньше её; ненулевой — не меньше 0,5. burn (может быть NULL) — уровень пробуксовки:
// при нём следы только у ведущих (задних) колёс
float skidLevel(const Car *c, const CarInput *in, float *burn);
float skidSoundLevel(const Car *c, const CarInput *in); // №446: braking/burnout as well as lateral slip
// после carUpdate: колёса на земле при уровне заноса > 0,01 тянут след (новый участок не ближе 0,22 м), остальные его заканчивают
void skidBeginLevel(int level);   // №443: same lifetime on every level; SKID_MARKS_ALL_OLD limits to L1
void skidCar(int slot, const Car *c, const CarInput *in, const ColWorld *w);
void skidSetGeRoom(int (*room)(unsigned bytes));   // правка 103: проверка места в списке GE (main.c geRoom)
// один вызов GE: квады ближе SKID_DRAW_DIST к глазу; меняет режимы GE — вызывать между resetState() main.c
void skidDraw(float ex, float ey, float ez);
void skidGetStat(SkidStat *s);
// для гейтов на Маке: квад кольца i (0..SKID_QUADS-1): 4 угла и альфа углов; 0 — пусто
unsigned skidSelect108(float ex, float ey, float ez, unsigned short *sel);   // 108 О13: квады в радиусе рисования (по возрастанию номера)
unsigned skidCheck108(void);   // сверка плотного массива с исходными (0 — верно)
unsigned skidLines108(void);   // модель кэша PSP: строк, затронутых выбором
extern int skidDense108Old;
int skidQuadAt(int i, float p[4][3], unsigned char a[4]);

unsigned skidQuadRgb(int i); // 424
void skidHoverOld(int on);  // правка 413: 1 — следы и под парящими (прежнее, красный контроль)
void skidOld(int on);       // №562: 1 — прежнее условие и громкость скрежета резины (красный контроль)
int skidHover(const char *name);   // 413: парящая машина (байт +657 SLES_518.97) — следов нет
void skidWideOld(int on);   // правка 270: 1 — прежняя ширина следа 0,5 м (служебно, красный контроль)
#endif
