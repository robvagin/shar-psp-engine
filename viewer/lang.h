// №1111 (02.11.2026): язык игры. Как в оригинале PAL: тексты и озвучка на четырёх языках — английский, французский, немецкий, испанский (итальянский там выключен, его таблица на диске пустая;
// португальского на диске нет). Язык берётся из системной настройки PSP; если она не из этих четырёх — при запуске экран выбора (bootscreen.h bootLangPrompt), выбор на запуск.
// gLangId — единственная переменная языка (g08-sound читает её же через sndLang / sndSetLanguage): 0 English, 1 French, 2 German, 3 Spanish.
// Файлы с текстами (пакеты интерфейса, миссии, карточки) лежат в lang/<E|F|G|S>/ рядом с английскими; нет файла языка — берётся английский. LANG1111_OLD=1 — только английский, как было.
#ifndef LANG_H
#define LANG_H
#ifndef LANG1111_OLD
#define LANG1111_OLD 0
#endif
#define LANG_N 4
extern int gLangId;
const char *langName(int id);                    // «English», «Français», «Deutsch», «Español» (знаки — коды msx экрана выбора)
int langFromSystem(void);                        // язык системы PSP -> 0..3, -1 — не из четырёх
void langSet(int id);                            // выбрать язык (зовёт sndSetLanguage, если звук его знает)
// имя файла: папка dir (с «/» на конце) + name, а если язык не английский и файл с текстами — lang/<буква>/name, когда такой файл есть
void langResolve(char *fn, int cap, const char *dir, const char *name);
// строки меню, которые движок находит по английской подписи (подпись пункта, строки настроек): на другом языке подпись из пакета сводится к английской по lang/<буква>/rowkeys.txt
// (tools/lang_rowkeys.py); подписи порта и английский — как есть
const char *langKey(const char *label);
void langSetDir(const char *dir);                // папка EBOOT с «/» на конце — откуда читать lang/<буква>/rowkeys.txt
#endif
