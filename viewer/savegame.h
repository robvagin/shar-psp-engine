// Сохранения как у оригинала (пакет «Сохранения», 28.09): четыре слота, экраны Scrooby оригинала, запись на карту памяти PSP.
//
// Что показывает диск (art/frontend/scrooby, tools/save_export.py):
//   frontend.p3d LoadGame.scr  = TVFrame + BigBoard + SelectMemoryDevice + GameSlots + Back + Accept   (главное меню → LOAD GAME)
//   ingame.p3d   SaveGame.scr  = PauseBgd + BigBoard + SelectMemoryDevice + GameSlots + Back + Accept  (пауза → SAVE GAME)
//   GameSlots.pag: слоты Slot0..Slot3 «< EMPTY SLOT >» (Slot4..7 — в скрытом слое Unused) и текст библии «only supports 4 save
//   games» → слотов 4. SAVE GAME есть только в паузе свободной езды (PauseSunday.pag), в PauseMission.pag его нет.
//   Тексты: MSG_SAVE_CONFIRM_OVERWRITE_(PS2), MSG_SAVING_GAME_(PS2), MSG_SAVE_SUCCESSFUL, MSG_LOAD_CONFIRM_(PS2),
//   MSG_LOADING_GAME_(PS2), MSG_LOAD_SUCCESSFUL, ERROR_DATA_CORRUPT_LOADING_(PS2), ERROR_HW_FAILURE_SAVING_(PS2),
//   MSG_CONFIRM_SAVE_BEFORE_QUIT, MSG_AUTO_LOADING_GAME_(PS2) («Auto-loading most recent saved game» — при запуске игра
//   сама берёт самое свежее сохранение, в меню появляется RESUME GAME). «< Damaged Save >» — строка библии (хеш 527255).
//
// Хранение на PSP — как у настоящих игр: системная утилита sceUtilitySavedata (savemenu.c), папка
// PSP/SAVEDATA/SLES51897<слот 00..03>, файл DATA.BIN, PARAM.SFO (название, строка слота), ICON0.PNG — иконка самой игры
// (ICON0 из EBOOT.PBP). Этот файл — только формат записи: чистый C99, без PSP, общий с проверкой на Маке
// (tests/savegame/host.sh).
//
// Формат DATA.BIN (SV_BYTES байт, little endian, поля по порядку ниже). Голова, байты 0..255: "SHSV", версия 2, размер 256, CRC-32
// (IEEE, по байтам 12..255), дальше поля до skin. Хвост (правка 334, байты 256..767): "SHSX", версия хвоста 1, размер 512, CRC-32
// тела (байты 268..767), тело: once[8] по 52 Б (n u16, sig u16, st 48 Б), ext 84 Б. Голова — ровно запись до 05.10: прежние
// сборки читают новую запись как свою (хвост не видят), новая читает прежнюю (256 Б или нули за головой — разовых нет). Любое
// несовпадение (магия, версия, размер, CRC головы или хвоста, оборванная запись 257..767 Б, выход за границы) — SV_DAMAGED: игра
// не падает, слот показывается «< Damaged Save >», загрузка сообщает об ошибке.
//
// Правила порта (данных нет, решено здесь; сверить с эталоном PS2):
//   строка заполненного слота: «LEVEL N» из библии + дата и время записи (дд/мм чч:мм) — что пишет оригинал, в данных нет
//   сохраняется: пройденные миссии уровня (маска), текущая сюжетная миссия и её чекпоинт (MsnProgress), карточки/осы/гэги
//   всех уровней, кошелёк монет, собранные монеты мира (docs/gameplay.md §1: «собранные сохраняются»), настройки Sound/Settings,
//   с версии 2 — купленные машины/костюмы по уровням (Coins.owned; «купи Plow King» в m3 не просит заново после загрузки),
//   и надетый костюм (правка 107: «костюм не сохраняется между загрузками»; байт после owned, версия та же)
//   в хвосте (правка 334) — разовые предметы с монетами по уровням (ящик разбит и записан — после загрузки его нет, монеты из
//   него второй раз не выпадают; once) и запас ext под поля следующих правок
//   загрузка ставит игрока туда, куда его ставит сценарий миссии (msn_resume: старт/чекпоинт текущей сюжетной миссии)
#ifndef SAVEGAME_H
#define SAVEGAME_H
#include <stdint.h>
#include "coins.h"
#include "stateprops.h"   // правка 334: разовые предметы (spOnce, spRestore)

#ifdef __cplusplus
extern "C" {
#endif

#define SV_SLOTS 4
#define SV_LEVELS 8               // индексы 1..7 (как collect.h COLL_LEVELS)
#define SV_WORLDCOINS 512         // бит i — монета мира i собрана (уровень 1: 207)
#define SV_VERSION 2               // версия ГОЛОВЫ (байты 0..255): 2 (29.09) + купленное за монеты (owned); запись версии 1 читается
#define SV_BYTES 768              // правка 334: размер DATA.BIN — голова 256 Б (как до 05.10) + хвост 512 Б со своей проверкой
#define SV_BYTES_V2 256           // размер головы; записи до 05.10 — ровно голова (прежние сборки читают голову новой записи, хвост не видят)
#define SV_TAIL_VERSION 1         // хвост с байта 256: "SHSX", версия 1, размер 512, CRC-32 тела (байты 268..767), тело: once[8], ext
#define SV_ONCE_MAX 96            // правка 334: разовых предметов на уровень (L1 61, L2 51, L3–L5 50–51, L6 46, L7 44 — build/levels 03.10)
#define SV_ONCE_BYTES (SV_ONCE_MAX / 2)   // по 4 бита на предмет
#define SV_EXT_BYTES 84           // запас в хвосте под поля следующих правок (сохранение разработчика и др.): пишется и читается как есть,
                                  // в записях до 05.10 — нули (байты 684..767 файла)
// Раскладка ext (кто добавляет поле — описывает здесь; в записях до 05.10 ext — нули):
//   ext[0] (правка 362, 05.10): настройки камеры — бит 7 SV_EXT_CAM_SET «поле записано», бит 0 SV_EXT_CAM_INV — Invert Camera
//          Control (L/R камеры пешком наоборот). Без бита 7 (запись до 05.10) загрузка настройку не трогает
//   ext[1..43] (облако 05.10, карьера): прогресс миссий ВСЕХ уровней (голова хранит только текущий: msnLevel/msnCompleted).
//          ext[1] SV_EXT_LV: бит 7 SV_EXT_LV_SET «поле записано», биты 0..2 — старший уровень, до которого дошла игра (1..7).
//          ext[2 + (L−1)·6], L = 1..7, по 6 Б: u32 маска пройденных миссий уровня L (LE, как msnCompleted), u8 строки MISSION SELECT
//          (бит 7 SV_LV_ROWS_OK «строки известны», биты 0..6 — строка k пройдена: 7 последних сюжетных уровня), u8 запас (0).
//          Запись до 05.10 (бит 7 ext[1] не стоит): уровни ниже уровня записи — пройдены (маска неизвестна: svLvGet -> 1), уровень
//          записи — маска головы, выше — нет. Прежние сборки ext не читают — их запись совместима в обе стороны
// ext[44]: ticket.c, original persistent is_cbg_first/is_ticket + L3 FMV unlock.
// Bit7 valid; bits0/1/2 intro/ticket/admission. Old saves0; VIEW MOVIES seen is separate.
// ext[71..74] SV_EXT_MOVIES (круг 2, g11b P07:24 и P14:12/22): ext[71] бит 7 «поле записано», бит 0 — ролик 8 (бит 8 cfg.movies); ext[72] — младшие 8 бит ролика;
// ext[73..74] — маска показанных подсказок обучения cfg.tutSeen (u16 LE). Загрузка объединяет с настройками устройства (ничего не стирает). Свободно: ext[75..82]
#define SV_EXT_MOVIES 71
#define SV_EXT_MOVIES_SET 0x80
// ext[70] SV_EXT_CHASE (сверка g10-flow, СВЕРКА_КАМЕРА 1.3): бит 7 «поле записано», биты 0..1 — выбранный вид за машиной (0 дальний, 1 ближний, 2 бампер).
// Запись до этого поля — нули (бит 7 не стоит): вид не трогаем. ext[83] занят окном g09b (секунды записи). Свободно: ext[75..82]
#define SV_EXT_CHASE 70
#define SV_EXT_CHASE_SET 0x80
// ext[75] SV_EXT_OPTS (круг 5, сверка g10-flow P08:22 и P16:08): бит 7 «поле записано», бит 0 — Jump Cameras включены, бит 1 — Navigation System включена (строки CAMERA / GAME паузы).
// Запись до этого поля — нули (бит 7 не стоит): настройки игрока не трогаем. Свободно: ext[76..82]
#define SV_EXT_OPTS 75
#define SV_EXT_OPTS_SET 0x80
#define SV_EXT_OPTS_JUMP 0x01
#define SV_EXT_OPTS_NAV 0x02
// ext[45..69] (сверка g10-flow 02.11.2026, P01 Д1): прочность личных машин. ext[45] SV_EXT_CARHP: бит 7 «поле записано», биты 0..3 — сколько машин (0..8);
// дальше по 3 байта на машину с ext[46]: хеш имени (u16 LE, FNV-1a/16, viewer/game.c) и доля прочности 0..255 (255 — цела, 0 — остов). Запись до этого
// поля — нули (бит 7 не стоит): таблица пуста, машины целы, как было. ext[83] — секунды записи (savemenu.c, g09b-menu: бит 7 «записано»). Свободно: ext[70..82]
#define SV_EXT_CARHP 45
#define SV_EXT_CARHP_SET 0x80
#define SV_EXT_CARHP_MAX 8
#define SV_EXT_TICKET 44
#define SV_EXT_TICKET_SET 0x80
#define SV_EXT_CAM 0
#define SV_EXT_CAM_SET 0x80
#define SV_EXT_CAM_INV 0x01
#define SV_EXT_LV 1
#define SV_EXT_LV_SET 0x80
#define SV_EXT_LV_REC 2
#define SV_EXT_LV_STRIDE 6
#define SV_LV_ROWS_OK 0x80
#define SV_GAMENAME "SLES51897"   // код диска Роба (SYSTEM.CNF: SLES_518.97) — имя папки сохранений, как у PS2 BESLES-51897

enum { SV_OK = 0, SV_EMPTY = 1, SV_DAMAGED = 2 };

typedef struct {
  uint32_t seq;                   // номер записи: больше — свежее (автозагрузка при запуске берёт наибольший)
  uint16_t year; uint8_t month, day, hour, minute;   // время записи (часы PSP)
  uint8_t level;                  // текущий уровень 1..7
  // прогресс миссий текущего уровня (viewer/mission msn_get_progress)
  uint8_t msnLevel, msnStory, msnPhase, msnStage; uint32_t msnCompleted;
  uint8_t cards[SV_LEVELS]; uint32_t wasps[SV_LEVELS]; uint64_t gags[SV_LEVELS];   // collect.h: маски по уровням
  int32_t coins;                  // кошелёк
  uint16_t nworld;                // монет мира на уровне (сверка при загрузке)
  uint8_t worldCoins[SV_WORLDCOINS / 8];
  int32_t music, effects, vehicle, voice, radar;   // настройки (main.c cfg): 0..1000, радар 0/1
  uint16_t owned[SV_LEVELS];      // v2: купленное/полученное (Coins.owned, viewer/shop.h): бит = номер BindReward уровня (их до 11 на уровень)
  uint8_t skin;                   // правка 107 (30.09): надетый костюм — номер строки skin в skins.txt (0 — обычный Гомер); байт после
                                  // owned, в записях до 30.09 там ноль (svWrite обнуляет запись) — версия не менялась, старые читаются как есть
  // хвост (правка 334): разовые предметы уровня L (стейтпропы с монетами, stateprops.h spOnce) по порядку экземпляров SPR1 уровня:
  // n — сколько их в уровне при записи, sig — отпечаток их типов и мест (svOnceSig); st — состояние предмета k в полубайте k
  // (младший полубайт байта k/2 — чётный k): 0 — цел, иначе номер состояния машины (1..15). Уровень пересобран (другие n/sig) —
  // запись уровня не применяется, предметы целы (безопасная сторона)
  struct SvOnce { uint16_t n, sig; uint8_t st[SV_ONCE_BYTES]; } once[SV_LEVELS];
  uint8_t ext[SV_EXT_BYTES];      // хвост: запас (см. SV_EXT_BYTES); раскладку полей внутри ведёт тот, кто их добавляет (описать здесь)
} SaveGame;
typedef struct SvOnce SvOnce;

void svClear(SaveGame *s);                                  // пустая игра (всё 0, настройки по умолчанию)
int  svWrite(const SaveGame *s, uint8_t out[SV_BYTES]);     // -> SV_BYTES (голова версии 2 + хвост)
// запись в раскладке ver: 1 и 2 — голова 256 Б, как писали сборки до 29.09 и до 05.10; 3 — голова + хвост (svWrite). Для гейта
// «старая запись читается» (tests/savegame/host.c)
int  svWriteVer(const SaveGame *s, uint8_t *out, unsigned n, int ver);   // -> байт записи, -1 — мало места / нет такой версии
int  svRead(const uint8_t *in, unsigned n, SaveGame *s);    // SV_OK / SV_DAMAGED (n — сколько байт прочитано: 256 — запись до 05.10, от 768 — с хвостом)
uint32_t svCrc32(const uint8_t *p, unsigned n);
// игра -> запись и обратно (общие с проверкой на Маке; NULL — модуля нет). Настройки (cfg) переносит main.c.
// svApply — после новой игры (collectNewGame / coinsInit); миссии — gameMsnOpenAt с svMsnProgress (viewer/game_msn.h)
struct MsnProgress; struct Collect;
void svCapture(SaveGame *s, const struct MsnProgress *msn, const struct Collect *coll, const Coins *coins);
void svApply(const SaveGame *s, struct Collect *coll, Coins *coins);
// Rob 350: DEV-only collection preset from loaded data; ambient non-coin gags are not progress
void svDevCollect(struct Collect *coll);
void svMsnProgress(const SaveGame *s, struct MsnProgress *p);
// строка прогресса для smoke.txt / сверки гейтом: одинаковый прогресс -> одинаковая строка (без seq и времени)
int  svProgressText(const SaveGame *s, char *out, int n);
// число пройденных миссий (биты маски) — для PARAM.SFO
int  svMissionsDone(const SaveGame *s);
// правка 334: разовые предметы уровня (stateprops.h spOnce). Снять с уровня в памяти / применить к только что начатому уровню
// (после newGame: монеты не выпадают, счётчики миссий не растут — spRestore). Apply -> сколько предметов поставлено разбитыми,
// -1 — запись от другой сборки уровня (n/sig не совпали), ничего не тронуто
unsigned svOnceSig(const StateProps *sp, unsigned *n);
void svOnceCapture(SvOnce *o, const StateProps *sp);
int  svOnceApply(const SvOnce *o, StateProps *sp);
int  svOnceCount(const SvOnce *o);                          // сколько предметов записаны не целыми
// облако 05.10 (карьера): прогресс миссий всех уровней в ext (раскладка выше).
// svLvPut — уровень L: маска пройденных и строки MISSION SELECT (rows с SV_LV_ROWS_OK, 0 — неизвестны); старший уровень растёт.
// svLvGet -> 2 записано (или уровень головы), 1 — пройден по записи до 05.10 (маска неизвестна: *mask = 0xFFFFFFFF, строки все),
// 0 — нет. svLvReached — старший уровень игры (не ниже s->level). svLvKeep — перенести поле ext всех уровней из src в dst
void svLvPut(SaveGame *s, int L, uint32_t mask, unsigned rows);
int  svLvGet(const SaveGame *s, int L, uint32_t *mask, unsigned *rows);
int  svLvReached(const SaveGame *s);
void svLvKeep(SaveGame *dst, const SaveGame *src);
// P01 Д1: прочность личных машин (раскладка выше). Put: n машин (до 8), доли 0..1; Get -> сколько (0 — таблица пуста), -1 — поля в записи нет
void svCarHealthPut(SaveGame *s, int n, const uint16_t *hash, const float *frac);
int  svCarHealthGet(const SaveGame *s, uint16_t *hash, float *frac);

#ifdef __cplusplus
}
#endif
#endif
