// №1161: таблица подписей строк меню rowkeys.txt берётся из папки языка (viewer/menu_rules.h mrLangDepends).
//   cc -std=c11 -I viewer tests/camera/camrowlang_host.c -lm -o /tmp/camrowlang && /tmp/camrowlang       Красный контроль: -DCAMROWLANG_OLD=1 (rowkeys.txt от языка не зависит — проверка падает)
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "menu_rules.h"
static int bad;
#define CHK(c, m) do { if (!(c)) { printf("FAIL %s\n", m); bad++; } else printf("ok   %s\n", m); } while (0)
int main(void) {
  CHK(!mrLangDepends(0, "rowkeys.txt"), "английский: rowkeys.txt не из папки языка");
  for (int l = 1; l <= 3; l++) { char m[80]; snprintf(m, sizeof m, "язык %d (F/G/S): rowkeys.txt из lang/<буква>/", l); CHK(mrLangDepends(l, "rowkeys.txt") && mrLangDepends(l, "lang/x/rowkeys.txt"), m); }
  { char o[200]; mrLangFile(o, sizeof o, "ms0:/PSP/GAME/SHARVIEW/", 3, "rowkeys.txt"); CHK(!strcmp(o, "ms0:/PSP/GAME/SHARVIEW/lang/S/rowkeys.txt"), "путь файла испанского: lang/S/rowkeys.txt"); }
  CHK(mrLangDepends(1, "pmenu.shr") && mrLangDepends(2, "ui.shr") && !mrLangDepends(2, "level.shr") && !mrLangDepends(1, "settings.bin"), "прежний список не тронут (pmenu.shr, ui.shr — да; level.shr, settings.bin — нет)");
  puts(bad ? "FAIL" : "PASS"); return bad;
}
