// Звуки интерфейса в игре: события HUD и паузы (viewer/ui.c uiPopHook) -> ресурсы звуковых таблиц диска
// (sound/scripts/world.spt, frontend.spt), играет viewer/sound.c. Плюс служебные флаги гейта анимаций HUD.
#ifndef SHAR_UI_SFX_H
#define SHAR_UI_SFX_H
// один раз после uiInit и sndInit; log — строки гейта (smoke.txt), может быть NULL
void uiSfxInit(void (*log)(const char *));
// флаг сценария смоука (uipulse, uipulse0, uihnr, uihnrold, uipop, uipop0, uipopmsg, uisfx0, uioff): 1 — обработан
int  uiSfxService(const char *flag, float a);
// сверка g08-sound (P09:29, P09 Д14): звук меню по имени ресурса на шине интерфейса (пауза его не глушит): continue / cancel (карточки миссии, итоги),
// phonebusy (будка занята), cheat_success / cheat_fail (ввод чита), car_stinger / dialog_stinger / music_stinger / sfx_stinger (пробные звуки
// ползунков громкости). Возврат 1 — звук запущен; нет такого в пакете — 0 (тихо)
int  uiSfxPlayNamed(const char *name);
#endif
