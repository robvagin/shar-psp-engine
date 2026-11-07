// Media Engine, ступень 0 — проба, сторона игры (правки Роба 269/273/276; модуль и порядок пробы — viewer/me/sharme.c).
// Вызывается только служебным флагом сценария «meprobe» (тур на PSP tests/me/psp_test8_me.txt); в обычной игре кода нет в пути.
// Защиты по порядку (правило порта, docs/ME_ПЛАН_2026-10-03.md «Наш вариант»):
//   файл «nome» рядом с EBOOT  -> «ME skip nome»: ME не трогать вовсе
//   эмулятор (emu != 0)        -> «ME skip emu»: PPSSPP код на ME не исполняет — ничего не грузим
//   нет модуля sharme.prx      -> «ME skip noprx»
// Иначе — модуль ядра через kubridge ARK-4 (как sharexc.prx, viewer/main.c kexcBoot), проба в нём, итог строкой:
//   «ME probe ok=… rt_us_min/avg/max=… sum_ok=… mode=mist …»; нет ответа ME за 50 мс — строка начинается «ME timeout».
// Проба одна на запуск: повторный флаг отдаёт прежнюю строку (модуль уже в памяти, ME второй раз не перезапускаем).
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <kubridge.h>
#include <psppower.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include "mebox.h"

static char meLast[320];
static int meRan, meSvc;                // meSvc — служба звука на ME поставлена (правка 276): пробу тогда не запускать

static int meHave(const char *dir, const char *name) {
  char fn[256]; snprintf(fn, sizeof fn, "%s%s", dir, name);
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return 0;
  sceIoClose(f); return 1;
}

// dir — папка игры с «/» в конце; prx — имя модуля рядом с EBOOT; emu — 1 в эмуляторе (smokeIsEmu), 0 — железо или красный
// контроль гейта («flag meprobe 2»: защита эмулятора снята). Итог — строка с «\n» в out (журнал smoke.txt и чёрный ящик)
int meProbeRun(const char *dir, const char *prx, int emu, char *out, int outN) {
  if (meSvc) { snprintf(out, outN, "ME skip audio (ME занят звуком, флаг-файл meaudio)\n"); return 0; }   // правка 276
  if (meRan) { snprintf(out, outN, "ME probe again: %s", meLast); return 0; }
  if (meHave(dir, "nome")) { snprintf(out, outN, "ME skip nome\n"); return 0; }
  if (emu) { snprintf(out, outN, "ME skip emu\n"); return 0; }
  if (!meHave(dir, prx)) { snprintf(out, outN, "ME skip noprx (%s)\n", prx); return 0; }
  meRan = 1;
  // одна память на всё: итог (MeProbeRes, кратно 64) + ящик 64 + слово DMA 64 + буфер 64 КБ, выровнено на 64
  unsigned resSz = (sizeof(MeProbeRes) + 63u) & ~63u;
  void *mem = memalign(64, resSz + 128 + ME_PROBE_BYTES);
  if (!mem) { snprintf(meLast, sizeof meLast, "ME probe ok=0 err=nomem mode=mist\n"); snprintf(out, outN, "%s", meLast); return -1; }
  MeProbeRes *R = (MeProbeRes *)mem; memset(R, 0, sizeof *R);
  MeProbeArgs a; memset(&a, 0, sizeof a);
  a.magic = ME_PROBE_MAGIC; a.jobs = ME_PROBE_JOBS; a.timeoutUs = ME_PROBE_TIMEOUT_US; a.res = (unsigned)mem;
  snprintf(a.dir, sizeof a.dir, "%s", dir);
  char fn[256]; snprintf(fn, sizeof fn, "%s%s", dir, prx);
  unsigned t0 = sceKernelGetSystemTimeLow();
  SceUID m = kuKernelLoadModule(fn, 0, 0);
  if (m < 0) { snprintf(meLast, sizeof meLast, "ME probe ok=0 err=load:%08x mode=mist\n", (unsigned)m); free(mem); snprintf(out, outN, "%s", meLast); return -1; }
  int st = 0, rc = sceKernelStartModule(m, sizeof a, &a, &st, 0);
  unsigned ms = (sceKernelGetSystemTimeLow() - t0) / 1000u;
  if (rc < 0 || R->magic != ME_PROBE_MAGIC) {   // модуль не дошёл до итога: память не отдаём (ME мог получить адрес ящика)
    snprintf(meLast, sizeof meLast, "ME probe ok=0 err=start:%08x/%d stage=%d mode=mist\n", (unsigned)rc, st, R->stage);
    snprintf(out, outN, "%s", meLast); return -1;
  }
  unsigned avg = R->ok ? R->rtSum / (unsigned)R->ok : 0; char core[288];
  snprintf(core, sizeof core, "ME probe ok=%d/%d rt_us_min/avg/max=%u/%u/%u sum_ok=%d mode=%s table=%d init=%d stage=%d "
           "cu1=%u fpu=%x>%x evf=%d/%d,%d>%d st=%x>%x beat=%u ms=%u\n",
           R->ok, R->jobs ? R->jobs : ME_PROBE_JOBS, R->ok ? R->rtMin : 0, avg, R->rtMax, R->sumOk, R->mode == 2 ? "reset" : "mist",
           R->table, R->init, R->stage, (R->status >> 29) & 1u, R->fcsr0, R->fcsr1, R->evf, R->evfFound, R->evfPre, R->evfEnd, R->st0, R->st1, R->beat, ms);
  if (R->timeout) snprintf(meLast, sizeof meLast, "ME timeout job=%d; %s", R->timeoutJob, core);   // ME молчит 50 мс: дальше без ME,
  else { snprintf(meLast, sizeof meLast, "%s", core); free(mem); }   // память не отдаём; ответил на всё — отдаём
  snprintf(out, outN, "%s", meLast);
  return R->timeout ? -1 : 0;
}

// ---------------------------------------------------------------- правка 313: служба ME — общая для звука и частиц
// Модуль sharme.prx ставит нашу задачу в ячейку ME и остаётся (режим службы, «MEA …» в me.txt). Клиенты: звук (0, viewer/audio.c,
// флаг-файл meaudio) и частицы (1, viewer/fx.c, флаг-файл mefx). Схема памяти — mebox.h ME_MEM (одна константа по итогам TEST 10),
// первый байт флаг-файла может заменить её для тура: u/b/c/k (заглавная — только нижняя память). Защиты: nome, эмулятор (содержимое
// «2» — красный контроль гейта: защита эмулятора снята), нет sharme.prx. В эмуляторе «3»/«4» — служба-пустышка на главном
// процессоре: тот же протокол (ящик, задание, ответ, занятость, перенос задания звука в «ядро» схемы k), «4» — ME молчит
#include "../mixdsp.h"
#include "../spatialdsp.h"
#include "../fxdsp.h"
#include "../citycull.h"
#include "../fpuguard.h"
static struct { unsigned kick, poll, quiesce; int scheme, direct, on; } sv = {0, 0, 0, -1, 0, 0};
static volatile int svSleep, svHold;
int meSvcScheme(void) { return sv.on ? sv.scheme : -1; }
int meSvcHeld(void) { return !sv.on || svSleep || svHold; }
int meSvcSlept(void) { return svSleep; }
void meSvcHold(int on) { svHold = on; }               // ролики (audio.c audioMeHold): ME занят прошивкой (кадры sceJpeg)
static unsigned svCall(unsigned fn, unsigned a1, unsigned a2, unsigned a3) {   // функция модуля в режиме ядра (kubridge)
  if (sv.direct) return ((unsigned (*)(unsigned, unsigned, unsigned))fn)(a1, a2, a3);
  struct KernelCallArg k; memset(&k, 0, sizeof k); k.arg1 = a1; k.arg2 = a2; k.arg3 = a3;
  kuKernelCall((void *)fn, &k); return k.ret1;
}
static MeQueue svQ;                     // очередь (облако 05.10, mebox.h meBoxSubmit): последний ящик клиента у ME и когда отдан
void meSvcForget(int c) { if (c >= 0 && c < 2) svQ.run[c] = 0; }   // клиент выключил себя с ME (тайм-аут, сон, чужая сборка)
static volatile int fkRun;
unsigned meSvcChained; unsigned meSvcDbg[4];   // №653: отладка отказов submit (sent, done, chain, run|fkRun<<1|клиент<<2)                  // сколько заданий звука ушло на ME следом за частицами (AUDV me=…/c<число>)
static void svWaitUs(unsigned us) { sceKernelDelayThread(us); }
static unsigned svNow(void) { return sceKernelGetSystemTimeLow(); }
static int svKickBox(volatile MeBox *u) { return (int)svCall(sv.kick, (unsigned)u, 0, 0); }
// задание клиента c на ME. ubox — ящик клиента в памяти игры (64 байта, выровнен; нужен в схеме 0). -> номер (>0) или <0 (занят/нет)
// Приоритет звука (облако 05.10): ME занят частицами — звук (клиент 0) прицепляется следом (mebox.h meChainAttach), ME посчитает его
// сразу после частиц; главному звук достаётся, только если прицепить не вышло. Частицы (1) за звуком не цепляются — считает главный
int meSvcSubmit(int c, volatile MeBox *ubox, unsigned kind, void *job, unsigned words) {
  if (!sv.on) return -3;
  if (!ME_SCHEME_KBOX(sv.scheme)) {
    volatile MeBox *u = (volatile MeBox *)(0x40000000u | (unsigned)ubox);
    unsigned s0 = u->sent, d0 = u->done, ch0 = u->chain; int rn = svQ.run[c & 1] == u, fr = fkRun;
    int q = meBoxSubmit(&svQ, c, u, kind, meUserAddr(sv.scheme, (unsigned)job), words, svKickBox, svNow, svWaitUs);
    if (q < 0) { meSvcDbg[0] = s0; meSvcDbg[1] = d0; meSvcDbg[2] = ch0; meSvcDbg[3] = (unsigned)rn | ((unsigned)fr << 1) | ((unsigned)c << 2); }
    meSvcChained = svQ.chained; return q;
  }
  return (int)svCall(sv.kick, (unsigned)c | (kind << 8), (unsigned)job, words);
}
// 1 — задание seq клиента c посчитано (sum — итог), 0 — ещё нет
int meSvcPoll(int c, volatile MeBox *ubox, unsigned seq, unsigned *sum) {
  if (!ME_SCHEME_KBOX(sv.scheme)) {
    volatile MeBox *u = (volatile MeBox *)(0x40000000u | (unsigned)ubox);
    if (u->done != seq) return 0;
    *sum = u->sum; return 1;
  }
  unsigned d = svCall(sv.poll, (unsigned)c, 0, 0);
  if ((d & 0x7fffffffu) != (seq & 0x7fffffffu)) return 0;
  *sum = (d & ME_POLL_BAD) ? ME_MIX_BADSIZE : 0; return 1;
}
unsigned meSvcQuiesce(void) { return sv.on ? svCall(sv.quiesce, 0, 0, 0) : 0; }

// --- служба-пустышка (только эмулятор, гейт tests/me/run.py): «ME» — поток на главном процессоре
static SceUID fkSema = -1; static int fkMute; static volatile unsigned fkBox, fkSeq[2];
static volatile int fkRun;               // пустышка считает задание
static MeBox fkK[2] __attribute__((aligned(64))); static MixJob *fkKjob;
static void fkOne(volatile MeBox *x) {
    unsigned j = x->job & 0x1fffffffu;   // у пустышки адреса ME — обычные адреса игры
    if (x->kind == ME_KIND_MIX) {
      if (x->words != sizeof(MixJob) || (j & 63u)) x->sum = ME_MIX_BADSIZE;
      else { MixJob *m = (MixJob *)(0x40000000u | j); m->data = (const unsigned char *)(0x40000000u | ((unsigned)m->data & 0x1fffffffu)); mixDspRun(m); x->sum = 0; }
    } else if (x->kind == ME_KIND_SPATIAL) {
      if(x->words!=sizeof(SpatialJob)||(j&63u))x->sum=ME_MIX_BADSIZE;
      else {mixSpatialBatchRun((SpatialJob *)(0x40000000u|j));x->sum=0;}
    } else if (x->kind == ME_KIND_CITY) {   // №653: пустышка эмулятора — тот же расчёт на потоке главного процессора (указатели задания — адреса игры)
      if (x->words != sizeof(CcJob) || (j & 63u)) x->sum = ME_MIX_BADSIZE;
      else { CcJob *J = (CcJob *)j;
        J->list = (const unsigned short *)((unsigned)J->list & 0x1fffffffu); J->crc = (const unsigned short *)((unsigned)J->crc & 0x1fffffffu);
        J->cr = (const CcRec *)((unsigned)J->cr & 0x1fffffffu); J->cells = (const CcCell *)((unsigned)J->cells & 0x1fffffffu); J->state = (const unsigned char *)((unsigned)J->state & 0x1fffffffu);
        unsigned fs = fpuGet(); fpuSet(J->fcsr); J->magic = 0; int ok = ccRun(J); fpuSet(fs);
        sceKernelDcacheWritebackInvalidateRange(J, sizeof *J); x->sum = ok ? 0 : 1; }
    } else if (x->kind == ME_KIND_FX) {
      if (x->words != sizeof(FxJob) || (j & 63u)) x->sum = ME_MIX_BADSIZE;
      else { FxJob *f = (FxJob *)j;      // FPU как у ME в модуле: FCR31 главного потока
        f->E = (const FxEmit *)((unsigned)f->E & 0x1fffffffu); f->BB = (const FxBB *)((unsigned)f->BB & 0x1fffffffu);
        f->blob = (const unsigned char *)((unsigned)f->blob & 0x1fffffffu); f->vout = (FxV *)(0x40000000u | ((unsigned)f->vout & 0x1fffffffu));
        unsigned fs = fpuGet(); fpuSet(f->fcsr); fxdRun(f); fpuSet(fs);
        sceKernelDcacheWritebackInvalidateRange(f, sizeof *f); x->sum = 0; }
    }
    __asm__ volatile("sync" : : : "memory"); x->done = x->sent;
}
static int fkThread(SceSize a, void *b) {
  (void)a; (void)b;
  for (;;) {
    if (sceKernelWaitSema(fkSema, 1, NULL) < 0) return 0;
    volatile MeBox *x = (volatile MeBox *)fkBox;
    if (!x || fkMute) continue;          // «4»: ME молчит — ответа нет никогда
    fkOne(x);
    if (!ME_SCHEME_KBOX(sv.scheme) && !ME_CHAIN_OLD) {   // очередь, как me_gametask.c: следом — прицепленный ящик
      unsigned n = meChainClose(x);
      if (n) { meChainTake(x, n); fkOne((volatile MeBox *)n); }
      meChainFinish(x);
    }
    fkRun = 0;
  }
}
static unsigned fkKick(unsigned a1, unsigned a2, unsigned a3) {
  if (fkRun) return (unsigned)ME_SVC_BUSY;
  volatile MeBox *x; unsigned r;
  if (!ME_SCHEME_KBOX(sv.scheme)) { x = (volatile MeBox *)a1; r = x->sent; }
  else {
    unsigned c = a1 & 1u, kind = (a1 >> 8) & 0xffu, job = meUserAddr(sv.scheme, a2);
    if (kind == ME_KIND_MIX && (sv.scheme & 3) == ME_MEM_KERN) {
      if (a3 != sizeof(MixJob) || !fkKjob) return (unsigned)-2;
      mixJobIn(fkKjob, (const MixJob *)a2); fkKjob->data = (const unsigned char *)meUserAddr(sv.scheme, (unsigned)fkKjob->data);
      sceKernelDcacheWritebackInvalidateRange(fkKjob, sizeof(MixJob)); job = (unsigned)fkKjob;
    }
    x = (volatile MeBox *)(0x40000000u | (unsigned)&fkK[c]);
    x->pad[0] = a2;                     // адрес задания игры — для переноса итога обратно (схема k)
    x->kind = kind; x->job = job; x->words = a3; x->sum = 0; x->sent = ++fkSeq[c]; r = x->sent;
  }
  fkRun = !fkMute; fkBox = (unsigned)x; sceKernelSignalSema(fkSema, 1);
  return r;
}
static unsigned fkPoll(unsigned c, unsigned b, unsigned d) {
  (void)b; (void)d; c &= 1u;
  volatile MeBox *x = (volatile MeBox *)(0x40000000u | (unsigned)&fkK[c]);
  unsigned done = x->done, sum = x->sum;
  if (done == fkSeq[c] && x->pad[1] != done && x->kind == ME_KIND_MIX && (sv.scheme & 3) == ME_MEM_KERN && sum != ME_MIX_BADSIZE) {
    sceKernelDcacheInvalidateRange(fkKjob, sizeof(MixJob)); mixJobOut((MixJob *)x->pad[0], fkKjob); x->pad[1] = done; }
  return (done & 0x7fffffffu) | (sum == ME_MIX_BADSIZE ? ME_POLL_BAD : 0u);
}
static unsigned fkQuiesce(unsigned a, unsigned b, unsigned c) { (void)a; (void)b; (void)c; return 0; }

// питание: сон — ME больше не трогаем (прошивка после пробуждения перезапускает ME, ячейка 13 снова пустая)
static int svPwrCb(int unk, int flags, void *arg) {
  (void)unk; (void)arg;
  if (flags & (PSP_POWER_CB_POWER_SWITCH | PSP_POWER_CB_SUSPENDING | PSP_POWER_CB_STANDBY)) svSleep = 1;
  return 0;
}
static int svPwrThread(SceSize a, void *b) {
  (void)a; (void)b;
  int cb = sceKernelCreateCallback("me_svc_pwr", svPwrCb, NULL);
  if (cb >= 0) scePowerRegisterCallback(-1, cb);
  sceKernelSleepThreadCB();
  return 0;
}

// поставить службу (один раз на запуск; второй клиент получает ту же). mode — первый байт флаг-файла. -> 0 есть, <0 нет; out — строка
static int svBoot(const char *dir, int emu, char mode, const char *who, char *out, int outN) {
  if (sv.on) { snprintf(out, outN, "ME %s on (служба уже стоит) scheme=%d\n", who, sv.scheme); return 0; }
  int scheme = meSchemeFromChar(mode, ME_MEM);
  if (mode == '2') emu = 0;             // красный контроль гейта tests/me/run.py
  if (meHave(dir, "nome")) { snprintf(out, outN, "ME %s skip nome\n", who); return -1; }
  if (emu && (mode == '3' || mode == '4')) {   // служебно, только эмулятор: пустышка на главном
    char m2 = 0; char fn[256]; snprintf(fn, sizeof fn, "%smescheme", dir);   // схема пустышки — файл mescheme (u/b/c/k), иначе ME_MEM
    SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoRead(f, &m2, 1); sceIoClose(f); }
    sv.scheme = meSchemeFromChar(m2, ME_MEM);
    fkMute = mode == '4'; fkSema = sceKernelCreateSema("fake_me", 0, 0, 1000, NULL);
    if ((sv.scheme & 3) == ME_MEM_KERN) fkKjob = memalign(64, sizeof(MixJob));
    SceUID t = sceKernelCreateThread("fake_me", fkThread, 0x14, 0x4000, PSP_THREAD_ATTR_USER, NULL);
    if (fkSema < 0 || t < 0 || ((sv.scheme & 3) == ME_MEM_KERN && !fkKjob)) { snprintf(out, outN, "ME %s off: fake\n", who); return -1; }
    sceKernelStartThread(t, 0, NULL);
    sv.kick = (unsigned)fkKick; sv.poll = (unsigned)fkPoll; sv.quiesce = (unsigned)fkQuiesce; sv.direct = 1; sv.on = 1;
    snprintf(out, outN, "ME %s on fake mute=%d scheme=%d\n", who, fkMute, sv.scheme); return 0;
  }
  if (emu) { snprintf(out, outN, "ME %s skip emu\n", who); return -1; }
  if (meRan || meSvc) { snprintf(out, outN, "ME %s skip probe\n", who); return -1; }
  if (!meHave(dir, "sharme.prx")) { snprintf(out, outN, "ME %s skip noprx\n", who); return -1; }
  meRan = 1;
  void *mem = memalign(64, 128); if (!mem) { snprintf(out, outN, "ME %s off: nomem\n", who); return -1; }
  memset(mem, 0, 128); MeSvcRes *R = (MeSvcRes *)mem;
  MeSvcArgs a; memset(&a, 0, sizeof a); a.magic = ME_SVC_MAGIC; a.res = (unsigned)mem; a.scheme = scheme; snprintf(a.dir, sizeof a.dir, "%s", dir);
  { int meGameTask(int, void *); a.gameFn = (unsigned)meGameTask; sceKernelDcacheWritebackAll(); }   // 04.10: функция ME в памяти игры (ядро ME не видит)
  sceKernelDcacheWritebackInvalidateRange(mem, 128);
  char fn[256]; snprintf(fn, sizeof fn, "%ssharme.prx", dir);
  unsigned t0 = sceKernelGetSystemTimeLow();
  SceUID m = kuKernelLoadModule(fn, 0, 0);
  if (m < 0) { snprintf(out, outN, "ME %s off: load:%08x\n", who, (unsigned)m); free(mem); return -1; }
  int st = 0, rc = sceKernelStartModule(m, sizeof a, &a, &st, 0);
  unsigned ms = (sceKernelGetSystemTimeLow() - t0) / 1000u;
  sceKernelDcacheInvalidateRange(mem, 128);
  if (rc < 0 || R->magic != ME_SVC_MAGIC || !R->kick || !R->quiesce || (ME_SCHEME_KBOX(scheme) && !R->poll)) {   // память не отдаём
    snprintf(out, outN, "ME %s off: start:%08x/%d stage=%d table=%d init=%d scheme=%d ms=%u\n", who, (unsigned)rc, st, R->stage, R->table, R->init, scheme, ms);
    int len = (int)strlen(out);
    snprintf(out + len, outN - len, "res magic=%08x kick=%08x quiesce=%08x poll=%08x expected=%08x\n", R->magic, R->kick, R->quiesce, R->poll, ME_SVC_MAGIC);
    return -1;
  }
  meSvc = 1; sv.kick = R->kick; sv.poll = R->poll; sv.quiesce = R->quiesce; sv.scheme = scheme; sv.on = 1;
  SceUID t = sceKernelCreateThread("me_svc_pwr", svPwrThread, 0x11, 0x1000, PSP_THREAD_ATTR_USER, NULL);
  if (t >= 0) sceKernelStartThread(t, 0, NULL);
  snprintf(out, outN, "ME %s on table=%d evf=%d/%d st0=%x scheme=%d kbox=%x ms=%u\n", who, R->table, R->evfFound, R->evfPre, R->st0, scheme, R->kbox, ms);
  return 0;
}
static int svFlag(const char *dir, const char *name, char *mode) {   // флаг-файл рядом с EBOOT: есть ли и первый байт
  char fn[256]; snprintf(fn, sizeof fn, "%s%s", dir, name);
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return 0;
  *mode = 0; sceIoRead(f, mode, 1); sceIoClose(f); return 1;
}
static const char *svZone(unsigned a) { return meIsHigh(a) ? "high" : "low"; }

// Правка 276 — пилот: смешивание звука на ME (viewer/mixdsp.h, исполнитель viewer/audio.c). Только с флаг-файлом «meaudio» рядом
// с EBOOT (по умолчанию выкл: игра как раньше). Ответ — строка для журнала и ящика, пустая — файла нет
// Standalone hardware clients: fixed proven user-memory scheme, never a fake backend or flag override
int meSvcBootUser(const char *dir, int emu, char *out, int outN) {
  return svBoot(dir, emu, 'u', "test", out, outN);
}
int audioMeAttach(void); unsigned audioMeJobAddr(void), audioMeDataAddr(void);
int meAudioBoot(const char *dir, int emu, char *out, int outN) {
  out[0] = 0; char c = 0;
  // Слово Роба 500 (10.10): звук на ME включён по умолчанию, без ручных файлов (SHARTEST5: опоздавших буферов 0, минус около 1 мс на кадр, пропуски L2 вдвое
  // меньше). Флаг-файл meaudio по-прежнему задаёт букву схемы памяти; ключ отката игрока — файл meaudiooff рядом с EBOOT (или nome — ME целиком);
  // в эмуляторе без файла ME не трогаем. ME_AUDIO_DEFAULT_OFF=1 при сборке — как раньше: только с файлом meaudio
#ifndef ME_AUDIO_DEFAULT_OFF
#define ME_AUDIO_DEFAULT_OFF 0
#endif
  if (!svFlag(dir, "meaudio", &c)) {
    if (ME_AUDIO_DEFAULT_OFF || emu || meHave(dir, "meaudiooff")) return 0;
    c = 'u';
  }
  char s1[200]; if (svBoot(dir, emu, c, "audio", s1, sizeof s1) < 0) { snprintf(out, outN, "%s", s1); return -1; }
  unsigned jb = audioMeJobAddr(), db = audioMeDataAddr();
  if ((sv.scheme & ME_MEM_LOW) && (meIsHigh(jb) || meIsHigh(db))) {   // ступень 8 не прошла: ME верхнюю память не видит
    snprintf(out, outN, "ME audio off: high job=%s data=%s; %s", svZone(jb), svZone(db), s1); return -1; }
  int ar = audioMeAttach();
  void audioMeWbOld(int on); int wo = meHave(dir, "mewbold"); audioMeWbOld(wo);   // правка 410 К1: прежний сброс целиком (тур A/B)
  int l = snprintf(out, outN, "%s", s1); if (l > 0 && out[l - 1] == '\n') out[--l] = 0;
  snprintf(out + l, outN - l, " audio=%s job=%s data=%s wb=%s\n", ar ? "off:attach" : "on", svZone(jb), svZone(db), wo ? "old" : "k1");
  return ar;
}
// Правка 313: частицы и свечение на ME (viewer/fxdsp.h, исполнитель viewer/fx.c) — флаг-файл «mefx» рядом с EBOOT
int fxMeAttach(void);
void fxMeEnable(int on);
int meFxBoot(const char *dir, int emu, char *out, int outN) {
  out[0] = 0; char c = 0;
  int pairOnly = 0;   // файл mefxpair: служба поднята, но частицы на ME только после «flag mefx 1» (A/B-пара в туре); по умолчанию (ни одного файла) ME частицам не отдаётся
  if (!svFlag(dir, "mefx", &c)) { if (!svFlag(dir, "mefxpair", &c)) return 0; pairOnly = 1; }
  char s1[200]; if (svBoot(dir, emu, c, "fx", s1, sizeof s1) < 0) { snprintf(out, outN, "%s", s1); return -1; }
  int r = fxMeAttach();
  if (!r && pairOnly) fxMeEnable(0);
  int l = snprintf(out, outN, "%s", s1); if (l > 0 && out[l - 1] == '\n') out[--l] = 0;
  snprintf(out + l, outN - l, " fx=%s\n", r ? "off:attach" : "on");
  return r;
}

// TEST 10 (04.10) — лесенка ME по ступеням (mebox.h MeLadder*, модуль viewer/me/sharme.c meLadderStep). Флаг сценария «melad N»:
// первый вызов грузит модуль (ступень 0 — только чтение: образ ядра ME, адреса, память), дальше ступень N через kuKernelCall.
// Защиты — как у пробы (nome, эмулятор, нет модуля, служба звука на ME). Ступень без ответа ME — «stuck»: модуль дальше ME не трогает,
// а ролик проверки (флаг mefmv) игра пропускает (meLadderStuck), чтобы не повесить приставку заданием прошивки
static MeLadderRes *meL;
static int meLFail;
static unsigned meLow[ME_LADDER_BYTES / 4] __attribute__((aligned(64)));   // буфер в нижней памяти игры (.bss, < 0x0A000000) — ступень 7
int meLadderStuck(void) { if (!meL) return 0;   // модуль не встал — ME не трогали, ролик можно
  sceKernelDcacheInvalidateRange(meL, sizeof *meL); return meL->stuck; }
int meLadderRun(const char *dir, int emu, int step, char *out, int outN) {
  if (meSvc) { snprintf(out, outN, "ML skip audio (ME занят звуком)\n"); return 0; }
  if (meHave(dir, "nome")) { snprintf(out, outN, "ML skip nome\n"); return 0; }
  if (emu) { snprintf(out, outN, "ML %d skip emu\n", step); return 0; }
  if (meLFail) { snprintf(out, outN, "ML %d skip: module failed\n", step); return 0; }
  if (!meL) {
    if (meRan) { snprintf(out, outN, "ML skip: probe ran\n"); return 0; }   // старая проба уже перезапускала ME в этом запуске
    if (!meHave(dir, "sharme.prx")) { snprintf(out, outN, "ML skip noprx\n"); meLFail = 1; return 0; }
    meRan = 1;
    MeLadderRes *R = (MeLadderRes *)memalign(64, sizeof(MeLadderRes));
    if (!R) { snprintf(out, outN, "ML 0 nomem\n"); meLFail = 1; return -1; }
    memset(R, 0, sizeof *R); sceKernelDcacheWritebackInvalidateRange(R, sizeof *R);
    MeLadderArgs a; memset(&a, 0, sizeof a);
    int meGameTask(int, void *);
    a.magic = ME_LADDER_MAGIC; a.res = (unsigned)R; a.lowBuf = (unsigned)meLow; a.lowBytes = sizeof meLow; a.gameFn = (unsigned)meGameTask;
    sceKernelDcacheWritebackAll();   // TEST 15: код meGameTask — в памяти (ME читает его своим кэшем)
    snprintf(a.dir, sizeof a.dir, "%s", dir);
    char fn[256]; snprintf(fn, sizeof fn, "%ssharme.prx", dir);
    SceUID m = kuKernelLoadModule(fn, 0, 0);
    if (m < 0) { snprintf(out, outN, "ML 0 load:%08x\n", (unsigned)m); meLFail = 1; return -1; }
    int st = 0, rc = sceKernelStartModule(m, sizeof a, &a, &st, 0);
    sceKernelDcacheInvalidateRange(R, sizeof *R);
    if (rc < 0 || R->magic != ME_LADDER_MAGIC || !R->step) { snprintf(out, outN, "ML 0 start:%08x/%d\n", (unsigned)rc, st); meLFail = 1; return -1; }
    meL = R;
    if (step == 0) { snprintf(out, outN, "%s", R->line); return R->last; }
  }
  if (step == 0) { sceKernelDcacheInvalidateRange(meL, sizeof *meL); snprintf(out, outN, "ML 0 again: %s", meL->line); return 0; }
  struct KernelCallArg k; memset(&k, 0, sizeof k); k.arg1 = (u32)step;
  unsigned t0 = sceKernelGetSystemTimeLow();
  kuKernelCall((void *)meL->step, &k);
  unsigned ms = (sceKernelGetSystemTimeLow() - t0) / 1000u;
  sceKernelDcacheInvalidateRange(meL, sizeof *meL);
  int n = snprintf(out, outN, "%s", meL->line);
  if (n > 0 && n < outN && out[n - 1] == '\n') snprintf(out + n - 1, outN - n + 1, " call_ms=%u\n", ms);
  return (int)k.ret1;
}
