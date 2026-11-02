// Подсказки обучения Барта (Tutorial.scr оригинала), 29.09. Уровень с EnableTutorialMode(1) в leveli.mfk (на диске — только
// уровень 1). Подсказка — один раз за игру на своё событие; пока она на экране, мир стоит, HUD (и радар) не рисуется, звучит
// реплика Барта (conversations/c_*_tutorial_brt_l1t1.rsd, дорожки music.sap v_*_tu_brt_l1t1); когда реплика кончилась — «Continue»
// (X закрывает); □ — «Disable Tutorials» (подсказки выключены; строка Tutorial в Settings паузы включает их снова).
// Текст — строка TUTORIAL_NNN библии (PS2-вариант), кнопки — картинки but_* (viewer/ui.c uiDrawTutorial).
// Какая подсказка на какое событие, с какой репликой — по записи PS2 (tools/ps2ref/CATALOG_L1.md «Подсказки обучения», текст на
// экране + расшифровка звука): 1:05 ходьба, 1:45 «△ — сесть», 1:50 газ, 2:22 разбил предмет машиной, 2:29 первая монета,
// 2:55 «△ — войти», 4:15 гаечный ключ, 11:20 оса-камера, 16:55 уличная гонка рядом, 22:55 бонус-миссия рядом, 33:36 машина
// взорвана (через 2 с после взрыва). Карточка, гэг, заезд на время — правило порта по тексту библии и реплике (в записи их нет).
#ifndef SHAR_TUTORIAL_H
#define SHAR_TUTORIAL_H

enum { TUT_WALK, TUT_GETIN, TUT_GAS, TUT_BREAK, TUT_COINS, TUT_INSIDE, TUT_WRENCH, TUT_WASP, TUT_RACE, TUT_BONUS, TUT_DESTROYED,
       TUT_CARD, TUT_GAG, TUT_TIMETRIAL, TUT_UNLOCKED, TUT_PHONE, TUT_FOREIGN, TUT_COUNT };   // TUT_FOREIGN — круг 5 (g07b, P15-10): «PRESS @ TO GET INTO THE CAR» (TUTORIAL_010) при угоне машины трафика

void tutReset(int levelMode);                // новая игра: EnableTutorialMode уровня (0 — подсказок нет); показанные сбрасываются
void tutSetUser(int on);                     // строка Tutorial в Settings паузы (и □ «Disable Tutorials»)
int  tutUser(void);
int  tutPending(int ev);                     // 1 — подсказка ev ещё покажется (включены, не была): её текст не дублировать сообщением
void tutForeignCar(void);                    // круг 5 (g07b, P15-10): угнали машину трафика: «в свою» ещё не показана — показывается она, «в чужую» считается показанной; иначе один раз «в чужую»; «газ» на эту посадку не показывается
void tutEvent(int ev);                       // событие игры: подсказка ev, если ещё не показана и подсказки включены
void tutEventIn(int ev, float sec);          // то же через sec секунд игрового времени (взрыв машины: Гомер успевает упасть)
void tutSetBusy(int busy);                   // 1 — разговор, брифинг, облёт, пауза: события копятся в очереди, покажутся потом
void tutTick(float dt);                      // игровое время (не идёт в паузе и пока подсказка на экране)
int  tutActive(void);                        // 1 — подсказка на экране (мир стоит)
// кадр подсказки: hit — нажатия; wallDt — время кадра. 1 — подсказка закрыта в этом кадре; *disabled = 1 — нажат □
int  tutInput(unsigned hit, float wallDt, int *disabled);
int  tutVoiceBusy(void);                     // сверка g10b-loop (P13:17): 1 — на экране подсказка и её реплика ещё звучит
int  tutDraw(void);
int  tutDrawExit(float wallDt);                      // №516: уход подсказки (обратный масштаб) поверх игры, после закрытия; 0 — нечего рисовать                          // вместо HUD; 0 — нет экрана в пакете
unsigned tutShown(void);                     // бит ev — подсказка показана (сохранения / гейт)
void tutSetShown(unsigned mask);             // загрузка сохранения: показанные не повторяются
void tutSetLog(void (*log)(const char *));
void tutService(const char *flag, float a);  // служебное: «tutnovoice» — без реплики (Continue через TUT_NOVOICE_S), «tutvoice», «tutagain N»

#endif
