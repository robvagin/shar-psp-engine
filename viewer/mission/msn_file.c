/* MSN1 v2 pack loader / validator and accessors (pure C99). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "msn.h"

#define FAIL(...) do { if (err && errlen > 0) snprintf(err, (size_t)errlen, __VA_ARGS__); return -1; } while (0)

static const char *EMPTY = "";

int msn_open(MsnFile *f, const void *data, size_t size, char *err, int errlen)
{
    static const uint32_t recsize[MSN_SEC_COUNT] = { 4, 1, sizeof(MsnLoc), sizeof(MsnTrig), sizeof(MsnArg), sizeof(MsnCmd),
        sizeof(MsnMission), sizeof(MsnPhase), sizeof(MsnStage), sizeof(MsnCond), sizeof(MsnConv), sizeof(MsnClip),
        sizeof(MsnLevelInfo), 4, sizeof(MsnText), 1 };
    const MsnHeader *h = (const MsnHeader *)data;
    uint32_t i;
    memset(f, 0, sizeof *f);
    if (sizeof(MsnLoc) != 40 || sizeof(MsnTrig) != 64 || sizeof(MsnArg) != 8 || sizeof(MsnCmd) != 12 ||
        sizeof(MsnMission) != 32 || sizeof(MsnPhase) != 32 || sizeof(MsnStage) != 40 || sizeof(MsnCond) != 12 ||
        sizeof(MsnConv) != 12 || sizeof(MsnClip) != 12 || sizeof(MsnHeader) != 144 || sizeof(MsnText) != 8)
        FAIL("struct layout mismatch (compiler padding)");
    if (!data || size < sizeof(MsnHeader)) FAIL("file too small");
    if (((uintptr_t)data & 3) != 0) FAIL("buffer not 4-byte aligned");
    if (memcmp(h->magic, MSN_MAGIC, 4) != 0) FAIL("bad magic");
    if (h->version != MSN_VERSION) FAIL("version %u, runtime expects %u", h->version, MSN_VERSION);
    if (h->size != size) FAIL("size %u != file %u", (unsigned)h->size, (unsigned)size);
    for (i = 0; i < MSN_SEC_COUNT; i++) {
        uint64_t end = (uint64_t)h->sec[i].off + (uint64_t)h->sec[i].count * recsize[i];
        if (h->sec[i].off < sizeof(MsnHeader) || end > size || (h->sec[i].off & 3)) FAIL("section %u out of bounds", (unsigned)i);
    }
    f->base = (const uint8_t *)data; f->size = (uint32_t)size; f->hdr = h;
#define SEC(T, s) ((const T *)(f->base + h->sec[s].off))
    f->strofs = SEC(uint32_t, MSN_SEC_STROFS);   f->nstr = h->sec[MSN_SEC_STROFS].count;
    f->strblob = SEC(char, MSN_SEC_STRBLOB);     f->strbytes = h->sec[MSN_SEC_STRBLOB].count;
    f->locs = SEC(MsnLoc, MSN_SEC_LOCS);         f->nlocs = h->sec[MSN_SEC_LOCS].count;
    f->trigs = SEC(MsnTrig, MSN_SEC_TRIGS);      f->ntrigs = h->sec[MSN_SEC_TRIGS].count;
    f->args = SEC(MsnArg, MSN_SEC_ARGS);         f->nargs = h->sec[MSN_SEC_ARGS].count;
    f->cmds = SEC(MsnCmd, MSN_SEC_CMDS);         f->ncmds = h->sec[MSN_SEC_CMDS].count;
    f->missions = SEC(MsnMission, MSN_SEC_MISSIONS); f->nmissions = h->sec[MSN_SEC_MISSIONS].count;
    f->phases = SEC(MsnPhase, MSN_SEC_PHASES);   f->nphases = h->sec[MSN_SEC_PHASES].count;
    f->stages = SEC(MsnStage, MSN_SEC_STAGES);   f->nstages = h->sec[MSN_SEC_STAGES].count;
    f->conds = SEC(MsnCond, MSN_SEC_CONDS);      f->nconds = h->sec[MSN_SEC_CONDS].count;
    f->convs = SEC(MsnConv, MSN_SEC_CONVS);      f->nconvs = h->sec[MSN_SEC_CONVS].count;
    f->clips = SEC(MsnClip, MSN_SEC_CLIPS);      f->nclips = h->sec[MSN_SEC_CLIPS].count;
    f->level = SEC(MsnLevelInfo, MSN_SEC_LEVEL);
    f->files = SEC(uint32_t, MSN_SEC_FILES);     f->nfiles = h->sec[MSN_SEC_FILES].count;
    f->texts = SEC(MsnText, MSN_SEC_TEXTS);      f->ntexts = h->sec[MSN_SEC_TEXTS].count;
#undef SEC
    if (h->sec[MSN_SEC_LEVEL].count != 1) FAIL("level info missing");
    if (f->nstr == 0 || f->strbytes == 0 || f->strblob[f->strbytes - 1] != 0) FAIL("string blob not terminated");
    for (i = 0; i < f->nstr; i++) if (f->strofs[i] >= f->strbytes) FAIL("string %u offset", (unsigned)i);
#define STR_OK(s) ((s) < f->nstr)
    for (i = 0; i < f->nlocs; i++) {
        const MsnLoc *l = &f->locs[i];
        if (!STR_OK(l->name) || !STR_OK(l->file) || (uint32_t)l->first_trig + l->ntrig > f->ntrigs) FAIL("locator %u", (unsigned)i);
        if (l->type == MSN_LT_INTERIOR && !STR_OK(l->data)) FAIL("locator %u interior", (unsigned)i);
    }
    for (i = 0; i < f->nargs; i++) {
        const MsnArg *a = &f->args[i];
        if (a->type > MSN_ARG_BARE) FAIL("arg %u type", (unsigned)i);
        if ((a->type == MSN_ARG_STR || a->type == MSN_ARG_BARE) && !STR_OK(a->v.s)) FAIL("arg %u string", (unsigned)i);
        if (a->loc > f->nlocs) FAIL("arg %u locator", (unsigned)i);
    }
    for (i = 0; i < f->ncmds; i++) {
        const MsnCmd *c = &f->cmds[i];
        if (c->op == 0 || c->op >= MSN_OP_COUNT) FAIL("cmd %u opcode %u", (unsigned)i, (unsigned)c->op);
        if ((uint64_t)c->arg0 + c->argc > f->nargs || c->file >= f->nfiles) FAIL("cmd %u args", (unsigned)i);
    }
    for (i = 0; i < f->nfiles; i++) if (!STR_OK(f->files[i])) FAIL("file %u", (unsigned)i);
    for (i = 0; i < f->ntexts; i++) if (!STR_OK(f->texts[i].key) || !STR_OK(f->texts[i].value)) FAIL("text %u", (unsigned)i);
    for (i = 0; i < f->nstages; i++) {
        const MsnStage *s = &f->stages[i];
        if ((uint64_t)s->cmd_first + s->cmd_count > f->ncmds || (uint32_t)s->cond_first + s->ncond > f->nconds ||
            !STR_OK(s->text) || !STR_OK(s->icon) || !STR_OK(s->lock_name) || !STR_OK(s->obj_arg) || s->obj >= MSN_OBJ_COUNT ||
            (s->obj_cmd != MSN_NONE16 && s->obj_cmd >= s->cmd_count)) FAIL("stage %u", (unsigned)i);
    }
    for (i = 0; i < f->nconds; i++) {
        const MsnCond *c = &f->conds[i];
        if ((uint64_t)c->cmd_first + c->cmd_count > f->ncmds || c->type >= MSN_COND_COUNT) FAIL("cond %u", (unsigned)i);
    }
    for (i = 0; i < f->nphases; i++) {
        const MsnPhase *p = &f->phases[i];
        if (!STR_OK(p->name) || !STR_OK(p->file) || (uint64_t)p->setup_first + p->setup_count > f->ncmds ||
            (uint64_t)p->load_first + p->load_count > f->ncmds || (uint32_t)p->first_stage + p->stage_count > f->nstages)
            FAIL("phase %u", (unsigned)i);
    }
    for (i = 0; i < f->nmissions; i++) {
        const MsnMission *m = &f->missions[i];
        if (!STR_OK(m->id) || !STR_OK(m->title) || !STR_OK(m->info) ||
            (m->intro != MSN_NONE16 && m->intro >= f->nphases) || (m->main != MSN_NONE16 && m->main >= f->nphases) ||
            (m->bonus_npc != 0xFFFFFFFFu && m->bonus_npc >= f->ncmds)) FAIL("mission %u", (unsigned)i);
    }
    if (f->nmissions > 32) FAIL("more than 32 missions in a level (completed bitmask)");
    for (i = 0; i < f->nconvs; i++) {
        const MsnConv *c = &f->convs[i];
        if (!STR_OK(c->name) || !STR_OK(c->mission) || (uint32_t)c->first + c->count > f->nclips) FAIL("conv %u", (unsigned)i);
    }
    for (i = 0; i < f->nclips; i++) if (!STR_OK(f->clips[i].file) || !STR_OK(f->clips[i].actor)) FAIL("clip %u", (unsigned)i);
    {
        const MsnLevelInfo *L = f->level;
        if ((uint64_t)L->lvl_first + L->lvl_count > f->ncmds || (uint64_t)L->init_first + L->init_count > f->ncmds ||
            (uint64_t)L->rew_first + L->rew_count > f->ncmds) FAIL("level ranges");
    }
#undef STR_OK
    return 0;
}

const char *msn_str(const MsnFile *f, uint32_t s)
{
    return s < f->nstr ? f->strblob + f->strofs[s] : EMPTY;
}

const char *msn_op_name(unsigned op)
{
    return op < MSN_OP_COUNT ? msn_opinfo[op].name : "?";
}

int msn_argc(const MsnCmd *c) { return c ? c->argc : 0; }

const MsnArg *msn_arg(const MsnFile *f, const MsnCmd *c, int i)
{
    if (!c || i < 0 || i >= c->argc) return NULL;
    return &f->args[c->arg0 + (uint32_t)i];
}

const char *msn_arg_s(const MsnFile *f, const MsnCmd *c, int i)
{
    static char buf[4][24]; static int k;
    const MsnArg *a = msn_arg(f, c, i);
    if (!a) return EMPTY;
    if (a->type == MSN_ARG_STR || a->type == MSN_ARG_BARE) return msn_str(f, a->v.s);
    k = (k + 1) & 3;
    if (a->type == MSN_ARG_INT) snprintf(buf[k], sizeof buf[k], "%d", (int)a->v.i);
    else snprintf(buf[k], sizeof buf[k], "%g", (double)a->v.f);
    return buf[k];
}

int msn_arg_i(const MsnFile *f, const MsnCmd *c, int i)
{
    const MsnArg *a = msn_arg(f, c, i);
    if (!a) return 0;
    if (a->type == MSN_ARG_INT) return a->v.i;
    if (a->type == MSN_ARG_FLOAT) return (int)a->v.f;
    {
        const char *s = msn_str(f, a->v.s);
        if (msn_streq(s, "true")) return 1;
        return (int)strtol(s, NULL, 10);
    }
}

float msn_arg_f(const MsnFile *f, const MsnCmd *c, int i)
{
    const MsnArg *a = msn_arg(f, c, i);
    if (!a) return 0.0f;
    if (a->type == MSN_ARG_INT) return (float)a->v.i;
    if (a->type == MSN_ARG_FLOAT) return a->v.f;
    return (float)strtod(msn_str(f, a->v.s), NULL);
}

const MsnLoc *msn_arg_loc(const MsnFile *f, const MsnCmd *c, int i)
{
    const MsnArg *a = msn_arg(f, c, i);
    return (a && a->loc) ? &f->locs[a->loc - 1] : NULL;
}

int msn_streq(const char *a, const char *b)
{
    if (!a || !b) return 0;
    for (;; a++, b++) {
        int x = (unsigned char)*a, y = (unsigned char)*b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
        if (!x) return 1;
    }
}

const MsnConv *msn_find_conv(const MsnFile *f, const char *name, const char *mission)
{
    uint32_t i; const MsnConv *fallback = NULL;
    if (!name || !*name) return NULL;
    for (i = 0; i < f->nconvs; i++) {
        const MsnConv *c = &f->convs[i];
        if (!msn_streq(msn_str(f, c->name), name)) continue;
        if (mission && msn_streq(msn_str(f, c->mission), mission)) return c;
        if (!fallback || !*msn_str(f, c->mission)) fallback = c;
    }
    return fallback;
}

int msn_find_mission(const MsnFile *f, const char *id)
{
    uint32_t i;
    for (i = 0; i < f->nmissions; i++) if (msn_streq(msn_str(f, f->missions[i].id), id)) return (int)i;
    return -1;
}

int msn_loc_contains(const MsnFile *f, const MsnLoc *l, MsnVec3 p, float default_radius)
{
    unsigned i;
    if (!l) return 0;
    if (l->ntrig == 0) {
        float dx = p.x - l->pos[0], dy = p.y - l->pos[1], dz = p.z - l->pos[2];
        return dx * dx + dy * dy + dz * dz <= default_radius * default_radius;
    }
    for (i = 0; i < l->ntrig; i++) {
        const MsnTrig *t = &f->trigs[l->first_trig + i];
        float d[3] = { p.x - t->center[0], p.y - t->center[1], p.z - t->center[2] };
        if (t->kind == 0) {
            if (d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= t->half[0] * t->half[0]) return 1;
        } else {
            int k, in = 1;
            for (k = 0; k < 3 && in; k++) {
                float q = d[0] * t->axis[k][0] + d[1] * t->axis[k][1] + d[2] * t->axis[k][2];
                if (fabsf(q) > t->half[k]) in = 0;
            }
            if (in) return 1;
        }
    }
    return 0;
}

const char *const msn_objective_name[MSN_OBJ_COUNT] = { "none", "goto", "getin", "talkto", "dialogue", "interior",
    "gooutside", "race", "follow", "losetail", "delivery", "dump", "destroy", "pickupitem", "destroyboss", "timer",
    "fmv", "buycar", "buyskin", "coins", "dummy" };
const char *const msn_cond_name[MSN_COND_COUNT] = { "none", "timeout", "outofvehicle", "damage", "position", "race",
    "followdistance", "keepbarrel" };
const char *const msn_fail_name[] = { "none", "timeout", "outofvehicle", "damage", "position", "race",
    "followdistance", "keepbarrel", "partime", "abort", "hitnrun", "collect" };

/* ------------------------------------------------------------------ texts (TEXTS section, sorted by key) */
const char *msn_text(const MsnFile *f, const char *key)
{
    uint32_t lo = 0, hi = f->ntexts;
    if (!key || !*key) return EMPTY;
    while (lo < hi) {                                  /* keys are sorted byte-wise by the compiler */
        uint32_t mid = (lo + hi) / 2;
        int c = strcmp(msn_str(f, f->texts[mid].key), key);
        if (c == 0) return msn_str(f, f->texts[mid].value);
        if (c < 0) lo = mid + 1; else hi = mid;
    }
    return key;                                        /* unknown key: show the key itself ("3", "!!!") */
}

/* текст по хешу ключа: сам ключ в коде не пишется (04.11, чистота); перебор по всем ключам, вызывается раз в секунду из события */
const char *msn_text_h(const MsnFile *f, uint32_t h)
{
    uint32_t i;
    for (i = 0; i < f->ntexts; i++) {
        const unsigned char *k = (const unsigned char *)msn_str(f, f->texts[i].key);
        uint32_t x = 2166136261u;
        for (; *k; k++) x = (x ^ *k) * 16777619u;
        if (x == h) return msn_str(f, f->texts[i].value);
    }
    return NULL;
}

static const char *const fail_key[] = { "", "MISSION_FAILED_TIME_OUT", "MISSION_FAILED_PLAYER_OUT_OF_VEHICLE",
    "MISSION_FAILED_VEHICLE_DAMAGE", "MISSION_FAILED_POSITION", "MISSION_FAILED_RACE", "MISSION_FAILED_FOLLOW_DISTANCE",
    "", /* keepbarrel: no bible key found, generic MISSION_FAILED */ "MISSION_FAILED_TIME_OUT" /* wager par time */, "",
    "MISSION_FAILED_HIT_N_RUN" /* «Hit & Run: You got busted!» + подсказки HINT0..4 */ };

const char *msn_fail_text(const MsnFile *f, int reason)
{
    if (reason > 0 && reason <= MSN_FAIL_HITNRUN && *fail_key[reason]) return msn_text(f, fail_key[reason]);
    return msn_text(f, "MISSION_FAILED");
}

const char *msn_fail_hint(const MsnFile *f, int reason, int n)
{
    char key[64];
    if (reason <= 0 || reason > MSN_FAIL_HITNRUN || !*fail_key[reason] || n < 0 || n > 7) return EMPTY;
    snprintf(key, sizeof key, "%s_HINT%d", fail_key[reason], n);
    {
        const char *t = msn_text(f, key);
        return t == key ? EMPTY : t;
    }
}

/* ------------------------------------------------------------------ rewards (BindReward lines of this level) */
int msn_reward_count(const MsnFile *f)
{
    uint32_t i; int n = 0;
    for (i = 0; i < f->level->rew_count; i++) if (f->cmds[f->level->rew_first + i].op == MSN_OP_BINDREWARD) n++;
    return n;
}

int msn_reward_get(const MsnFile *f, int k, MsnReward *r)
{
    uint32_t i; int n = 0;
    memset(r, 0, sizeof *r);
    for (i = 0; i < f->level->rew_count; i++) {
        const MsnCmd *c = &f->cmds[f->level->rew_first + i];
        if (c->op != MSN_OP_BINDREWARD) continue;
        if (n++ != k) continue;
        r->name = msn_arg_s(f, c, 0); r->file = msn_arg_s(f, c, 1);
        r->kind = msn_streq(msn_arg_s(f, c, 2), "skin") ? MSN_OWN_SKIN : MSN_OWN_CAR;
        r->quest = msn_arg_s(f, c, 3);
        r->price = msn_argc(c) > 5 ? msn_arg_i(f, c, 5) : 0;
        r->seller = msn_argc(c) > 6 ? msn_arg_s(f, c, 6) : "";
        return 1;
    }
    return 0;
}

int msn_reward_find(const MsnFile *f, const char *quest, MsnReward *r)
{
    int k, n = msn_reward_count(f);
    for (k = 0; k < n; k++) if (msn_reward_get(f, k, r) && msn_streq(r->quest, quest)) return k;
    memset(r, 0, sizeof *r);
    return -1;
}
