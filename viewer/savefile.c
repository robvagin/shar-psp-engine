// Правка Роба 274 (04.10 07:3x: «сохранение не работает — поправить точно, стопроцентно»). Системная утилита сохранений
// (savemenu.c util) на железе отвергала параметры (0x80110388); её варианты параметров — первая линия. Если утилита отказывает во
// всех вариантах (или её нет), запись идёт сюда: обычный файл рядом с игрой, ms0:/PSP/GAME/SHARVIEW/save<NN>.bin, теми же
// sceIo, которыми игра пишет чёрный ящик (на PSP Роба работают с 01.10). Формат — тот же DATA.BIN (savegame.h svWrite/svRead,
// CRC). Запись — во временный .tmp, затем на место прежнего: прежнее сохранение цело, пока новое не легло целиком; после записи —
// чтение обратно и сравнение байт в байт. Правило порта: в меню PSP «Сохранённые данные» такие записи не видны — видна только игре.
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
#include "savefile.h"

static void slotPath(char *out, int n, const char *dir, int slot, const char *ext) { snprintf(out, n, "%ssave%02d%s", dir ? dir : "", slot, ext); }

int svFileRead(const char *dir, int slot, void *buf, int n) {
  char p[320]; slotPath(p, sizeof p, dir, slot, ".bin");
  SceUID f = sceIoOpen(p, PSP_O_RDONLY, 0); if (f < 0) return f;
  int r = sceIoRead(f, buf, n); sceIoClose(f);
  return r;
}
int svFileWrite(const char *dir, int slot, const void *data, int n) {
  char p[320], t[320]; slotPath(p, sizeof p, dir, slot, ".bin"); slotPath(t, sizeof t, dir, slot, ".tmp");
  SceUID f = sceIoOpen(t, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f < 0) return f;
  int w = sceIoWrite(f, data, n); sceIoClose(f);
  if (w != n) { sceIoRemove(t); return -2; }
  sceIoRemove(p);
  if (sceIoRename(t, p) < 0) {   // переименование не прошло — пишем на место напрямую
    f = sceIoOpen(p, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f < 0) return f;
    w = sceIoWrite(f, data, n); sceIoClose(f); sceIoRemove(t);
    if (w != n) return -3;
  }
  static unsigned char chk[2048];   // №1114: запись SHV3 1536 Б
  if (n > (int)sizeof chk) return 0;
  int r = svFileRead(dir, slot, chk, sizeof chk);
  return r == n && !memcmp(chk, data, n) ? 0 : -4;
}
int svFileDelete(const char *dir, int slot) {
  char p[320]; slotPath(p, sizeof p, dir, slot, ".bin");
  int r = sceIoRemove(p); return r < 0 && (unsigned)r != 0x80010002u ? r : 0;   // 0x80010002 — файла нет
}
