// skin: skinned characters (Homer & co.) from build/chars/<name>.chr, drawn with GE hardware skinning.
//
// Data comes from tools/char_export.py (the owner's disc: art/chars/<name>_m.p3d + <name>_a.p3d + global.p3d).
// Homer: 33 joints, 8 draw batches (<= 8 bones each), 2312 vertices, 5 CLUT8 textures (64x64), 67 clips,
// 472 KB file (loaded whole, one allocation).
//
// ---------------------------------------------------------------------------------------------------------
// USAGE
//   SkinModel *homer = skinLoad("ms0:/.../homer.chr");          // once (NULL on error)
//   SkinPlayer p; skinInit(&p, homer);
//   skinPlay(&p, skinFindClip(homer, "loco_walk"), 1, .2f);   // loop, 0.2 s crossfade from the current pose
//   per frame (CPU, before or while building the list):
//     skinUpdate(&p, dt);                                     // advance time + blink + compute bone palette
//   per frame (inside the open display list, after sceGumUpdateMatrix / with the view+projection set):
//     ScePspFMatrix4 w; skinPlace(&w, x, y, z, heading);      // heading like main.c: forward = (sin h, 0, cos h)
//     skinDraw(&p, &w);
//     resetState();                                           // main.c: its GE state cache is stale now
//
// Lower level (locomotion blends, several layers, attachments):
//   skinSample(m, clip, t, xf)        local transforms of one clip at t seconds (clamped; root translation at rest)
//   skinBlend(a, b, w, n)             a = mix(a, b, w)   (quaternion shortest-path mix, translation lerp)
//   skinPalette(m, xf, pal, world)    bone palette (+ optional joint world matrices, model space)
//   skinDrawPal(m, pal, world, eye)   draw with an explicit palette
//   skinRootMotion(m, clip, t, xyz)   Motion_Root translation at t (model space): e.g. walk moves +0.87 m per
//                                     1.0 s cycle, get_into_car ends at (0.99, 0, 0.88); the pose itself always
//                                     stays in place, the game decides how to move the character.
//   skinClipSpeed(m, clip)            root-motion speed: walk 0.87 m/s, run 3.39, dash 9.16 -> playback rate
//                                     = ground speed / clip speed keeps the feet planted.
//   Walk/run blend by speed (phase-synchronised, no foot sliding), e.g. in the Homer controller:
//     w = clamp((v - 0.87) / (3.39 - 0.87)); len = lerp(1.00, 0.53, w); phase = fmodf(phase + dt / len, 1);
//     skinSample(m, walk, phase * 1.00f, p.xf); skinSample(m, run, phase * 0.53f, p.tmp);
//     skinBlend(p.xf, p.tmp, w, nJoints); skinPalette(m, p.xf, p.pal, p.world); skinTick(&p, dt) for the blink.
//   Off-screen characters: skinTick only (no pose, ~1 us).
//
// COORDINATES: hit-and-run-web / level.shr space (game z mirrored), metres, y up. The model faces +Z
//   (the car model faces -Z), so for main.c's heading h Homer is rotated by h, the car by h + pi.
//   Feet at y = 0, height 1.84 m (bind-pose bbox in SkinModel header).
//
// ---------------------------------------------------------------------------------------------------------
// GE STATE CHANGED BY skinDraw / skinDrawPal (nothing else is touched):
//   GU_MODEL matrix       = world (sceGuSetMatrix; the gum stack is NOT updated — main.c's lean path sets
//                           GU_MODEL itself before every draw, so nothing to do there)
//   bone matrices 0..7    overwritten (only skinned draws use them)
//   GU_LIGHTING           ENABLED during the draw, DISABLED on return; GU_LIGHT0 enabled/disabled likewise
//   light 0               directional (skinLight: default sun), ambient light colour, material colours
//                         (ambient/diffuse white, specular/emissive 0) — only matter while lighting is on
//   GU_TEXTURE_2D         ENABLED;  texture: CLUT8 64x64 swizzled, level 0 only (sceGuTexMode(GU_PSM_T8,0,0,1))
//   CLUT                  sceGuClutMode(GU_PSM_8888, 0, 0xff, 0) (same as main.c) + sceGuClutLoad(32, own)
//   texture filter        sceGuTexFilter(GU_LINEAR, GU_LINEAR)
//   texture function      sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA)   (same as main.c)
//   tex scale / offset    sceGuTexScale(128,128), sceGuTexOffset(0,0)   (same as main.c)
//   GU_BLEND, GU_ALPHA_TEST  DISABLED;  depth writes ON (sceGuDepthMask(GU_FALSE))
//   alpha func            only for batches with flag 2 (cut-out textures; none on Homer): sceGuAlphaFunc(GU_GREATER,
//                         0x40, 0xff) = main.c's value, alpha test enabled for that batch and disabled again
// NOT touched: depth test/func/range, fog (on/off and params: the character is fogged like the city),
//   cull face (stays as main.c has it: disabled; the PS2 strips do not have a consistent winding), shade model,
//   tex wrap, level mode, scissor/viewport, view/projection matrices, clear colour, blend function.
// => main.c: call resetState() after skinDraw (bound texture / flags cache). CLUT mode is identical to main.c's.
//
// COST (Homer; PSP numbers are ESTIMATES from the -O2 Allegrex code, PPSSPP timing is not PSP timing —
// the test EBOOT tests/homer shows the real us on the PSP, SELECT = 1/9/25 Homers for the GE side):
//   skinUpdate, one clip: 33 joints x (key search + quaternion mix + 1 sqrt/div) + 33 x (quat->3x3 + two 3x4
//     products) ~ 13 k instructions -> ~0.07 ms;  during a crossfade (2 clips + blend) ~ 25 k -> ~0.13 ms
//   skinDraw: 8 draws, 49 bone uploads x 13 GE words (sceGuBoneMatrix) + ~70 state words ~ 7.5 k instructions
//     -> ~0.025 ms CPU;  CPU total per Homer ~0.1 ms (target was < 0.3 ms)
//   GE: 2312 skinned vertices (up to 8 weights each), ~1300 visible triangles, 1 texture switch (eyes);
//     not measurable in the emulator. build/chars/homer_b4.chr is the same Homer with <= 4 bones per draw
//     (22 draws, 2383 vertices, 4 weights each, pixel-identical in PPSSPP) in case weights dominate on the PSP.
//   Memory: 472 KB per model (shared by all instances; clips included) + ~12 KB per SkinPlayer
//     (64-joint arrays); skinPalette also keeps a 4 KB matrix array on the caller's stack.
// ---------------------------------------------------------------------------------------------------------
#ifndef SKIN_H
#define SKIN_H

#if defined(SKIN_HOST) || defined(AUDIO_HOST) // audio now sees car/traffic types too; host needs no pspsdk
typedef struct { float x, y, z, w; } ScePspFVector4;
typedef struct { ScePspFVector4 x, y, z, w; } ScePspFMatrix4;
#else
#include <psptypes.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SKIN_MAXJ 64                 // biggest skeleton on the disc: 44 joints (h_evil)

// ---- file format CHR1 (little-endian; offsets from file start; all sections 16-byte aligned) ----
typedef struct {                     // 128 bytes
  char magic[4];                     // "CHR1"
  unsigned version, size;            // v1 arrays, v2 optional indexed batches; file size
  unsigned nJoints, nBatches, nTex, nClips, nBlink;
  unsigned offJoints, offBatches, offTex, offClips, offBlink;
  unsigned rootJoint;                // joint whose translation is root motion (Motion_Root = 0)
  float bbox[6];                     // bind pose, model space
  float blinkFps;
  unsigned blinkBatch;               // batch whose texture the blink animation replaces (0xffffffff = none)
  unsigned totalVerts, totalKeys, pad[8];
} SkinHeader;
typedef struct {                     // 128 bytes
  char name[32]; int parent; unsigned flags;
  float q[4], t[3], s[3];            // rest local transform (quaternion x,y,z,w; translation; scale)
  float inv[12];                     // inverse bind (model -> joint), 4 columns x 3 rows (GE bone layout)
} SkinJoint;
typedef struct {                     // 48 bytes: one sceGuDrawArray(GU_TRIANGLE_STRIP)
  unsigned vtype, prim, nverts, offVerts;
  unsigned short tex, nb;            // texture index, bones (= weights per vertex)
  unsigned char bones[8];            // joint index for bone slot 0..nb-1
  unsigned stride, flags;            // flags 1 = eyes (blink texture animation), 2 = alpha test (cut-out)
  unsigned pad[3];                  // v2 flags4: index offset, stored vertex count, 0
} SkinBatch;
// Logical strip length differs from the stored vertex buffer in CHR1 v2.
// All readers that copy/modify vertices (including outline caches) use this count.
static inline unsigned skinBatchStoredVerts(const SkinBatch *b) {
#ifdef CHAR_INDEXED_OLD
  return b->nverts;
#else
  return b->flags & 4 ? b->pad[1] : b->nverts;
#endif
}
typedef struct {                     // 48 bytes
  char name[24]; unsigned short w, h; unsigned psm, offClut, offPix, swizzled, pad;
} SkinTex;
typedef struct {                     // 80 bytes
  char name[48];
  float frames, fps, length;         // length = (frames - 1) / fps seconds
  unsigned flags;                    // 1 = source loop flag (0 for all of Homer's), 2 = root translation keys
  unsigned offTracks;                // nJoints x SkinTrackRec
  unsigned pad[3];
} SkinClip;
typedef struct {                     // 16 bytes; n = 0 -> rest value
  unsigned short nRot, nTrans;
  unsigned offRot;                   // u16 frame[nRot] (padded to 4 B), then s16 q[nRot][4] (x,y,z,w * 32767)
  unsigned offTrans;                 // u16 frame[nTrans] (padded to 4 B), then f32 t[nTrans][3]
  unsigned pad;
} SkinTrackRec;
typedef struct { unsigned short frame, tex; unsigned pad; } SkinBlinkKey;

typedef struct SkinModel {
  const SkinHeader *h; const SkinJoint *j; const SkinBatch *b; const SkinTex *tex; const SkinClip *c;
  const SkinBlinkKey *blink; const unsigned char *base;
} SkinModel;

typedef struct { float q[4], t[3]; } SkinXf;          // local joint transform

typedef struct {
  const SkinModel *m;
  int clip, prev;                    // current clip, clip fading out (-1 none)
  float t, prevT, speed;             // seconds; playback rate (1 = original)
  int loop, prevLoop;
  float fade, fadeLen;               // crossfade progress
  float blinkT, blinkWait; unsigned seed;
  int eyeTex;                        // texture drawn on the eye batch this frame (-1 = its own)
  SkinXf xf[SKIN_MAXJ], tmp[SKIN_MAXJ];
  ScePspFMatrix4 pal[SKIN_MAXJ] __attribute__((aligned(16)));   // bone palette (model space)
  ScePspFMatrix4 world[SKIN_MAXJ] __attribute__((aligned(16))); // joint world matrices (model space)
  int jaw; float jawOpen;            // сустав Jaw (-1 нет) и открытие рта, рад (разговор: skinJawOpen в skinPose)
} SkinPlayer;

int skinBatchValid(const SkinModel *m, const SkinBatch *b);
SkinModel *skinLoad(const char *path);
void skinSlotOpt(const SkinModel *m);   // №914: переставить слоты костей пачек для общих слотов (skin.c)
void skinSetSlotOpt(int on); extern unsigned skinSlotModels, skinSlotBefore, skinSlotAfter;
extern int skinLoadErr; extern unsigned skinLoadGot;   // №775: причина последнего отказа skinLoad (skin.c)
typedef struct SkinPre SkinPre;                 // предзагрузка модели: чтение файла в фоне (PSP: sceIoReadAsync), без остановки кадра
#ifndef SKINPRE_ERR_OLD
#define SKINPRE_ERR_OLD 0   // 1 — ошибка фонового чтения модели считается «ещё идёт» (прежнее, ревизия 1.2)
#endif
int skinPreDone(int pollRet);                 // ответ sceIoPollAsync -> прочитано ли (ошибка тоже «закончено»); для хост-проверки
SkinPre *skinPreStart(const char *path);        // NULL — файла нет / нет памяти
int skinPreReady(SkinPre *p);                   // 1 — файл прочитан целиком (хост — всегда)
SkinModel *skinPreFinish(SkinPre *p);           // разбор из памяти, освобождает p (ждёт остаток чтения); NULL — файл повреждён
void skinPreCancel(SkinPre *p);                 // отказаться от предзагрузки
void skinFree(SkinModel *m);
int skinFindClip(const SkinModel *m, const char *name);    // exact name, or suffix ("loco_walk"); -1 if none
int skinFindJoint(const SkinModel *m, const char *name);
static inline int skinClipCount(const SkinModel *m) { return (int)m->h->nClips; }
static inline const char *skinClipName(const SkinModel *m, int c) { return m->c[c].name; }
static inline float skinClipLength(const SkinModel *m, int c) { return m->c[c].length; }

void skinSample(const SkinModel *m, int clip, float t, SkinXf *out);
void skinBlend(SkinXf *a, const SkinXf *b, float w, int n);
void skinPalette(const SkinModel *m, const SkinXf *xf, ScePspFMatrix4 *pal, ScePspFMatrix4 *world);
void skinRootMotion(const SkinModel *m, int clip, float t, float out[3]);
// Рот в разговоре (правка Роба 38): у всех персонажей диска сустав Jaw (ребёнок Head); в клипах dialogue_* он не движется
// (замер tools/char_export: 0° во всех), рот открывает поворот его локальной оси +Z (проверено рендером головы Мардж, Гомера,
// Апу: +Z — челюсть вниз). Повернуть локальную позу сустава на rad вокруг +Z (после выборки клипа, до палитры)
void skinJawOpen(SkinXf *xf, int joint, float rad);

void skinInit(SkinPlayer *p, const SkinModel *m);
void skinPlay(SkinPlayer *p, int clip, int loop, float fade);   // same clip again = no restart
void skinUpdate(SkinPlayer *p, float dt);                     // = skinTick + skinPose
void skinTick(SkinPlayer *p, float dt);                       // time, crossfade, blink only (off-screen characters)
void skinPose(SkinPlayer *p);                                 // sample + blend + palette for the current state
float skinClipSpeed(const SkinModel *m, int clip);            // root-motion ground speed, m/s (walk 0.87, run 3.4)

float skinFootLift(const SkinModel *m, int clip);
void skinSetSoleOld(int on);   // №762: 1 — подошва позы привязки не вычитается (как было; «flag soleold 1»)   // №510: поправка высоты стоп клипа к позе привязки, м (прибавить к y при расстановке персонажа, не игрока)
void skinPlace(ScePspFMatrix4 *w, float x, float y, float z, float heading);
// то же с наклоном тела pitch вокруг оси X модели в точке (0, pivotY, 0): +pitch — голова (+Y) к курсу (+Z); ось остаётся на
// (x, y + pivotY, z). Правка 147 (выброс взрывом: летит горизонтально головой вперёд)
void skinPlaceTilt(ScePspFMatrix4 *w, float x, float y, float z, float heading, float pitch, float pivotY);
// Свет персонажей на улице: форма (одно солнце + фон) — правило порта, принятое Робом на уровне 1 и сверенное с записью PS2
// уровня 1 (docs/РЕЦЕПТ_УРОВНЯ.md §12); это значения уровня 1. Уровень N множит их на яркость и цвет своей группы света
// "sun" (блок LGT1 в CAR1 v3, viewer/levlight.h). Числа читает и tools/level_light.py (одна правда: этот файл)
#define SKIN_SUN_DIR_X 0.40f
#define SKIN_SUN_DIR_Y 0.80f
#define SKIN_SUN_DIR_Z 0.45f
#define SKIN_SUN_ABGR 0xff909090u
#define SKIN_AMB_ABGR 0xff707070u
#ifndef SHAR_TOON_RELEASE_OLD
#define SHAR_TOON_RELEASE_OLD 0
#endif
#define SHAR_TOON_AVAILABLE (!SHAR_TOON_RELEASE_OLD) // №428 (Роб 16.10 «всё отложенное сейчас»): режим «Cartoon» опцией Settings паузы, по умолчанию выкл. -DSHAR_TOON_RELEASE_OLD=1 — как 09.10: режим недоступен, пункта нет
#ifndef HOMER_LIT_OLD
#define HOMER_LIT_OLD 0   // 1 — свет персонажей как до 16.10 (файл homerlitold рядом с EBOOT)
#endif
#ifndef SKIN_LIT_SUN_PCT
#define SKIN_LIT_SUN_PCT 118   // №78: flag homerlit 1
#endif
#ifndef SKIN_LIT_AMB_PCT
#define SKIN_LIT_AMB_PCT 78
#endif
extern int skinSunPct, skinAmbPct;
extern int skinToon;
static inline int skinToonEnabled(void) { return SHAR_TOON_AVAILABLE && skinToon; }
// №675 (Роб 17.10 «режим мультик отлично»): в мультике контур приглушённый (≈70 % чёрного, как в старых Симпсонах: фон проходит на 30 %), на глазах обводки нет,
// тени-пятна слабее (TOON_SHADOW_PCT %), контур и у мелких предметов мира (пропы). -DSKIN_TOON_V675_OLD=1 — прежнее (чёрный контур, обводка глаз, тени как были, мир без контура)
#ifndef SKIN_TOON_V675_OLD
#define SKIN_TOON_V675_OLD 0
#endif
#ifndef TOON_SHADOW_PCT
#define TOON_SHADOW_PCT (TOON_INK_OLD ? 35 : 100)   // №754: в мультике тень не слабее, а в тон туши (spritelight.h slShFlushVec, TOON_SH_A)
#endif
#define TOON_SH_A 72   // №754: плотность тени в мультике, 0..255 (≈ 28 %, план: «индиго 25–30 %»)
#ifndef TOON_HULL_DST
#define TOON_HULL_DST 0x4d4d4du   // доля фона, проходящая сквозь контур (0x4d = 30 %): контур 70 % чёрного. Правило порта (вкус)
#endif
// №679 (Роб 17.10: «глаза обводят аккуратно», раньше «на глазах обводки нет» — действует последнее): глаза (пачки flags&1) в мультике обведены тонко: оболочка
// 0,8 см вместо 2,5 см у тела. skinToonEyes: 1 — тонкий контур глаз (по умолчанию), 0 — без обводки глаз (вариант для показа, flag tooneyes 0). TOON_EYES_OLD=1 — как WIP №675 (без)
#ifndef TOON_EYES_OLD
#define TOON_EYES_OLD 0
#endif
#define SKIN_TOON_EYE_W .008f
extern int skinToonEyes;
// №779/№781 (Роб 18.10): толщина контура и обводка глаз — из настроек (main_save.h cfgApply): width 0..10 (0 — тело без контура, 5 — по умолчанию),
// eyes 0 — без кольца, 1 — мягкое кольцо (тушь на TOON_EYE_SOFT_PCT % поверх лица, по умолчанию), 2 — полной тушью. Толщина линий мира (toonlines) — тот же width
void skinToonLineParams(int width, int eyes);
extern int skinToonLineW, skinToonEyeMode;
void skinToonAir(unsigned abgr);                 // №779: цвет дали (туман неба, main_camera.h skyFogColor): к нему светлеет тушь вдали
unsigned skinToonAirMix(unsigned ink, float d);
unsigned skinToonAirColor(void);
float skinToonWorldWNpc(float d);                // №931: то же для NPC-машин (тоньше с расстоянием, дальше TOON_NPC_N — 0)
unsigned skinToonAirMixNpc(unsigned ink, float d); // №931: тушь NPC вдали светлее
void skinToonNpc(int on);                          // №931: следующие skinDraw — прохожий NPC (контур тоньше с расстоянием), 0 — герой
float skinToonWorldW(float d);                   // №779: вытяжка контура машины на расстоянии d, м (0 — без контура)                 // №779: сам цвет дали (ABGR)  // №779: тушь на расстоянии d, м (0 до 5 м, к 40 м — 0,6 цвета дали)
#ifndef TOON_EYE_SOFT_OLD
#define TOON_EYE_SOFT_OLD 0
#endif
#ifndef TOON_EYE_SOFT_PCT
#define TOON_EYE_SOFT_PCT 45
#endif
// №675 (Роб: «одна ступень тени, как у мультика»): вторым проходом по пачкам героя — рампа из 2 цветов по свету (shade mapping GE: GU_ENVIRONMENT_MAP, U = (N·L+1)/2
// считает видеочип, текстура 32 texel NEAREST даёт жёсткую границу), результат умножает цвет заливки (blend src*dst), глубина EQUAL. Все пачки одним куском состояния:
// SKIN_TOON_SHADE_BASE слов на модель + SKIN_TOON_SHADE_WORDS на пачку (оценка сверху для skinGeBytes: кости 13 + рисование 6 + запас). TOON_SHADE_OLD=1 — без ступени (как №675)
#ifndef TOON_SHADE_OLD
#define TOON_SHADE_OLD 0
#endif
#define SKIN_TOON_SHADE_BASE 96u
#define SKIN_TOON_SHADE_WORDS 24u
static inline int skinToonShadeOn(void) { return SHAR_TOON_AVAILABLE && skinToon && !TOON_SHADE_OLD; }
// №754/№719 (Роб 17.10: «тёмный контур, индиго, цвета 90-х»; план КОНТУР_МУЛЬТИКА_ПЛАН этап 1 «одно перо»): у героев, своей машины, линий города и теней одна тушь —
// индиго-сливовая #2A2440 (около 85 % тёмного, не чёрная; реф: линия #373138). TOON_INK_OLD=1 — прежние серые 70/80 % и тень × 0,35
#ifndef TOON_INK_OLD
#define TOON_INK_OLD 0
#endif
#define TOON_INK_ABGR 0xff40242au
// №790/№796 (Роб 18.10 после TEST34: «контур фиолетово-индиго офигенный — оставить звонким, но темнее и контрастнее; у машин обводка очень светлая»): у линий
// (герои, машины, линии города) своё перо — индиго-фиолетовое темнее и сочнее #1D0E36 (оттенок 262°, насыщенность 0,73, яркость 0,21 против #2A2440: 249°, 0,44, 0,25);
// тени остаются тушью TOON_INK_ABGR. TOON_LINE796_OLD=1 — линии тушью #2A2440, как TEST34
// №797 (Роб 18.10 после TEST34: «у Барта на фоне его машины контура нет»; дирижёр: «оболочку рисовать ДО тела героя без записи глубины, с тестом глубины
// против мира — тело потом закроет внутреннее, край останется и против машины»). SKIN_TOON_PRE_OLD=1 — как TEST34 (оболочка у каждой пачки со сдвигом глубины)
#ifndef SKIN_TOON_PRE_OLD
#define SKIN_TOON_PRE_OLD 0
#endif
#ifndef TOON_AIR796_OLD
#define TOON_AIR796_OLD 0   // №796: светление линий к цвету дали — с 8 м, не больше 0,45 (skin.c toonAirMix, main_levels.h линии города)
#endif
#ifndef TOON_LINE796_OLD
#define TOON_LINE796_OLD 0
#endif
// №821 (Роб 18.10: «оттенок контура слишком светлый — чернильно-чёрный, с чернильно-сатиновым фиолетовым»; следом: «контрастней, фиолетовая,
// ядрёная, чернильная, тёмная-тёмная»): перо #16052C (оттенок 266°, насыщенность 0,89, яркость 0,17 против #1D0E36: 262°, 0,73, 0,21 — темнее и
// сочнее фиолетом, не чёрное); светление вдали — с 12 м и не больше 0,25. TOON_INK821_OLD=1 — #1D0E36, как TEST35
#ifndef TOON_INK821_OLD
#define TOON_INK821_OLD 0
#endif
// №827 (Роб 18.10: «в профиле выпуклые глаза без обводки — обводка по всему силуэту, включая глаза; внутри анфас глазам не нужна — только внешний
// выпуклый край»): пачки глаз входят во внешний силуэт предпрохода копией оболочки толщиной тела (skin.c toonHull, проход 1); мягкое кольцо глаз — как было.
// EYE_SIL827_OLD=1 — глаза вне силуэта, как TEST35
#ifndef EYE_SIL827_OLD
#define EYE_SIL827_OLD 0
#endif
#ifndef EYE_SIL838_OLD
#define EYE_SIL838_OLD 0   // №838 (цена на PSP): глаза в силуэте только у ближних (ступени 0–1, до 8 м); 1 — у всех
#endif
#ifndef TOON_RANGE823_OLD
#define TOON_RANGE823_OLD 0   // №823: ровнее толщина контура (skin.c toonPxAt, main_render.h carToonHull)
#endif
#define TOON_LINE_ABGR (TOON_LINE796_OLD ? TOON_INK_ABGR : TOON_INK821_OLD ? 0xff360e1du : 0xff2c0516u)
// №951 (Роб 25.10, игра TEST59: «варианты контура в настройках»): строка Ink на странице CARTOON. 0 Indigo — фиолетовое перо №821, как TEST59; 1 Black — перо везде
// контрастный чёрный без фиолетового отлива (линии, тени); 2 Tone — герои, машины и тени чёрным, второстепенное (линии зданий, край асфальта, мелкие пропы) — тёмный
// насыщенный тон своего цвета: асфальт — приглушённый изумрудный, здания и пропы — умбра. Цвет линий мира один на уровень (в файле линий цвета поверхности нет).
// INK951_OLD=1 — перо только индиго, строка не влияет
#ifndef INK951_OLD
#define INK951_OLD 0
#endif
#ifndef INK951_DEF
#define INK951_DEF 1   // стиль нового файла настроек: 1 Black
#endif
enum { INK_CHAR, INK_WORLD, INK_ROAD, INK_PROP, INK_SHADOW };
#define INK_BLACK_ABGR 0xff0a0a0au
#define INK_ROAD_ABGR 0xff32401au   // RGB 26,64,50 — приглушённый изумруд
#define INK_WORLD_ABGR 0xff0e2042u  // RGB 66,32,14 — умбра
// №951б (Роб 25.10, вариант 2 Tone, слово дирижёра): самое важное (герои, их машины, персонажи, тени) — чёрным; второстепенное — близким насыщенным тоном СВОЕГО цвета: здания и пропы — тёмный
// насыщенный тон среднего цвета своей текстуры, деревья и пальмы (зелёные текстуры) — насыщенно-коричневые, асфальт — приглушённый изумруд. Среднее по текстуре — tstat (texToonStat), без нового формата.
// Линии мира — один тон на уровень (среднее по не-земле и не-зелени текстур уровня, skinInkWorldAbgr). Tone берёт в обводку и высокие предметы (столбы, деревья) — только при Ink 2.
// INK951B_OLD=1 / «flag ink951bold 1» — Tone как №951 (умбра для мира и пропов, деревья без контура)
#ifndef INK951B_OLD
#define INK951B_OLD 0
#endif
#define INK_TREE_ABGR 0xff0e2c5cu   // RGB 92,44,14 — насыщенно-коричневый (стволы, пальмы, кроны)
extern int skinInkStyle, skinInk951bOld; extern unsigned skinInkWorldAbgr;
static inline int skinInkToneOn(void) { return !INK951_OLD && skinInkStyle == 2 && !skinInk951bOld; }
// средний цвет текстуры (0..255) -> тёмный насыщенный тон того же оттенка; зелень — коричневый; почти серое — тёмный нейтральный
static inline unsigned skinInkTone(unsigned r, unsigned g, unsigned b) {
  unsigned mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
  if (g * 100u > r * 105u && g * 100u > b * 112u && mx - mn > 10u) return INK_TREE_ABGR;   // зелёное (листва, трава) — коричневое перо
  float v = mx / 255.f, sat = mx ? (float)(mx - mn) / (float)mx : 0.f, v2 = v * .5f; v2 = v2 < .2f ? .2f : v2 > .34f ? .34f : v2;
  if (sat < .08f) { unsigned c = (unsigned)(v2 * 255.f * .8f); return 0xff000000u | c << 16 | c << 8 | c; }
  float s2 = sat * 1.4f + .2f; s2 = s2 > 1.f ? 1.f : s2; float hi = v2, lo = v2 * (1.f - s2), span = (float)(mx - mn);
  float c3[3] = {(float)r, (float)g, (float)b}, o[3];
  for (int i = 0; i < 3; i++) o[i] = c3[i] == (float)mx ? hi : c3[i] == (float)mn ? lo : lo + (hi - lo) * (c3[i] - (float)mn) / span;
  return 0xff000000u | (unsigned)(o[2] * 255.f + .5f) << 16 | (unsigned)(o[1] * 255.f + .5f) << 8 | (unsigned)(o[0] * 255.f + .5f);
}
static inline unsigned skinInkAbgr(int kind) {
  if (INK951_OLD || skinInkStyle <= 0) return kind == INK_SHADOW ? TOON_INK_ABGR : TOON_LINE_ABGR;
  if (skinInkStyle == 1 || kind == INK_CHAR || kind == INK_SHADOW) return INK_BLACK_ABGR;
  if (kind == INK_WORLD && skinInkToneOn() && skinInkWorldAbgr) return skinInkWorldAbgr;
  return kind == INK_ROAD ? INK_ROAD_ABGR : INK_WORLD_ABGR;
}
#ifndef TOON_HULL_ABGR
#define TOON_HULL_ABGR (TOON_INK_OLD ? 0xff4d4d4du : skinInkAbgr(INK_CHAR))   // цвет контура персонажей (было: ровный тёмно-серый 30 % яркости). Правило порта (вкус)
#endif
#ifndef TOON_HULL_BLEND_OLD
#define TOON_HULL_BLEND_OLD 0   // 1 — контур через смешивание «фон × 30 %» как WIP c535cd20
#endif
static inline float skinToonShadowK(void) { return (SHAR_TOON_AVAILABLE && skinToon && !SKIN_TOON_V675_OLD) ? (float)TOON_SHADOW_PCT / 100.f : 1.f; }
void skinSetToon(int on);
extern unsigned skinToonHullCalls, skinToonSeamFixed;
#ifndef SKIN_HOST
void skinSunDirXZ(float *x, float *z);   // 16.10: живое направление на солнце (x, z): по нему пятно Гомера смещается от солнца уровня, не L1
void skinLight(float dx, float dy, float dz, unsigned sunAbgr, unsigned ambientAbgr);  // direction TO the sun
// Свет уровня (LGT1): солнце SKIN_SUN_ABGR × dirS, фон SKIN_AMB_ABGR × ambS (R, G, B); NULL — свет уровня 1 как есть
// (направление SKIN_SUN_DIR_* без нормировки, цвета побайтно прежние). Возвращает итоговые цвета (для строки LIGHT смоука)
void skinLightLevel(const float ambS[3], const float dirS[3], unsigned *sunAbgr, unsigned *ambAbgr);
// Комната (пункт 11.2): до 4 направленных источников (направление НА источник, ABGR) + фоновый — LightGroup комнаты из
// данных (пакет SHG1). n = 0 — снова одно солнце skinLight. Действует на все следующие skinDrawPal
void skinLights(int n, const float dir[][3], const unsigned *abgr, unsigned ambientAbgr);
void skinDrawPal(const SkinModel *m, const ScePspFMatrix4 *pal, const ScePspFMatrix4 *world, int eyeTex);
void skinDraw(const SkinPlayer *p, const ScePspFMatrix4 *world);
// Правка 103 (01.10): skinDraw рисует, только если skinGeRoom(байт) = 1 (место в списке GE; NULL — без проверки), иначе skinGeSkips++.
// skinGeBytes — оценка сверху байт списка GE на одну модель. skinDrawPal (Гомер) не проверяет
extern int (*skinGeRoom)(unsigned bytes);
extern unsigned skinToonDraws, skinToonVerts, skinToonBytesNow;   // правка 428: мультяшный режим (расширение порта)
void skinToonFlush(void);
void skinToonEye(float x, float y, float z);   // №754: глаз камеры кадра — толщина контура персонажей по расстоянию (skin.c toonLevel)
extern unsigned skinGeSkips;
unsigned skinGeBytes(const SkinModel *m);
#endif

#ifdef __cplusplus
}
#endif
#endif
