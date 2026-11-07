// Правка Роба 274 (04.10 07:3x: «сохранение не работает — поправить точно, стопроцентно»). Системная утилита сохранений
// (savemenu.c util) на железе отвергала параметры (0x80110388); её варианты параметров — первая линия. Если утилита отказывает во
// всех вариантах (или её нет), запись идёт сюда: обычный файл рядом с игрой, ms0:/PSP/GAME/SHARVIEW/save<NN>.bin, теми же
// sceIo, которыми игра пишет чёрный ящик (на PSP Роба работают с 01.10). Формат — тот же DATA.BIN (savegame.h svWrite/svRead,
// CRC). Запись — во временный .tmp, затем на место прежнего: прежнее сохранение цело, пока новое не легло целиком; после записи —
// чтение обратно и сравнение байт в байт. Правило порта: в меню PSP «Сохранённые данные» такие записи не видны — видна только игре.
// №1170 (разбор надёжности 04.11, §5): между sceIoRemove старого и sceIoRename нового слота не было вовсе; теперь svAtomicWrite (старое уходит в .bak, а не удаляется)
// и svAtomicRecover (при чтении слота и настроек подбирает .tmp и .bak после обрыва). SAVEAT_OLD=1 — как было.
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>
#include "savefile.h"
#include "savegame.h"

#ifndef SAVEAT_OLD
#define SAVEAT_OLD 0
#endif
static int cutOn;   // flag svcut 1 (только гейт в эмуляторе): запись обрывается сразу после шага, оставляющего слот без файла
void svFileCut(int on) { cutOn = on; }

static void slotPath(char *out, int n, const char *dir, int slot, const char *ext) { snprintf(out, n, "%ssave%02d%s", dir ? dir : "", slot, ext); }

// p с заменой последнего расширения на ext (".tmp", ".bak"); нет расширения — дописывается
static void withExt(char *out, int n, const char *p, const char *ext) {
  snprintf(out, n, "%s", p);
  char *dot = strrchr(out, '.'), *sl = strrchr(out, '/');
  if (dot && (!sl || dot > sl)) *dot = 0;
  size_t l = strlen(out); snprintf(out + l, n - (int)l, "%s", ext);
}
static int fileExists(const char *p) { SceUID f = sceIoOpen(p, PSP_O_RDONLY, 0); if (f < 0) return 0; sceIoClose(f); return 1; }
static int readFile(const char *p, void *buf, int max) { SceUID f = sceIoOpen(p, PSP_O_RDONLY, 0); if (f < 0) return -1; int r = sceIoRead(f, buf, max); sceIoClose(f); return r; }

int svAtomicWrite(const char *p, const void *data, int n) {
  char t[320], b[320]; withExt(t, sizeof t, p, ".tmp"); withExt(b, sizeof b, p, ".bak");
  SceUID f = sceIoOpen(t, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f < 0) return f;
  int w = sceIoWrite(f, data, n); sceIoClose(f);
  if (w != n) { sceIoRemove(t); return -2; }
  if (SAVEAT_OLD) {
    sceIoRemove(p);
    if (cutOn) return -9;   // обрыв: старое удалено, новое лежит в .tmp (окно прежнего порядка)
    if (sceIoRename(t, p) < 0) {   // переименование не прошло — пишем на место напрямую
      f = sceIoOpen(p, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f < 0) return f;
      w = sceIoWrite(f, data, n); sceIoClose(f); sceIoRemove(t);
      if (w != n) return -3;
    }
    return 0;
  }
  int had = fileExists(p);
  if (had) {
    sceIoRemove(b);   // чужой .bak от прошлого обрыва: слот p цел, .bak не нужен
    if (sceIoRename(p, b) < 0) { sceIoRemove(t); return -5; }   // слот не тронут
    if (cutOn) return -9;   // обрыв: старое в .bak, новое в .tmp (окно нового порядка: слот подбирается при чтении)
  }
  if (sceIoRename(t, p) < 0) {   // новое на место не встало: вернуть прежнее, затем запись напрямую
    if (had) sceIoRename(b, p);
    f = sceIoOpen(p, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f < 0) return f;
    w = sceIoWrite(f, data, n); sceIoClose(f); sceIoRemove(t);
    if (w != n) return -3;
    return 0;
  }
  if (had) sceIoRemove(b);
  return 0;
}

// Остатки обрыва. Слот p есть: .tmp (недописанное или неподложенное) и .bak (прежнее после подмены) лишние. Слота нет: годный .tmp (новое целиком, обрыв
// между двумя rename) ложится на место, иначе годный .bak (прежнее). Годность решает valid (CRC записи или настроек).
int svAtomicRecover(const char *p, int (*valid)(const void *buf, int n)) {
  if (SAVEAT_OLD) return 0;
  char t[320], b[320]; withExt(t, sizeof t, p, ".tmp"); withExt(b, sizeof b, p, ".bak");
  static unsigned char rb[4096];
  int hadT = fileExists(t), hadB = fileExists(b);
  if (!hadT && !hadB) return 0;
  if (fileExists(p)) { if (hadT) sceIoRemove(t); if (hadB) sceIoRemove(b); return 1; }
  if (hadT) { int n = readFile(t, rb, sizeof rb); if (n > 0 && valid(rb, n) && sceIoRename(t, p) >= 0) { if (hadB) sceIoRemove(b); return 1; } sceIoRemove(t); }
  if (hadB) { int n = readFile(b, rb, sizeof rb); if (n > 0 && valid(rb, n) && sceIoRename(b, p) >= 0) return 1; }
  return 1;
}

static int slotValid(const void *buf, int n) { SaveGame g; return svRead((const uint8_t *)buf, (unsigned)n, &g) == SV_OK; }

int svFileRead(const char *dir, int slot, void *buf, int n) {
  char p[320]; slotPath(p, sizeof p, dir, slot, ".bin");
  svAtomicRecover(p, slotValid);   // №1170: остатки обрыва записи — до чтения
  SceUID f = sceIoOpen(p, PSP_O_RDONLY, 0); if (f < 0) return f;
  int r = sceIoRead(f, buf, n); sceIoClose(f);
  return r;
}
int svFileWrite(const char *dir, int slot, const void *data, int n) {
  char p[320]; slotPath(p, sizeof p, dir, slot, ".bin");
  int e = svAtomicWrite(p, data, n); if (e < 0) return e;
  static unsigned char chk[2048];   // №1114: запись SHV3 1536 Б
  if (n > (int)sizeof chk) return 0;
  int r = svFileRead(dir, slot, chk, sizeof chk);
  return r == n && !memcmp(chk, data, n) ? 0 : -4;
}
int svFileDelete(const char *dir, int slot) {
  char p[320], t[320], b[320]; slotPath(p, sizeof p, dir, slot, ".bin"); withExt(t, sizeof t, p, ".tmp"); withExt(b, sizeof b, p, ".bak");
  sceIoRemove(t); sceIoRemove(b);   // №1170: удалённый слот не должен «воскреснуть» из .bak при следующем чтении
  int r = sceIoRemove(p); return r < 0 && (unsigned)r != 0x80010002u ? r : 0;   // 0x80010002 — файла нет
}
