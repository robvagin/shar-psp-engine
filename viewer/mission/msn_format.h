/* MSN1 (version 2) - compiled SHAR mission pack (one file per level), produced by tools/missions/compile.py.
 * Little-endian, every section 16-byte aligned, all references are indices (no pointers).
 * Keep in sync with Pack.build() in compile.py (struct formats noted per record). */
#ifndef MSN_FORMAT_H
#define MSN_FORMAT_H
#include <stdint.h>

#define MSN_MAGIC   "MSN1"
#define MSN_VERSION 2      /* 2: + TEXTS section (UI / failure texts), header 144 bytes */

enum MsnSection {
    MSN_SEC_STROFS = 0,  /* u32[count] offsets into STRBLOB; string 0 = ""          */
    MSN_SEC_STRBLOB,     /* count = bytes, UTF-8, NUL-terminated                     */
    MSN_SEC_LOCS,        /* MsnLoc[count]       '<IBBHII3ffII'                       */
    MSN_SEC_TRIGS,       /* MsnTrig[count]      '<I3f3f9f'                           */
    MSN_SEC_ARGS,        /* MsnArg[count]       8 bytes                              */
    MSN_SEC_CMDS,        /* MsnCmd[count]       '<HBBIHH'                            */
    MSN_SEC_MISSIONS,    /* MsnMission[count]   '<IIIHHHHIII'                        */
    MSN_SEC_PHASES,      /* MsnPhase[count]     '<IIIHHIHHHHI'                       */
    MSN_SEC_STAGES,      /* MsnStage[count]     '<IHHhHIIIBBBBHHI4x'                 */
    MSN_SEC_CONDS,       /* MsnCond[count]      '<BBHIi'                             */
    MSN_SEC_CONVS,       /* MsnConv[count]      '<IIHH'                              */
    MSN_SEC_CLIPS,       /* MsnClip[count]      '<IIHBB'                             */
    MSN_SEC_LEVEL,       /* MsnLevelInfo[1]     '<8I'                                */
    MSN_SEC_FILES,       /* u32[count] string index of source file names (debug)     */
    MSN_SEC_TEXTS,       /* MsnText[count] sorted by key: bible key -> English text   */
    MSN_SEC_SPARE,       /* reserved, count 0                                        */
    MSN_SEC_COUNT
};

typedef struct { uint32_t off, count; } MsnSec;

typedef struct {
    char     magic[4];
    uint16_t version, level;
    uint32_t size, flags;
    MsnSec   sec[MSN_SEC_COUNT];
} MsnHeader;                                  /* 144 bytes */

/* locator types (Pure3D Locator2) */
enum { MSN_LT_EVENT = 0, MSN_LT_SCRIPT, MSN_LT_LOCATION, MSN_LT_CARSTART, MSN_LT_SPLINE, MSN_LT_ZONE,
       MSN_LT_OCCLUSION, MSN_LT_INTERIOR, MSN_LT_DIRECTIONAL, MSN_LT_ACTION, MSN_LT_FOV, MSN_LT_11,
       MSN_LT_STATICCAM, MSN_LT_PEDGROUP, MSN_LT_COIN, MSN_LT_SPAWNPOINT };

typedef struct {
    uint32_t name;                /* string                                           */
    uint8_t  type, ntrig;         /* locator type, number of trigger volumes          */
    uint16_t first_trig;
    uint32_t file;                /* string: p3d it came from                          */
    uint32_t data;                /* EVENT: event id; INTERIOR: string (interior name) */
    float    pos[3];              /* engine coordinates (z mirrored like level.shr)    */
    float    heading;             /* CARSTART heading (rad), NaN if none               */
    uint32_t flags;               /* bit0 = heading valid (CARSTART); bit1 = heading from the locator matrix (1317)  */
    uint32_t reserved;
} MsnLoc;                                     /* 40 bytes */

typedef struct {
    uint32_t kind;                /* 0 sphere (radius = half[0]), 1 box                */
    float    center[3];
    float    half[3];             /* half extents along axis[0..2]                     */
    float    axis[3][3];          /* unit local axes in engine coordinates             */
} MsnTrig;                                    /* 64 bytes */

enum { MSN_ARG_INT = 0, MSN_ARG_FLOAT = 1, MSN_ARG_STR = 2, MSN_ARG_BARE = 3 };
typedef struct {
    uint8_t  type, pad;
    uint16_t loc;                 /* resolved locator index + 1, 0 = not a locator     */
    union { int32_t i; float f; uint32_t s; } v;
} MsnArg;                                     /* 8 bytes */

enum { MSN_CTX_BODY = 0, MSN_CTX_OBJ = 1, MSN_CTX_COND = 2 };
typedef struct {
    uint16_t op;                  /* enum MsnOp (msn_ops.h)                            */
    uint8_t  argc, ctx;
    uint32_t arg0;                /* index of first MsnArg                             */
    uint16_t line, file;          /* source line / MSN_SEC_FILES index                 */
} MsnCmd;                                     /* 12 bytes */

enum { MSN_MF_BONUS = 1, MSN_MF_STREETRACE = 2, MSN_MF_GAMBLE = 4, MSN_MF_BONUSMISSION = 8, MSN_MF_NOSCRIPT = 16,
       MSN_MF_LEVELOVER = 32, MSN_MF_GAMEOVER = 64, MSN_MF_FORCEDCAR = 128, MSN_MF_TRANSITION = 256 };
#define MSN_NONE16 0xFFFFu
typedef struct {
    uint32_t id, title, info;     /* strings: "m1", title text, description text       */
    uint16_t flags, sched;        /* MSN_MF_*, index in level.mfk schedule             */
    uint16_t intro, main;         /* phase indices, MSN_NONE16 = none                  */
    uint32_t bonus_npc;           /* cmd index of AddNPCCharacterBonusMission or ~0    */
    uint32_t reserved[2];
} MsnMission;                                 /* 32 bytes */

enum { MSN_PF_INTRO = 1, MSN_PF_SYNTHETIC = 2 };
typedef struct {
    uint32_t name, file;          /* "m1sd" / "m1", source script                      */
    uint32_t setup_first;         /* commands before the first AddStage                */
    uint16_t setup_count, load_count;
    uint32_t load_first;          /* commands of the load script (mXl / mXsdl)         */
    uint16_t first_stage, stage_count, flags, reserved;
    uint32_t reserved2;
} MsnPhase;                                   /* 32 bytes */

enum { MSN_SF_FINAL = 1, MSN_SF_CHECKPOINT = 2, MSN_SF_LOCKED = 4, MSN_SF_SHOWCOMPLETE = 8, MSN_SF_MUSTACTION = 16,
       MSN_SF_GOTODIALOGOFF = 32, MSN_SF_ELAPSED = 64, MSN_SF_MUSICON = 128 };
enum { MSN_NAV_DEFAULT = 0, MSN_NAV_NEITHER, MSN_NAV_BOTH, MSN_NAV_NEAREST_ROAD, MSN_NAV_INTERSECTION };
typedef struct {
    uint32_t cmd_first;           /* all commands AddStage..CloseStage                 */
    uint16_t cmd_count, flags;    /* MSN_SF_*                                          */
    int16_t  num;                 /* AddStage(<int>) raw value, -1 none (meaning unknown) */
    uint16_t msg;                 /* SetStageMessageIndex, MSN_NONE16 none             */
    uint32_t text;                /* string: objective text (English)                  */
    uint32_t icon;                /* string: SetHUDIcon                                */
    uint32_t lock_name;           /* string: AddStage("locked", kind, name)            */
    uint8_t  lock_kind;           /* 0 none, 1 car, 2 skin                             */
    uint8_t  obj;                 /* enum MsnObjective                                 */
    uint8_t  ncond, nav;          /* condition count, MSN_NAV_*                        */
    uint16_t cond_first, obj_cmd; /* obj_cmd = AddObjective offset inside the stage    */
    uint32_t obj_arg;             /* string: buycar/buyskin target, "gamble", ...      */
    uint32_t pad;
} MsnStage;                                   /* 40 bytes */

typedef struct {
    uint8_t  type, pad;           /* enum MsnCondType                                  */
    uint16_t cmd_count;
    uint32_t cmd_first;           /* AddCondition .. CloseCondition                    */
    int32_t  arg;                 /* AddCondition 2nd int arg (keepbarrel 2)           */
} MsnCond;                                    /* 12 bytes */

typedef struct { uint32_t key, value; } MsnText;                                 /* 8 bytes, strings */
typedef struct { uint32_t name, mission; uint16_t first, count; } MsnConv;      /* 12 bytes */
enum { MSN_CLIP_CONVINIT = 0, MSN_CLIP_NOBOX = 1, MSN_CLIP_TUTORIAL = 2 };
typedef struct { uint32_t file, actor; uint16_t line; uint8_t kind, pad; } MsnClip; /* 12 bytes */

typedef struct {
    uint32_t lvl_first, lvl_count;     /* level.mfk                                    */
    uint32_t init_first, init_count;   /* leveli.mfk                                   */
    uint32_t rew_first, rew_count;     /* BindReward lines of this level (rewards.mfk) */
    uint32_t n_story, reserved;
} MsnLevelInfo;

/* objective / condition enums: order = commands.OBJECTIVES / CONDITIONS (+1) */
enum MsnObjective { MSN_OBJ_NONE = 0, MSN_OBJ_GOTO, MSN_OBJ_GETIN, MSN_OBJ_TALKTO, MSN_OBJ_DIALOGUE, MSN_OBJ_INTERIOR,
    MSN_OBJ_GOOUTSIDE, MSN_OBJ_RACE, MSN_OBJ_FOLLOW, MSN_OBJ_LOSETAIL, MSN_OBJ_DELIVERY, MSN_OBJ_DUMP, MSN_OBJ_DESTROY,
    MSN_OBJ_PICKUPITEM, MSN_OBJ_DESTROYBOSS, MSN_OBJ_TIMER, MSN_OBJ_FMV, MSN_OBJ_BUYCAR, MSN_OBJ_BUYSKIN, MSN_OBJ_COINS,
    MSN_OBJ_DUMMY, MSN_OBJ_COUNT };
enum MsnCondType { MSN_COND_NONE = 0, MSN_COND_TIMEOUT, MSN_COND_OUTOFVEHICLE, MSN_COND_DAMAGE, MSN_COND_POSITION,
    MSN_COND_RACE, MSN_COND_FOLLOWDISTANCE, MSN_COND_KEEPBARREL, MSN_COND_COUNT };

#endif
