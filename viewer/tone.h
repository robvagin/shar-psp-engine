// Кривая тона палитр мира (правка Роба 29, 99, 144): гамма + контраст + насыщенность CLUT при загрузке и при смене настройки.
// Правка Роба 144 (01.10): «чуть повысить насыщенность; в настройках отображения, чтобы можно было донастроить».
// ПРАВИЛО ПОРТА: в данных игры настройки насыщенности нет (библия srr2.p3d: только DISPLAY / Display Mode / Colour Depth для ПК).
// Без копии палитр в ОЗУ (вылет 103 — память): при смене настройки исходная палитра перечитывается из файла, откуда пришёл
// блок (level.shr, traffic.shr, msncars-lN.shr, interiors-lN-gfx.bin), и проходит кривую заново — ползунок обратим.
// Файл без PSP-вызовов: собирается и на Маке (tests/tone/host.c).
#ifndef TONE_H
#define TONE_H

// насыщенность в сотых: 1.00 .. 1.60 шагом 0.04 (16 значений строки Colour в Settings паузы)
#define TONE_SAT_MIN 100
#define TONE_SAT_STEP 4
#define TONE_SAT_N 16
// решено терминалом за Роба (правка 144): по умолчанию 1.36 — «чуть выше» прежних 1.28 (правка 99)
#ifndef TONE_SAT_DEF
#define TONE_SAT_DEF 136
#endif
// мягкий потолок: у ярких цветов множитель насыщенности уменьшается ровно до края (цвет не обрезается по каналу,
// оттенок жёлтого не уезжает в лимонный). 0 — жёсткий кламп по каналам, как было до 01.10 (вкусовой выбор — константа)
#ifndef TONE_SOFT
#define TONE_SOFT 1
#endif

void toneSetGamma(int pct);        // проба калибровки (flag gamma N): out = 255·(in/255)^(N/100) по каналу после насыщенности; 100 — как есть
int  toneGammaPct(void);
void toneSetSat(int pct);          // сотые, зажимается в диапазон строки
int  toneSatPct(void);
int  toneSatIndex(int pct);        // номер значения строки 0..TONE_SAT_N-1
int  toneSatFromIndex(int idx);
void tonePal(unsigned char *pal);  // 256 записей RGBA на месте (альфа не трогается)
void toneColorRGB(unsigned char *c);   // №785: один цвет RGB(A) той же цепочкой, что запись палитры (без стягивания к среднему текстуры): цвета вершин купола неба
void toneColorRGBNT(unsigned char *c);   // №954: цвет RGB(A) цепочкой тона без шагов мультика (палитра неба-текстуры); слой неба-градиент переходит к toneColorRGB к верху
unsigned toneKey(void);           // №785: ключ текущей настройки тона (мультик, насыщенность, гамма, контраст) — пересчитать то, что тонировано по нему
void toneSet1026(int on);   // №1026: вывески (notoon) проходят шаги мультика и контраст
int tone1026Get(void);
void toneSetStat(const unsigned char *st);   // №754: перед tonePal/toneRedo* — средний цвет текстуры и доля рисунка {r, g, b, k·255}; 0 — нет (палитра не стягивается)
void toneSetToon(int on);
void toneToonTex(int pct);        // №773/№777: доля родного рисунка текстуры в мультике, одна для всех: 0 — ровная заливка, 100 — как есть (по умолчанию 35); сменить — toneApplyAll
int  toneToonTexPct(void);
void toneSetContrast(int pct);    // №777: контраст палитр 50..200 (100 — как есть), оба режима; сменить — toneApplyAll
int  toneContrastPct(void);
void toneToonPrep(void);          // проверка 18.10: таблица пресета — заранее (при включении Cartoon)          // №754: пресет «Мультик 90-х» поверх кривой (режим Cartoon); сменить — toneApplyAll
int  toneToon(void);

// блоки памяти, прочитанные из файла целиком подряд: палитра по адресу pal лежит в файле file по смещению off + (pal - base)
typedef int (*ToneRead)(void *user, unsigned file, unsigned off, void *dst, unsigned n);   // 0 — прочитано
void toneBlkAdd(const void *base, unsigned size, unsigned file, unsigned off);
void toneBlkDel(const void *base);
void toneBlkClear(void);
// исходная палитра — заново из файла и через кривую; -1 — блока нет (палитра не тронута), -2 — чтение не удалось
int  toneRedo(unsigned char *pal, ToneRead rd, void *user);
// 16.10: фоновая смена тона без остановки (main_save.h toneJob): то же в отдельный буфер; toneBlkGen меняется при любом добавлении/удалении блока
int  toneRedoTo(const unsigned char *pal, unsigned char *dst, ToneRead rd, void *user);
int  toneReadOrig(const unsigned char *pal, unsigned char *dst, ToneRead rd, void *user);   // проверка 18.10: исходная палитра без кривой
int  toneBlkHas(const unsigned char *pal);
unsigned toneBlkGen(void);

#endif
