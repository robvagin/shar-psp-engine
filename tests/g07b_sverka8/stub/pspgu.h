// заглушка GU: считает полноэкранные затемнения (спрайт 0,0 — 480,272 с альфой цвета), остальное — пусто
#ifndef STUB_PSPGU_H
#define STUB_PSPGU_H
#define GU_COLOR_BUFFER_BIT 1
#define GU_DEPTH_BUFFER_BIT 2
#define GU_TRUE 1
#define GU_DEPTH_TEST 1
#define GU_BLEND 2
#define GU_ALPHA_TEST 3
#define GU_TEXTURE_2D 4
#define GU_ADD 0
#define GU_SRC_ALPHA 0
#define GU_ONE_MINUS_SRC_ALPHA 0
#define GU_PSM_8888 0
#define GU_TFX_REPLACE 0
#define GU_TFX_MODULATE 1
#define GU_TCC_RGB 0
#define GU_TCC_RGBA 1
#define GU_NEAREST 0
#define GU_REPEAT 0
#define GU_CLAMP 1
#define GU_SPRITES 6
#define GU_TEXTURE_32BITF 0x100
#define GU_COLOR_8888 0x1800
#define GU_VERTEX_32BITF 0x1000000
#define GU_VERTEX_16BIT 0x800000
#define GU_TRANSFORM_2D 0x800000
extern int gsDarkDraws; extern unsigned gsLastDarkA;
void *sceGuGetMemory(int n);
void sceGuDrawArray(int prim, int vt, int n, const void *idx, const void *v);
#define GU_NOP(name) static inline void name() {}
static inline void sceGuClearColor(unsigned c) { (void)c; }
static inline void sceGuClearDepth(unsigned c) { (void)c; }
static inline void sceGuClear(int f) { (void)f; }
static inline void sceGuDisable(int s) { (void)s; }
static inline void sceGuEnable(int s) { (void)s; }
static inline void sceGuDepthMask(int s) { (void)s; }
static inline void sceGuBlendFunc(int a, int b, int c, int d, int e) { (void)a; (void)b; (void)c; (void)d; (void)e; }
static inline void sceGuTexMode(int a, int b, int c, int d) { (void)a; (void)b; (void)c; (void)d; }
static inline void sceGuTexImage(int a, int b, int c, int d, const void *e) { (void)a; (void)b; (void)c; (void)d; (void)e; }
static inline void sceGuTexFunc(int a, int b) { (void)a; (void)b; }
static inline void sceGuTexFilter(int a, int b) { (void)a; (void)b; }
static inline void sceGuTexWrap(int a, int b) { (void)a; (void)b; }
#endif
