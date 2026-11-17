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
  // сверка g08-sound круг 2 (g09a M1 Д1, M3:6): образец после остановки ползунка Sound — событие UI_EV_STING (в ui.h g09a сразу после UI_EV_BACK), on = строка + 1:
  // 1 музыка, 2 эффекты, 3 двигатель, 4 голос. Пока ветка g09a не влита, событие не приходит и строки молчат
  {UI_EV_BACK + 1, 1, "music_stinger", 1}, {UI_EV_BACK + 1, 2, "sfx_stinger", 1}, {UI_EV_BACK + 1, 3, "car_stinger", 1}, {UI_EV_BACK + 1, 4, "dialog_stinger", 1},
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

int uiSfxPlayNamed(const char *name) {
  if (muted || !name || !sndReady()) return 0;
  int id = sndId(name), h = id < 0 ? 0 : sndPlayUi(id, 1.f);
  if (logFn) { char s[140]; snprintf(s, sizeof s, "UISFX named=%s id=%d h=%d t=%.3f\n", name, id, h, t0 + (sceKernelGetSystemTimeLow() - tus0) * 1e-6f); logFn(s); }
  return h != 0;
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
  if (!strcmp(f, "mcrwait1337old")) { extern int mvMcrWait1337Old; mvMcrWait1337Old = (int)a != 0; if (logFn) logFn(mvMcrWait1337Old ? "MCRWAIT1337 old=1\n" : "MCRWAIT1337 old=0\n"); return 1; }   // 1337: красный контроль ожидания машины цели
  if (!strcmp(f, "skipcount1331old")) { extern int mvSkipCount1331Old; mvSkipCount1331Old = (int)a != 0; if (logFn) logFn(mvSkipCount1331Old ? "SKIPCOUNT1331 old=1\n" : "SKIPCOUNT1331 old=0\n"); return 1; }   // 1331: красный контроль счёта попыток
  if (!strcmp(f, "fadeclk1362bold")) { extern int mvFadeClk1362bOld; mvFadeClk1362bOld = (int)a != 0; if (logFn) logFn(mvFadeClk1362bOld ? "FADECLK1362B old=1\n" : "FADECLK1362B old=0\n"); return 1; }   // 1362Б: красный контроль часов затемнения
  if (!strcmp(f, "loadwait1362old")) { extern int loadWait1362Old; loadWait1362Old = (int)a != 0; if (logFn) logFn(loadWait1362Old ? "LOADWAIT1362 old=1\n" : "LOADWAIT1362 old=0\n"); return 1; }   // 1362: красный контроль ожидания под газетой
  if (!strcmp(f, "fadedir1349old")) { extern int mvFadeDir1349Old; mvFadeDir1349Old = (int)a != 0; if (logFn) logFn(mvFadeDir1349Old ? "FADEDIR1349 old=1\n" : "FADEDIR1349 old=0\n"); return 1; }   // 1349: красный контроль выхода из чёрного
  if (!strcmp(f, "worldhold1326old")) { extern int mvWorldHold1326Old; mvWorldHold1326Old = (int)a != 0; if (logFn) logFn(mvWorldHold1326Old ? "WORLDHOLD1326 old=1\n" : "WORLDHOLD1326 old=0\n"); return 1; }   // 1326: красный контроль ожидания мира после карточки
  if (!strcmp(f, "timerfloor1324old")) { extern int msnTimerFloor1324Old; msnTimerFloor1324Old = (int)a != 0; if (logFn) logFn(msnTimerFloor1324Old ? "TIMERFLOOR1324 old=1\n" : "TIMERFLOOR1324 old=0\n"); return 1; }   // 1324: красный контроль округления таймера
  if (!strcmp(f, "timerhold1324old")) { extern int msnTimerHold1324Old; msnTimerHold1324Old = (int)a != 0; if (logFn) logFn(msnTimerHold1324Old ? "TIMERHOLD1324 old=1\n" : "TIMERHOLD1324 old=0\n"); return 1; }   // 1324: красный контроль ожидания мира
  if (!strcmp(f, "lockhint1327old")) { extern int msnLockHint1327Old; msnLockHint1327Old = (int)a != 0; if (logFn) logFn(msnLockHint1327Old ? "LOCKHINT1327 old=1\n" : "LOCKHINT1327 old=0\n"); return 1; }   // 1327: красный контроль окна подсказки запертого этапа
  if (!strcmp(f, "lockicon1321old")) { extern int msnLockIcon1321Old; msnLockIcon1321Old = (int)a != 0; if (logFn) logFn(msnLockIcon1321Old ? "LOCKICON1321 old=1\n" : "LOCKICON1321 old=0\n"); return 1; }   // 1321: красный контроль значка после запертого этапа
  if (!strcmp(f, "resetpt1264old")) { extern int msnResetPt1264Old; msnResetPt1264Old = (int)a != 0; if (logFn) logFn(msnResetPt1264Old ? "RESETPT1264 old=1\n" : "RESETPT1264 old=0\n"); return 1; }   // №1264д: красный контроль точки перезапуска
  if (!strcmp(f, "racehold1264old")) { extern int trfRaceHold1264Old; trfRaceHold1264Old = (int)a != 0; if (logFn) logFn(trfRaceHold1264Old ? "RACEHOLD1264 old=1\n" : "RACEHOLD1264 old=0\n"); return 1; }   // №1264: красный контроль признака гонки
  if (!strcmp(f, "racefence1264old")) { extern int mvRaceFence1264Old; mvRaceFence1264Old = (int)a != 0; if (logFn) logFn(mvRaceFence1264Old ? "RACEFENCE1264 old=1\n" : "RACEFENCE1264 old=0\n"); return 1; }   // №1264: красный контроль заборов гонки
  if (!strcmp(f, "racexitplant")) { extern int mvRaceExitPlant; mvRaceExitPlant = (int)a != 0; return 1; }   // №1264: красный контроль гейта выходов из гонки
  if (!strcmp(f, "msncosm1264old")) { extern int msnCosm1264Old; msnCosm1264Old = (int)a; return 1; }   // №1264 A1.8/A1.11/A1.12: красный контроль, маска 4|1|2
  if (!strcmp(f, "bubble1264old")) { extern int mvBubble1264Old; mvBubble1264Old = (int)a; { extern int uiBub1264Old; uiBub1264Old = (int)a; } if (logFn) { char b_[48]; snprintf(b_, sizeof b_, "BUBBLE1264 old=%d\n", mvBubble1264Old); logFn(b_); } return 1; }   // №1264: красный контроль, маска 1|2|4
  if (!strcmp(f, "msnartio107bold")) { extern int msnartIo107bOld; msnartIo107bOld = (int)a != 0; if (logFn) logFn(msnartIo107bOld ? "MSNARTIO107B old=1\n" : "MSNARTIO107B old=0\n"); return 1; }   // 107б: красный контроль (картинки карточки сдаются сразу, afd −1 молчит)
  if (!strcmp(f, "msnartreopenfail")) { extern int msnartReopenFail107; msnartReopenFail107 = (int)a; if (logFn) logFn("MSNARTIO107B reopenfail set\n"); return 1; }   // 107б служебно: N первых переоткрытий дескриптора отказывают
  if (!strcmp(f, "msnartio108old")) { extern int msnartIo108Old; msnartIo108Old = (int)a != 0; if (logFn) logFn(msnartIo108Old ? "MSNARTIO108 old=1\n" : "MSNARTIO108 old=0\n"); return 1; }   // 108: красный контроль (отказ запуска чтения значка — только повтор раз в секунду)
  if (!strcmp(f, "msnartiostuck")) { extern int msnartIoStuck108; msnartIoStuck108 = (int)a != 0; if (logFn) logFn(msnartIoStuck108 ? "MSNARTIO108 stuck=1\n" : "MSNARTIO108 stuck=0\n"); return 1; }   // 108 служебно: модель залипшего асинхронного дескриптора
  if (!strcmp(f, "rideinh1274old")) { extern int aoRideInh107Old; aoRideInh107Old = (int)a != 0; if (logFn) logFn(aoRideInh107Old ? "RIDEINH1274 old=1\n" : "RIDEINH1274 old=0\n"); return 1; }   // 107б Г1: красный контроль (прыжок с платформы без скорости опоры)
  if (!strcmp(f, "ride1274old")) { extern int aoRide1274Old; aoRide1274Old = (int)a != 0; if (logFn) logFn(aoRide1274Old ? "RIDE1274 old=1\n" : "RIDE1274 old=0\n"); return 1; }   // №1274: красный контроль (Гомер на платформе не едет с ней)
  if (!strcmp(f, "quarlog107")) { extern unsigned msnartLeakBytes, msnartQuarBytes; char l_[80]; snprintf(l_, sizeof l_, "QUAR107 leak=%u held=%u\n", msnartLeakBytes, msnartQuarBytes); if (logFn) logFn(l_); return 1; }   // 107б служебно: строка QUAR107 сейчас
  if (!strcmp(f, "propghost107old")) { extern int propsGhost107Old; propsGhost107Old = (int)a != 0; if (logFn) logFn(propsGhost107Old ? "PROPGHOST107 old=1\n" : "PROPGHOST107 old=0\n"); return 1; }   // 107б П1/П2: красный контроль (лежащий предмет призрак)
  if (!strcmp(f, "spother107old")) { extern int spOther107Old; spOther107Old = (int)a != 0; if (logFn) logFn(spOther107Old ? "SPOTHER107 old=1\n" : "SPOTHER107 old=0\n"); return 1; }   // 107б П3: красный контроль (чужие машины сквозь стейтпропы)
  if (!strcmp(f, "proploss107old")) { extern int propsLoss107Old; propsLoss107Old = (int)a != 0; if (logFn) logFn(propsLoss107Old ? "PROPLOSS107 old=1\n" : "PROPLOSS107 old=0\n"); return 1; }   // 107б П5: красный контроль (две формулы потери скорости о фонарь)
  if (!strcmp(f, "propstomp107old")) { extern int propsStomp107Old; propsStomp107Old = (int)a != 0; if (logFn) logFn(propsStomp107Old ? "PROPSTOMP107 old=1\n" : "PROPSTOMP107 old=0\n"); return 1; }   // 107б П12: красный контроль (стомп по простому предмету 400, не 800)
  if (!strcmp(f, "propboom107old")) { extern int propsBoom107Old; propsBoom107Old = (int)a != 0; if (logFn) logFn(propsBoom107Old ? "PROPBOOM107 old=1\n" : "PROPBOOM107 old=0\n"); return 1; }   // 107б П11: красный контроль (взрыв по всем предметам радиуса)
  if (!strcmp(f, "cardmgprop107old")) { extern int carDmgProp107Old; carDmgProp107Old = (int)a != 0; if (logFn) logFn(carDmgProp107Old ? "CARDMGPROP107 old=1\n" : "CARDMGPROP107 old=0\n"); return 1; }   // 107б П10: красный контроль (урона от предметов нет)
  if (!strcmp(f, "pole1276old")) { extern int propsPoleKick1276Old; propsPoleKick1276Old = (int)a != 0; if (logFn) logFn(propsPoleKick1276Old ? "POLE1276 old=1\n" : "POLE1276 old=0\n"); return 1; }   // 107б П6: красный контроль (толчок 1046: столб ложится за 0,5 с)
  if (!strcmp(f, "spbush1284old")) { extern int spBush1284Old; spBush1284Old = (int)a != 0; if (logFn) logFn(spBush1284Old ? "SPBUSH1284 old=1\n" : "SPBUSH1284 old=0\n"); return 1; }   // 1284: красный контроль (круглые кусты насквозь)
  if (!strcmp(f, "spzone1284old")) { extern int spZone1284Old; spZone1284Old = (int)a != 0; if (logFn) logFn(spZone1284Old ? "SPZONE1284 old=1\n" : "SPZONE1284 old=0\n"); return 1; }   // 1284: красный контроль (сбитые ящики возвращаются через 5 с дальше 60 м)
  if (!strcmp(f, "propvol107old")) { extern int propsVol107Old; propsVol107Old = (int)a != 0; if (logFn) logFn(propsVol107Old ? "PROPVOL107 old=1\n" : "PROPVOL107 old=0\n"); return 1; }   // 107в П4: красный контроль (контакт по стволу/кругу)
  if (!strcmp(f, "collstall107")) { extern int collStall107; collStall107 = (int)a != 0; if (logFn) logFn(collStall107 ? "COLLSTALL107 on\n" : "COLLSTALL107 off\n"); return 1; }   // 107в служебно: чтение гэга «висит»
  if (!strcmp(f, "collcancelhang107")) { extern int collCancelHang107; collCancelHang107 = (int)a != 0; if (logFn) logFn(collCancelHang107 ? "COLLCANCELHANG107 on\n" : "COLLCANCELHANG107 off\n"); return 1; }   // 107в служебно: отмена чтения не возвращается
  if (!strcmp(f, "collcancelold107")) { extern int collCancelOld107; collCancelOld107 = (int)a != 0; if (logFn) logFn(collCancelOld107 ? "COLLCANCELOLD107 old=1\n" : "COLLCANCELOLD107 old=0\n"); return 1; }   // 107в: красный контроль (отмена без предела)
  if (!strcmp(f, "collkillasync107")) { extern void collKillAsync107(void); collKillAsync107(); if (logFn) logFn("COLLKILLASYNC107 async fd dead\n"); return 1; }   // 107в служебно: только фоновый дескриптор гэгов мёртв
  if (!strcmp(f, "msnartcap109old")) { extern int msnartCap109Old; msnartCap109Old = (int)a != 0; if (logFn) logFn(msnartCap109Old ? "MSNARTCAP109 old=1\n" : "MSNARTCAP109 old=0\n"); return 1; }
  if (!strcmp(f, "msnartcap109")) { extern int msnartCapSet109; msnartCapSet109 = (int)a; if (logFn) logFn("MSNARTCAP109 set\n"); return 1; }   // служебно: предел мест (стенд переполнения)
  if (!strcmp(f, "camstuck1315")) { extern void mvCamStuck1315(int); mvCamStuck1315((int)a != 0); if (logFn) logFn("CAMSTUCK1315 set\n"); return 1; }   // служебно: «камера занята» застряла (стенд К5)
  if (!strcmp(f, "camfree1310")) { extern int mvCamFree1310; mvCamFree1310 = (int)a != 0; if (logFn) logFn(mvCamFree1310 ? "CAMFREE1310 on\n" : "CAMFREE1310 off\n"); return 1; }   // служебно: пузырь не ждёт камеру старта (стенд К1)
  if (!strcmp(f, "proxbar1318old")) { extern int mvProxBar1318Old; mvProxBar1318Old = (int)a != 0; if (logFn) logFn(mvProxBar1318Old ? "PROXBAR1318 old=1\n" : "PROXBAR1318 old=0\n"); return 1; }
  if (!strcmp(f, "finline1317old")) { extern int mvFinLine1317Old; mvFinLine1317Old = (int)a != 0; if (logFn) logFn(mvFinLine1317Old ? "FINLINE1317 old=1\n" : "FINLINE1317 old=0\n"); return 1; }
  if (!strcmp(f, "bubvis1310old")) { extern int mvBubVis1310Old; mvBubVis1310Old = (int)a != 0; if (logFn) logFn(mvBubVis1310Old ? "BUBVIS1310 old=1\n" : "BUBVIS1310 old=0\n"); return 1; }
  if (!strcmp(f, "bubcam1315old")) { extern int mvBubCam1315Old; mvBubCam1315Old = (int)a != 0; if (logFn) logFn(mvBubCam1315Old ? "BUBCAM1315 old=1\n" : "BUBCAM1315 old=0\n"); return 1; }
  if (!strcmp(f, "loadq1303old")) { extern int mvLoadQ1303Old; mvLoadQ1303Old = (int)a != 0; if (logFn) logFn(mvLoadQ1303Old ? "LOADQ1303 old=1\n" : "LOADQ1303 old=0\n"); return 1; }
  if (!strcmp(f, "loadchr109old")) { extern int mvChr109Old; mvChr109Old = (int)a != 0; if (logFn) logFn(mvChr109Old ? "LOADCHR109 old=1\n" : "LOADCHR109 old=0\n"); return 1; }
  if (!strcmp(f, "loadlist109old")) { extern int mvList109Old; mvList109Old = (int)a != 0; if (logFn) logFn(mvList109Old ? "LOADLIST109 old=1\n" : "LOADLIST109 old=0\n"); return 1; }
  if (!strcmp(f, "loadready109old")) { extern int mvReady109Old; mvReady109Old = (int)a != 0; if (logFn) logFn(mvReady109Old ? "LOADREADY109 old=1\n" : "LOADREADY109 old=0\n"); return 1; }   // 109: красный контроль (карточка без ожидания очереди)
  if (!strcmp(f, "loadend109old")) { extern int msnartEnd109Old; msnartEnd109Old = (int)a != 0; if (logFn) logFn(msnartEnd109Old ? "LOADEND109 old=1\n" : "LOADEND109 old=0\n"); return 1; }   // 109: красный контроль (записи миссии снимаются на старте следующей)
  if (!strcmp(f, "mvcarsdelay109")) { extern void mvCarsDelay109(float s); mvCarsDelay109(a); if (logFn) logFn("MVCARSDELAY109 set\n"); return 1; }   // 109 служебно: модели машин фазы готовы через N с после открытия карточки
  if (!strcmp(f, "quar107old")) { extern int msnartQuar107Old; msnartQuar107Old = (int)a != 0; if (logFn) logFn(msnartQuar107Old ? "QUAR107 old=1\n" : "QUAR107 old=0\n"); return 1; }   // 107б P0: красный контроль (брошенный буфер в утечку насовсем)
  if (!strcmp(f, "msnartstall107")) { extern int msnartStall107; msnartStall107 = (int)a != 0; if (logFn) logFn(msnartStall107 ? "MSNARTSTALL107 on\n" : "MSNARTSTALL107 off\n"); return 1; }   // 107б служебно: чтение msnart «зависает» в полёте
  if (!strcmp(f, "pwr107old")) { extern int msnartPwr107Old, collPwr107Old; msnartPwr107Old = collPwr107Old = (int)a != 0; if (logFn) logFn(msnartPwr107Old ? "PWR107 old=1\n" : "PWR107 old=0\n"); return 1; }   // 107б P0: красный контроль (пробуждение пометок не сбрасывает)
  if (!strcmp(f, "collheal107old")) { extern int collHeal107Old; collHeal107Old = (int)a != 0; if (logFn) logFn(collHeal107Old ? "COLLHEAL107 old=1\n" : "COLLHEAL107 old=0\n"); return 1; }
  if (!strcmp(f, "prophot108old")) { extern int propsHot108Old; propsHot108Old = (int)a != 0; if (logFn) logFn(propsHot108Old ? "PROPHOT108 old=1\n" : "PROPHOT108 old=0\n"); return 1; }   // 108 О6/О9: красный контроль (прежние обходы предметов)
  if (!strcmp(f, "skiddense108old")) { extern int skidDense108Old; skidDense108Old = (int)a != 0; if (logFn) logFn(skidDense108Old ? "SKIDDENSE108 old=1\n" : "SKIDDENSE108 old=0\n"); return 1; }   // 108 О13: красный контроль (два обхода кольца следов)
  if (!strcmp(f, "spidle108old")) { extern int spIdle108Old; spIdle108Old = (int)a != 0; if (logFn) logFn(spIdle108Old ? "SPIDLE108 old=1\n" : "SPIDLE108 old=0\n"); return 1; }   // 108 О11: красный контроль (ранний выход шага по таблице состояний)
  if (!strcmp(f, "fxfind108old")) { extern int fxFind108Old; fxFind108Old = (int)a != 0; if (logFn) logFn(fxFind108Old ? "FXFIND108 old=1\n" : "FXFIND108 old=0\n"); return 1; }   // 108 О17: красный контроль (перебор таблицы картинок эффектов каждый раз)
  if (!strcmp(f, "fxfind108red")) { extern int fxFind108Red; fxFind108Red = (int)a; if (logFn) logFn(fxFind108Red ? "FXFIND108 red=1\n" : "FXFIND108 red=0\n"); return 1; }
  if (!strcmp(f, "fxfind108chk")) { extern int fxFind108Chk; fxFind108Chk = (int)a != 0; if (logFn) logFn(fxFind108Chk ? "FXFIND108 chk=1\n" : "FXFIND108 chk=0\n"); return 1; }
  if (!strcmp(f, "msafind108old")) { extern int msaFind108Old; msaFind108Old = (int)a != 0; if (logFn) logFn(msaFind108Old ? "MSAFIND108 old=1\n" : "MSAFIND108 old=0\n"); return 1; }   // 108 О17: красный контроль (поиск рисунка метки по имени каждый раз)
  if (!strcmp(f, "msafind108red")) { extern int msaFind108Red; msaFind108Red = (int)a; if (logFn) logFn(msaFind108Red ? "MSAFIND108 red=1\n" : "MSAFIND108 red=0\n"); return 1; }
  if (!strcmp(f, "msafind108chk")) { extern int msaFind108Chk; msaFind108Chk = (int)a != 0; if (logFn) logFn(msaFind108Chk ? "MSAFIND108 chk=1\n" : "MSAFIND108 chk=0\n"); return 1; }
  if (!strcmp(f, "prophotred")) { extern int propsHotRed; propsHotRed = (int)a != 0; if (logFn) logFn(propsHotRed ? "PROPHOT red=1\n" : "PROPHOT red=0\n"); return 1; }   // 108: красный контроль (удар не обновляет запись)
  if (!strcmp(f, "pbver108old")) { extern int pbVer108Old; pbVer108Old = (int)a != 0; if (logFn) logFn(pbVer108Old ? "PBVER108 old=1\n" : "PBVER108 old=0\n"); return 1; }   // 108 О7: красный контроль (опрос подписей запечённых предметов каждый раз)
  if (!strcmp(f, "prophotchk")) { extern int propsHotChk; propsHotChk = (int)a != 0; if (logFn) logFn(propsHotChk ? "PROPHOT chk=1\n" : "PROPHOT chk=0\n"); return 1; }   // 108: сверка горячего массива каждый кадр
  if (!strcmp(f, "bubble1247old")) { extern int mvBubble1247Old; mvBubble1247Old = (int)a != 0; uiBubble1247Set(!mvBubble1247Old); if (logFn) logFn(mvBubble1247Old ? "BUBBLE1247 old=1\n" : "BUBBLE1247 old=0\n"); return 1; }   // №1247: красный контроль (до загрузки уровня: флаг сценария не ждёт миссии)
  if (!strcmp(f, "avdelay")) { extern float msnartAvDelayS; msnartAvDelayS = a; return 1; }   // №1247: значок задания приходит через a секунд после запроса (медленная карта)
  if (!strcmp(f, "bubble1262old")) { uiBubble1262Set(!((int)a != 0)); return 1; }   // №1262: 1 — бабл без выезда, как №1251 (красный контроль)
  if (!strcmp(f, "bubble1251old")) { uiBubble1251Set(!((int)a != 0)); return 1; }   // №1251: 1 — кружок и рамка вместе, как №1247 (красный контроль)
  if (!strcmp(f, "pix8")) { uiPix8((int)a); return 1; }   // №1251 (служебно, сверка шрифта бабла с PSP): flag pix8 0 — сообщения шрифтом пакета, 1 — пиксельным 8×8
  if (!strcmp(f, "bubtrace")) { uiTrace(UI_TRACE_POP); return 1; }   // №1247: служебно — построчный журнал пузыря задания (BUBBLE1247)
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

