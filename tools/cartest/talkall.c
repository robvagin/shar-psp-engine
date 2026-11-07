// Все цели разговора в комнатах уровня: «ближе всего подойти» против радиуса скрипта (таблица). Цели — стадии talkto (SetTalkToTarget: имя, радиус 4-го аргумента или 1,3; положение — AddNPC той же миссии с тем же именем)
// и бонус-персонажи (AddNPCCharacterBonusMission, радиус по умолчанию). Комната цели — по полу под ней; достижимость — залив клеток 0,5 м кодом EBOOT (rooms.c, homer.c), как tools/cartest/talkreach.c.
//   talkall LEVEL.msn interiors-lN.bin homer.shr [MINРАДИУС_СКРИПТА_ДЛЯ_ПЕЧАТИ]
// Строка: T миссия стадия имя комната x y z радиус ближе_всего запас(радиус − ближе_всего) вердикт
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "homer.h"
#include "skin.h"
#include "rooms.h"
#include "mission/msn.h"
#include "mission/msn_format.h"
#include "mission/msn_ops.h"
unsigned homerSolidFrame = 0;
static unsigned char *readAll(const char *p, size_t *n) { FILE *f = fopen(p, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); long s = ftell(f); rewind(f); unsigned char *b = aligned_alloc(16, ((size_t)s + 15) & ~15u); if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); return 0; } fclose(f); *n = (size_t)s; return b; }
static Rooms R; static SkinModel *HM;
static float reach(Room *rm, float tx, float ty, float tz, float g, float *bx, float *bz, int *nreach) {
  const float SP = 0.5f; enum { NG = 160 }; float nn[3];
  static signed char vis[NG][NG]; static float gyc[NG][NG]; memset(vis, 0, sizeof vis);
  float ox = tx - NG * SP / 2, oz = tz - NG * SP / 2; int sx0 = (int)floorf((rm->start[0] - ox) / SP), sz0 = (int)floorf((rm->start[2] - oz) / SP);
  if (sx0 < 0 || sz0 < 0 || sx0 >= NG || sz0 >= NG) return -1.f;   // старт комнаты вне окна 32 м вокруг цели
  static int qx[NG * NG], qz[NG * NG]; int qh = 0, qt = 0; qx[qt] = sx0; qz[qt++] = sz0; vis[sz0][sx0] = 1; gyc[sz0][sx0] = rm->start[1];
  float best = 1e9f; *nreach = 0;
  while (qh < qt) { int cx = qx[qh], cz = qz[qh++]; float px = ox + (cx + .5f) * SP, pz = oz + (cz + .5f) * SP, py = gyc[cz][cx]; (*nreach)++;
    float d3 = sqrtf((px - tx) * (px - tx) + (py - ty) * (py - ty) + (pz - tz) * (pz - tz)); if (d3 < best) { best = d3; *bx = px; *bz = pz; }
    static const int DX[4] = {1, -1, 0, 0}, DZ[4] = {0, 0, 1, -1};
    for (int d = 0; d < 4; d++) { int nx = cx + DX[d], nz = cz + DZ[d]; if (nx < 0 || nz < 0 || nx >= NG || nz >= NG || vis[nz][nx]) continue;
      float qx_ = ox + (nx + .5f) * SP, qz_ = oz + (nz + .5f) * SP, gy2; if (!colGround(&rm->col, qx_, qz_, py + 1.2f, py - 1.2f, &gy2, nn)) continue;
      Homer H; memset(&H, 0, sizeof H); if (homerInit(&H, HM)) continue; float hd = atan2f(qx_ - px, qz_ - pz); homerPlace(&H, &rm->col, px, py, pz, hd);
      HomerInput in; memset(&in, 0, sizeof in); in.my = 1.f; in.camYaw = hd; int ok = 0;
      for (int f = 0; f < 45; f++) { homerUpdate(&H, &rm->col, NULL, &in, 1.f / 60.f); if (hypotf(H.x - qx_, H.z - qz_) < 0.12f && fabsf(H.y - gy2) < 0.5f) { ok = 1; break; } }
      if (ok) { vis[nz][nx] = 1; gyc[nz][nx] = gy2; qx[qt] = nx; qz[qt++] = nz; } } }
  (void)g; return best;
}
static void report(const char *kind, const char *mid, unsigned stage, const char *name, const float *p, float rad) {
  float g, nn[3]; int rk = -1;
  for (int k = 0; k < R.nroom; k++) if (colGround(&R.room[k].col, p[0], p[2], p[1] + 1.5f, p[1] - 3.f, &g, nn)) { rk = k; break; }
  if (rk < 0) { printf("%s\t%s\t%u\t%s\t(вне комнат)\t%.1f\t%.1f\t%.1f\t%.2f\n", kind, mid, stage, name, p[0], p[1], p[2], rad); return; }
  float bx = 0, bz = 0; int nr = 0; float best = reach(&R.room[rk], p[0], p[1], p[2], g, &bx, &bz, &nr);
  if (best < 0) { printf("%s\t%s\t%u\t%s\t%s\t%.1f\t%.1f\t%.1f\t%.2f\tстарт вне окна\n", kind, mid, stage, name, R.room[rk].name, p[0], p[1], p[2], rad); return; }
  printf("%s\t%s\t%u\t%s\t%s\t%.1f\t%.1f\t%.1f\t%.2f\tближе всего %.2f\tзапас %+.2f\t%s\n", kind, mid, stage, name, R.room[rk].name, p[0], p[1], p[2], rad, best, rad - best, rad > best ? "достижимо" : "НЕДОСТИЖИМО");
}
int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "talkall LEVEL.msn interiors-lN.bin homer.shr\n"); return 2; }
  size_t n; unsigned char *mb = readAll(argv[1], &n); if (!mb) return 2; static MsnFile F; char err[128];
  if (msn_open(&F, mb, n, err, sizeof err)) { fprintf(stderr, "msn: %s\n", err); return 2; }
  unsigned char *rb = readAll(argv[2], &n); if (!rb) return 2; int e = roomsParse(&R, rb, n); if (e) { fprintf(stderr, "rooms: %s\n", roomsError(e)); return 2; }
  HM = skinLoad(argv[3]); if (!HM) return 2;
  for (unsigned mi = 0; mi < F.nmissions; mi++) { const MsnMission *ms = &F.missions[mi];
    for (int ph = 0; ph < 2; ph++) { unsigned pi = ph ? ms->main : ms->intro; if (pi == MSN_NONE16) continue; const MsnPhase *P = &F.phases[pi];
      for (unsigned k = 0; k < P->stage_count; k++) { const MsnStage *st = &F.stages[P->first_stage + k]; if (st->obj != MSN_OBJ_TALKTO) continue;
        const MsnCmd *ct = 0; for (unsigned c = 0; c < st->cmd_count; c++) { const MsnCmd *C = &F.cmds[st->cmd_first + c]; if (C->op == MSN_OP_SETTALKTOTARGET) ct = C; }
        if (!ct) continue; const char *name = msn_arg_s(&F, ct, 0); float rad = 1.3f; if (msn_argc(ct) > 3 && msn_arg_f(&F, ct, 3) > 0.f) rad = msn_arg_f(&F, ct, 3);
        const MsnLoc *loc = 0; for (unsigned c = 0; c < st->cmd_count && !loc; c++) { const MsnCmd *C = &F.cmds[st->cmd_first + c]; if (C->op == MSN_OP_ADDNPC && !strcmp(msn_arg_s(&F, C, 0), name)) loc = msn_arg_loc(&F, C, 1); }
        for (unsigned q = 0; q < F.nmissions && !loc; q++) for (int ph2 = 0; ph2 < 2 && !loc; ph2++) { unsigned p2 = ph2 ? F.missions[q].main : F.missions[q].intro; if (p2 == MSN_NONE16) continue;   // имя могло быть добавлено другой стадией
          if (q != mi) continue; const MsnPhase *PP = &F.phases[p2]; for (unsigned kk = 0; kk < PP->stage_count && !loc; kk++) { const MsnStage *s2 = &F.stages[PP->first_stage + kk];
            for (unsigned c = 0; c < s2->cmd_count && !loc; c++) { const MsnCmd *C = &F.cmds[s2->cmd_first + c]; if (C->op == MSN_OP_ADDNPC && !strcmp(msn_arg_s(&F, C, 0), name)) loc = msn_arg_loc(&F, C, 1); } } }
        char mid[32]; snprintf(mid, sizeof mid, "%s.%d.%u", msn_str(&F, ms->id), ph, k);
        if (!loc) { printf("T\t%s\t%u\t%s\t(положение NPC не найдено)\t\t\t\t%.2f\n", mid, k, name, rad); continue; }
        float p[3] = {loc->pos[0], loc->pos[1], loc->pos[2]}; report("T", mid, k, name, p, rad); } }
    if (ms->bonus_npc != 0xFFFFFFFFu) { const MsnCmd *C = &F.cmds[ms->bonus_npc]; const MsnLoc *loc = 0; for (int i = 0; i < msn_argc(C) && !loc; i++) loc = msn_arg_loc(&F, C, i);
      if (loc) { float p[3] = {loc->pos[0], loc->pos[1], loc->pos[2]}; report("B", msn_str(&F, ms->id), 0, msn_arg_s(&F, C, 0), p, 1.3f); } } }
  return 0;
}
