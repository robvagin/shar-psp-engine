// Частицы: родные системы SHAR из build/fx.shr (tools/fx_pack.py, формат FXP1, разбор — docs/fx_spec.md).
// Эмиттеры с кривыми по времени системы и по жизни частицы, смешивание alpha/add/subtract, кадры текстур (полосы),
// биллборды взрыва, фоновые системы уровня (пар, факелы — включаются сами в радиусе 60 м от камеры),
// кодовые эффекты (искры spark4, пыль/сколы cloud) и блёстки (scratch2/glow2, аддитивно).
// Без fx.shr рядом с EBOOT всё работает вхолостую (ничего не рисуется, вызовы безопасны).
#ifndef FX_H
#define FX_H

#define FX_MAX 400          // пул частиц (бюджет docs/fx_spec.md 5.3)
#define FX_MAX_INST 32      // одновременно живущих систем

// Старый API (одна частица): SMOKE/DARK/FIRE = стадии car_damage3 (светлый дым, чёрный дым, огонь fireSeq),
// SPARK = искра spark4 (скорость задаёт вызывающий), PAINT = скол краски, DUST = клуб пыли cloud.
enum { FX_SMOKE, FX_DARK, FX_FIRE, FX_SPARK, FX_PAINT, FX_DUST };

extern float fxVel;         // FX_VEL: м/кадр данных -> м/с (по умолчанию 30), гравитация масштабируется fxVel²

void fxInit(int level);                     // грузит fx.shr уровня из папки EBOOT (cwd; уровни 2–7 — l<N>/fx.shr), без него — пусто
int  fxLoad(const char *path);              // явный путь (1 = ок)
void fxFree(void);                          // выгрузить пакет уровня (смена уровня); потом fxInit(N)
int  fxReady(void);
void fxSpawn(int kind, float x, float y, float z, float vx, float vy, float vz);
// Пыль из-под ног Гомера (правка Роба 45 «из-под ног идёт дым»): клуб code_dust (cloud.tga — у оригинала слот 2 частиц,
// leveli.mfk уровня 1: SetParticleTexture(2, "cloud.tga") «dust cloud when running/jumping»), не дым машины car_damage3.
// kind: FXD_LAND — приземление (power = скорость падения, м/с; пыль только с высоты, power > FXD_LAND_MIN),
// FXD_BOUNCE — взлёт с подкидывающей точки, FXD_STEP — шаг бега (один маленький клуб у ступни, запись PS2 18:13).
// Число частиц и разлёт — правило порта (сверить с эталоном PS2)
enum { FXD_LAND, FXD_BOUNCE, FXD_STEP };
#define FXD_LAND_MIN 6.f
int  fxHomerDust(int kind, float x, float y, float z, float power);   // -> сколько частиц пущено (0 — нет пакета/тихо)
void fxUpdate(float dt);
// cam: положение глаза и единичное направление взгляда. Сбрасывает состояние GE (вызывать resetState после)
void fxDraw(float ex, float ey, float ez, float fx, float fy, float fz);
// Правка 313 (Media Engine): fxDraw = fxDrawBegin + fxDrawEnd. Между ними главный делает своё (main.c: отложенное полупрозрачное),
// а задание частиц кадра (шаг жизни + сборка биллбордов, viewer/fxdsp.h) считает ME; без ME — fxDrawEnd считает сам, как раньше.
// Между Begin и End — никаких вызовов fx* (задание в работе)
void fxDrawBegin(float ex, float ey, float ez, float fx, float fy, float fz);
void fxDrawEnd(void);
void fxLab941(const float c[3], const float R[3], const float U[3], float s, int kind);   // №941 служебно: лист форм (flag glintlab)
void fxSetMeMax(int n);                       // ME-доля B: предел элементов задания частиц на ME (8..400)
int  fxMeAttach(void);                       // viewer/me/meprobe.c (флаг-файл mefx): служба ME поставлена — частицы на ME
int  fxMeStateStr(char *out, int n);         // для журнала: «on…» / «off:…» / «-»
void fxProfJob(unsigned *us, unsigned *n);   // замер (main.c «flag meprof»): мкс счёта задания частиц на главном и кадров с прошлого вызова
// правка 103: room(байт) = 0 — в списке GE кадра нет места (viewer/main.c geRoom): группа частиц этого кадра не рисуется
void fxSetGeRoom(int (*room)(unsigned bytes));
int  fxCount(void);                          // живых частиц
extern float fxFill;       // заливка частиц прошлого кадра, пикселей экрана (для замеров)
extern int fxSkipped;      // ближних частиц, не нарисованных из-за бюджета заливки
extern int fxCutout;       // №731 (Роб 17.10): полупрозрачные частицы (дым, вода гидранта) — вырезкой по краю альфа-тестом, как спрайты Doom; 0 — смешиванием, как раньше
extern int fxSoftSmoke953;   // №953: дым (smoke1/2/3/3alph) мягким смешиванием, без вырезки (SMOKE953_OLD=1 — вырезка)

// Системы. id — номер системы в fx.shr; dir — куда смотрит ось +Y системы (NULL = вверх).
int  fxFind(const char *name);               // по имени фабрики ("car_damage3SysShape"), -1 = нет
int  fxFindInst(int type);                   // InstParticleSystem по номеру типа (14 дым машины, 19 огонь, 9 пыль…)
int  fxStartSystem(int id, const float pos[3], const float dir[3]);   // -> handle (>0) или 0; петли живут до fxStop
int  fxBurstAt(int id, const float pos[3], const float dir[3], float frame);   // правка 80: один раз, с кадра frame системы
int  fxStart(const char *name, const float pos[3], const float dir[3]);
void fxMove(int h, const float pos[3], const float dir[3]);           // для систем на машине
void fxSetRate(int h, float k);              // множитель RAT (стадии дыма), 0 = не рождать
void fxSetEmitRate(int h, int emitter, float k);   // множитель одного эмиттера (car_damage3: 0 дым, 1–2 огонь)
void fxStop(int h);                          // перестать рождать, частицы доживают
int  fxAlive(int h);
// Разовые: проиграть таймлайн один раз (даже у петли). Обломок = все его фабрики + биллборды (24 взрыв машины,
// 3 гидрант, 14 ель, 15 дуб, 19 витрина Krusty, 20 кипарис, 8 томако).
int  fxBurst(int id, const float pos[3], const float dir[3]);
int  fxBurstBreakable(int type, const float pos[3], const float dir[3]);
int  fxBurstBreakableYaw(int type, const float pos[3], float yaw);   // №923: то же, система повёрнута вокруг вертикали (осколки по ходу машины); yaw = atan2(vx, vz)
void fxExplosionOld(int on);                // №391: отрицательный контроль ориентации кольца по старому ROT

// Блёстки/свечение (аддитивно, к камере, только на этот кадр). phase 0..1 — пульс и поворот.
void fxSparkle(float x, float y, float z, float size, float phase);
extern int fxGlint976Old;   // №976: 1 — кольцо цели и звёздочки столба как в TEST66; flag glint976old
extern int fxGlintOne970;   // №970: следующий блик очереди — одна звезда (маленькие звёзды монет)
// №989 (Роб, TEST70 на PSP: «блинг-блинг штучки на объектах в необычных местах уровня слишком яркие, в 2–3 раза прозрачнее»): блёстки над гэгами, свечения предметов и бонус-объектов, блики выключателей —
// прозрачность BLING989_K от прежней (1 -> .4, форма и анимация те же); ключ, будка, монеты, столб цели, огни и значки миссий не тронуты (настроены №908/№910/№965/№970/№976).
// BLING989_OLD=1 / файл bling989old рядом с EBOOT / «flag bling989 0» — как в TEST70
#ifndef BLING989_OLD
#define BLING989_OLD 0
#endif
#ifndef BLING989_K
#define BLING989_K .4f
#endif
extern int fxBling989;
#define BLING989_A (fxBling989 ? BLING989_K : 1.f)
void fxSparkleCA(float x, float y, float z, float size, float phase, unsigned abgr, float alpha);
extern unsigned fxCoinStat[4];   // №1020 служебно (журнал, приёмка): монет с блёстками, блёсток, наибольшее число блёсток у монеты за кадр, монет с блёстками в кадре
extern int fxCoin1020;   // №1020: блики монет — не больше двух на монету, случайно по зерну монеты (flag coin1020); сдвиг кадра оборота спрайта у каждой монеты
void fxSparkleCoin1020(float x, float y, float z, float size, float age, unsigned seed, float ex, float ey, float ez);
extern int fxGlint970Old;   // №970: 1 — блики как в TEST66 (ключ, будка, монеты, огни машин); flag glint970old
void fxSparkleCoin(float x, float y, float z, float size, float phase, float ex, float ey, float ez);   // №910: блёстки монет прозрачнее
void fxSparkleC(float x, float y, float z, float size, float phase, unsigned abgr);
// №839 (Роб 19.10: «все блюрные свечения — нашим векторным бликом, везде по тому же принципу»): блик вместо размытой картинки — ставит main
// (spritelight.h glowGlintQ_: очередь кадра, сброс после частиц). x, y, z — центр, size — размер блика (м), abgr — цвет, a — альфа 0..1, phase — фаза пульса
extern void (*fxGlowGlint)(float x, float y, float z, float size, unsigned abgr, float a, float phase);
#ifndef GLOW_GLINT_OLD
#define GLOW_GLINT_OLD 0   // 1 — свечения картинкой, как до №839
#endif
int  fxTexFind(const char *name);            // "scratch2.bmp", "glowsolid.bmp", "glow64.bmp", "bluestarglow.bmp"…
// правка 249: привязать текстуру пакета (CLUT 8888 + индексы) для своего рисования (следы шин viewer/skid.c); 0 — нет пакета/номера
int  fxTexBind(int tex);
// Произвольный спрайт на этот кадр: blend 0 alpha, 1 add, 2 subtract; flat=1 — лежит на земле (XZ)
extern int fxAddStatOn; void fxTexStat(char *out, int n);   // №899 (служебно): строка FXTEX
void fxSprite(int tex, float x, float y, float z, float w, float h, float rot, unsigned abgr, int blend, int flat);

#endif
