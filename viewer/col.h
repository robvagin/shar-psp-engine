// Коллизия уровня по секции COL1 (формат: tools/psp_export.py, «Коллизия: секция COL1»).
// Чистый C99, float, без PSP-заголовков и без malloc в кадре (одна аллокация в colInit).
// Координаты мира как у движка: метры, y вверх, z уже отзеркален экспортом.
#ifndef SHAR_COL_H
#define SHAR_COL_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { COL_TRI = 0, COL_BOX = 1, COL_CYL = 2, COL_SEG = 3 };
#define COL_XFENCE 0x1f000000u   // номер контакта заборов трассы гонки (colExtraFences) — не индекс мира
enum { COL_MAT_HARD = 0, COL_MAT_FOLIAGE = 1 }; // COL1 reserved float: finite volume material

typedef struct { char magic[4]; float ox, oz, cell; uint32_t W, H, ntri, nbox, ncyl, nseg, nref, flags; } ColHdr;  // 48 B
typedef struct { float a[3], b[3], c[3], n[3]; } ColTri;                              // 48 B
typedef struct { float c[3], ax[3], ay[3], az[3], h[3], pad; } ColBox;                // 64 B
typedef struct { float c[3], axis[3], r, halfLen; uint32_t flat; float pad[3]; } ColCyl;  // 48 B
typedef struct { float a[2], b[2], n[2], y0, y1; } ColSeg;                            // 32 B
// Блок JMP1 (с 29.09; смещение от начала секции — в ColHdr.flags, 0 — нет; tools/jumps_export.py, правки Роба 46–47):
// зоны прыжка машин (Event 48 jumpzone*, файл сценария «jump boost zones») и подкидывающие точки Гомера (Event 6:
// отдушины, кондиционеры, пружины). Объём — TriggerVolume локатора: ящик (оси единичные) или сфера (h = радиус ×3).
typedef struct { float c[3], ax[3], ay[3], az[3], h[3]; uint32_t shape; char name[16]; } ColZone;                    // 80 B
typedef struct { float c[3], ax[3], ay[3], az[3], h[3]; uint32_t shape; char name[16]; float apex[3]; uint32_t pad; } ColPad;   // 96 B: + вершина прыжка
typedef struct { float p[3], r; char name[16]; } ColWrench;   // 32 B: гаечный ключ (ActionEvent Wrench): точка и радиус подбора
// Правка 166 (JMP1 reserved[3], в порядке будок): ActionEvent-локатор будки — положение loc (над будкой: значок phone_icon, начало
// кадра камеры phonecamera) и строки его матрицы ax/ay/az (оси движка); место машины — CarStart того же имени: car, курс движка carH;
// flags бит 0 — CarStart найден
typedef struct { float loc[3], ax[3], ay[3], az[3], car[3], carH; uint32_t flags, pad[3]; } ColPhoneX;   // 80 B

#define COL_WALK_NY 0.5f      // n.y больше — по поверхности можно ехать (земля); меньше — стена
#define COL_MAX_CONTACTS 8
#ifndef COL_HUMP
#define COL_HUMP 0.75f        // горб-капсула (лежачая, не куст): торчит из земли рядом не больше этого — рельеф, не стена
#endif
#define COL_HUMP_DS 1.0f      // шаг замера земли вдоль оси горба, м
#define COL_HUMP_LEN 8.0f     // горбом считается только длинная лежачая капсула (кромка насыпи, бровка), короткие — предметы
#ifndef COL_RAMP_RIDGE_OLD
#define COL_RAMP_RIDGE_OLD 0  // 1 — прежнее: стена трамплина-ящика только по одной верхней грани (конёк дома — отвесная стена)
#endif
#ifndef COL_RIDGE_EDGE_OLD
#define COL_RIDGE_EDGE_OLD 0  // 1 — прежнее: ребро конька (две проходимые верхние грани ящика) не опора днища (colBoxEdgesUnderPlane)
#endif
#ifndef COL_RIDGE_MIN
#define COL_RIDGE_MIN 2.0f    // конёк (две проходимые верхние грани ящика): обе не уже этого, м — для стены трамплина и для опоры днища
#endif
#ifndef COL_RAMP_STEP
#define COL_RAMP_STEP 0.4f    // трамплин (наклонный ящик с проходимым верхом): не стена, пока верх ниже пола + lo + это
#endif
#ifndef COLPROBE_FAST_OLD
#define COLPROBE_FAST_OLD 0   // 1 — без быстрого отсева непроходимых треугольников и заборов в gather (прежнее поведение)
#endif

typedef struct ColScratch ColScratch;

typedef struct {
  const ColTri *tri; const ColBox *box; const ColCyl *cyl; const ColSeg *seg;
  const uint32_t *cellStart, *refs;
  float ox, oz, cell, inv;
  int W, H;
  uint32_t ntri, nbox, ncyl, nseg, nref;
  ColScratch *scr;            // рабочая память (штампы, заборы, фигуры запроса) — выделяется один раз в colInit
  const ColZone *zone; const ColPad *pad; uint32_t nzone, npad;   // блок JMP1 (нули — в уровне его нет)
  const ColWrench *wrench; uint32_t nwrench;                      // JMP1 reserved[0]: гаечные ключи (пункт 4)
  const ColWrench *phone; uint32_t nphone;                        // JMP1 reserved[1]: телефонные будки (правка 62), формат ключа
  const ColPad *death; uint32_t ndeath;                           // JMP1 reserved[2]: зоны DEATH (правка 86): объём, apex — точка возврата
  const ColPhoneX *phonex; uint32_t nphonex;                      // JMP1 reserved[3]: будки+ (правка 166), nphonex == nphone или 0
  uint32_t npocket; uint32_t pocketSeg[32]; float pocketSide[32]; // №590 (общее правило K3-3): звенья забора покрышек у зоны DEATH R6D01 и сторона «за забором»: -1/+1 по нормали звена
} ColWorld;

// Кузов машины для colPushCar: прямоугольник в xz + твёрдый слой по высоте над «полом» (плоскость под колёсами).
typedef struct {
  // вход
  float x, z;                 // центр прямоугольника
  float fx, fz;               // единичная ось «вперёд» в xz
  float hw, hl;               // полуширина (вдоль оси вправо = (-fz, fx)), полудлина (вдоль fx, fz)
  float y, gx, gz;            // пол: Y(X,Z) = y + gx*(X-x) + gz*(Z-z)  (x, z — центр на момент вызова)
  float lo, hi;               // твёрдый слой кузова: Y+lo .. Y+hi (например 0.35 .. 1.6)
  // правка Роба 339 (05.10, L1 F4-8): опора машины для внутренних заборов (конечный верх, правка 47). fsupOn = 1 — машина на земле
  // (колёсами или днищем) на высоте fsup: забор для неё не ниже fsup + COL_FENCE_HOLD — по опоре через линию не проехать, какой бы
  // высоты ни была опора (труба, ящик-пандус, видимая земля). В полёте (fsupOn = 0) — верх из данных: над крышей пролетает, как было
  float fsup; int fsupOn;
  // выход
  int n;                                          // число контактов (по одному на примитив)
  float nx[COL_MAX_CONTACTS], nz[COL_MAX_CONTACTS];  // нормаль контакта: от препятствия к машине, единичная, xz
  float depth[COL_MAX_CONTACTS];                  // на сколько вытолкнули по этой нормали (сумма по итерациям)
  unsigned char type[COL_MAX_CONTACTS];           // COL_TRI / COL_BOX / COL_CYL / COL_SEG
  uint32_t index[COL_MAX_CONTACTS];               // primitive identity; distinguish adjacent materials
  float maxDepth;             // colPushCar: сколько осталось после выталкивания (0 = чисто); colProbeCar: худшее
  int behindFence;            // центр кузова оказался за забором (с изнанки)
  int nshapes;                // сколько примитивов попало в узкую фазу (для статистики)
} CarBox;

// Секция COL1 в памяти (указатель выровнен на 4, данные живут, пока жив мир). 0 = ок, <0 = ошибка формата/памяти.
int colDeathBelow(const ColWorld *w, float x, float y, float z, float maxDrop);   // зона DEATH под точкой не дальше maxDrop вниз (индекс или -1)
int colDeathAt(const ColWorld *w, float x, float y, float z, float grow);   // правка 86: зона DEATH с точкой, -1 нет
// Правка 349: у локатора DEATH бывает несколько объёмов (R6D01 горящих покрышек — два ящика, двор за забором целиком) — в JMP1
// они идут подряд записями с тем же именем и точкой. first — только первый объём локатора (как до 349). Курс возврата — pad записи:
// float курс движка + 16 (π − курс строки 2 LocatorMatrix); 0 — курса нет (прежние level.shr)
int colDeathAtEx(const ColWorld *w, float x, float y, float z, float grow, int first);
int colDeathFirst(const ColWorld *w, uint32_t i);                      // запись i — первый объём своего локатора
int colDeathHeading(const ColWorld *w, uint32_t i, float *h);          // 1 — у точки возврата есть курс (h)
extern int colGatherCacheOld;   // №984: 1 — без кэша «пусто» широкой фазы кузова (flag colcache984old, COL_GATHERCACHE984_OLD=1)
extern int colRampRail1050;   // №1050 (Б): у бортов большого трамплина L5 нет торцов (flag ramprail1050 0 — как было)
extern int colRamp1050;   // №1050: 1 — торец тонкой стенки не гасит машину в ноль, а отводит вбок (RAMP1050_OLD=1 / flag ramp1050 0 — как было)
int colInit(ColWorld *w, const void *section, unsigned size);
void colFree(ColWorld *w);
const char *colError(int code);

// Самая высокая проходимая поверхность в точке (x, z) с высотой в [yBottom, yTop]: треугольники земли с n.y > 0.5,
// верхние грани ящиков / торцы цилиндров / купола капсул и сфер, если там n.y > 0.5. 0 = ничего нет.
int colGround(const ColWorld *w, float x, float z, float yTop, float yBottom, float *outY, float *outN3);
// то же + материал найденной опоры (COL_MAT_HARD / COL_MAT_FOLIAGE — верх куста); outMat может быть NULL
int colGroundMat(const ColWorld *w, float x, float z, float yTop, float yBottom, float *outY, float *outN3, int *outMat);

// Same query plus identity of the actual support: type << 29 | index.
int colGroundMatId(const ColWorld *w, float x, float z, float yTop, float yBottom, float *outY, float *outN3, int *outMat, uint32_t *outRef);

// Вытолкнуть кузов из всего, что пересекает слой [lo, hi]: крутые треугольники, ящики, цилиндры/капсулы/сферы
// (срез слоем, проекция в xz, SAT), заборы (односторонние: к стороне нормали). Итерации: сначала самый глубокий
// контакт, потом пересчёт (так стыки соседних стен не цепляют). box->x, box->z сдвигаются в итоговое положение;
// *mtvX, *mtvZ — суммарный сдвиг; *nX, *nZ — нормаль самого глубокого контакта. Возврат: число контактов.
// правка 122: заборы трассы уличной гонки поверх мира (машина их чувствует, как заборы уровня); NULL — снять; массив живёт у вызывающего
void colExtraFences(const ColSeg *segs, unsigned n);
typedef struct { float p[3],n[3],depth; uint32_t ref; } ColSphereContact;
extern int colWheelPreOld;   // 1: без предотсева в колесо-стена (гейт tests/cars/wheel_pre.c, mcrprof)
int colWheelWallContacts(const ColWorld *w,const float p[3],float radius,ColSphereContact *out,int max);
// Правка 388/377 (08.10): верхние рёбра ящиков коллизии под прямоугольником днища — узкая опора поперёк машины (ребро балки между
// точками днища). o — центр прямоугольника (мир), u — его нормаль (ось вверх тела), f/l — оси, hl/hw — полуразмеры. Контакт — точка
// ребра, глубже всех зашедшая над плоскостью днища внутри прямоугольника, но не выше dmax (выше — стена, дело слоя кузова): n = u,
// depth — глубина вдоль u. Только ящики с проходимым верхом (n.y > COL_WALK_NY), листва — нет (точки днища берут её мягко)
// maxWidth — только узкие ящики: верхняя грань уже этого по меньшей стороне (балки, парапеты, бордюры-рёбра: колёса на них не встают
// обе сразу) и почти ровная (n.y ≥ 0,9); широкие и наклонные плиты (откосы, обломки) остаются делом точек днища (08.10: иначе
// J11-9 садится на наклонную плиту 927 иначе, чем оригинал, а у cyl 582 угол носа задевает ребро откоса 1363)
int colBoxEdgesUnderPlane(const ColWorld *w,const float o[3],const float u[3],const float f[3],const float l[3],float hl,float hw,float dmax,float maxWidth,ColSphereContact *out,int max);
int colPushCar(const ColWorld *w, CarBox *box, float *mtvX, float *mtvZ, float *nX, float *nZ);

// То же без сдвига: худшая глубина проникновения в box->maxDepth, контакты — в box. Возврат: число контактов.
int colProbeCar(const ColWorld *w, CarBox *box);
int colContactMaterial(const ColWorld *w, const CarBox *box, int contact);
// есть ли объём-куст (материал FOLIAGE) ближе margin к точке (x, y, z): забор по изгороди — удар куста (правка 71)
int colFoliageNear(const ColWorld *w, float x, float y, float z, float margin);
// Правка Роба 173 (01.10, L1 J9-4: «перестань делать из кустов стены — хочу проехать через кусты»): машина проезжает кусты
// (объёмы FOLIAGE) и изгороди насквозь. Изгородь — забор, помеченный экспортом (y0 < COL_SEG_FOL_Y0: над землёй вдоль него
// только листва, tools/psp_export.py col_fence_tops) или лежащий у куста (COL_FOLIAGE_FENCE). car.c включает на время своего
// шага: сбор препятствий (colPushCar/colProbeCar) и опора (colGround*) их пропускают; colFoliageHits — сколько раз за шаг
// машина прошла сквозь листву (вязнет и шуршит). Гомер и граница карты — как были.
#define COL_SEG_FOL_Y0 (-1.5e6f)
void colSetFoliagePass(int on);
// правка 339: 1 — прежнее (опора fsup не поднимает внутренний забор; красный гейта tests/pockets). По умолчанию 0 (-DCOL_FENCE_SUP=0 — 1)
extern int colFenceSupOff;
int colFoliageHits(void);
// правка 304: доля кузова (0..1) внутри листвы — объёмов FOLIAGE и полосы ±COL_FOL_FENCE_HALF у заборов-изгородей; car.c вязнет
// пропорционально ей (сетка 5 × 3 точек на двух высотах: у колёс и середина кузова). Не зависит от colSetFoliagePass
float colFoliageCover(const ColWorld *w, const CarBox *b);
#define COL_FOL_WHEEL_Y 0.1f     // нижняя высота проб листвы над полом кузова: низкая изгородь (в H10-5 торчит на 0.4 м) задевает колёса и днище
#define COL_FOL_FENCE_HALF 0.7f  // полутолщина изгороди у забора-изгороди: радиус лежачих объёмов l*_hedgeunit в данных (0.70 м)
#define COL_FOLIAGE_FENCE 1.5f   // забор не дальше этого от куста — его часть (в L1 заборы у 56 изгородей лежат в 2 м от оси, следующие — от 3 м)
// Правка Роба 200, остаток сверки tests/humps/sweep.c (03.10): капсула-горб, на которую кузов въехал с горбатой стороны, остаётся горбом, пока
// кузов над ней (ось ближе радиуса к прямоугольнику кузова), — без порога-перескока «горб/стена» под стоящим на ней кузовом.
// car.c на свой подшаг задаёт список прошлого подшага (colHumpKeep) и забирает новый (colHumpRiding); вне шага машины — пусто
#define COL_HUMP_KEEP 4
// правка 304: заборы-изгороди, пропущенные кузовом на прошлом подшаге, остаются пропущенными, пока в досягаемости кузова (решение по
// ближней точке забора не меняется под стоящим на нём кузовом); car.c задаёт список прошлого подшага и забирает новый, как горбы
#define COL_FOL_KEEP 8
void colFolKeep(const uint32_t *idx, int n);
int colFolRiding(uint32_t *idx, int max);
// Сверка PS2 09.10 (docs/handoff/СТЕНЫ_СВЕРКА_PS2_2026-10-09.md, семья «забор с изнанки»): забор Wall оригинала держит машину только
// с лица (сторона нормали). Кузов, чей центр за линией (изнанка), забор не держит — машина выезжает на лицо, как на PS2; забор,
// пропущенный так на прошлом подшаге, остаётся пропущенным, пока кузов стоит на линии (выступает за неё). car.c включает на время
// своего шага (colSetFenceBack), память — как у изгородей (colBackKeep / colBackRiding). Гомер и пробы вне шага машины — как были.
// -DCOL_FENCE_BACK_OLD=1 — прежнее (изнанка толкает дальше на изнанку; красный tests/walls/ps2_placement_gate.sh)
#define COL_BACK_KEEP 8
void colSetFenceBack(int on);
void colBackKeep(const uint32_t *idx, int n);
int colBackRiding(uint32_t *idx, int max);
void colHumpKeep(const uint32_t *idx, int n);
int colHumpRiding(uint32_t *idx, int max);

// Луч a -> b (правки Роба 40, 27: камера не заходит в стены): доля пути [0, 1] до первого препятствия COL1 — треугольники
// земли и стен (с обеих сторон), ящики, цилиндры/капсулы/сферы, заборы (вертикальная стена по отрезку). 1 = свободно.
// Объём, внутри которого начинается луч, не мешает; кусты (материал FOLIAGE) — только при foliage != 0.
float colRayFrac(const ColWorld *w, const float a[3], const float b[3], int foliage);
// CAMPERF: n лучей за один обход клеток (out[i] == colRayFrac(w, a[i], b[i], 0) для каждого; куски вне коробки лучей отсеиваются ящиком). n <= COL_RAYS_MAX
#define COL_RAYS_MAX 24
void colRaysFrac(const ColWorld *w, int n, const float (*a)[3], const float (*b)[3], float *out);

// Путь точки (x0,z0) -> (x1,z1) пересекает забор с лицевой стороны на изнанку? 1 = да (нормаль забора в *nX, *nZ).
int colCrossFence(const ColWorld *w, float x0, float z0, float x1, float z1, float *nX, float *nZ);
// То же на высоте кузова: забор с конечным верхом (ColSeg y1 < 1e6 — внутренний забор, за ним есть опора; правило
// tools/psp_export.py col_fence_tops, правка Роба 47) не мешает пути, если слой [ylo, yhi] целиком выше его верха.
// Настоящие границы карты (за забором пустота) остаются бесконечными (y0/y1 = ∓1e6), закон 3 очереди.
int colCrossFenceY(const ColWorld *w, float x0, float z0, float x1, float z1, float ylo, float yhi, float *nX, float *nZ);
// то же, слой кузова — по его наклонному полу в точке пересечения забора (b: y, gx, gz, x, z, lo, hi)
int colCrossFenceBox(const ColWorld *w, float x0, float z0, float x1, float z1, const CarBox *b, float *nX, float *nZ);
// слой кузова по высоте для colCrossFenceY — так же, как его берёт colPushCar: [пол + lo − наклон, пол + hi + наклон]
void colBoxBand(const CarBox *b, float *ylo, float *yhi);

// Блок JMP1. Точка (x, y, z) внутри объёма, раздутого на grow (м) во все стороны: 1 / 0. Верх объёма — мировая y.
int colZoneContains(const ColZone *v, float x, float y, float z, float grow);
float colZoneTop(const ColZone *v);
// первая зона прыжка / подкидывающая точка, содержащая точку (grow — запас), -1 — нет
int colJumpZoneAt(const ColWorld *w, float x, float y, float z);
int colBouncePadAt(const ColWorld *w, float x, float y, float z, float grow);

#ifdef __cplusplus
}
#endif
#endif
