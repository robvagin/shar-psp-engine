// №1270 (Роб 13.11): на загрузочном экране стоял номер сборки 102 при сборке 106 (число вписывали руками в bootseq.h и забыли).
// Теперь номер берётся из файла viewer/BUILD_NUMBER: CMakeLists.txt читает его и передаёт -DSHAR_BUILD_NUM=N; выпускной скрипт следующей сборки пишет туда число (echo 107 > viewer/BUILD_NUMBER).
// Без -D (хост-проверки, старая сборка) — 106. BOOT_BUILDNUM1270_OLD=1 — прежнее число 102 руками
#ifndef SHAR_BUILDNUM_H
#define SHAR_BUILDNUM_H
#ifndef BOOT_BUILDNUM1270_OLD
int rb_BOOT_BUILDNUM1270_OLD __attribute__((weak));   // 107б rollback.txt (rollback.h): 1 — на загрузке номер 102, как руками
#define BOOT_BUILDNUM1270_OLD rb_BOOT_BUILDNUM1270_OLD
#endif
#ifndef SHAR_BUILD_NUM
#define SHAR_BUILD_NUM 106
#endif
#define BOOT_W_BUILDS (BOOT_BUILDNUM1270_OLD ? 102 : SHAR_BUILD_NUM)
#endif
