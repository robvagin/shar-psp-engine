#include <pspkernel.h>
#include "genest.h"   // №1188
#include <pspctrl.h>
#include <pspgu.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <string.h>
#include "ending_view.h"
#include "ui.h"
#include "sound.h"
#include "audio.h"

// PORT_RULE, copied from main.c VIEW CREDITS; original scrolling speed is not in Scrooby data
#define ENDING_CREDITS_SPEED (35.f * 272.f / 480.f)

int endingRun(const char *file, void *geList, void (*reset)(void), void *(*swap)(void),
              unsigned (*frame)(float, int), void (*log)(const char *), void **fbp) {
  GE_NEST_GUARD_RET("endingRun", 0);   // №1188
#ifndef C3_ENDING_OLD
  if (uiScrOpen(file)) { if (log) log("ENDING missing\n"); return 0; }
  int mode = sndModeCur(); sndMode(SND_MODE_CREDITS);
  int music = audioTrack("credits"); if (music >= 0) audioMusic(music, .5f);
  if (log) log("ENDING open screen=CreditsPostFMV kang=1 kodos=1\n");
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad,1);
  unsigned prev = pad.Buttons, last = sceKernelGetSystemTimeLow();
  if (frame) prev |= frame(0.f, 1);
  float scroll=0.f; UiCredits info; memset(&info,0,sizeof info);
  int alive=1, reason=0, once=0;
  for (;;) {
    unsigned now=sceKernelGetSystemTimeLow(); float dt=(now-last)*1e-6f; last=now; if(dt>.05f)dt=.05f;
    sceCtrlPeekBufferPositive(&pad,1); unsigned buttons=pad.Buttons | (frame ? frame(dt,0) : 0), hit=buttons & ~prev; prev=buttons;
    if (uiScrBackInput(hit)) { reason=1; break; }  // same Back action as the existing VIEW CREDITS
    if (once && !alive) break;
    sceGuStart(GU_DIRECT,geList); reset();
    sceGuScissor(0,0,480,272); sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
    alive=uiScrDrawCredits(0,scroll,&info);
    if (!once && log) { char s[160]; snprintf(s,sizeof s,"ENDING start blocks=%d lines=%d end=%.0f speed=%.4f\n",info.blocks,info.lines,info.end,ENDING_CREDITS_SPEED); log(s); }
    once=1; reset(); sceGuClutMode(GU_PSM_8888,0,0xff,0);
    sceGuFinish(); sceGuSync(0,0); sceDisplayWaitVblankStart(); *fbp=swap();
    scroll += dt*ENDING_CREDITS_SPEED;
  }
  if (log) { char s[100]; snprintf(s,sizeof s,"ENDING close reason=%s scroll=%.1f blocks=%d\n",reason?"skip":"end",scroll,info.blocks); log(s); }
  uiScrClose(); sndMode(mode);
  return 1;
#else
  return 0;
#endif
}
