// Сохранение файлом на карте памяти — последняя страховка правки 274 (см. savefile.c).
#ifndef SHAR_SAVEFILE_H
#define SHAR_SAVEFILE_H
int svFileWrite(const char *dir, int slot, const void *data, int n);   // 0 — записано и прочитано обратно байт в байт; <0 — ошибка
int svFileRead(const char *dir, int slot, void *buf, int n);           // число прочитанных байт; <0 — файла нет или ошибка чтения
int svFileDelete(const char *dir, int slot);                           // 0 — удалён или его не было
// №1170 (надёжность TEST90): запись в два шага без окна «слота нет». Новое пишется в <имя>.tmp, прежнее переименовывается в <имя>.bak, новое — на место, .bak
// удаляется. При обрыве питания на любом шаге остаётся либо прежняя запись, либо новая целиком; svAtomicRecover при старте подбирает остатки.
// SAVEAT_OLD=1 — прежний порядок (удалить старое, переименовать новое) без подбора.
int svAtomicWrite(const char *path, const void *data, int n);                                   // 0 — записано; <0 — ошибка (прежняя запись цела)
int svAtomicRecover(const char *path, int (*valid)(const void *buf, int n));                  // 1 — слот восстановлен или остатки убраны, 0 — нечего делать
void svFileCut(int on);   // служебное (flag svcut 1, гейт tests/savegame/emu.sh): «обрыв питания» сразу после шага записи, после которого слота на месте нет (-9)
#endif
