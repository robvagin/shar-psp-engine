// Маркеры цели и значки заданий миссий из данных диска (build/msnart.shr рядом с EBOOT, tools/msnart_export.py, формат там).
// Запись файла грузится по LoadP3DFile скрипта в тот же момент, что в оригинале: уровень — на старте уровня (missgen:
// сферы carsphere/triggersphere, стрелки, «!» над собеседником), миссия — на её старте (m0: ведёрко icebuck, звезда двери
// kwik_mission_doorstar, значки kwike/apu/icecream/marage), выгружается при старте следующей миссии.
#ifndef MSNART_H
#define MSNART_H

int  msnartInit(const char *dir);                 // открыть таблицу msnart.shr (каталог EBOOT со слешем); 0 = ок
void msnartReset(void);                           // выгрузить всё (NEW GAME)
void msnartShutdown(void);                        // закрыть msnart.shr уровня (смена уровня): таблица, файл, записи
void msnartLoad(const char *p3dPath, int level);  // LoadP3DFile("art\...\x.p3d"): подгрузить запись, если есть
const char *msnartWantIcon(void);                     // №958б: имя значка задания, которого ждёт угол (диагностика)
void msnartMissionStart(void);                    // новая миссия: записи прошлой выгружаются (уровня — остаются)
extern int (*msnartRoom)(unsigned);              // №637: освободить далёкие клетки под запись; 1 — цельный кусок нужного размера есть
void msnartPump(void);                            // раз в кадр: асинхронное чтение очереди записей (sceIoReadAsync)
int  msnartIconPixOk(void);                      // №993: диагностика — пиксели в углу те, что просит миссия
int  msnartImage(const char *name);               // 1 = есть значок задания (SetHUDIcon, msnicons) с этим именем
void msnartSetObjIcon(const char *name);          // значок в рамку задания HUD (ui.c: UI_OBJICON_EXTERNAL); NULL — нет
int  msnartImageData(const char *name, const void **pix, int *tw, int *th, int *w, int *h);   // 1 = картинка (брифинг) загружена
// составной объект: pos = место локатора (NULL — объект смоделирован в координатах мира), t — секунды (анимация квадов).
// Состояние GE после: GU_MODEL = единичная, tex scale 128, блендинг как у main.c, fog выключен -> main.c resetState()
void msnartDraw(const char *name, const float *pos, float ex, float ey, float ez, float t);
void msnartDrawNpcMarker(const char *name, int radarType, const float *pos, float ex, float ey, float ez, float t);
extern int msnartMarkerLodOld;                    // отрицательный контроль №392
extern int msnartGlowBias;
void msnartDrawRot(const char *name, const float *pos, float heading, float ex, float ey, float ez, float t);
void msnartDrawCull(const char *name, const float *pos, float ex, float ey, float ez, float t, float dist);   // меши дальше dist не рисуются (пропы гонки)   // то же, повёрнутое по курсу (стрелки на дороге)
int  msnartHas(const char *name);                 // 1 — составной объект name загружен (04.10: пятно маркера цели <рисунок>_shadow)
extern unsigned msnartLostPics;                   // №512: картинок брифинга, не доехавших с карты (после одного повтора)
unsigned msnartBytes(void);                       // сколько сейчас занято записями (служебный счётчик)
// правка 103 (01.10): меш / квад рисуется, только если msnartGeRoom(байт) = 1 (место в списке GE; NULL — без проверки), иначе
// msnartGeSkips++ (main.c ставит предел GE_HARD бюджета, как у кусков города)
extern int (*msnartGeRoom)(unsigned bytes);
// Добавка Роба 18.10 (спрайты значков): рисунок с меш-суставом и квадом-свечением main.c может нарисовать сам (spritelight.h iconSprHook: «!» и трубка —
// кадр ленты + общий блик). c — центр меша 0 после позы кадра клипа, cr — он же в кадре 0 клипа (поза покоя), bt/br — нижняя точка меша сейчас и в покое,
// pv — её место в кадре 0 ленты от центра (x, y модели, м), tilt — наклон меша в плоскости XY (рад), cf — место в клипе (кадр с долей, по кругу клипа),
// g/gw/gd — центр, ширина и сдвиг от камеры первого квада-свечения; 1 — нарисовано, 3D не рисуется. NULL — всё 3D. MSNART_SPRITE_OLD=1 — всё 3D
extern int (*msnartSprite)(const char *name, const float *c, const float *cr, const float *bt, const float *br, const float *pv, float tilt, float cf, const float *g, float gw, float gd, float ex, float ey, float ez);
extern unsigned msnartGeSkips;
extern int msnartDecal1052;   // №1052: пятно <рисунок>_shadow со сложением не рисуется
extern int msnartArrow1025; extern int (*msnartGroundN)(float x, float z, float yHi, float yLo, float *y, float *n3);   // №1025: стрелки пути по поверхности дороги, без бликов
// №793 (Роб 18.10: «свечение под персонажами с миссией — векторное на всех уровнях; найти другие размытые свечения»): пятно
// «<рисунок>_shadow» (билборд-квадрат на земле, missgen.p3d: exclamation/dice/checkered/dollar_shadow — синий диск сложением,
// carsphere/triggersphere_shadow — голубое кольцо) — замер его картинки: радиус, две ступени яркости по кольцам (≥ 50 % и ≥ 12 %
// пика) и их цвет; рисует векторно main (lightsprite.h vecGlowFlat). 0 — нет такого или не сложение
typedef struct { float rad, in0, in1, out0, out1; unsigned colIn, colOut; } MsnartFlat;   // доли радиуса; цвет ABGR, альфа = яркость ступени
int msnartFlat(const char *name, MsnartFlat *f);
// №805: луч (рисунок с «beam» в имени — UfoBeam L7): билборд к камере сложением рисует main векторным конусом с эллипсом на земле;
// p — центр квада, X — половина ширины (горизонталь), Y — половина высоты, abgr — цвет кадра. 0 — рисовать картинкой, как было
extern int (*msnartVecBeam)(const float p[3], const float X[3], const float Y[3], unsigned abgr);
// №995: свет под тарелкой одним конусом от днища до земли (ставит main: lightsprite.h beam995Draw); ufo — центр тарелки. 1 — нарисован, картинку луча UfoBeam не рисовать; 0 — рисовать, как было
extern unsigned mvBeamUs, mvBeamN;   // №995 служебно (msn_view.c)
extern int (*msnartBeam995)(const float ufo[3], float ex, float ey, float ez);
// №839: размытые свечения значков и предметов (квады сложением) — нашим векторным бликом (ставит main: spritelight.h glowGlintHook); p — центр
// самого крупного квада свечения, w — его ширина, c1/c2 — цвет двух крупных (ABGR кадра), a — альфа кадра. 0 — картинкой, как было
extern int (*msnartGlowGlint)(const char *name, const float p[3], float w, unsigned c1, unsigned c2, float a, float ex, float ey, float ez);
// №944 (служебно): перепись путей бликов взаимодействия — звёзды входа за кадры (старое кольцо / общий шейп) и журнал первого появления картинки со сложением/прозрачностью (name, blend, tex)
void msnartList944(void (*out)(const char *name, int nbill, int nmesh));
void msnartDoorLab944(const float *p, const float *X, const float *Y, unsigned col, float tsec);   // служебно: звезда входа на листе (flag glintlab 2)
extern unsigned g944DoorOld, g944DoorNew;
extern void (*msnartNote944)(const char *site, const char *what, const float *p, int a, int b);
#endif
extern unsigned msaCallN, msaFindUs, msaDrawUs;   // №849 служебно: вызовы msnartDraw и мкс поиска по имени
