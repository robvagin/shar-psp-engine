// №1104, код Trippy (оригинал: размытие кадра на PS2 — минимальная доля прошлого кадра поднимается с 0,15 до 0,8). Наш способ: один спрайт 480x272 с прошлым кадром
// из показываемого буфера VRAM (так же, как подмес viewer/jitter.h, копия кадра не нужна), смесь 80 % прошлого на готовый кадр мира, перед интерфейсом.
// Цена: один полноэкранный спрайт с текстурой из VRAM — по замеру TEST7 для jitter 0,2–0,5 мс видеочипа и +0,05 мс процессору (на железе не мерялось). Число 0,8 — из оригинала.
// TRIPPY_BLUR_OLD=1 — кода нет (размытия не будет)
#ifndef TRIPPY_BLUR_OLD
#define TRIPPY_BLUR_OLD 0
#endif
#define TRIPPY_ALPHA 0.8f
static int trippyBlend(unsigned prevOff, int fb16_) {
  if (TRIPPY_BLUR_OLD) return 0;
  sceGuDisable(GU_DEPTH_TEST); sceGuDepthMask(GU_TRUE); sceGuDisable(GU_ALPHA_TEST); sceGuDisable(GU_FOG); sceGuDisable(GU_LIGHTING); sceGuDisable(GU_CULL_FACE);
  sceGuEnable(GU_TEXTURE_2D); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuTexMode(fb16_ ? GU_PSM_5650 : GU_PSM_8888, 0, 0, 0);
  sceGuTexImage(0, 512, 512, BUF_W, (void *)((unsigned)sceGeEdramGetAddr() + prevOff));
  sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); sceGuTexFilter(GU_NEAREST, GU_NEAREST); sceGuTexScale(1.f, 1.f); sceGuTexOffset(0.f, 0.f);
  JitV *q = (JitV *)sceGuGetMemory(2 * sizeof(JitV)); unsigned col = ((unsigned)(TRIPPY_ALPHA * 255.f) << 24) | 0xffffffu;
  q[0] = (JitV){0.f, 0.f, col, 0.f, 0.f, 0.f}; q[1] = (JitV){(float)SCR_W, (float)SCR_H, col, (float)SCR_W, (float)SCR_H, 0.f};
  sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, q);
  sceGuTexScale(128.f, 128.f); sceGuDisable(GU_BLEND); sceGuEnable(GU_DEPTH_TEST); sceGuDepthMask(GU_FALSE);
  return 1;
}
