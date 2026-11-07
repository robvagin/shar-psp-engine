/* Mission runtime for the SHAR PSP port - public API (pure C99, no PSP dependencies).
 *
 * Data: one compiled pack per level (build/missions/levelNN.msn, tools/missions/compile.py).
 * Model: level -> missions (story m0..m7 in order, bonus sr1..sr3/gr1/bm1 on demand)
 *        mission -> phases (intro "m1sd": drive to the giver and talk; main "m1")
 *        phase -> stages; stage = 1 objective + 0..n failure conditions + side commands.
 *
 * The runtime owns the flow (stage order, objective completion, failure, checkpoints, timers).
 * The engine owns the world. They talk through MsnHost:
 *   - host.command(): every script command the engine must execute (spawn car / NPC, load p3d,
 *     cameras, music, traffic ...), with the moment it applies (MSN_AT_*). The engine switch()es on
 *     call->cmd->op (enum MsnOp in msn_ops.h) and ignores what it does not implement yet.
 *   - host.event():   what the runtime decided (stage started, marker, timer, dialogue to play,
 *     stage complete, mission failed ...). Drive HUD / audio / fades from here.
 *   - host.entity():  live state of a named mission vehicle / NPC (position, health, finished path).
 *   - host.owns():    does the player own car/skin X (shop).
 *   - MsnWorld (per tick): player position, on foot or in car, vehicle health, interior, action button.
 *   - msn_notify(): engine-side facts (dialogue finished, FMV finished, boss hit, car rammed ...).
 *
 * Call order per frame:  fill MsnWorld -> msn_tick(rt, dt, &world) -> (callbacks fire inside).
 * Nothing allocates after msn_open(); MsnRuntime is a plain struct (~3 KB), keep one per level.
 */
#ifndef MSN_H
#define MSN_H
#include <stddef.h>
#include "msn_format.h"
#include "msn_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- loaded pack (read-only view over a caller-owned buffer) ---------------- */
typedef struct MsnFile {
    const uint8_t *base; uint32_t size;
    const MsnHeader *hdr;
    const uint32_t *strofs; const char *strblob; uint32_t nstr, strbytes;
    const MsnLoc *locs; uint32_t nlocs;
    const MsnTrig *trigs; uint32_t ntrigs;
    const MsnArg *args; uint32_t nargs;
    const MsnCmd *cmds; uint32_t ncmds;
    const MsnMission *missions; uint32_t nmissions;
    const MsnPhase *phases; uint32_t nphases;
    const MsnStage *stages; uint32_t nstages;
    const MsnCond *conds; uint32_t nconds;
    const MsnConv *convs; uint32_t nconvs;
    const MsnClip *clips; uint32_t nclips;
    const MsnLevelInfo *level;
    const uint32_t *files; uint32_t nfiles;
    const MsnText *texts; uint32_t ntexts;
} MsnFile;

/* Validates everything (bounds of every index) so the runtime never checks again.
 * data must stay alive and be 4-byte aligned. Returns 0 on success, else writes err. */
int msn_open(MsnFile *f, const void *data, size_t size, char *err, int errlen);

const char   *msn_str(const MsnFile *f, uint32_t s);
const char   *msn_op_name(unsigned op);
/* argument accessors (i >= argc returns 0 / "" / NULL); numeric getters convert numeric strings */
int           msn_argc(const MsnCmd *c);
const MsnArg *msn_arg(const MsnFile *f, const MsnCmd *c, int i);
const char   *msn_arg_s(const MsnFile *f, const MsnCmd *c, int i);   /* numbers are formatted */
int           msn_arg_i(const MsnFile *f, const MsnCmd *c, int i);
float         msn_arg_f(const MsnFile *f, const MsnCmd *c, int i);
const MsnLoc *msn_arg_loc(const MsnFile *f, const MsnCmd *c, int i); /* resolved locator or NULL */
const MsnConv *msn_find_conv(const MsnFile *f, const char *name, const char *mission);
int           msn_find_mission(const MsnFile *f, const char *id);
int           msn_streq(const char *a, const char *b);                /* case-insensitive */

/* English UI texts from the game's text bible (TEXTS section): "MISSION_COMPLETE" -> "MISSION COMPLETE!",
 * countdown keys ("3", "GO" -> "GO!"). Unknown key -> the key itself. '\n' = line break of the bible. */
const char   *msn_text(const MsnFile *f, const char *key);
const char   *msn_text_h(const MsnFile *f, uint32_t fnv1a);   /* текст по хешу ключа (FNV-1a 32 по байтам ключа); NULL, если такого ключа в пакете нет */
const char   *msn_fail_text(const MsnFile *f, int reason);            /* enum MsnFail -> "Timed out."     */
const char   *msn_fail_hint(const MsnFile *f, int reason, int n);     /* hint n (0..7) or ""              */

/* rewards of this level (rewards.mfk BindReward): default car/skin, bonus-mission car, street-race car, for sale */
typedef struct MsnReward {
    const char *name, *file;  /* "plowk_v", "art\cars\plowk_v.p3d"                                    */
    int kind;                 /* MSN_OWN_CAR / MSN_OWN_SKIN                                           */
    const char *quest;        /* "defaultcar" "defaultskin" "bonusmission" "streetrace" "forsale"     */
    int price;                /* coins (forsale)                                                      */
    const char *seller;       /* "simpson" (Barney) / "gil" / "interior" (costume shop)               */
} MsnReward;
int msn_reward_count(const MsnFile *f);
int msn_reward_get(const MsnFile *f, int k, MsnReward *r);
int msn_reward_find(const MsnFile *f, const char *quest, MsnReward *r);   /* index or -1 */

typedef struct { float x, y, z; } MsnVec3;
/* point inside any trigger volume of the locator (sphere / oriented box); if the locator has no
 * trigger, inside the sphere of default_radius around its position. */
int msn_loc_contains(const MsnFile *f, const MsnLoc *l, MsnVec3 p, float default_radius);

/* ---------------- engine interface ---------------- */
enum MsnWhen {
    MSN_AT_LEVEL = 1,     /* level.mfk: loads, gags, teleports ...                            */
    MSN_AT_LEVEL_INIT,    /* leveli.mfk: player car, characters, traffic/ped groups, NPCs ... */
    MSN_AT_REWARDS,       /* BindReward lines of this level                                   */
    MSN_AT_LOAD,          /* mission load script (mXl / mXsdl): LoadP3DFile, LoadDisposableCar */
    MSN_AT_SETUP,         /* phase setup before 1st stage: reset player position, cameras ...  */
    MSN_AT_STAGE,         /* stage started: spawns, waypoints, music, traffic ...              */
    MSN_AT_STAGE_END,     /* stage completed: fade out, swap car back ...                      */
    MSN_AT_REPLAY         /* retry from checkpoint: re-spawn what earlier stages had spawned   */
};

typedef struct MsnCall {
    const MsnFile *f;
    const MsnCmd  *cmd;       /* cmd->op = enum MsnOp; args via msn_arg_*(f, cmd, i)           */
    int when;                 /* enum MsnWhen                                                  */
    int mission, stage;       /* indices (-1 for level scope)                                  */
} MsnCall;

enum MsnEventType {
    MSN_EV_MISSION_START = 1, /* mission, name=id, text=title, value=1 if bonus               */
    MSN_EV_PHASE_START,       /* mission, index = 0 intro / 1 main, name = "m1sd"              */
    MSN_EV_STAGE_START,       /* stage, index=objective, text=objective text, name=HUD icon, value=time limit (s) or 0 */
    MSN_EV_STAGE_SKIPPED,     /* locked stage skipped because the car/skin is owned: name       */
    MSN_EV_MARKER,            /* objective marker: pos, loc (may be NULL), name = drawable (carsphere ...), index=nav mode */
    MSN_EV_MARKER_CLEAR,
    MSN_EV_TIMER,             /* value = seconds (remaining, or elapsed if index==1); sent on change of whole second */
    MSN_EV_COUNTDOWN,         /* text = "3" / "GO" ..., value = seconds this step lasts          */
    MSN_EV_DIALOGUE,          /* conv (clips to play in order), name = pc, name2 = npc, text = conversation name.
                                 Engine plays it, then msn_notify(MSN_IN_DIALOGUE_DONE).          */
    MSN_EV_FMV,               /* name = movie file (fmv2.rmv); then msn_notify(MSN_IN_FMV_DONE)  */
    MSN_EV_ITEMS,             /* item list changed: index = collected, value = total             */
    MSN_EV_COLLECT,           /* index = item, loc, name = effect (wrench_collect ...), name2 = one-liner conv */
    MSN_EV_DROP,              /* dump objective: item index released at pos                      */
    MSN_EV_PAY,               /* coins objective: value = coins to take                          */
    MSN_EV_STAGE_COMPLETE,    /* index = 1 if "stage complete" flourish; conv/text = completion line, name2 = speaker */
    MSN_EV_CHECKPOINT,        /* stage became the retry point                                    */
    MSN_EV_MISSION_COMPLETE,  /* mission, name=id, text=title, index = reward quest (0 none, 1 bonusmission, 2 streetrace, 3 gamble) value=coins won */
    MSN_EV_MISSION_FAILED,    /* index = enum MsnFail, name = reason id, text = reason text ("Timed out."), name2 = hint */
    MSN_EV_CLEANUP,           /* despawn everything the mission spawned                          */
    MSN_EV_LEVEL_COMPLETE,    /* index = 1 if game over (last level)                             */
    MSN_EV_BONUS_AVAILABLE,   /* free roam: bonus giver name, name2 = mission id, index = mission, loc = where (at level start) */
    MSN_EV_ITEM_SHOW          /* objective item appears (delivery / race at objective start): index = item, pos, loc,
                                 name = drawable ("bonestorm", "carsphere"; "" = invisible checkpoint),
                                 name2 = collectible name, value = 1 if it must be BROKEN (delivery item without a
                                 drawable, e.g. L1 m4 "PP_powerbox3": the world state prop powerbox3; the engine
                                 reports it with msn_notify(MSN_IN_DESTROYED, "powerbox3", 0))           */
    ,
    /* сверка g07-missions (02.11.2026): новые события, вид и модули подхватывают по мере готовности; без обработчика ничего не ломают */
    MSN_EV_LOCKED_HINT,       /* P06-8 / P07-2: запертый этап кончился, а нужной машины/костюма нет: index = MSN_OWN_*, name = имя замка, text = текст этапа,
                                 value = номер сообщения этапа (или -1); следующему этапу стартовое сообщение подавлено */
    MSN_EV_PAR_LATE,          /* P06-4 / P07-4: ставочная гонка дольше пар-времени, но не дольше пар + 15 с: index = секунды сверх пара, value = секунд до провала
                                 (выдаётся каждую целую секунду; текст проигрыша ставочной гонки из библии) */
    MSN_EV_IRIS,              /* P06-10: SetIrisWipe конца этапа: index = 1 закрыть ирис (скорость 1,0), 0 открыть (старт следующего этапа) */
    MSN_EV_OUTOFVEHICLE,      /* P06 Д3 / P07-12: условие «не выходи из машины»: index = 1 включилось (игрок вышел; name = машина), 0 выключилось; value = окно, с */
    MSN_EV_FOLLOW_NEAR,       /* P07-29: проверка followdistance раз в 100 мс: value = близость 1 − расстояние/максимум (0..1), index = расстояние, м (целое) */
    MSN_EV_WAGER_DENIED,      /* P07-22: взнос ставки не хватает монет: отказ-звук и миссия отменяется (msn_abort) на этом же кадре */
    MSN_EV_DROP_EXPIRE,       /* P07-10: выпавший предмет режима «столкновения» пропал по сроку 15 с: index = предмет; value = 1 — за 5 с до этого мигал */
    MSN_EV_DROP_BLINK,        /* P07-10: последние 5 с жизни выпавшего предмета: index = предмет, value = 1 — мигание включено (период 256 мс у вида) */
    MSN_EV_HIDE_CHARACTER,    /* P06-21: SetCharacterToHide(имя): на выходе из этапа персонаж убирается из мира; name = имя */
    MSN_EV_HUD_SHOW,          /* P06-21: ShowHUD: index = 1 показать, 0 спрятать HUD миссии */
    MSN_EV_TIMER_PAUSE,       /* P06-11 / P07-26: таймер миссии стоит (переход в интерьер, арест): index = 1 встал, 0 пошёл */
    MSN_EV_LEVEL_OVER_NOW     /* P06-22: SetLevelOver / SetGameOver на СТАРТЕ этапа: index = 1 если SetGameOver; уровень завершается сразу */
};
enum MsnFail { MSN_FAIL_NONE = 0, MSN_FAIL_TIMEOUT, MSN_FAIL_OUTOFVEHICLE, MSN_FAIL_DAMAGE, MSN_FAIL_POSITION,
               MSN_FAIL_RACE, MSN_FAIL_FOLLOWDISTANCE, MSN_FAIL_KEEPBARREL, MSN_FAIL_PARTIME, MSN_FAIL_ABORT, MSN_FAIL_HITNRUN, MSN_FAIL_COLLECT /* круг 7: выпавший предмет дальше 200 м от игрока */ };

typedef struct MsnEvent {
    int type;                 /* enum MsnEventType */
    int mission, stage, index;
    float value;
    MsnVec3 pos;
    const char *name, *name2, *text;
    const MsnLoc *loc;
    const MsnConv *conv;
} MsnEvent;

typedef struct MsnEntity {
    MsnVec3 pos;
    float health;             /* 1 = intact, 0 = destroyed                                     */
    int finished;             /* AI vehicle reached the end of its stage waypoints              */
    int waypoint;             /* index of the last waypoint passed (-1 none)                   */
} MsnEntity;

enum { MSN_OWN_CAR = 1, MSN_OWN_SKIN = 2 };
typedef struct MsnHost {
    void *user;
    void (*command)(void *user, const MsnCall *call);
    void (*event)(void *user, const MsnEvent *ev);
    int  (*entity)(void *user, const char *name, MsnEntity *out);   /* 0 = unknown / not spawned */
    int  (*owns)(void *user, int kind, const char *name);
    /* the giver of bonus mission `mission` where he stands (port, edit 180: a bonus giver and an ambient character may share a
       name, e.g. level 1 Ralph); NULL or 0 = look it up by name with entity() */
    int  (*bonus_entity)(void *user, int mission, const char *name, MsnEntity *out);
    /* №ночь 11.10: метка этапа buyskin — ближайший гардероб (или дверь его комнаты); NULL или 0 = метки нет */
    int  (*skin_spot)(void *user, MsnVec3 *out);
    /* сверка g07 (P06-8 / P07-2): имя костюма, надетого сейчас (как в замке этапа «locked» "skin"); NULL у хоста — костюм неизвестен, замок считается пройденным */
    const char *(*cur_skin)(void *user);
} MsnHost;

typedef struct MsnWorld {
    MsnVec3 player;           /* player (character or car) position                            */
    int on_foot;
    const char *vehicle;      /* current/last player vehicle name ("famil_v")                  */
    float vehicle_health;     /* 0..1                                                          */
    const char *interior;     /* NULL outside, else interior name as scripts use it ("KwikEMart") */
    int action;               /* action button pressed this frame (edge)                       */
    int coins;
    /* сверка g07: признаки состояния мира; хост, не заполняющий их, оставляет 0 (прежнее поведение рантайма) */
    int transition;           /* идёт вход в помещение или выход из него (от начала перехода до конца): таймер миссии и условия времени стоят (P06-11, P07-26) */
    int arrested;             /* полиция ловит игрока (busted): то же */
    int vehicle_boom;         /* 0 — хост не знает; 1 — машина под игроком взрывается (анимация идёт); 2 — анимация взрыва доиграна (P07-13) */
    const char *entering;     /* P07-32: имя интерьера, в который игрок ВХОДИТ (переход начался), NULL — нет */
} MsnWorld;

enum MsnInput { MSN_IN_DIALOGUE_DONE = 1, MSN_IN_FMV_DONE, MSN_IN_BOSS_HIT, MSN_IN_CARGO_LOST,
                MSN_IN_HIT_VEHICLE /* name = rammed vehicle */, MSN_IN_COLLECTED /* value = item index */,
                MSN_IN_DESTROYED   /* name = world object broken ("powerbox3"; "PP_" prefix optional) */,
                MSN_IN_BUSTED      /* Hit & Run: police caught the player -> running mission fails (MISSION_FAILED_HIT_N_RUN) */ };

/* ---------------- runtime ---------------- */
enum MsnStatus { MSN_IDLE = 0, MSN_RUNNING, MSN_COUNTDOWN, MSN_BETWEEN, MSN_MISSION_DONE, MSN_FAILED, MSN_LEVEL_DONE };

#define MSN_MAX_ITEMS 48
#define MSN_MAX_REG   96

typedef struct MsnRuntime {
    const MsnFile *f;
    MsnHost host;
    /* configuration (seconds / meters), defaults set by msn_init */
    float delay_stage, delay_stage_flourish, delay_mission;
    float radius_car, radius_foot, talk_radius;
    /* progress */
    uint32_t completed;               /* bit per mission index                        */
    int story;                        /* story mission index (mission table)          */
    int cur;                          /* running mission index                        */
    int phase;                        /* 0 intro, 1 main                              */
    int stage;                        /* index inside phase                           */
    int cp_phase, cp_stage;           /* checkpoint                                   */
    int cp_timer, restoring; float cp_remaining;
    int status, fail;
    float delay;
    int auto_bonus;                   /* 1 (default): action button next to a bonus giver in free roam starts it */
    int dev_bonus_replay;            /* transient DEV-only access to completed bonus NPCs; not saved */
    int fail_count;                   /* rotates the failure hints                                */
    int defer_first;                  /* №943: msn_level_start does not start the first story mission (the chosen MISSION SELECT row starts itself) */
    /* stage state */
    const MsnStage *st; int st_index; int obj;
    float t_stage;
    int timer_on, timer_up; float remaining, par; int timer_last;
    float countdown; int cd_step, cd_count; const MsnCmd *cd_cmds[12]; float cd_left;
    float out_time, follow_time, escape_time, hit_cooldown;
    int cargo, flag_dialogue, flag_fmv, flag_boss, flag_cargo_lost, items_changed;
    const char *hit_vehicle; char hitbuf[32];
    /* objective cache */
    const MsnCmd *c_dest, *c_target, *c_talk, *c_dialog, *c_fmv, *c_completion, *c_pickup_prop;
    const char *target; float objdist, duration; int fee, laps;
    const MsnCmd *items[MSN_MAX_ITEMS]; int nitems;         /* one lap */
    uint8_t done[MSN_MAX_ITEMS]; uint8_t released[MSN_MAX_ITEMS]; MsnVec3 relpos[MSN_MAX_ITEMS];
    int ncollected, lap, next_item;
    int bind_item[MSN_MAX_ITEMS], bind_wp[MSN_MAX_ITEMS], nbind;
    /* registries: name -> where it stands (fallback when host.entity() does not know it) */
    struct { uint32_t name; uint16_t loc; uint8_t kind, level; uint32_t extra; } reg[MSN_MAX_REG];
    int nreg;
    /* statistics for the dry run / profiling */
    uint32_t n_dispatch[MSN_OP_COUNT];
    uint8_t *trace;                   /* optional: trace[cmd index] = 1 when the command was processed */
    /* сверка g07 (02.11.2026) */
    const MsnWorld *w_now;            /* мир текущего тика (только внутри msn_tick) */
    char w_vehicle[32]; int w_on_foot, w_prev_foot, w_have;      /* что видел последний msn_tick: машина под игроком, пешком ли, был ли тик */
    int suppress_start;               /* P06-8: у следующего этапа подавить стартовое сообщение */
    int iris_closed;                  /* P06-10: ирис закрыт концом этапа, откроется на старте следующего */
    int barrel_back;                  /* круг 3: разбитая бочка — на сколько этапов откатить (0 — нет) */
    int par_late, par_warn;           /* ставка: гонка дольше пар-времени (целые секунды), последняя выданная секунда предупреждения */
    int oov_on; char oov_car[32]; float oov_win;   /* «не выходи из машины»: включено ли, машина первого выхода, окно (с) */
    int paused; float follow_acc;     /* таймер стоит (переход/арест); накопитель шага followdistance 100 мс */
    int boom_wait;                    /* условие damage ждёт конец взрыва */
    int allow_abort;                  /* AllowMissionAbort("false") вступления: 0 — Accept/Cancel не показывать */
    uint32_t rng;                     /* детерминированный случайный: подсказки провала, задержка выпадения */
    float drop_delay[MSN_MAX_ITEMS], drop_life[MSN_MAX_ITEMS]; uint8_t drop_pend[MSN_MAX_ITEMS], drop_blink[MSN_MAX_ITEMS];
    int buy_fallback;                 /* круг 5, P06-24: хост не смог поставить купленную машину игроку — этап buycar закрывается по владению, как раньше */
    float hit_delay;                  /* P07-10: удар по цели dump засчитан, предмет выпадет через 1 с (<= 0 — ждать удара) */
} MsnRuntime;

enum { MSN_REG_NPC = 1, MSN_REG_VEHICLE, MSN_REG_PROP, MSN_REG_BONUS, MSN_REG_SELLER };

void msn_init(MsnRuntime *rt, const MsnFile *f, const MsnHost *host);
/* level.mfk + leveli.mfk + rewards, then the first story mission not in rt->completed */
void msn_level_start(MsnRuntime *rt);
/* start mission (index in f->missions) from its intro phase (or checkpoint if restore) */
void msn_start_mission(MsnRuntime *rt, int mission, int restore);
int msn_service_pass(MsnRuntime *rt);   /* служебное: засчитать текущий этап, как цель (прогон «начало-конец миссий») */
void msn_enter_mission(MsnRuntime *rt, int mission);   /* круг 7 (P06-5): вход «как после загрузки» — вступление с этапа RESET_TO_HERE */
/* player talked to a bonus giver in free roam; returns 0 if not allowed now */
int  msn_start_bonus(MsnRuntime *rt, int mission);
/* free roam: bonus mission whose giver stands within talk range of the player on foot (for the HUD
 * "talk" prompt), -1 none. msn_tick starts it on the action button when rt->auto_bonus. */
int  msn_bonus_near(MsnRuntime *rt, const MsnWorld *w);
/* Правка 345 (атлас миссий 332, по данным и PS2): дальность разговора △ с персонажем — радиус триггера SetTalkToTarget, аргумент 4
   («"3.0" is the size of the NPC trigger area», m0i.mfk), без него — MSN_TALK_R; от точки самого персонажа, без запаса.
   c_talk — команда SetTalkToTarget этапа (NULL — бонус-персонаж). msnTalkOld = 1 (flag talkold 1) — прежнее: 3 м (или аргумент) + 1 м */
#define MSN_TALK_R 1.3f
extern int msnTalkOld;
extern int msnTalkRoomOld;   /* 1 — радиус разговора в комнате без нижней границы 2,6 м (как в TEST89) */
float msn_talk_reach_w(const MsnRuntime *rt, const MsnCmd *c_talk, const MsnWorld *w);   /* msn_talk_reach с поправкой для комнат */
float msn_talk_reach(const MsnRuntime *rt, const MsnCmd *c_talk);
/* 1 = free roam (no mission objective beyond "drive to the next story giver"): bonus givers, shops, gags active */
int  msn_free_roam(const MsnRuntime *rt);
void msn_tick(MsnRuntime *rt, float dt, const MsnWorld *w);
/* MsnWorld.vehicle_health = health of the player's current (or last used) car, 1.0 if intact / none */
void msn_notify(MsnRuntime *rt, int input, const char *name, int value);
void msn_retry(MsnRuntime *rt, int from_start);
/* сверка g07 (P06-20 / P07-23): 0 — AllowMissionAbort("false") (L1 m3sd, L2 m6sd, L6 m1sd): отказаться от начала миссии нельзя, кнопки Accept/Cancel не показывать */
int  msn_abort_allowed(const MsnRuntime *rt);
void msn_abort(MsnRuntime *rt);
/* №943 (Роб 24.10, «остаётся миссия "приехать домой"»): смена миссии выбором (MISSION SELECT, служебное) — цель, маркер и реестр прежней миссии снимаются
   (msn_abort делал так, выбор через CLEANUP — нет), потом msn_start_mission. msn_switch_clear — только снятие; msn_reg_mission_count — сколько записей реестра
   принадлежит миссии (после снятия 0). msn_start_first — старт «первой непройденной сюжетной» (то, что msn_level_start делал в конце), когда она была отложена */
void msn_switch_clear(MsnRuntime *rt);
int  msn_reg_mission_count(const MsnRuntime *rt);
void msn_start_first(MsnRuntime *rt);
/* where the objective arrow should point; 0 = none */
int  msn_nav_target(MsnRuntime *rt, const MsnWorld *w, MsnVec3 *out);
/* the locator behind the current target (NULL if the target is an entity). Its trigger volumes
 * (f->trigs[loc->first_trig ..]) are what actually completes the objective; the marker may stand
 * elsewhere (e.g. L1 m1 "drive to the school": marker at the school, triggers = lines leaving the block). */
const MsnLoc *msn_nav_locator(MsnRuntime *rt, const MsnWorld *w);
/* current objective summary (HUD / debug) */
typedef struct MsnObjInfo {
    int mission, phase, stage, obj, flags, nav;
    const char *mission_id, *text, *icon, *target, *arg, *dest;
    int items_total, items_done, lap, laps;
    float remaining; int timer_on, timer_up;
    int need_action, need_on_foot;
} MsnObjInfo;
void msn_objective_info(const MsnRuntime *rt, MsnObjInfo *o);

/* save / restore */
typedef struct MsnProgress { uint8_t version, level, story, phase, stage; uint8_t pad[3]; uint32_t completed; } MsnProgress;
void msn_get_progress(const MsnRuntime *rt, MsnProgress *p);
void msn_resume(MsnRuntime *rt, const MsnProgress *p);
void msn_resume_restart(MsnRuntime *rt, const MsnProgress *p);
extern int (*msnWagerPayFn)(int fee);   /* круг 2, P06-3: выплата ставки (NULL — вход × 2) */   /* P14-16: миссия с начала, не с чекпоинта записи */

extern const char *const msn_objective_name[MSN_OBJ_COUNT];
extern const char *const msn_cond_name[MSN_COND_COUNT];
extern const char *const msn_fail_name[];

#ifdef __cplusplus
}
#endif
#endif
