// Номер выпуска данных (TEST93, окно Данные-93, C1). Движок знает, от какого выпуска должны лежать файлы набора рядом с EBOOT:
// tools/setrev.sh пишет в набор set.txt с этим номером, при старте движок сверяет. Не совпал — экран «Game files are from another build».
//
// 🔴 ПОВЫШАТЬ ОБЯЗАН КАЖДЫЙ, кто меняет формат любого файла из tools/ship_rules.txt (level.shr, traffic.shr, ui.shr, msnart.shr, звук, миссии
// и т.д.): движок, который читает новое, не должен молча работать на старых данных и наоборот. Поменял формат — +1 здесь, в том же коммите.
// Не повысил — старые данные у людей молча сломают загрузку (беда 07.11: «я обновляю, а у людей не подгружается»).
#ifndef SHAR_DATAREV_H
#define SHAR_DATAREV_H
#ifndef SHAR_DATA_REV
#define SHAR_DATA_REV 93
#endif
#ifndef SETREV_OLD
#define SETREV_OLD 0   // 1 — как до 93: set.txt не читается, чужой набор играет молча
#endif
#ifndef SETCHECK_OLD
#define SETCHECK_OLD 0   // 1 — как до 93: полнота набора не проверяется, нет уровня — молчаливый уход в меню / на уровень 1
#endif
#endif
