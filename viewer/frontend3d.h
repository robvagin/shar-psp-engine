// frontend3d: 3D-гостиная главного меню оригинала (Scrooby 3dFE.pag) из build/fe.shr (tools/frontend3d_export.py,
// формат FE1 описан в шапке экспортёра). Комната camset + Гомер меню (скин, цикл 61 кадр) + экран ТВ с помехами +
// машинка на треке + подсветка Glow текущего пункта (номер пункта = номер строки MainMenu.pag = номер Glow).
// Камера оригинала FE_Camera стоит в «главном виде» (кадр меню из заголовка файла), 16:9 Hor+ (вертикаль 4:3);
// по × летит к предмету пункта (feFly, ниже). Порядок отрисовки — как на странице 3dFE.pag: комната, подсветки ТВ и
// дивана (флаг F_PREHOMER), Гомер, остальные подсветки — Гомер закрывает подсветку дивана, на котором лежит.
//
//   feLoad(path)       вход в меню: файл целиком одной аллокацией + плеер Гомера; 0 = ок (нет файла — меню без фона)
//   feDraw(sel)        каждый кадр меню, внутри открытого списка GE, ДО интерфейса; свои часы (sceKernelGetSystemTimeLow)
//   feFree()           выход из меню: всё освобождено (куча как до feLoad)
//
// СОСТОЯНИЕ GE, которое меняет feDraw (остальное не трогает): матрицы GU_PROJECTION/GU_VIEW/GU_MODEL, тест глубины
// GEQUAL (как main.c) + очистка глубины в 0, запись глубины, текстуры (T8 + CLUT 8888, мипы, фильтр, wrap repeat,
// TexScale 128, TexFunc modulate RGBA), альфа-тест (GREATER 0x40), смешивание (альфа / сложение), освещение и
// источники 0..1 (только на время Гомера; на выходе выключены), кости 0..7, туман выключен, отсечение граней выключено,
// выбор мипа (правка 265: на время кусков GU_TEXTURE_SLOPE и регистр наклона sceGuTexSlope; на выходе снова
// GU_TEXTURE_AUTO с наклоном 0, как раньше; регистр наклона остаётся последним значением гостиной).
// После feDraw main.c вызывает resetState() (его кэш привязанной текстуры устарел); ui.c задаёт своё сам (beginUi).
//
// Правка Роба 265 (04.10, повтор 244, настоящая PSP): «NEW GAME, наезд к газете — за долю секунды до конца пол комнаты
// на мгновение пропадает, зелёный полигон». Пол гостиной — floor3.bmp 256×256: бирюзовый ковёр с запечёнными тенями
// мебели и затемнением к краям; у его самого мелкого мипа (2×2) разброс 8 из 255 — ровный бирюзовый треугольник без теней.
// В AUTO GE выбирает мип по экранным производным; на железе — один на треугольник (правки 77/88: ступеньки между
// треугольниками асфальта), в эмуляторе — на квад 2×2 (PPSSPP Rasterizer.cpp CalculateSamplingParams), поэтому эмулятор
// пропажу не показывает. В конце наезда треугольники пола тянутся от газеты (0,9 м) за край кадра к дальней стене (62/3: на
// экране 4,5–7 %, его рамка — до 44 %): одна оценка на треугольник уходит далеко от настоящей (модель «в углу рамки»: уровень
// 3–5,5 в подкадрах 35,5 и 38,75–40 — последние 0,04–0,15 с, tests/fe_guard/mip_model.py). Правило (все куски гостиной с мипами):
// мип по глубине пикселя — GU_TEXTURE_SLOPE, наклон куска = плотность его текстуры × размер пикселя (как город, правки
// 77/88; у оригинала PS2 GS уровень мипа тоже только по глубине пикселя: −log2(Q)·2^L + K, регистр TEX1 —
// build/twins/pcsx2/src/pcsx2/GS/Renderers/SW/GSDrawScanline.cpp:746). Уровень = log2(тексели на пиксель лицом) + FE_MIP_LIFT.
// Подсветки (билборды) и Гомер — как раньше (AUTO / без мипов).
#ifndef FRONTEND3D_H
#define FRONTEND3D_H

extern int feMipOld;       // служебное (smoke flag feold265 N): 1 — прежний выбор мипа (AUTO, красный контроль 265);
                           // 2 — имитация того, что видел Роб: самый мелкий мип у всех кусков (красный контроль гейта картинки)
extern int feSweep;        // служебное (smoke flag fe265sweep 1): следующий перелёт «туда» идёт покадрово — один кадр рисования =
                           // одна поза камеры (кадры go0..go1−10 через 1, дальше через 0,5), проход 0 — прежний мип (AUTO),
                           // проход 1 — текущий (feMipOld); часы гостиной стоят; метка позы 72×4 px у x 60, y 40 (frontend3d.c
                           // sweepMark). Каждый показанный кадр гостиная пишет сама (sweepDump, через msio): папка fe.shr/shots,
                           // fe265_a|b_<кадр×100>.bmp + fe265_sweep.txt — одинаково на PSP и в эмуляторе; читает
                           // tests/fe_guard/sweep_read.py. После прохода выключается сам (перелёт кончается в go1)

int  feLoad(const char *path);
extern int feFailTest;   // №841 тест: «flag fefail N» — следующие N чтений fe.shr как без памяти (memalign вернул 0)
// №841: память fe.shr (5,2 МБ одним куском) — крючок main: при выгруженном уровне участок уровня lva (сплошной всегда), иначе куча; 0 — memalign/free
extern void *(*feAllocHook)(unsigned size); extern void (*feFreeHook)(void *p);
void feFree(void);
int  feReady(void);
void feDraw(int sel);
// крючок хода чтения fe.shr (кусками по 256 КБ): main.c рисует экран загрузки меню; 0 — без него
extern void (*feReadHook)(unsigned done, unsigned total);
extern int feHero1021;   // №1021-герои: «flag fehero1021 0|1» — Гомер и герои меню в Cartoon путём игры; файл fehero1021old — выкл
extern int feGlowTest;     // гейт (smoke flag feglow N): подсветка пункта N вместо sel; -1 выкл (feLoad сбрасывает)

// Перелёты камеры оригинала (правка Роба 28.09): таймлайн FE_CameraController (camset.p3d, 1036 кадров @ 30) содержит
// отрезки «главный вид -> предмет пункта [-> главный вид]»; какой отрезок к какому пункту — из данных (экспорт: что в
// центре кадра в момент прилёта и какая подсветка Glow закрывает это место из главного вида), таблица FLT1 в хвосте
// fe.shr. Изинг — сами кадры данных (скорость камеры 0 -> 42 см/кадр -> 0), между кадрами — линейно по времени.
// Подсветки Glow — плоскости перед предметами, совпадают с ними только из главного вида: вне него не рисуются.
enum { FE_GO = 0, FE_BACK = 1, FE_ROUND = 2 };   // туда / обратно / туда и сразу обратно (неготовый пункт)
int   feFlight(int item, int *hasBack);   // 1: в данных есть перелёт к предмету пункта item (*hasBack: и обратный)
int   feFly(int item, int mode);          // запуск перелёта (FE_*); 0 — такого перелёта в данных нет (камера не двигается)
int   feFlying(void);                     // 1, пока перелёт идёт (feDraw двигает время)
void  feHome(void);                       // камера сразу в главный вид (кадр меню), перелёт остановлен
float feFrame(void);                      // текущий кадр таймлайна (для смоука)

typedef struct {
  unsigned bytes;          // размер fe.shr (одна аллокация) + плеер Гомера
  float loadMs;            // чтение файла
  int pieces, tris, bills; // нарисовано в последнем кадре (куски, треугольники с Гомером, билборды)
  int frames; float fps;   // кадров меню и средняя частота кадров (по интервалам между feDraw)
  float cpuMs;             // средняя работа процессора в feDraw (сборка списка), мс
  int flights;             // перелётов в таблице FLT1 fe.shr (0 — старый файл, камера стоит)
  int gags, gag, gagStarts; float gagT;   // №404: сценок в GAG1, играет (-1 нет), запусков, время клипа, с
  unsigned gagElapsedMs, gagNextMs; int gagPending, gagMenuActive;
} FeStats;
// №404 (служебное, смоук): flag fegag N — сразу сценка N (порядок Gag0..7 страницы 3dFEGags.pag); flag fegagat С — сценка стоит
// на С с клипа (снимок позы; -1 — играет); flag fegagauto 0 — без расписания. feGagLog — строки FEGAG start/end (NULL — выкл.)
extern int feGagForce, feGagAuto, feGagSeg; extern float feGagAt;   // fegagseg K — отрезок K (0..4) сценки Гомера
extern void (*feGagLog)(const char *line);
// PS2 V100: freeze the scheduling clock while flying or outside main menu;
// pending/event180 at next-2000ms, event181 starts the prepared scene.
void feGagMenu(int active);
extern void (*feGagEvent)(int event, int index, int segment);
extern FeStats feStats;

extern void (*feFrameLog)(const char *line);   // правка 293: служебный журнал перелёта по кадрам (FEFR …), NULL — выкл.
// правка 293 (служебное, smoke flag fecap N, тур TEST 10): следующий перелёт «туда» — НАСТОЯЩИЙ (часы идут, кадры не
// стоят), но каждый показанный кадр (вместе с рамкой меню) GE копирует в ОЗУ: первые 2/3 N кадров и последние
// остальные (кольцо). После перелёта feCapFlush пишет их в папку fe.shr/shots: fe293_<номер кадра перелёта>_<кадр×100>.bmp
// и строку FECAP в журнал (через feFrameLog, если он включён). Нет памяти — меньше кадров или ничего (строка FECAP nomem)
extern int feCapReq;
extern int feGlowCut;       // правка 293 (smoke flag feglowcut 1): прежнее — подсветка пункта пропадает в первом кадре перелёта, без угасания
extern int feBillOld;       // правка 325 (smoke flag febillold 1): прежнее — подсветка квадом как есть (у PSP GE выбрасывает свет из окна у газеты)
extern int feCapFrom, feCapTag;   // правка 325 (smoke flag fecapfrom N, fecaptag M): fecap со своего кадра перелёта, файлы fe<M>_*.bmp
void feCapGrab(void *fbAbs, int fb32);    // main.c menuFrame перед sceGuFinish: fbAbs — буфер рисования (адрес VRAM)
void feCapFlush(void);                    // main.c после перелёта: на карту и освободить
#endif
