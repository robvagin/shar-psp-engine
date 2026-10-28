// Звуки интерфейса в игре (см. ui_sfx.h). Имена — ресурсы daSoundResourceData звуковых таблиц диска; в пакете звука
// (tools/sound_export.py SFX) они уже есть. Что где играет, сверено по коду порта 28.09:
//   карточка (card_collect) и осы (wasp_*) — уже в main.c по событиям сбора, здесь не дублируются;
//   провал миссии — отдельного звука интерфейса в таблицах нет (музыка M1_lose, viewer/sound.c).
#include <stdio.h>
#include <string.h>
#include <pspkernel.h>
#include "ui.h"
#include "sound.h"
#include "ui_sfx.h"
#ifndef UI_SFX_POPSLOT_OLD
#define UI_SFX_POPSLOT_OLD 0
#endif

static const struct { int ev, on; const char *name; int fe; } MAP[] = {   // fe = 1: звук меню (шина AUDIO_BUS_UI, правка 114)
  // world.spt: pop_up -> soundfx/gameplay/pop_up_03.rsd (trim 0.63) — «чпок» нового задания в рамке HUD
  {UI_T_OBJECTIVE, 1, "pop_up"},
  {UI_T_STAGE_COMPLETE, 1, "stage_complete"},       // world.spt -> gameplay/stage_complete.rsd
  {UI_T_MISSION_COMPLETE, 1, "mission_complete"},   // world.spt -> gameplay/mission_complete.rsd
  {UI_T_HNR_TICKET, 1, "busted"},                   // world.spt -> gameplay/busted_03.rsd (штраф H&R: тег пока не включается)
  // frontend.spt: пауза и её меню (accept / scroll -> optionsmenu/button_315_22, scroll; pause_on_02, pause_off)
  // правка 114 (30.09): вход в паузу был не слышен — набор шин «Pause» тюнера диска глушит эффекты в тот же кадр; звуки меню — на своей шине
  {UI_EV_PAUSE, 1, "pause_on", 1}, {UI_EV_PAUSE, 0, "pause_off", 1},
  {UI_EV_SCROLL, 1, "scroll", 1}, {UI_EV_ACCEPT, 1, "accept", 1},
  // 29.09: главное меню и OPTIONS — те же звуки; затемнённый пункт — locked_out, назад (△/○) — back (frontend.spt)
  {UI_EV_LOCKED, 1, "locked_out", 1}, {UI_EV_BACK, 1, "back", 1},
};
#define NMAP ((int)(sizeof MAP / sizeof MAP[0]))
static int ids[NMAP], resolved, muted;
static void (*logFn)(const char *);
static float t0; static unsigned tus0;

static void onEvent(int ev, int on) {
  if (muted) return;
  if (!resolved) { for (int i = 0; i < NMAP; i++) ids[i] = sndId(MAP[i].name); resolved = sndReady(); }
  for (int i = 0; i < NMAP; i++) {
    if (MAP[i].ev != ev || MAP[i].on != on) continue;
    int h = ids[i] < 0 ? -1 : MAP[i].fe ? sndPlayUi(ids[i], 1.f) : sndPlay2D(ids[i], 1.f);
    if (logFn) {
      char s[140]; snprintf(s, sizeof s, "UISFX ev=%d on=%d name=%s id=%d h=%d mode=%d t=%.3f\n", ev, on, MAP[i].name, ids[i], h,
                            sndModeCur(), t0 + (sceKernelGetSystemTimeLow() - tus0) * 1e-6f);   // mode — режим тюнера (шина эффектов)
      logFn(s);
    }
  }
}

void uiSfxInit(void (*log)(const char *)) {
  logFn = log; resolved = 0; muted = 0; tus0 = sceKernelGetSystemTimeLow();
  uiPopHook(onEvent);
  uiSetLog(log);
}

int uiSfxService(const char *f, float a) {
  if (!strcmp(f, "uipulse") || !strcmp(f, "uipulse0")) {   // △ на a секунд поверх игры, покадровый замер масштаба
    uiAnimOff(f[7] == '0' ? UI_ANIM_NOPULSE : 0);
    uiTrace(UI_TRACE_PULSE); uiForceShow(UI_T_ACTION, a > 0.f ? a : 4.f); return 1;
  }
  if (!strcmp(f, "uihnr") || !strcmp(f, "uihnrold")) {   // кольцо H&R по сценарию, покадровый замер доли круга и цвета
    uiAnimOff(f[5] == 'o' ? UI_ANIM_OLDHNR : 0);
    uiTrace(UI_TRACE_HNR); uiHnrScript(1); return 1;
  }
  if (!strcmp(f, "uihnrtrace") || !strcmp(f, "uihnrtraceold")) {   // правка 241: замер кольца от настоящей шкалы игры (без сценария);
    uiAnimOff(f[10] == 'o' ? UI_ANIM_OLDHNR : 0); uiTrace(UI_TRACE_HNR); return 1;   // ...old — прежний код кольца (красный контроль)
  }
  if (!strcmp(f, "uipop")) { uiTrace(UI_TRACE_POP); uiForceShow((int)a, 1.5f); return 1; }   // тег a всплывает на 1.5 с
  if (!strcmp(f, "brtrace")) { uiTrace(UI_TRACE_POP); return 1; }   // №994: служебно — трасса кадров карточки миссии (BRIEFANIM/BRIEFDRAW)
  if (!strcmp(f, "uipop0")) { uiAnimOff(UI_ANIM_NOPOP); return 1; }
  if (!strcmp(f, "uipopmsg") || !strcmp(f, "uipopmsgh")) { uiTrace(UI_TRACE_POP); uiMessage(a > 5.5f ? "GO TO THE KWIK-E-MART AND BUY SOME COLA FOR MARGE" : a > 4.5f ? "GATE: NEW OBJECTIVE B" : a > 3.5f ? "VEHICLE DESTROYED! FIND A WRENCH, THEN GET BACK IN THE CAR AND DRIVE TO THE KWIK-E-MART" : a > 2.5f ? "GO TO THE KWIK-E-MART AND BUY SOME COLA FOR MARGE" : a > 1.5f ? "TELEPHONE BOOTH UNAVAILABLE" : a > 0.f ? "GATE: NEW OBJECTIVE B" : "GATE: NEW OBJECTIVE A", a > 3.5f ? (UI_SFX_POPSLOT_OLD ? 0 : UI_OBJICON_EXTERNAL) : -1, f[8] == 'h' ? 40.f : 1.5f); return 1; }   // №640/646: тексты для кадров пузыря; a > 3,5 — с аватаркой задания (в игре значок всегда из msnicons, в кольце; малый слот пакета — только прежний тест, UI_SFX_POPSLOT_OLD=1)
  if (!strcmp(f, "uidraw1040")) { uiDrawLog1040Set((int)a); return 1; }
  if (!strcmp(f, "uimsg1037q")) { static const char *const T[] = {"GET HOMER'S MR. PLOW TRUCK. HE IS IN THE HOSPITAL PARKING LOT", "FOLLOW THE PIZZA VAN TO HERMAN'S MILITARY ANTIQUES"}; uiMessage(T[(int)a & 1], -1, 3.f); return 1; }   // №1040 служебно: сообщение без трассы
  if (!strcmp(f, "bubble1040")) { uiBubble1040Set((int)a); return 1; }   // №1040: 1 — простое появление пузыря, 0 — как в TEST72
  if (!strcmp(f, "wrap1037")) { uiWrap1037Set((int)a); return 1; }   // №1037: 1 — наименьшее число строк прежде красоты, 0 — как в TEST72
  if (!strcmp(f, "uimsg1037")) { static const char *const T[] = {"TALK TO THE SEA CAPTAIN", "GET HOMER'S MR. PLOW TRUCK. HE IS IN THE HOSPITAL PARKING LOT", "HIT THE DUFF TRUCK AND DESTROY THE LASER BOXES", "GET THE \"ZOMBIE CAR.\" THE ZOMBIE IS AT THE CEMETERY", "FOLLOW THE PIZZA VAN TO HERMAN'S MILITARY ANTIQUES", "DRIVE TO THE SIMPSONS HOUSE"};
    uiTrace(UI_TRACE_POP); uiMessage(T[(int)a % 6], -1, 3.f); return 1; }   // №1037: служебно — сообщение для листа переноса (a — номер текста)
  if (!strcmp(f, "uipopdest")) { uiTrace(UI_TRACE_POP); uiMessage("VEHICLE DESTROYED!\nFIND A WRENCH,\nOR GO TO A PHONEBOOTH,\nTO REPAIR YOUR VEHICLE.", -1, 6.f); return 1; }   // №689: служебно (гейт): настоящий текст «разбита» из main_loop.h (4 жёстких переноса)
  if (!strcmp(f, "uipopmsgl")) { uiTrace(UI_TRACE_POP); uiMessage("VEHICLE DESTROYED! FIND A WRENCH, THEN GET BACK IN THE CAR AND DRIVE TO THE KWIK-E-MART", -1, 2.5f); return 1; }   // №638: длинный текст пузыря (рамка растёт под текст)
  if (!strcmp(f, "uiclk")) { uiClkTest(a > 0.f ? 1 : 0, 0.f); return 1; }   // №942: часы HUD стоят (1) / идут (0)
  if (!strcmp(f, "uiclkadv")) { uiClkTest(2, a); return 1; }   // №942: сдвиг часов на a секунд
  if (!strcmp(f, "uisfx0")) { muted = 1; return 1; }   // отрицательный контроль: звуки интерфейса не играют
  if (!strcmp(f, "uimenutrace")) { uiTrace(UI_TRACE_MENU); return 1; }   // правка 413: MENUITEM покадрово (пульс меню)
  if (!strcmp(f, "failynpulse0")) { uiFailynPulseOld(1); return 1; }   // №572: откат пульса YES/NO (красный контроль)
  if (!strcmp(f, "failynpulse1")) { uiFailynPulseOld(0); return 1; }
  if (!strcmp(f, "uioff")) { uiTrace(0); uiAnimOff(0); uiHnrScript(0); muted = 0; return 1; }
  return 0;
}
