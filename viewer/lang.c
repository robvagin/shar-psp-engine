// №1111: язык игры (см. lang.h). Правила выбора пути и таблица языков системы — viewer/menu_rules.h (проверяет tests/sverka_g09b).
#include <pspkernel.h>
#include <psputility.h>
#include <stdio.h>
#include <string.h>
#include "lang.h"
#include "menu_rules.h"
int gLangId = 0;
extern void sndSetLanguage(int lang) __attribute__((weak));   // g08-sound: язык озвучки со следующей загрузки уровня
const char *langName(int id) {
  static const char *const n[LANG_N] = {"English", "Fran\x87" "ais", "Deutsch", "Espa\xa4" "ol"};   // коды шрифта экрана загрузки (msx): ç = 0x87, ñ = 0xA4
  return id >= 0 && id < LANG_N ? n[id] : n[0];
}
int langFromSystem(void) {
  int v = -1; if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &v) < 0) return -1;
  return mrLangFromPsp(v);
}
void langSet(int id) {
  gLangId = LANG1111_OLD || id < 0 || id >= LANG_N ? 0 : id;
  if (sndSetLanguage) sndSetLanguage(gLangId);
}
static char lkDir[200], lkBuf[3200]; static const char *lkLoc[96], *lkEn[96]; static int lkN, lkLang = -1;
void langSetDir(const char *dir) { snprintf(lkDir, sizeof lkDir, "%s", dir ? dir : ""); lkLang = -1; }
static int lkEq(const char *a, const char *b) { for (; *a && *b; a++, b++) { int x = *a, y = *b; if (x >= 'a' && x <= 'z') x -= 32; if (y >= 'a' && y <= 'z') y -= 32; if (x != y) return 0; } return *a == *b; }
const char *langKey(const char *label) {
  if (LANG1111_OLD || gLangId <= 0 || !label) return label;
  if (lkLang != gLangId) {
    lkLang = gLangId; lkN = 0;
    char fn[300]; langResolve(fn, sizeof fn, lkDir, "rowkeys.txt"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);
    if (f >= 0) { int n = sceIoRead(f, lkBuf, (int)sizeof lkBuf - 1); sceIoClose(f);
      if (n > 0) { lkBuf[n] = 0; char *q = lkBuf; while (*q && lkN < 96) { char *e = strchr(q, '\n'); if (e) *e = 0; char *t = strchr(q, '\t');
          if (t) { *t = 0; lkLoc[lkN] = q; lkEn[lkN] = t + 1; lkN++; } q = e ? e + 1 : q + strlen(q); } } }
  }
  for (int i = 0; i < lkN; i++) if (lkEq(lkLoc[i], label)) return lkEn[i];
  return label;
}
void langResolve(char *fn, int cap, const char *dir, const char *name) {
  snprintf(fn, (size_t)cap, "%s%s", dir, name);
  if (!mrLangDepends(gLangId, name)) return;
  char alt[256]; mrLangFile(alt, sizeof alt, dir, gLangId, name);
  SceIoStat st; if (sceIoGetstat(alt, &st) >= 0) snprintf(fn, (size_t)cap, "%s", alt);
}
