// Список ресурсов миссий уровня в порядке чтения (запекание З9, 109; формат MRS1 — tools/missions/msnres.py, файл msnres-l<N>.shr рядом с level.shr).
// Мак по скриптам миссий выписывает, что миссии нужно и сколько это весит (записи msnart, модели машин msncars, персонажи chars/, картинка карточки), в порядке чтения:
// сначала команды загрузки фазы, потом этапы. Движок ничего не вычисляет: открыл файл, отдал таблицу. Нет файла (старый набор) — msnResOpen = -1, всё остаётся как было.
// Кто пользуется: загрузка миссии под карточкой (читать по списку до кнопки), готовность карточки («очередь пуста и всё из списка прочитано»), план памяти до закрытия экрана
// (msnResPeak — сколько нужно области «миссия»). Гейт: tools/missions/msnres_gate.py (всё, что движок реально читал на миссии, есть в списке этой миссии или уровня).
#ifndef MSNRES_H
#define MSNRES_H
enum { MSNRES_ART = 1, MSNRES_CAR = 2, MSNRES_CHR = 3, MSNRES_PIC = 4 };   // kind
enum { MSNRES_PH_LOAD = 0xFF };                                             // stage: команда загрузки фазы (иначе номер этапа)
enum { MSNRES_F_MISSING = 1, MSNRES_F_ENGINE = 2 };                         // flags: в наборе нет / читает сам движок
typedef struct { unsigned char kind, phase, stage, flags; unsigned bytes; char key[88]; } MsnResEnt;   // 96 Б, как в файле
int msnResOpen(const char *dir, int level);     // dir — папка набора со слешем; 0 — прочитан, -1 — файла нет или он не тот (молча)
void msnResClose(void);
int msnResMission(const char *id);              // номер миссии по имени ("m1", "bm1"; "L" — уровневая) или -1
unsigned msnResCount(int mi);                   // записей у миссии
const MsnResEnt *msnResAt(int mi, unsigned i);  // i-я запись в порядке чтения; NULL — нет
int msnResShared(const char *key);              // 1363: запись art (не картинка карточки) в списке уровня «L» или у двух и более миссий; ключ в нижнем регистре через «/»
unsigned msnResPeak(int mi);                    // art + самая большая сумма машин одного этапа + персонажи, байт
unsigned msnResBytes(int mi, int kind);         // сумма по виду (MSNRES_ART/CAR/CHR; PIC входит в ART)
#endif
