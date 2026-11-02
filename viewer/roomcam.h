// Камеры комнат (пункт 11.2, Роб 28.09: «камеры в комнатах — как в оригинале»): данные с диска — локаторы комнаты
// вида 12 (StaticCam) и вида 4 (Spline 0x3000007 + RailCam 0x300000A) со своими TriggerVolume, экспорт —
// tools/interior_export.py (записи 512 Б в блобе комнаты SHG1). Камера снимает, пока Гомер стоит в её триггере.
// Чистый C99 без PSP (проверка на Маке: tests/rooms/cams.sh).
//   StaticCam: позиция — локатор; цель — TargetPosition (точка мира) или, при FollowPlayer, Гомер + TargetPosition
//     (смещение, м); TargetLag — запаздывание цели. RailCam (у всех 129 на диске Behaviour 1): камера на рельсе (ломаная
//     по точкам сплайна) держит горизонтальное расстояние до Гомера в кольце [MinRadius, MaxRadius] — стоит, пока он в
//     кольце, иначе подъезжает к ближайшей точке или отъезжает от неё — до MaxRadius (назад, к началу рельса;
//     ReverseSense — к концу); цель — Гомер + TargetOffset в осях рельса (правка 138: x вбок, y вверх, z вдоль рельса; roomcam.c railLook);
//     PositionLag / TargetLag — запаздывание. FOV — градусы из данных, как у Pure3D — по горизонтали кадра 4:3 (в кадр
//     16:9 переводится по вертикали, Hor+). Смысл полей выведен из данных (кода оригинала нет), он — толкование порта.
//   Декодированы, в движке не используются: TrackRail, TrackDist, AxisPlay (рельс); TransitionTargetRate, Flags, CutInOut,
//     Data (статичная). Смена камеры — срез. Запаздывание L — доля пути к цели за кадр 1/60 с (1 — без запаздывания).
#ifndef ROOMCAM_H
#define ROOMCAM_H
#define ROOM_CAMS 16
#define ROOM_CAM_TRIGS 4
#define ROOM_CAM_PTS 9       // точек рельса на месте в записи (@384); длиннее — массив после таблицы камер (@504 смещение)
#define ROOM_CAM_PTS_MAX 4096   // проверка битого файла, не предел данных (на диске до 13)
#define ROOM_CAM_REC 512
#define ROOM_NEST_RATIO 0.25f   // №229: «вложенная» камера (триггер < 1/4 объёма триггера текущей) берёт кадр сразу (школа camera04, Барт 02); остальные — через ROOM_SWITCH_DELAY от входа
#define ROOM_SWITCH_DELAY 1.f   // правка 172: переход на камеру с запаздыванием в комнате — через 1 с в её триггере (близнец)

typedef struct { int kind; float c[3], half[3], ax[3][3]; } RoomTrig;   // kind: 0 сфера (радиус half[0]), 1 коробка, -1 нет
int roomTrigIn(const RoomTrig *t, float x, float y, float z, float r);   // сфера радиуса r касается объёма (kind -1 — никогда)
int roomTrigRead(RoomTrig *t, const unsigned char *b);                   // 64 Б: u32 вид, центр, полуоси, оси 3x3; 0 = ок

typedef struct {
  char name[32]; int type, ntrig, npts, follow;   // type: 0 статичная, 1 рельс
  float pos[3], target[3], fov, targetLag, posLag, minR, maxR, trackDist;
  int trackRail, reverse, behaviour; float offset[3]; int cut, data;
  RoomTrig trig[ROOM_CAM_TRIGS];
  const float (*pts)[3];         // npts точек рельса — прямо в блобе, из которого разобрана камера (он живёт, пока живёт камера)
  float axisPlay[3];
} RoomCam;
// сверка g06 (02.11.2026): в конце состояния — стек вошедших камер, поза прошлого кадра для переходов (g06-camera:16,19,25,36), выход
typedef struct { int cur; float eye[3], look[3], t; unsigned in; int newWins; int want; float wantT; int switchOld; float inT[ROOM_CAMS];
  int stk[8], nstk, firstDone, trOn, exiting; float trT, trFromE[3], trFromL[3], outE[3], outL[3], exitT, exitE[3], exitL[3]; float lastP[3]; int hasP; } RoomCamState;   // switchOld: 1 — правило задержки 172 до школы (файл roomcamswitchold)   // want/wantT — камера, на которую переходим, и сколько ждём (правка 172)   // cur: камера прошлого кадра (-1 — ходьба); t — место на рельсе
// newWins (правка 186, комнаты): камера, в чей триггер Гомер только что вошёл, берёт кадр, даже если он ещё в триггере текущей
// (близнец v17_house_cam: оригинал переключил на int_sh_path2, когда Гомер был в обоих); in — маска «внутри» прошлого кадра

// таблица камер блоба комнаты RMG1 (заголовок @120 смещение, @124 число): сколько прочитано (0 — нет), <0 — битая.
// Точки рельса не копируются: c->pts указывает в blob — блоб держать, пока нужны камеры (комната: rg.mem, зоны: zcamBuf)
int roomCamsParse(RoomCam *c, int max, const unsigned char *blob, unsigned size);
// камера, в триггере которой точка p (ноги Гомера); текущая остаётся, пока он в её триггере; -1 — ни одной (камера ходьбы)
int roomCamPick(const RoomCam *c, int n, int cur, const float p[3]);
// поза без запаздывания: глаз и точка взгляда для Гомера в p
void roomCamIdeal(const RoomCam *c, const float p[3], float eye[3], float look[3]);
float roomCamFovY(const RoomCam *c);   // вертикальный угол кадра, градусы
// рельс: длина, точка по длине дуги, ближайшая к p, место камеры по поведению Distance (t < 0 — камера только включилась)
float roomRailLen(const RoomCam *c);
void roomRailAt(const RoomCam *c, float t, float o[3]);
float roomRailClosest(const RoomCam *c, const float p[3]);
float roomRailParam(const RoomCam *c, float t, const float p[3]);
void roomRailPose(const RoomCam *c, float t, const float p[3], float eye[3], float look[3]);   // глаз в t и точка взгляда на p
// сверка g06 (g06-camera:1,2,9,12,37): допуск камеры по битам данных. Статичная: Data 0 — всем, бит 1 — только машине (от 20 м/с), бит 2 — только пешком, 3 — никому;
// рельс: AxisPlay.z — бит 1 только машине (от 20 м/с), 2 срез на входе и выходе, 4 сбросить рельс при включении, 8 только пешком. inCar — игрок за рулём, speed — скорость машины, м/с.
// ROOMCAM_FLAGS_SV_OLD=1 — допуск не проверяется (как было)
#define ROOMCAM_CAR_V 20.f
#ifndef ROOMCAM_TRANS_SV_OLD
#define ROOMCAM_TRANS_SV_OLD 0   // 1 — переходы и выход прежние: срез; в main_loop.h работает прежний плавный переход зон (zoneCutOld = 0)
#endif
int roomCamAllows(const RoomCam *c, int inCar, float speed);
#ifndef CUT1085_OLD
#define CUT1085_OLD 0   // №1085: 1 — первая камера после загрузки/переноса героя включается переходом 1 с, как в TEST82
#endif
void roomCamRestart(RoomCamState *s);   // №1085: герой поставлен заново (загрузка, перенос, вход/выход из помещения) — первая камера включается срезом
int roomCamCut(const RoomCam *c);   // срез на входе и выходе: CutInOut статичной / бит 2 рельса / скорость перехода ≥ 1 (g06-camera:25)
float roomCamFovFromH(float hdeg);  // градусы по горизонтали кадра 4:3 -> вертикаль 4:3 (как roomCamFovY)
// угол статичной камеры для машины (g06-camera:12): ближе 10 м максимальный (данные; нет — 90°), дальше 25 м минимальный 20°, между линейно, запаздывание 0,022 за 16 мс.
// *cur — текущий горизонтальный угол (0 — начать с цели); возврат — вертикаль 4:3. Рельс — всегда максимальный
float roomCamFovCar(const RoomCam *c, float dist, float *cur, float dt);
float roomRailParamDt(const RoomCam *c, float t, const float p[3], float dt);   // как roomRailParam, ход по рельсу не быстрее 0,25 отрезка сплайна за 1/60 с (g06-camera:22)
// кадр с переходами оригинала (g06-camera:16,19,25,36): baseE/baseL — поза камеры ходьбы этого кадра (NULL — срез, как roomCamStep). Возврат: номер камеры; -1 — камера ходьбы;
// -2 — камера ходьбы, но идёт плавный возврат от камеры зоны (eye/look — поза кадра)
int roomCamStepFrom(const RoomCam *c, int n, RoomCamState *s, const float p[3], float dt, float eye[3], float look[3], float *fovY, const float baseE[3], const float baseL[3]);
// кадр: выбор камеры и поза с запаздыванием (смена камеры — срез). Возврат: номер камеры или -1 (камера ходьбы)
int roomCamStep(const RoomCam *c, int n, RoomCamState *s, const float p[3], float dt, float eye[3], float look[3], float *fovY);
#endif
