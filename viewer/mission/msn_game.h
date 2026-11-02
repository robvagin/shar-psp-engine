/* Mission system <-> game modules: the typed plug-in interface (pure C99, no PSP dependencies).
 *
 * msn.h is the runtime (flow, objectives, failure, checkpoints). It talks to "the engine" through one
 * generic MsnHost (every script command + every runtime event). This layer turns that stream into
 * typed calls grouped by the module that owns the feature, so each module plugs in its own functions:
 *
 *   player  - Homer on foot / player car   (homer module + car.c)        MsnPlayerApi
 *   cars    - mission AI cars, disposable car loads (traffic / car AI)   MsnCarsApi
 *   npc     - mission / ambient characters, bonus givers, car sellers    MsnNpcApi
 *   nav     - objective marker (sphere), arrow, radar icons              MsnNavApi
 *   ui      - objective text, timer, counter, countdown, flashes, fail   MsnUiApi   (ui.c)
 *   sound   - dialogue lines, one-liners, music events                  MsnSoundApi (audio.c)
 *   fx      - fades, FMV                                                 MsnFxApi
 *   props   - objective items, breakable power boxes (spActivateTag)    MsnPropsApi (stateprops.c)
 *   shop    - coins, ownership, unlocks                                 MsnShopApi  (coins.c)
 *   traffic - traffic limit per stage                                   MsnTrafficApi
 *   feature - everything else, passed through untyped by feature        (camera, gags, peds, police, ...)
 *
 * Every function pointer may be NULL: that call is skipped and the mission still runs (a mission whose
 * objective needs the missing module simply cannot be completed yet, e.g. no dialogue -> call
 * msn_notify(MSN_IN_DIALOGUE_DONE) yourself or leave sound.dialogue NULL: then the dialogue is
 * auto-finished after MsnGame.auto_dialogue_s seconds so the flow never blocks).
 *
 * Usage:
 *   static MsnFile mf; static MsnRuntime mrt; static MsnGame mg;
 *   msn_open(&mf, buf, size, err, sizeof err);             // buf = whole build/missions.shr, 4-byte aligned
 *   MsnGameApi api = {0}; api.user = ...; api.player.reset = homer_msn_reset; ...
 *   msn_game_init(&mg, &mrt, &mf, &api);
 *   msn_level_start(&mrt);                                 // on the loading screen
 *   each frame: MsnWorld w = {...}; msn_game_tick(&mg, dt, &w);
 *   engine facts: msn_notify(&mrt, MSN_IN_DIALOGUE_DONE / MSN_IN_FMV_DONE / MSN_IN_DESTROYED "powerbox3" /
 *                 MSN_IN_HIT_VEHICLE "cBone" ...)
 *   failure menu (after ui.failed): msn_retry(&mrt, 0) or msn_abort(&mrt).
 * Integration details: docs/missions_integration.md.
 */
#ifndef MSN_GAME_H
#define MSN_GAME_H
#include "msn.h"

#ifdef __cplusplus
extern "C" {
#endif

/* a resolved script locator (engine coordinates, z mirrored like level.shr) */
typedef struct MsnPlace {
    MsnVec3 pos;
    float heading;            /* radians (CARSTART locators), valid if has_heading            */
    int has_heading;
    const char *locator;      /* script name ("m1_marge_sd"), "" if the script gave none     */
    const MsnLoc *loc;        /* NULL if unresolved; trigger volumes: f->trigs[loc->first_trig..] */
} MsnPlace;

/* ------------------------------------------------------------------ player (homer + car) */
typedef struct MsnPlayerApi {
    /* mission start / retry / scene setup. on_foot: player at `at`, car parked at `car` (may be NULL);
       in car: at == NULL, player sits in the car at `car`. car_name "current" = the car the player uses.
       (SetMissionResetPlayerInCar / SetMissionResetPlayerOutCar / AddStageCharacter)                 */
    void (*reset)(void *u, const MsnPlace *at, const MsnPlace *car, const char *car_name);
    /* the level's default car (forced = 0, "DEFAULT") or a mission's forced car (forced = 1, "OTHER")
       with its start locator (InitLevelPlayerVehicle)                                               */
    void (*vehicle)(void *u, const char *car, int forced, const MsnPlace *at);
    void (*place_car)(void *u, const char *car, const MsnPlace *at);          /* PlacePlayerCar          */
    void (*enter_car)(void *u);                                               /* PutMFPlayerInCar        */
    void (*forced_car)(void *u, int on);            /* SetForcedCar: this mission drives the "OTHER" car */
    /* stage end: give the default car back (SwapInDefaultCar + SetSwap*Locator); any place may be NULL */
    void (*swap_default_car)(void *u, const MsnPlace *player, const MsnPlace *default_car, const MsnPlace *forced_car);
    void (*walk_to)(void *u, const MsnPlace *to);   /* SetInitialWalk: Homer walks there by himself     */
    void (*character)(void *u, const char *name, const char *model);          /* AddCharacter          */
} MsnPlayerApi;

/* ------------------------------------------------------------------ mission AI cars */
typedef struct MsnCarSpawn {
    const char *name;         /* model name = id ("skinn_v")                                   */
    MsnPlace at;
    const char *ai;           /* "race" "chase" "evade" "target" "waypoint" or "NULL" (parked)  */
    const char *con;          /* physics/AI tuning file ("Missions\level01\M1race.con")        */
    const char *driver;       /* character model at the wheel ("skinner"), "" none             */
} MsnCarSpawn;
typedef struct MsnCarsApi {
    void (*load)(void *u, const char *name, const char *file, const char *slot);   /* LoadDisposableCar: slot AI/DEFAULT/OTHER */
    void (*spawn)(void *u, const MsnCarSpawn *c);                                 /* AddStageVehicle                */
    /* AddStageWaypoint: path of this stage's AI cars; index 0 starts a new path (race finish = last point) */
    void (*waypoint)(void *u, int index, const MsnPlace *at);
    void (*activate)(void *u, const char *name, const MsnPlace *at, const char *ai); /* ActivateVehicle, at NULL = stay */
    void (*ai_params)(void *u, const char *name, int a, int b);                   /* SetVehicleAIParams (shortcut range) */
    /* rubber band: kind 0 = SetStageAIRaceCatchupParams(name, dist, slow, normal, fast), 1 = Target(name, min, max) */
    void (*catchup)(void *u, const char *name, int kind, const float *p, int n);
    void (*suppress_driver)(void *u, const char *name);                           /* SuppressDriver                 */
} MsnCarsApi;

/* ------------------------------------------------------------------ characters */
enum { MSN_NPC_MISSION = 0, MSN_NPC_AMBIENT = 1, MSN_NPC_PLACE = 2 };   /* PLACE: seller of AddPurchaseCarReward (port, edit 140) */
enum { MSN_GIVER_BONUS = 1, MSN_GIVER_STREETRACE, MSN_GIVER_WAGER, MSN_GIVER_SELLER };
typedef struct MsnGiver {
    int kind;                 /* MSN_GIVER_*                                                   */
    const char *name, *model; /* "milhouse", "npd"                                             */
    MsnPlace at;
    const char *mission;      /* bonus: "sr1" (index = mission index); seller: shop id ("gil", "simpson") */
    int mission_index;
    const char *icon, *icon_shadow;   /* drawable over the head ("checkered", "exclamation", "dice")     */
    const char *conv;         /* intro conversation name                                       */
    float radius;             /* seller talk radius                                            */
    MsnPlace car_at;          /* seller: where the car for sale is shown                        */
} MsnGiver;
typedef struct MsnNpcApi {
    /* AddNPC (mission character; interior = "" outside) / AddAmbientCharacter (radius = talk zone) */
    void (*spawn)(void *u, const char *name, const MsnPlace *at, int kind, float radius, const char *interior);
    void (*waypoint)(void *u, const char *name, const MsnPlace *at);   /* Add*NPCWaypoint: walk route      */
    void (*remove)(void *u, const char *name);                         /* RemoveNPC                        */
    void (*giver)(void *u, const MsnGiver *g);   /* AddNPCCharacterBonusMission / AddPurchaseCarReward     */
    /* talkto objective: show the talk icon over `name` (SetTalkToTarget: icon type, height, radius)     */
    void (*talk_target)(void *u, const char *name, int icon, float height, float radius);
    void (*driver)(void *u, const char *car, const char *character, int add);   /* AddDriver / RemoveDriver */
} MsnNpcApi;

/* ------------------------------------------------------------------ navigation */
typedef struct MsnMarker {
    MsnVec3 pos;              /* where to draw the marker / aim the arrow                      */
    const MsnLoc *loc;        /* locator whose trigger volumes complete the objective (may be NULL) */
    const char *drawable;     /* "carsphere" "triggersphere" or an item model; "" = arrow only  */
    int nav;                  /* MSN_NAV_* arrow mode of the objective                         */
    int mission_start;        /* 1 = first stage of a story mission intro: the "mission" radar icon */
    const char *target;       /* entity the marker follows (talkto/destroy/follow), NULL = fixed */
} MsnMarker;
typedef struct MsnNavApi {
    void (*marker)(void *u, const MsnMarker *m);   /* NULL m = clear. Re-sent when the target moves > 0.5 m */
    /* bonus giver / seller radar icon (ui.h UI_R_BONUS / UI_R_STREETRACE / UI_R_WAGER / UI_R_PURCHASE) */
    void (*giver_icon)(void *u, const MsnGiver *g, int on);
} MsnNavApi;

/* ------------------------------------------------------------------ HUD */
enum MsnFlash { MSN_FLASH_STAGE_COMPLETE = 1, MSN_FLASH_MISSION_COMPLETE, MSN_FLASH_BONUS_COMPLETE,
                MSN_FLASH_LEVEL_COMPLETE, MSN_FLASH_GAME_COMPLETE };
typedef struct MsnUiApi {
    void (*mission_title)(void *u, const char *id, const char *title, const char *info, int bonus);  /* main phase start */
    /* stage start: English text ("" = none, e.g. dialogue), bible index (ui.h UI_F_OBJECTIVE; -1 none),
       HUD icon name ("simpsons" -> msnicons; ui.h UI_F_OBJICON is the caller's lookup)             */
    void (*objective)(void *u, const char *text, int msg_index, const char *icon);
    void (*timer)(void *u, int ms, int counting_up);     /* ms < 0 = hide (UI_F_TIMER)              */
    void (*par_time)(void *u, int ms);                   /* wager race time to beat, < 0 hide       */
    void (*counter)(void *u, int done, int total);       /* items "3/9"; total 0 = hide              */
    void (*lap)(void *u, int lap, int laps);             /* race laps (laps > 1); laps 0 = hide      */
    void (*countdown)(void *u, const char *text, float seconds);   /* "3" "2" "GO!"; NULL = over     */
    void (*flash)(void *u, int what, const char *text);  /* MSN_FLASH_*, text from the bible         */
    /* mission failed: reason text + hint; show the retry menu, then msn_retry(rt, 0) / msn_abort(rt) */
    void (*failed)(void *u, int reason, const char *text, const char *hint);
    void (*presentation)(void *u, const char *bitmap);   /* SetPresentationBitmap (pause/loading art) */
} MsnUiApi;

/* ------------------------------------------------------------------ sound */
#define MSN_MAX_LINES 32
typedef struct MsnDialogue {
    const char *name;         /* conversation ("project")                                      */
    const char *pc, *npc;     /* speakers ("homer", "marge"); one-liner: npc = speaker          */
    int nlines;
    const char *file[MSN_MAX_LINES];    /* "conversations/c_project_1_convinit_hom_l1m1.rsd"    */
    const char *actor[MSN_MAX_LINES];   /* short actor code from the file name ("hom")          */
} MsnDialogue;
typedef struct MsnSoundApi {
    /* dialogue objective: play the lines in order (camera/animations: feature MSN_FEAT_DIALOGUE /
       MSN_FEAT_CAMERA pass-through), then msn_notify(rt, MSN_IN_DIALOGUE_DONE, 0, 0)               */
    void (*dialogue)(void *u, const MsnDialogue *d);
    void (*oneliner)(void *u, const MsnDialogue *d);     /* completion / pickup line, does not block */
    void (*music_event)(void *u, const char *event);     /* StageStartMusicEvent ("M4_start")        */
    void (*music_change)(void *u);                       /* AddStageMusicChange                      */
    void (*music_always_on)(void *u);                    /* SetStageMusicAlwaysOn                    */
    void (*music_state)(void *u, const char *state, const char *value); /* SetMusicState */
} MsnSoundApi;

/* ------------------------------------------------------------------ fades / movies */
typedef struct MsnFxApi {
    void (*fade)(void *u, int out, float seconds);       /* SetFadeOut (stage end): out = 1          */
    void (*stay_black)(void *u);                         /* StayInBlack: keep black into next stage  */
    void (*fmv)(void *u, const char *file);              /* fmv objective; then msn_notify(MSN_IN_FMV_DONE) */
} MsnFxApi;

/* ------------------------------------------------------------------ objective items / state props */
typedef struct MsnItem {
    MsnPlace at;
    const char *name;         /* collectible / checkpoint locator ("m6_videogames3", "PP_powerbox3") */
    const char *drawable;     /* "bonestorm" "carsphere" ... ; "" = invisible                  */
    const char *effect;       /* SetCollectibleEffect of the stage ("wrench_collect"), ""       */
    int breakable;            /* 1 = world object to break (report msn_notify(MSN_IN_DESTROYED))  */
    int tag;                  /* breakable "powerboxN" -> N (stateprops SpInst.tag), else 0      */
    int dropped;              /* dump: dropped by the rammed target at at.pos                   */
} MsnItem;
typedef struct MsnPropsApi {
    void (*item_show)(void *u, int index, const MsnItem *it);
    void (*item_collected)(void *u, int index, const MsnItem *it);   /* hide + pickup effect      */
    void (*items_clear)(void *u);                                   /* stage over: hide the rest  */
    int  (*activate_tag)(void *u, int tag);          /* stateprops spActivateTag (power boxes)      */
    void (*race_props)(void *u, const char *name, int load);   /* StreetRacePropsLoad / Unload       */
    void (*state_prop)(void *u, const char *name, const MsnPlace *at);   /* AddCollectibleStateProp (L7) */
} MsnPropsApi;

/* ------------------------------------------------------------------ coins, ownership */
enum { MSN_COINS_WAGER_FEE = 1, MSN_COINS_WAGER_WIN };
typedef struct MsnShopApi {
    void (*coins_add)(void *u, int delta, int why);      /* delta < 0 = take                        */
    int  (*owns)(void *u, int kind, const char *name);   /* MSN_OWN_CAR / MSN_OWN_SKIN             */
    void (*unlock)(void *u, const MsnReward *r);         /* bonus-mission / street-race car earned  */
    int  (*skin_spot)(void *u, MsnVec3 *out);            /* buyskin stage: nearest wardrobe (or its room door) from the player; 0 = none */
    const char *(*cur_skin)(void *u);                    /* сверка g07 (P06-8): имя надетого костюма (как в замке "locked" "skin"); NULL — функции нет, костюм неизвестен */
} MsnShopApi;

typedef struct MsnTrafficApi {
    /* SetMaxTraffic(n) / NoTrafficForStage (0) at stage start; -1 = this stage says nothing (level default) */
    void (*max_cars)(void *u, int n);
    /* сверка g07 (P06-25): ClearTrafficForStage / NoTrafficForStage — вычистить трафик в радиусе (м) вокруг игрока; NULL — не чистить */
    void (*clear)(void *u, float radius);
} MsnTrafficApi;

typedef struct MsnGameApi {
    void *user;
    /* live state of a named mission car / character (position, health, path finished); 0 = unknown */
    int  (*entity)(void *u, const char *name, MsnEntity *out);
    /* the giver of bonus mission `mission` at his spot (port, edit 180); NULL / 0 = entity() by name */
    int  (*bonus_entity)(void *u, int mission, const char *name, MsnEntity *out);
    MsnPlayerApi player; MsnCarsApi cars; MsnNpcApi npc; MsnNavApi nav; MsnUiApi ui; MsnSoundApi sound;
    MsnFxApi fx; MsnPropsApi props; MsnShopApi shop; MsnTrafficApi traffic;
    /* untyped pass-through (feature = MSN_FEAT_*: camera, gags, peds, police, collect, level, load ...) */
    void (*feature)(void *u, int feature, const MsnCall *call);
    /* cleanup: despawn everything the mission spawned (cars, NPCs, items, markers) */
    void (*cleanup)(void *u);
    /* optional tap: every runtime event after the typed calls above (debug overlay, logging, save hooks) */
    void (*on_event)(void *u, const MsnEvent *e);
} MsnGameApi;

/* HUD state kept by the adapter: poll it instead of (or in addition to) the ui callbacks */
typedef struct MsnHud {
    const char *mission_id, *title;
    const char *objective; int objective_msg; const char *icon;
    int timer_on, timer_up, timer_ms, par_ms;
    int items_done, items_total, lap, laps;
    const char *countdown;           /* NULL when not counting                                   */
    int flash; const char *flash_text; float flash_t;   /* MSN_FLASH_*, seconds since shown       */
    int failed; const char *fail_text, *fail_hint;
    int talk_prompt;                 /* bonus mission index the player can start now, -1           */
} MsnHud;

enum MsnGameRoute { MSN_GR_NONE = 0,  /* engine command this adapter does not handle (counts as unsupported) */
                    MSN_GR_RUNTIME,   /* consumed by the runtime / compiler (flow, objectives, texts)        */
                    MSN_GR_TYPED,     /* translated into a typed call above                                 */
                    MSN_GR_PASS };    /* forwarded untyped to api.feature (module not owned by this adapter)  */

typedef struct MsnGame {
    MsnGameApi api;
    MsnRuntime *rt; const MsnFile *f;
    MsnHud hud;
    float auto_dialogue_s;           /* sound.dialogue == NULL: finish dialogues after this (default 1 s) */
    float auto_fmv_s;                /* fx.fmv == NULL: same for movies                                    */
    /* internal */
    int wp_index;
    MsnPlace swap_player, swap_default, swap_forced; int swap_mask;
    int marker_on; MsnVec3 marker_pos; const MsnLoc *marker_loc;
    int wait_dialogue, wait_fmv; float wait_t;
    int stage_traffic;
    int traffic_zero;                /* P06-25: NoTrafficForStage поставил лимит 0 — он держится, пока этап не задаст SetMaxTraffic */
    /* statistics */
    uint32_t op_calls[MSN_OP_COUNT];  /* engine commands seen, by opcode                         */
    uint32_t n_typed, n_pass, n_none, n_events;
} MsnGame;

/* СВЕРКА_КАМЕРА 7.5 (передано от g06-camera): «точка в кадре» для миссий — дальше range (по умолчанию MSN_VIEW_RANGE = 250 м) не видна; направление dir — единичное,
   fov_cos — косинус половины угла обзора (диагональ кадра). 1 — видна. Камера (глаз, направление) — у вида; здесь только геометрия */
#define MSN_VIEW_RANGE 250.0f
int msn_point_in_view(const float eye[3], const float dir[3], float fov_cos, const float p[3], float range);
extern int msnSwitchOld;   /* №943: 1 — прежнее поведение смены миссии (см. msn_game.c) */
/* installs itself as the runtime's host (calls msn_init(rt, f, host)) */
void msn_game_init(MsnGame *g, MsnRuntime *rt, const MsnFile *f, const MsnGameApi *api);
/* msn_tick + marker tracking + HUD timer/prompt + auto-finish of dialogue/FMV without a module */
void msn_game_tick(MsnGame *g, float dt, const MsnWorld *w);
/* how the adapter treats an opcode (for coverage reports) */
int  msn_game_route(unsigned op);
extern const char *const msn_game_route_name[4];

#ifdef __cplusplus
}
#endif
#endif
