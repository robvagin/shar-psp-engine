/* Typed plug-in layer between the mission runtime and the game modules (see msn_game.h). Pure C99. */
#include <string.h>
#include <math.h>
#include "msn_game.h"
#include "briefcards_data.h"
#ifndef MSN_DLG_TEXT842_OLD
#define MSN_DLG_TEXT842_OLD 0
#endif
#ifndef MSN_TRAFFIC25_OLD
#define MSN_TRAFFIC25_OLD 0   /* сверка g07, P06-25: NoTrafficForStage — чистка 200 м и лимит 0 до следующего SetMaxTraffic; ClearTrafficForStage — чистка 200 м. 1 — прежнее */
#endif
#define MSN_TRAFFIC_CLEAR_M 200.0f

#define U (g->api.user)
#define CALL(grp, fn, ...) do { if (g->api.grp.fn) g->api.grp.fn(U, __VA_ARGS__); } while (0)
#define CALL0(grp, fn) do { if (g->api.grp.fn) g->api.grp.fn(U); } while (0)

const char *const msn_game_route_name[4] = { "UNSUPPORTED", "runtime", "typed", "pass-through" };

/* ------------------------------------------------------------------ classification */
static int is_typed(unsigned op)
{
    switch (op) {
    /* player */
    case MSN_OP_SETMISSIONRESETPLAYERINCAR: case MSN_OP_SETMISSIONRESETPLAYEROUTCAR: case MSN_OP_ADDSTAGECHARACTER:
    case MSN_OP_INITLEVELPLAYERVEHICLE: case MSN_OP_PLACEPLAYERCAR: case MSN_OP_PUTMFPLAYERINCAR: case MSN_OP_SETFORCEDCAR:
    case MSN_OP_SETSWAPPLAYERLOCATOR: case MSN_OP_SETSWAPDEFAULTCARLOCATOR: case MSN_OP_SETSWAPFORCEDCARLOCATOR:
    case MSN_OP_SWAPINDEFAULTCAR: case MSN_OP_SETINITIALWALK: case MSN_OP_ADDCHARACTER:
    /* cars */
    case MSN_OP_LOADDISPOSABLECAR: case MSN_OP_ADDSTAGEVEHICLE: case MSN_OP_ADDSTAGEWAYPOINT: case MSN_OP_ACTIVATEVEHICLE:
    case MSN_OP_SETVEHICLEAIPARAMS: case MSN_OP_SETSTAGEAIRACECATCHUPPARAMS: case MSN_OP_SETSTAGEAITARGETCATCHUPPARAMS:
    case MSN_OP_SUPPRESSDRIVER: case MSN_OP_ADDDRIVER: case MSN_OP_REMOVEDRIVER:
    /* characters */
    case MSN_OP_ADDNPC: case MSN_OP_ADDAMBIENTCHARACTER: case MSN_OP_ADDOBJECTIVENPCWAYPOINT: case MSN_OP_ADDAMBIENTNPCWAYPOINT:
    case MSN_OP_ADDBONUSMISSIONNPCWAYPOINT: case MSN_OP_ADDPURCHASECARNPCWAYPOINT: case MSN_OP_REMOVENPC:
    case MSN_OP_ADDNPCCHARACTERBONUSMISSION: case MSN_OP_ADDPURCHASECARREWARD:
    /* sound, fx, props, traffic, hud */
    case MSN_OP_STAGESTARTMUSICEVENT: case MSN_OP_ADDSTAGEMUSICCHANGE: case MSN_OP_SETSTAGEMUSICALWAYSON:
    case MSN_OP_SETFADEOUT: case MSN_OP_STAYINBLACK:
    case MSN_OP_STREETRACEPROPSLOAD: case MSN_OP_STREETRACEPROPSUNLOAD: case MSN_OP_ADDCOLLECTIBLESTATEPROP:
    case MSN_OP_SETMAXTRAFFIC: case MSN_OP_NOTRAFFICFORSTAGE: case MSN_OP_CLEARTRAFFICFORSTAGE:
    case MSN_OP_SETPRESENTATIONBITMAP:
    /* data read through msn_reward_*() */
    case MSN_OP_BINDREWARD:
        return 1;
#ifndef C3_CONTENT_MUSIC_OLD
    case MSN_OP_SETMUSICSTATE: return 1;
#endif
    default: return 0;
    }
}

/* features whose commands go to api.feature untyped (no module of the listed groups owns them) */
static int is_pass_feature(int feat)
{
    switch (feat) {
    case MSN_FEAT_CAMERA: case MSN_FEAT_DIALOGUE: case MSN_FEAT_COLLECT: case MSN_FEAT_GAGS: case MSN_FEAT_FX:
    case MSN_FEAT_LEVEL: case MSN_FEAT_LOAD: case MSN_FEAT_PEDS: case MSN_FEAT_POLICE: case MSN_FEAT_TRAFFIC:
    case MSN_FEAT_SHOP: case MSN_FEAT_INTERIORS: case MSN_FEAT_FMV: case MSN_FEAT_ENDGAME: case MSN_FEAT_DEMO:
    case MSN_FEAT_CAR_PHYS:
        return 1;
    default: return 0;
    }
}

int msn_game_route(unsigned op)
{
    if (op == 0 || op >= MSN_OP_COUNT || msn_opinfo[op].route == 0) return MSN_GR_NONE;
    if (!(msn_opinfo[op].route & MSN_ROUTE_ENG)) return MSN_GR_RUNTIME;
    if (is_typed(op)) return MSN_GR_TYPED;
    if (is_pass_feature(msn_opinfo[op].feature)) return MSN_GR_PASS;
    return MSN_GR_NONE;
}

/* ------------------------------------------------------------------ helpers */
static MsnPlace place_loc(const MsnLoc *l, const char *name)
{
    MsnPlace p; memset(&p, 0, sizeof p);
    p.locator = name ? name : ""; p.loc = l;
    if (l) {
        p.pos.x = l->pos[0]; p.pos.y = l->pos[1]; p.pos.z = l->pos[2];
        p.has_heading = (l->flags & 1) != 0; p.heading = p.has_heading ? l->heading : 0.0f;
    }
    return p;
}
static MsnPlace place_arg(const MsnFile *f, const MsnCmd *c, int i) { return place_loc(msn_arg_loc(f, c, i), msn_arg_s(f, c, i)); }
static int has_loc(const MsnFile *f, const MsnCmd *c, int i) { return msn_arg_loc(f, c, i) != NULL; }

static void giver_from_cmd(const MsnGame *g, const MsnCmd *c, MsnGiver *gv)
{
    const MsnFile *f = g->f;
    memset(gv, 0, sizeof *gv);
    gv->name = msn_arg_s(f, c, 0); gv->model = msn_arg_s(f, c, 1);
    if (c->op == MSN_OP_ADDPURCHASECARREWARD) {        /* (shop, seller, model, locator, radius, car locator) */
        gv->kind = MSN_GIVER_SELLER; gv->mission = msn_arg_s(f, c, 0); gv->mission_index = -1;
        gv->name = msn_arg_s(f, c, 1); gv->model = msn_arg_s(f, c, 2);
        gv->at = place_arg(f, c, 3); gv->radius = msn_arg_f(f, c, 4); gv->car_at = place_arg(f, c, 5);
        gv->icon = gv->icon_shadow = gv->conv = "";
        return;
    }
    /* (name, model, locator, mission, icon, conversation, flag, icon shadow) */
    gv->at = place_arg(f, c, 2); gv->mission = msn_arg_s(f, c, 3);
    gv->mission_index = msn_find_mission(f, gv->mission);
    gv->icon = msn_arg_s(f, c, 4); gv->conv = msn_arg_s(f, c, 5); gv->icon_shadow = msn_arg_s(f, c, 7);
    gv->kind = MSN_GIVER_BONUS;
    if (gv->mission_index >= 0) {
        uint16_t fl = f->missions[gv->mission_index].flags;
        if (fl & MSN_MF_STREETRACE) gv->kind = MSN_GIVER_STREETRACE;
        else if (fl & MSN_MF_GAMBLE) gv->kind = MSN_GIVER_WAGER;
    }
}

static int item_tag(const char *name)
{
    const char *p;
    int n = 0;
    if ((name[0] == 'P' || name[0] == 'p') && (name[1] == 'P' || name[1] == 'p') && name[2] == '_') name += 3;
    if (strlen(name) < 9) return 0;
    {   /* "powerbox" + digits (stateprops SpInst.tag) */
        static const char pb[] = "powerbox"; int i;
        for (i = 0; i < 8; i++) { int ch = (unsigned char)name[i]; if (ch >= 'A' && ch <= 'Z') ch += 32; if (ch != pb[i]) return 0; }
    }
    for (p = name + 8; *p >= '0' && *p <= '9'; p++) n = n * 10 + (*p - '0');
    return *p ? 0 : n;
}

static const char *stage_effect(const MsnGame *g)
{
    const MsnStage *st = g->rt->st; uint32_t i;
    if (!st) return "";
    for (i = 0; i < st->cmd_count; i++)
        if (g->f->cmds[st->cmd_first + i].op == MSN_OP_SETCOLLECTIBLEEFFECT) return msn_arg_s(g->f, &g->f->cmds[st->cmd_first + i], 0);
    return "";
}

static void make_item(const MsnGame *g, int i, MsnItem *it)
{
    const MsnRuntime *rt = g->rt; const MsnFile *f = g->f;
    memset(it, 0, sizeof *it);
    if (i < 0 || i >= rt->nitems) return;
    it->at = place_arg(f, rt->items[i], 0);
    if (rt->released[i]) it->at.pos = rt->relpos[i];
    it->dropped = rt->released[i];
    it->name = msn_arg_s(f, rt->items[i], 0); it->drawable = msn_arg_s(f, rt->items[i], 1);
    it->effect = stage_effect(g);
    it->breakable = rt->obj == MSN_OBJ_DELIVERY && msn_argc(rt->items[i]) == 1;
    it->tag = it->breakable ? item_tag(it->name) : 0;
}

static void make_dialogue(const MsnFile *f, const MsnConv *cv, const char *name, const char *pc, const char *npc, MsnDialogue *d)
{
    int i;
    memset(d, 0, sizeof *d);
    d->name = name ? name : ""; d->pc = pc ? pc : ""; d->npc = npc ? npc : "";
    if (!cv) return;
    for (i = 0; i < cv->count && i < MSN_MAX_LINES; i++) {
        d->file[i] = msn_str(f, f->clips[cv->first + i].file);
        d->actor[i] = msn_str(f, f->clips[cv->first + i].actor);
    }
    d->nlines = i;
}

static void flush(MsnGame *g)
{
    if (g->swap_mask & 8) {                           /* SwapInDefaultCar seen; its locators follow it in the script */
        CALL(player, swap_default_car, (g->swap_mask & 1) ? &g->swap_player : NULL, (g->swap_mask & 2) ? &g->swap_default : NULL,
             (g->swap_mask & 4) ? &g->swap_forced : NULL);
        g->swap_mask = 0;
    }
    if (g->hud.countdown && g->rt->status != MSN_COUNTDOWN) {
        g->hud.countdown = NULL;
        if (g->api.ui.countdown) g->api.ui.countdown(U, NULL, 0.0f);
    }
}

static void flash(MsnGame *g, int what, const char *key)
{
    g->hud.flash = what; g->hud.flash_text = msn_text(g->f, key); g->hud.flash_t = 0;
    CALL(ui, flash, what, g->hud.flash_text);
}

static void timer_hide(MsnGame *g)
{
    g->hud.timer_on = 0; g->hud.par_ms = -1;
    CALL(ui, timer, -1, 0);
    CALL(ui, par_time, -1);
}

/* ------------------------------------------------------------------ host: commands */
static void on_command(void *user, const MsnCall *c)
{
    MsnGame *g = (MsnGame *)user;
    const MsnFile *f = c->f; const MsnCmd *k = c->cmd;
    unsigned op = k->op;
    MsnPlace a, b;
    if (op < MSN_OP_COUNT) g->op_calls[op]++;
    switch (msn_game_route(op)) {
    case MSN_GR_TYPED: g->n_typed++; break;
    case MSN_GR_PASS:
        g->n_pass++;
        if (g->api.feature) g->api.feature(U, msn_opinfo[op].feature, c);
        return;
    default: g->n_none++;
        if (g->api.feature) g->api.feature(U, op < MSN_OP_COUNT ? msn_opinfo[op].feature : -1, c);
        return;
    }
    switch (op) {
    /* ---- player */
    case MSN_OP_SETMISSIONRESETPLAYERINCAR: b = place_arg(f, k, 0); CALL(player, reset, NULL, &b, "current"); break;
    case MSN_OP_SETMISSIONRESETPLAYEROUTCAR:
        a = place_arg(f, k, 0); b = place_arg(f, k, 1);
        CALL(player, reset, &a, has_loc(f, k, 1) ? &b : NULL, "current"); break;
    case MSN_OP_ADDSTAGECHARACTER:                      /* (character, locator, "", car, car locator) */
        a = place_arg(f, k, 1); b = place_arg(f, k, 4);
        /* №ночь 11.10: локатор персонажа пустой («» — L4 m2 этап 0 Мардж, L6 m5 этап 0 Барт) значит «игрок в машине у локатора машины»; раньше at = (0,0,0) и Гомер
           ставился в начало мира и падал (миссионер ночи: L6 m5 «время вышло», Гомер на x 0 z 0 при y −2…−60). MSN_STAGECHAR_OLD=1 — как раньше */
#ifndef MSN_STAGECHAR_OLD
        CALL(player, reset, has_loc(f, k, 1) ? &a : NULL, has_loc(f, k, 4) ? &b : NULL, *msn_arg_s(f, k, 3) ? msn_arg_s(f, k, 3) : "current"); break;
#else
        CALL(player, reset, &a, has_loc(f, k, 4) ? &b : NULL, *msn_arg_s(f, k, 3) ? msn_arg_s(f, k, 3) : "current"); break;
#endif
    case MSN_OP_INITLEVELPLAYERVEHICLE:
        a = place_arg(f, k, 1); CALL(player, vehicle, msn_arg_s(f, k, 0), msn_streq(msn_arg_s(f, k, 2), "OTHER"), &a); break;
    case MSN_OP_PLACEPLAYERCAR: a = place_arg(f, k, 1); CALL(player, place_car, msn_arg_s(f, k, 0), &a); break;
    case MSN_OP_PUTMFPLAYERINCAR: CALL0(player, enter_car); break;
    case MSN_OP_SETFORCEDCAR: CALL(player, forced_car, 1); break;
    case MSN_OP_SETSWAPPLAYERLOCATOR: g->swap_player = place_arg(f, k, 0); g->swap_mask |= 1; break;
    case MSN_OP_SETSWAPDEFAULTCARLOCATOR: g->swap_default = place_arg(f, k, 0); g->swap_mask |= 2; break;
    case MSN_OP_SETSWAPFORCEDCARLOCATOR: g->swap_forced = place_arg(f, k, 0); g->swap_mask |= 4; break;
    case MSN_OP_SWAPINDEFAULTCAR: g->swap_mask |= 8; break;          /* fired by flush() after the locators */
    case MSN_OP_SETINITIALWALK: a = place_arg(f, k, 0); CALL(player, walk_to, &a); break;
    case MSN_OP_ADDCHARACTER: CALL(player, character, msn_arg_s(f, k, 0), msn_arg_s(f, k, 1)); break;
    /* ---- cars */
    case MSN_OP_LOADDISPOSABLECAR: CALL(cars, load, msn_arg_s(f, k, 1), msn_arg_s(f, k, 0), msn_arg_s(f, k, 2)); break;
    case MSN_OP_ADDSTAGEVEHICLE: {
        MsnCarSpawn s;
        s.name = msn_arg_s(f, k, 0); s.at = place_arg(f, k, 1); s.ai = msn_arg_s(f, k, 2);
        s.con = msn_arg_s(f, k, 3); s.driver = msn_arg_s(f, k, 4);
        CALL(cars, spawn, &s); break; }
    case MSN_OP_ADDSTAGEWAYPOINT: a = place_arg(f, k, 0); CALL(cars, waypoint, g->wp_index, &a); g->wp_index++; break;
    case MSN_OP_ACTIVATEVEHICLE:
        a = place_arg(f, k, 1); CALL(cars, activate, msn_arg_s(f, k, 0), has_loc(f, k, 1) ? &a : NULL, msn_arg_s(f, k, 2)); break;
    case MSN_OP_SETVEHICLEAIPARAMS: CALL(cars, ai_params, msn_arg_s(f, k, 0), msn_arg_i(f, k, 1), msn_arg_i(f, k, 2)); break;
    case MSN_OP_SETSTAGEAIRACECATCHUPPARAMS: case MSN_OP_SETSTAGEAITARGETCATCHUPPARAMS: {
        float p[6]; int i, n = msn_argc(k) - 1;
        if (n > 6) n = 6;
        for (i = 0; i < n; i++) p[i] = msn_arg_f(f, k, i + 1);
        CALL(cars, catchup, msn_arg_s(f, k, 0), op == MSN_OP_SETSTAGEAITARGETCATCHUPPARAMS, p, n < 0 ? 0 : n); break; }
    case MSN_OP_SUPPRESSDRIVER: CALL(cars, suppress_driver, msn_arg_s(f, k, 0)); break;
    case MSN_OP_ADDDRIVER: CALL(npc, driver, msn_arg_s(f, k, 1), msn_arg_s(f, k, 0), 1); break;
    case MSN_OP_REMOVEDRIVER: CALL(npc, driver, "", msn_arg_s(f, k, 0), 0); break;
    /* ---- characters */
    case MSN_OP_ADDNPC: a = place_arg(f, k, 1); CALL(npc, spawn, msn_arg_s(f, k, 0), &a, MSN_NPC_MISSION, 0.0f, msn_arg_s(f, k, 2)); break;
    case MSN_OP_ADDAMBIENTCHARACTER: a = place_arg(f, k, 1); CALL(npc, spawn, msn_arg_s(f, k, 0), &a, MSN_NPC_AMBIENT, msn_arg_f(f, k, 2), ""); break;
    case MSN_OP_ADDOBJECTIVENPCWAYPOINT: case MSN_OP_ADDAMBIENTNPCWAYPOINT: case MSN_OP_ADDBONUSMISSIONNPCWAYPOINT:
    case MSN_OP_ADDPURCHASECARNPCWAYPOINT:
        a = place_arg(f, k, 1); CALL(npc, waypoint, msn_arg_s(f, k, 0), &a); break;
    case MSN_OP_REMOVENPC: CALL(npc, remove, msn_arg_s(f, k, 0)); break;
    case MSN_OP_ADDNPCCHARACTERBONUSMISSION: case MSN_OP_ADDPURCHASECARREWARD: {
        MsnGiver gv; giver_from_cmd(g, k, &gv);
        CALL(npc, giver, &gv);
        if (gv.kind == MSN_GIVER_SELLER) CALL(nav, giver_icon, &gv, 1);   /* bonus icons: MSN_EV_BONUS_AVAILABLE */
        break; }
    /* ---- sound / fx */
    case MSN_OP_STAGESTARTMUSICEVENT: CALL(sound, music_event, msn_arg_s(f, k, 0)); break;
    case MSN_OP_ADDSTAGEMUSICCHANGE: CALL0(sound, music_change); break;
    case MSN_OP_SETSTAGEMUSICALWAYSON: CALL0(sound, music_always_on); break;
#ifndef C3_CONTENT_MUSIC_OLD
    case MSN_OP_SETMUSICSTATE: CALL(sound, music_state, msn_arg_s(f, k, 0), msn_arg_s(f, k, 1)); break;
#endif
    case MSN_OP_SETFADEOUT: CALL(fx, fade, 1, msn_arg_f(f, k, 0)); break;
    case MSN_OP_STAYINBLACK: CALL0(fx, stay_black); break;
    /* ---- props / traffic / hud */
    case MSN_OP_STREETRACEPROPSLOAD: CALL(props, race_props, msn_arg_s(f, k, 0), 1); break;
    case MSN_OP_STREETRACEPROPSUNLOAD: CALL(props, race_props, msn_arg_s(f, k, 0), 0); break;
    case MSN_OP_ADDCOLLECTIBLESTATEPROP: a = place_arg(f, k, 1); CALL(props, state_prop, msn_arg_s(f, k, 0), &a); break;
    case MSN_OP_SETMAXTRAFFIC: g->traffic_zero = 0; CALL(traffic, max_cars, msn_arg_i(f, k, 0)); break;
    case MSN_OP_NOTRAFFICFORSTAGE:
        if (!MSN_TRAFFIC25_OLD) { g->traffic_zero = 1; CALL(traffic, clear, MSN_TRAFFIC_CLEAR_M); }
        CALL(traffic, max_cars, 0); break;
    case MSN_OP_CLEARTRAFFICFORSTAGE: if (!MSN_TRAFFIC25_OLD) CALL(traffic, clear, MSN_TRAFFIC_CLEAR_M); break;
    case MSN_OP_SETPRESENTATIONBITMAP: CALL(ui, presentation, msn_arg_s(f, k, 0)); break;
    case MSN_OP_BINDREWARD: break;                      /* read with msn_reward_get() by the shop */
    default: break;
    }
}

/* ------------------------------------------------------------------ host: events */
static int stage_has_op(const MsnGame *g, unsigned op1, unsigned op2)
{
    const MsnStage *st = g->rt->st; uint32_t i;
    for (i = 0; st && i < st->cmd_count; i++) {
        unsigned op = g->f->cmds[st->cmd_first + i].op;
        if (op == op1 || op == op2) return 1;
    }
    return 0;
}

static void send_marker(MsnGame *g, MsnVec3 p, const MsnLoc *l, const char *drawable)
{
    MsnRuntime *rt = g->rt; MsnMarker m;
    memset(&m, 0, sizeof m);
    m.pos = p; m.loc = l; m.drawable = drawable ? drawable : ""; m.nav = rt->st ? rt->st->nav : 0;
    m.mission_start = rt->cur >= 0 && rt->cur == rt->story && rt->phase == 0 && rt->stage == 0;
    switch (rt->obj) {
    case MSN_OBJ_TALKTO: case MSN_OBJ_FOLLOW: case MSN_OBJ_LOSETAIL: case MSN_OBJ_DESTROY: case MSN_OBJ_DESTROYBOSS:
    case MSN_OBJ_GETIN: m.target = rt->target; break;
    default: break;
    }
    g->marker_on = 1; g->marker_pos = p; g->marker_loc = l;
    CALL(nav, marker, &m);
}

#ifndef BRIEFCARDS_423_OLD
/* №512: имя картинки из SetPresentationBitmap миссии (первое по порядку фаз: вступление, основная), NULL — нет. Картинка брифинга берётся по данным миссии, а не
   по тому, выполнилось ли вступление: после «Retry», продолжения и входа сразу в основную фазу вступление не идёт, и карточка оставалась без картинки */
static const char *scriptedBitmapName(const MsnFile *f,const MsnMission *m) {
    unsigned phases[2]={m->intro,m->main};
    for(int k=0;k<2;k++) {
        if(phases[k]==MSN_NONE16)continue;
        const MsnPhase *p=&f->phases[phases[k]];
        for(unsigned j=0;j<p->setup_count;j++) if(f->cmds[p->setup_first+j].op==MSN_OP_SETPRESENTATIONBITMAP)return msn_arg_s(f,&f->cmds[p->setup_first+j],0);
        for(unsigned j=0;j<p->load_count;j++) if(f->cmds[p->load_first+j].op==MSN_OP_SETPRESENTATIONBITMAP)return msn_arg_s(f,&f->cmds[p->load_first+j],0);
        for(unsigned j=0;j<p->stage_count;j++) {
            const MsnStage *s=&f->stages[p->first_stage+j];
            for(unsigned i=0;i<s->cmd_count;i++) if(f->cmds[s->cmd_first+i].op==MSN_OP_SETPRESENTATIONBITMAP)return msn_arg_s(f,&f->cmds[s->cmd_first+i],0);
        }
    }
    return NULL;
}
static int hasScriptedBitmap(const MsnFile *f,const MsnMission *m) { return scriptedBitmapName(f,m)!=NULL; }
#endif

#ifndef MSN_SWITCH_OLD
#define MSN_SWITCH_OLD 0   /* №943: 1 — прежнее (смена миссии без снятия маркера и реестра, цель HUD живёт до следующего этапа); служебно flag msnswitchold 1 */
#endif
int msnSwitchOld = MSN_SWITCH_OLD;

static void on_event(void *user, const MsnEvent *e)
{
    MsnGame *g = (MsnGame *)user;
    MsnRuntime *rt = g->rt; const MsnFile *f = g->f;
    g->n_events++;
    flush(g);
    switch (e->type) {
    case MSN_EV_MISSION_START:
        g->hud.mission_id = e->name; g->hud.title = e->text; g->hud.failed = 0;
        break;
    case MSN_EV_PHASE_START:
        if (e->index == 1 && rt->cur >= 0) {
            const MsnMission *m = &f->missions[rt->cur];
#ifndef BRIEFCARDS_423_OLD
            /* 423: SLES_518.97 maps b_<bonus giver> to misXX/misNN_08. Scripted art has priority. */
            if(m->bonus_npc!=0xFFFFFFFFu && !hasScriptedBitmap(f,m)) {
                const char *bitmap=msnBriefBuiltin(msn_arg_s(f,&f->cmds[m->bonus_npc],0));
                if(bitmap)CALL(ui,presentation,bitmap);
            }
#ifndef BRIEFPIC_512_OLD
            else if(hasScriptedBitmap(f,m)) { const char *bitmap=scriptedBitmapName(f,m); if(bitmap)CALL(ui,presentation,bitmap); }   /* №512: повтор безвреден (msnartLoad не дублирует) */
#endif
#endif
            CALL(ui, mission_title, msn_str(f, m->id), msn_str(f, m->title), msn_str(f, m->info), (m->flags & MSN_MF_BONUS) != 0);
        }
        break;
    case MSN_EV_STAGE_START: {
        const MsnStage *st = rt->st;
        g->wp_index = 0; g->wait_dialogue = g->wait_fmv = 0; g->wait_t = 0;
        /* №842 (Роб 19.10, L3: «у Лизы задание ехать домой, в дом на другой карте»): у шага-разговора (dialogue) строка задания — остаток данных
           (L3 M5 sd, шаг 3: MISSION_OBJECTIVE_03 «GO TO THE FLANDERS HOUSE AND TALK TO NED»); оригинал её не показывает — идёт разговор.
           Порт: у шага-разговора строки задания нет. MSN_DLG_TEXT842_OLD=1 — как было */
        int dlg_ = !MSN_DLG_TEXT842_OLD && st->obj == MSN_OBJ_DIALOGUE;
        g->hud.objective = dlg_ ? "" : e->text; g->hud.icon = dlg_ ? "" : e->name; g->hud.objective_msg = dlg_ || st->msg == MSN_NONE16 ? -1 : st->msg;
        g->hud.items_done = g->hud.items_total = g->hud.lap = g->hud.laps = 0;
        CALL(ui, objective, dlg_ ? "" : e->text, g->hud.objective_msg, dlg_ ? "" : e->name);
        CALL(ui, counter, 0, 0);
        CALL(ui, lap, 0, 0);
        if (rt->timer_on) {
            g->hud.timer_on = 1; g->hud.timer_up = rt->timer_up; g->hud.timer_ms = (int)(rt->remaining * 1000.0f);
            CALL(ui, timer, g->hud.timer_ms, rt->timer_up);
        } else timer_hide(g);
        if (rt->par > 0) { g->hud.par_ms = (int)(rt->par * 1000.0f); CALL(ui, par_time, g->hud.par_ms); }
        if (rt->obj == MSN_OBJ_TALKTO && rt->c_talk)
            CALL(npc, talk_target, msn_arg_s(f, rt->c_talk, 0), msn_arg_i(f, rt->c_talk, 1), msn_arg_f(f, rt->c_talk, 2), msn_arg_f(f, rt->c_talk, 3));
        if (!stage_has_op(g, MSN_OP_SETMAXTRAFFIC, MSN_OP_NOTRAFFICFORSTAGE) && (MSN_TRAFFIC25_OLD || !g->traffic_zero)) CALL(traffic, max_cars, -1);   /* P06-25: лимит 0 держится до SetMaxTraffic */
        break; }
    case MSN_EV_MARKER: send_marker(g, e->pos, e->loc, e->name); break;
    case MSN_EV_MARKER_CLEAR:
        if (g->marker_on) { g->marker_on = 0; CALL(nav, marker, NULL); }
        break;
    case MSN_EV_TIMER:
        g->hud.timer_on = 1; g->hud.timer_up = e->index; g->hud.timer_ms = (int)(e->value * 1000.0f);
        CALL(ui, timer, g->hud.timer_ms, e->index);
        break;
    case MSN_EV_COUNTDOWN:
        g->hud.countdown = msn_text(f, e->text);
        CALL(ui, countdown, g->hud.countdown, e->value);
        break;
    case MSN_EV_DIALOGUE: {
        MsnDialogue d;
        make_dialogue(f, e->conv, e->text, e->name, e->name2, &d);
        if (g->api.sound.dialogue) g->api.sound.dialogue(U, &d);
        else { g->wait_dialogue = 1; g->wait_t = 0; }
        break; }
    case MSN_EV_FMV:
        if (g->api.fx.fmv) g->api.fx.fmv(U, e->name);
        else { g->wait_fmv = 1; g->wait_t = 0; }
        break;
    case MSN_EV_ITEMS:
        if (rt->obj == MSN_OBJ_RACE) {
            g->hud.lap = rt->lap + 1 > rt->laps ? rt->laps : rt->lap + 1; g->hud.laps = rt->laps;
            if (rt->laps > 1) CALL(ui, lap, g->hud.lap, rt->laps);
        }
        g->hud.items_done = e->index; g->hud.items_total = (int)e->value;
        CALL(ui, counter, e->index, (int)e->value);
        break;
    case MSN_EV_ITEM_SHOW: case MSN_EV_DROP: {
        MsnItem it; make_item(g, e->index, &it);
        CALL(props, item_show, e->index, &it);
        if (e->type == MSN_EV_ITEM_SHOW && it.breakable && it.tag) CALL(props, activate_tag, it.tag);
        break; }
    case MSN_EV_COLLECT: {
        MsnItem it; make_item(g, e->index, &it);
        CALL(props, item_collected, e->index, &it);
        if (e->name2 && *e->name2 && g->api.sound.oneliner) {
            MsnDialogue d;
            make_dialogue(f, msn_find_conv(f, e->name2, rt->cur >= 0 ? msn_str(f, f->missions[rt->cur].id) : NULL), e->name2, "", "", &d);
            g->api.sound.oneliner(U, &d);
        }
        break; }
    case MSN_EV_PAY: CALL(shop, coins_add, -(int)e->value, MSN_COINS_WAGER_FEE); break;
    case MSN_EV_STAGE_COMPLETE:
        CALL0(props, items_clear);
        if (e->index) flash(g, MSN_FLASH_STAGE_COMPLETE, "STAGE_COMPLETE");
        if (e->text && *e->text && g->api.sound.oneliner) {
            MsnDialogue d; make_dialogue(f, e->conv, e->text, "", e->name2, &d);
            g->api.sound.oneliner(U, &d);
        }
        break;
    case MSN_EV_MISSION_COMPLETE: {
        const MsnMission *m = rt->cur >= 0 ? &f->missions[rt->cur] : NULL;
        int bonus = m && (m->flags & MSN_MF_BONUS);
        MsnReward r;
        timer_hide(g);
        CALL(ui, counter, 0, 0);
        flash(g, bonus ? MSN_FLASH_BONUS_COMPLETE : MSN_FLASH_MISSION_COMPLETE, bonus ? "BONUS_MISSION_COMPLETE" : "MISSION_COMPLETE");
        if ((e->index == 1 && msn_reward_find(f, "bonusmission", &r) >= 0) || (e->index == 2 && msn_reward_find(f, "streetrace", &r) >= 0))
            CALL(shop, unlock, &r);
        if (e->index == 3 && e->value > 0) CALL(shop, coins_add, (int)e->value, MSN_COINS_WAGER_WIN);
        if (bonus && m->bonus_npc != 0xFFFFFFFFu) {
            MsnGiver gv; giver_from_cmd(g, &f->cmds[m->bonus_npc], &gv);
            CALL(nav, giver_icon, &gv, rt->dev_bonus_replay);
        }
        break; }
    case MSN_EV_MISSION_FAILED:
        g->hud.failed = 1; g->hud.fail_text = e->text; g->hud.fail_hint = e->name2;
        timer_hide(g);
        CALL(ui, failed, e->index, e->text, e->name2);
        break;
    case MSN_EV_CLEANUP:
        CALL0(props, items_clear);
        if (g->marker_on) { g->marker_on = 0; CALL(nav, marker, NULL); }
        /* №943: цель прежней миссии (текст, значок, счётчики) не живёт до следующего этапа — отсюда её брал ✕ Continue брифинга и служебные чтения */
        if (!msnSwitchOld) { g->hud.objective = g->hud.icon = ""; g->hud.objective_msg = -1; g->hud.items_done = g->hud.items_total = g->hud.lap = g->hud.laps = 0; }
        timer_hide(g);
        CALL(ui, counter, 0, 0);
        g->traffic_zero = 0;
        g->swap_mask = 0; g->wait_dialogue = g->wait_fmv = 0;
        if (g->api.cleanup) g->api.cleanup(U);
        break;
    case MSN_EV_LEVEL_COMPLETE:
        flash(g, e->index ? MSN_FLASH_GAME_COMPLETE : MSN_FLASH_LEVEL_COMPLETE, e->index ? "GAME_COMPLETE" : "LEVEL_COMPLETE");
        break;
    case MSN_EV_BONUS_AVAILABLE:
        if (e->index >= 0 && (uint32_t)e->index < f->nmissions && f->missions[e->index].bonus_npc != 0xFFFFFFFFu &&
            (!(rt->completed & (1u << e->index)) || rt->dev_bonus_replay || !(msn_arg_i(f, &f->cmds[f->missions[e->index].bonus_npc], 6) == 1))) {   /* круг 2, P06-2: повторяемый (7-й аргумент не 1) остаётся со значком */
            MsnGiver gv; giver_from_cmd(g, &f->cmds[f->missions[e->index].bonus_npc], &gv);
            CALL(nav, giver_icon, &gv, 1);
        }
        break;
    default: break;
    }
    if (g->api.on_event) g->api.on_event(U, e);
}

static int on_entity(void *user, const char *name, MsnEntity *out)
{
    MsnGame *g = (MsnGame *)user;
    return g->api.entity ? g->api.entity(U, name, out) : 0;
}

static int on_bonus_entity(void *user, int mission, const char *name, MsnEntity *out)
{
    MsnGame *g = (MsnGame *)user;
    return g->api.bonus_entity ? g->api.bonus_entity(U, mission, name, out) : 0;
}

static int on_owns(void *user, int kind, const char *name)
{
    MsnGame *g = (MsnGame *)user;
    return g->api.shop.owns ? g->api.shop.owns(U, kind, name) : 0;
}

static const char *on_cur_skin(void *user)
{
    MsnGame *g = (MsnGame *)user;
    return g->api.shop.cur_skin ? g->api.shop.cur_skin(U) : NULL;
}

static int on_skin_spot(void *user, MsnVec3 *out)
{
    MsnGame *g = (MsnGame *)user;
    return g->api.shop.skin_spot ? g->api.shop.skin_spot(U, out) : 0;
}

/* СВЕРКА_КАМЕРА 7.5: точка в кадре с дальностью 250 м (range <= 0 — MSN_VIEW_RANGE) */
int msn_point_in_view(const float eye[3], const float dir[3], float fov_cos, const float p[3], float range)
{
    float dx = p[0] - eye[0], dy = p[1] - eye[1], dz = p[2] - eye[2], d2 = dx * dx + dy * dy + dz * dz, d, dot;
    if (range <= 0.0f) range = MSN_VIEW_RANGE;
    if (d2 > range * range) return 0;
    if (d2 < 1e-6f) return 1;
    d = sqrtf(d2); dot = (dx * dir[0] + dy * dir[1] + dz * dir[2]) / d;
    return dot >= fov_cos;
}

/* ------------------------------------------------------------------ public */
void msn_game_init(MsnGame *g, MsnRuntime *rt, const MsnFile *f, const MsnGameApi *api)
{
    MsnHost h;
    memset(g, 0, sizeof *g);
    if (api) g->api = *api;
    g->rt = rt; g->f = f;
    g->auto_dialogue_s = 1.0f; g->auto_fmv_s = 1.0f;
    g->hud.objective = g->hud.icon = ""; g->hud.objective_msg = -1; g->hud.par_ms = -1; g->hud.talk_prompt = -1;
    h.user = g; h.command = on_command; h.event = on_event; h.entity = on_entity; h.owns = on_owns; h.bonus_entity = on_bonus_entity; h.skin_spot = on_skin_spot; h.cur_skin = on_cur_skin;
    msn_init(rt, f, &h);
}

void msn_game_tick(MsnGame *g, float dt, const MsnWorld *w)
{
    MsnRuntime *rt = g->rt;
    flush(g);
    msn_tick(rt, dt, w);
    flush(g);
    g->hud.flash_t += dt;
    if (rt->status == MSN_RUNNING) {
        MsnVec3 p; const MsnLoc *l = msn_nav_locator(rt, w);
        if (msn_nav_target(rt, w, &p)) {                 /* moving targets, next race checkpoint, nearest item */
            float dx = p.x - g->marker_pos.x, dy = p.y - g->marker_pos.y, dz = p.z - g->marker_pos.z;
            if (!g->marker_on || l != g->marker_loc || dx * dx + dy * dy + dz * dz > 0.25f)
                send_marker(g, p, l, rt->c_dest ? msn_arg_s(g->f, rt->c_dest, 1)
                                   : rt->obj == MSN_OBJ_RACE && rt->next_item < rt->nitems ? msn_arg_s(g->f, rt->items[rt->next_item], 1) : "");
        }
        if ((g->wait_dialogue && rt->obj == MSN_OBJ_DIALOGUE) || (g->wait_fmv && rt->obj == MSN_OBJ_FMV)) {
            g->wait_t += dt;
            if (g->wait_t >= (g->wait_dialogue ? g->auto_dialogue_s : g->auto_fmv_s)) {
                msn_notify(rt, g->wait_dialogue ? MSN_IN_DIALOGUE_DONE : MSN_IN_FMV_DONE, NULL, 0);
                g->wait_dialogue = g->wait_fmv = 0;
            }
        }
    }
    g->hud.talk_prompt = msn_bonus_near(rt, w);
}
