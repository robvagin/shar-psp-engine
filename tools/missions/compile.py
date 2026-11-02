#!/usr/bin/env python3
"""Compile SHAR mission scripts (.mfk) of one level into a compact binary pack (MSN1) for the PSP runtime.

  python tools/missions/compile.py --level 1 [--out build/missions] [--json scratch/level01.json]
  python tools/missions/compile.py --all

What it does
  * parses level.mfk, leveli.mfk, every scheduled mission (mXl/mXi + intro mXsdl/mXsdi), rewards.mfk
  * validates: every command known (commands.py = donut registry), argument count per donut signature,
    argument types, block nesting (auto-repairs the two broken spots on the disc, with a warning),
    one objective per stage, objective/condition types known
  * resolves locator names against the level's p3d files (mission-scoped: files the mission loads first,
    then level-wide, then world zones), incl. trigger volumes (sphere/box), car headings, interior doors
  * resolves texts (English text bible srr2.p3d): stage objectives, mission titles, descriptions
  * resolves conversations (SetDialogueInfo / SetCompletionDialog / bonus intros) to the list of
    conversations/*.rsd lines (mission-specific first, level fallback)
  * checks referenced files exist (p3d, cars, .con, FMV, presentation bitmaps)
  * synthesizes the intro dialogue phase of bonus missions from leveli.mfk (as the original does in code)
Binary layout: see viewer/mission/msn_format.h (kept in sync by hand; version field checked at load).
"""
import argparse, json, math, re, struct, sys, hashlib
from pathlib import Path
from collections import defaultdict, Counter

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
GAME = ROOT / 'hit-and-run-web/source/game'
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
import mfk
import commands as CMD
from p3dlocs import read_locators, TYPE_NAMES

VERSION = 2
# fixed UI / failure texts exported into the TEXTS section (English bible keys; values UTF-8)
UI_TEXT_KEYS = ['MISSION_COMPLETE', 'BONUS_MISSION_COMPLETE', 'STAGE_COMPLETE', 'MISSION_FAILED', 'LEVEL_COMPLETE',
                'GAME_COMPLETE', 'GO', 'READY', 'RETRY', 'RETRY_MISSION', 'ABORT_MISSION', 'CANCEL_MISSION',
                'ACCEPT_MISSION', 'BONUS_MISSIONS', 'COINS', 'TIME_TO_BEAT', 'TO_PURCHASE', 'RACE_COMPLETE_SOME',
                'RACE_SUMMARY', 'BUSTED']
FAIL_TEXT_KEYS = ['TIME_OUT', 'PLAYER_OUT_OF_VEHICLE', 'VEHICLE_DAMAGE', 'POSITION', 'RACE', 'FOLLOW_DISTANCE',
                  'HIT_N_RUN', 'OUT_OF_BOUNDS']
NAV_MODES = {'': 0, 'neither': 1, 'both': 2, 'nearest road': 3, 'intersection': 4}
CTX_BODY, CTX_OBJ, CTX_COND = 0, 1, 2
ARG_INT, ARG_FLOAT, ARG_STR, ARG_BARE = 0, 1, 2, 3
MF_BONUS, MF_STREETRACE, MF_GAMBLE, MF_BONUSMISSION, MF_NOSCRIPT, MF_LEVELOVER, MF_GAMEOVER, MF_FORCEDCAR, MF_TRANSITION = 1, 2, 4, 8, 16, 32, 64, 128, 256
SF_FINAL, SF_CHECKPOINT, SF_LOCKED, SF_SHOWCOMPLETE, SF_MUSTACTION, SF_GOTODIALOGOFF, SF_ELAPSED, SF_MUSICON = 1, 2, 4, 8, 16, 32, 64, 128
PF_INTRO, PF_SYNTHETIC = 1, 2
TITLE_NUM = {'sr1': 8, 'sr2': 9, 'sr3': 10, 'gr1': 11, 'bm1': 12}

# ------------------------------------------------------------------ helpers
class FileIndex:
    """Case-insensitive lookup of disc files (the scripts use mixed case and backslashes)."""
    def __init__(self, root):
        self.map = {}; self.base = {}
        for p in root.rglob('*'):
            if p.is_file():
                self.map[str(p.relative_to(root)).lower()] = p
                self.base.setdefault(p.name.lower(), p)
    def find(self, rel):
        return self.map.get(str(rel).replace('\\', '/').lower().lstrip('/'))

def bible():
    """English text bible (same algorithm as hit-and-run-web/tools/campaign.py, MIT)."""
    from p3d import read, walk, string
    root = read(GAME / 'art/frontend/scrooby/resource/txtbible/srr2.p3d')
    c = next(c for c in walk(root) if c.id == 0x1800e and c.data[string(c.data)[1]:string(c.data)[1] + 1] == b'E')
    _, p = string(c.data); n, mod, size = struct.unpack_from('<III', c.data, p + 1); p += 13
    hashes = struct.unpack_from('<' + str(n) + 'I', c.data, p); offsets = struct.unpack_from('<' + str(n) + 'I', c.data, p + 4 * n)
    buf = c.data[p + 8 * n:]
    values = {h: buf[o:].decode('utf-16-le', errors='replace').split('\0')[0] for h, o in zip(hashes, offsets)}
    def get(key):
        h = 0
        for ch in key.encode():
            h = (ch + (h << 6)) % mod
        return values.get(h)
    get.values = values   # все строки (поиск по содержанию: у предложений «Congratulations! You've unlocked …» ключей не знаем)
    return get

CONV_RE = re.compile(r'^c_(.+?)_(\d+)_([a-z]+)_([a-z0-9]+)(?:_l(\d)(?:([a-z])(\d+)?)?)?\.rsd$', re.I)
def conversation_index():
    idx = defaultdict(list)   # (name, level, missioncode) -> [(line, kind, actor, file)]
    for p in sorted((GAME / 'conversations').glob('*.rsd')):
        m = CONV_RE.match(p.name)
        if not m:
            continue
        name, line, kind, actor, level, mk, mn = m.groups()
        level = int(level) if level else 0
        code = ''
        if mk:
            code = {'m': 'm', 'r': 'sr', 'b': 'bm', 'g': 'gr', 't': 'tutorial'}.get(mk.lower(), mk.lower()) + (mn or '')
            if mk.lower() == 't':
                code = 'tutorial'
        idx[(name.lower(), level, code)].append((int(line), kind.lower(), actor.lower(), 'conversations/' + p.name))
    for v in idx.values():
        v.sort()
    return idx

# ------------------------------------------------------------------ binary writer
class Pack:
    def __init__(self):
        self.strings = ['']; self.sidx = {'': 0}
        self.args = []; self.cmds = []; self.locs = []; self.trigs = []; self.lidx = {}
        self.missions = []; self.phases = []; self.stages = []; self.conds = []
        self.convs = []; self.clips = []; self.cidx = {}; self.files = []; self.fidx = {}
        self.level = None
        self.texts = {}            # bible key -> value (TEXTS section)

    def s(self, text):
        text = '' if text is None else str(text)
        i = self.sidx.get(text)
        if i is None:
            i = self.sidx[text] = len(self.strings); self.strings.append(text)
        return i

    def f(self, name):
        i = self.fidx.get(name)
        if i is None:
            i = self.fidx[name] = len(self.files); self.files.append(self.s(name))
        return i

    def loc(self, L):
        key = (L.file, L.name.lower())
        i = self.lidx.get(key)
        if i is None:
            i = self.lidx[key] = len(self.locs)
            first = len(self.trigs)
            for kind, center, half, axes in L.triggers:
                self.trigs.append((kind, center, half, axes))
            data = L.data
            if isinstance(data, str):
                data = self.s(data)
            self.locs.append(dict(name=self.s(L.name), type=L.type, ntrig=len(L.triggers), first=first, file=self.s(L.file),
                                  data=int(data) & 0xffffffff, pos=L.pos, heading=L.heading))
        return i

    def cmd(self, op, args, ctx, line, file, locs=None):
        first = len(self.args)
        for k, a in enumerate(args):
            li = (locs or {}).get(k)
            loc = 0 if li is None else li + 1
            if isinstance(a, bool):
                a = int(a)
            if isinstance(a, int) and not -2**31 <= a < 2**31:
                a = float(a)            # d1i.mfk SetDemoLoopTime(40000000000) does not fit int32
            if isinstance(a, int):
                self.args.append((ARG_INT, loc, a))
            elif isinstance(a, float):
                self.args.append((ARG_FLOAT, loc, a))
            else:
                self.args.append((ARG_BARE if getattr(a, 'bare', False) else ARG_STR, loc, self.s(a)))
        self.cmds.append((op, len(args), ctx, first, line, self.f(file)))
        return len(self.cmds) - 1

    def conv(self, name, mission, clips):
        key = (name.lower(), mission)
        i = self.cidx.get(key)
        if i is None:
            first = len(self.clips)
            for line, kind, actor, file in clips:
                self.clips.append((self.s(file), self.s(actor), line, {'convinit': 0, 'noboxconv': 1, 'tutorial': 2}.get(kind, 3), 0))
            i = self.cidx[key] = len(self.convs)
            self.convs.append((self.s(name.lower()), self.s(mission), first, len(clips)))
        return i

    def build(self, level_no):
        sec = []
        blob = bytearray(); offs = []
        for t in self.strings:
            offs.append(len(blob)); blob += t.encode('utf-8') + b'\0'
        while len(blob) % 4:
            blob += b'\0'
        def rec(fmt, rows):
            return b''.join(struct.pack(fmt, *r) for r in rows)
        nan = float('nan')
        locs = []
        for L in self.locs:
            flags = 1 if L['heading'] is not None else 0
            locs.append((L['name'], L['type'], L['ntrig'], L['first'], L['file'], L['data'], *L['pos'],
                         L['heading'] if L['heading'] is not None else nan, flags, 0))
        trigs = [(k, *c, *h, *a[0], *a[1], *a[2]) for k, c, h, a in self.trigs]
        args = []
        for t, loc, v in self.args:
            if t == ARG_FLOAT:
                args.append(struct.pack('<BBHf', t, 0, loc, v))
            elif t == ARG_INT:
                args.append(struct.pack('<BBHi', t, 0, loc, v))
            else:
                args.append(struct.pack('<BBHI', t, 0, loc, v))
        L = self.level
        sections = [
            (struct.pack(f'<{len(offs)}I', *offs), len(offs)),
            (bytes(blob), len(blob)),
            (rec('<IBBHII3ffII', locs), len(locs)),
            (rec('<I3f3f9f', trigs), len(trigs)),
            (b''.join(args), len(args)),
            (rec('<HBBIHH', self.cmds), len(self.cmds)),
            (rec('<IIIHHHHIII', self.missions), len(self.missions)),
            (rec('<IIIHHIHHHHI', self.phases), len(self.phases)),
            (rec('<IHHhHIIIBBBBHHI4x', self.stages), len(self.stages)),
            (rec('<BBHIi', self.conds), len(self.conds)),
            (rec('<IIHH', self.convs), len(self.convs)),
            (rec('<IIHBB', self.clips), len(self.clips)),
            (struct.pack('<8I', *L), 1),
            (struct.pack(f'<{len(self.files)}I', *self.files), len(self.files)),
        ]
        texts = sorted(self.texts.items())
        tx = [(self.s(k), self.s(v)) for k, v in texts]
        sections.append((struct.pack(f'<{2 * len(tx)}I', *[x for kv in tx for x in kv]), len(tx)))   # MSN_SEC_TEXTS
        sections.append((b'', 0))                                                                  # MSN_SEC_SPARE
        # strings added above must be in the blob: rebuild the two string sections
        blob = bytearray(); offs = []
        for t in self.strings:
            offs.append(len(blob)); blob += t.encode('utf-8') + b'\0'
        while len(blob) % 4:
            blob += b'\0'
        sections[0] = (struct.pack(f'<{len(offs)}I', *offs), len(offs))
        sections[1] = (bytes(blob), len(blob))
        header_size = 16 + 8 * len(sections)
        out = bytearray(header_size)
        table = []
        for data, count in sections:
            while len(out) % 16:
                out += b'\0'
            table.append((len(out), count)); out += data
        struct.pack_into('<4sHHII', out, 0, b'MSN1', VERSION, level_no, len(out), 1)
        for k, (o, c) in enumerate(table):
            struct.pack_into('<II', out, 16 + 8 * k, o, c)
        return bytes(out)

# ------------------------------------------------------------------ compiler
class Compiler:
    def __init__(self, level, strict=False):
        self.level = level; self.strict = strict
        self.dir = GAME / f'scripts/missions/level{level:02d}'
        self.files = FileIndex(GAME)
        self.sig = CMD.load_signatures(ROOT)
        self.get = bible()
        self.convidx = conversation_index()
        self.warn = []; self.err = []; self.stats = Counter()
        self.pack = Pack()
        self.used_ops = Counter()
        self._load_world()

    # ---- diagnostics
    def W(self, where, msg):
        self.warn.append(f'{where}: {msg}')
    def E(self, where, msg):
        self.err.append(f'{where}: {msg}')

    # ---- locators
    def _load_world(self):
        lv = self.level
        self.p3d_locs = {}   # rel path (lower) -> {name_lower: Locator}
        self.interior_doors = {}  # interior name (lower) -> door locator (type 7)
        cands = sorted((GAME / 'art').glob(f'l{lv}*.p3d')) + sorted((GAME / f'art/missions/level{lv:02d}').glob('*.p3d')) + \
                sorted((GAME / 'art/missions/generic').glob('*.p3d'))
        for p in cands:
            rel = str(p.relative_to(GAME)).lower()
            locs, err = read_locators(p, rel)
            if err:
                self.W('p3d', err)
            d = {}
            for L in locs:
                d.setdefault(L.name.lower(), L)
                if L.type == 7 and isinstance(L.data, str):
                    self.interior_doors.setdefault(L.data.lower(), L)
            self.p3d_locs[rel] = d
        self.world_files = [r for r in self.p3d_locs if r.startswith(f'art/l{lv}')]
        self.fallback_files = list(self.p3d_locs)

    def loaded_p3d(self, calls):
        out = []
        for c in calls:
            if c.op == 'LoadP3DFile' and c.args:
                rel = str(c.args[0]).replace('\\', '/').lower()
                if rel in self.p3d_locs:
                    out.append(rel)
        return out

    def resolve(self, name, scope, where):
        key = str(name).lower()
        if key in ('', 'null', 'none'):
            return None
        for rel in scope:
            L = self.p3d_locs.get(rel, {}).get(key)
            if L:
                return self.pack.loc(L)
        for rel in self.fallback_files:
            L = self.p3d_locs[rel].get(key)
            if L:
                self.stats['loc_fallback'] += 1
                return self.pack.loc(L)
        self.stats['loc_missing'] += 1
        self.W(where, f'locator "{name}" not found in level p3d files')
        return None

    # ---- validation of one call
    def check(self, c):
        where = f'{c.file}:{c.line}'
        spec = CMD.BY_NAME.get(c.op.lower())
        if not spec:
            self.E(where, f'unknown command {c.op}'); return None
        if spec.name != c.op:
            self.W(where, f'command case {c.op} -> {spec.name}')
        mn, variants = self.sig.get(spec.name, (0, [[]]))
        n = len(c.args)
        mx = max(len(v) for v in variants)
        if n < mn or n > mx:
            self.E(where, f'{spec.name}: {n} args, expected {mn}..{mx}')
        else:
            ok = False
            for v in variants:
                if n > len(v):
                    continue
                good = True
                for a, t in zip(c.args, v):
                    if t in ('Int', 'Int64', 'Float') and isinstance(a, str):
                        try:
                            float(a)
                        except ValueError:
                            if not (t == 'Int' and a.lower() in ('true', 'false')):
                                good = False
                    if t == 'Int' and isinstance(a, float) and not a.is_integer():
                        good = False
                if good:
                    ok = True; break
            if not ok:
                self.W(where, f'{spec.name}: argument types {c.args} do not match {variants}')
        self.used_ops[spec.name] += 1
        return spec

    def loc_args(self, spec, c, scope, objective=None):
        idx = list(spec.locs)
        if spec.name == 'GagSetPosition':
            idx = [0] if len(c.args) == 1 else []
        if spec.name == 'GagSetTrigger':
            idx = [1] if len(c.args) == 3 else []
        if spec.name == 'SetCamBestSide':
            idx = [0]
        out = {}
        for k in idx:
            if k >= len(c.args) or not isinstance(c.args[k], str):
                continue
            where = f'{c.file}:{c.line}'
            if spec.name == 'SetDestination' and objective == 'interior' and k == 0:
                nm = str(c.args[0]).lower()
                L = self.interior_doors.get(nm) or self.interior_doors.get(nm.rstrip('0123456789'))  # "moe1" -> door "moe" 
                if L:
                    out[k] = self.pack.loc(L)
                else:
                    self.W(where, f'no interior door locator for "{c.args[0]}"')
                continue
            r = self.resolve(c.args[k], scope, where)
            if r is not None:
                out[k] = r
        return out

    def check_refs(self, c, spec):
        """Existence checks for files named by commands."""
        where = f'{c.file}:{c.line}'
        a = c.args
        def need(rel, what):
            if not self.files.find(rel):
                self.W(where, f'{what} not on disc: {rel}'); self.stats['missing_file'] += 1
        if spec.name in ('LoadP3DFile', 'GagBegin') and a:
            rel = str(a[0]).replace('\\', '/')
            if spec.name == 'GagBegin':
                if not self.files.base.get(rel.split('/')[-1].lower()):
                    self.W(where, f'gag file not found: {a[0]}'); self.stats['missing_file'] += 1
                return
            need(rel, 'p3d')
        if spec.name == 'LoadDisposableCar' and a:
            need(str(a[0]), 'car p3d')
        if spec.name in ('AddStageVehicle', 'InitLevelPlayerVehicle') and len(a) > 3 and str(a[3]) and str(a[3]).upper() != 'NULL':
            need('scripts/cars/' + str(a[3]).replace('\\', '/'), '.con')
        if spec.name == 'AddStageVehicle' and a:
            need(f'art/cars/{a[0]}.p3d', 'car')
        if spec.name in ('SetFMVInfo', 'GagPlayFMV', 'SetPostLevelFMV') and a:
            need('movies/' + str(a[0]), 'fmv')
        if spec.name == 'SetPresentationBitmap' and a:
            need(str(a[0]), 'presentation bitmap')

    # ---- conversations
    def conversation(self, name, mission_code, where):
        name = str(name).lower()
        clips = self.convidx.get((name, self.level, mission_code))
        key = mission_code
        if not clips:
            clips = self.convidx.get((name, self.level, '')); key = ''
        if not clips:
            # some lines are recorded for a sibling mission code (e.g. bonus intro "intro" = l1r1)
            cands = [k for k in self.convidx if k[0] == name and k[1] == self.level]
            if len(cands) == 1:
                clips = self.convidx[cands[0]]; key = cands[0][2]
        if not clips:
            clips = self.convidx.get((name, 0, ''))
            key = ''
        if not clips:
            self.W(where, f'conversation "{name}" ({mission_code}) has no audio on disc'); self.stats['conv_missing'] += 1
            return None
        return self.pack.conv(name, key, clips)

    # ---- emit a flat list of calls (level scripts, load scripts, phase setup)
    def emit_flat(self, calls, scope, ctx=CTX_BODY):
        first = len(self.pack.cmds)
        for c in calls:
            spec = self.check(c)
            if not spec:
                continue
            self.check_refs(c, spec)
            locs = self.loc_args(spec, c, scope)
            self.pack.cmd(spec.op, c.args, ctx, c.line, c.file, locs)
        return first, len(self.pack.cmds) - first

    # ---- one mission phase (xxi.mfk)
    def phase(self, calls, name, file, scope, load_range, mission_code, flags=0):
        P = self.pack
        setup = []; i = 0
        while i < len(calls) and calls[i].op != 'AddStage':
            if calls[i].op not in ('CloseMission',):
                setup.append(calls[i])
            i += 1
        sel = [c for c in setup if c.op == 'SelectMission']
        sname = str(sel[0].args[0]) if sel else name
        s_first, s_count = self.emit_flat(setup, scope)
        first_stage = len(P.stages)
        stage = None
        rest = calls[i:]
        for c in rest:
            where = f'{c.file}:{c.line}'
            spec = self.check(c)
            if not spec:
                continue
            if c.op == 'CloseMission':
                if stage:
                    self.W(where, 'CloseMission inside open stage; closing it'); self._close_stage(stage, where, mission_code)
                    stage = None
                self.pack.cmd(spec.op, c.args, CTX_BODY, c.line, c.file)
                continue
            if c.op == 'AddStage':
                if stage:
                    self.W(where, 'AddStage without CloseStage; closing previous'); self._close_stage(stage, where, mission_code)
                stage = dict(first=len(P.cmds), args=list(c.args), obj=None, objcmd=None, obj_open=False, conds=[], flags=0,
                             msg=0xFFFF, icon=0, lock=(0, 0), line=c.line, file=c.file, objtype=None, objmode='', cond=None)
                a = [str(x).lower() for x in c.args]
                num = next((x for x in c.args if isinstance(x, int)), None)
                if num is None and c.args and isinstance(c.args[0], str) and re.fullmatch(r'\d+', str(c.args[0])):
                    num = int(c.args[0])
                stage['num'] = -1 if num is None else num
                if 'final' in a:
                    stage['flags'] |= SF_FINAL
                if a and a[0] == 'locked':
                    stage['flags'] |= SF_LOCKED
                    kind = 1 if len(a) > 1 and a[1] == 'car' else 2
                    stage['lock'] = (kind, P.s(str(c.args[2]) if len(c.args) > 2 else ''))
                P.cmd(spec.op, c.args, CTX_BODY, c.line, c.file)
                continue
            if stage is None:
                if c.op in ('CloseStage', 'CloseCondition', 'CloseObjective'):
                    self.W(where, f'stray {c.op} outside a stage; ignored')
                else:
                    self.W(where, f'{c.op} outside a stage; kept as setup-after-stages')
                    P.cmd(spec.op, c.args, CTX_BODY, c.line, c.file)
                continue
            if c.op == 'CloseStage':
                if stage['cond']:
                    self.W(where, 'CloseStage with open condition; auto-closed')
                    self._end_cond(stage)
                if stage['obj_open']:
                    self.W(where, 'CloseStage with open objective; auto-closed')
                P.cmd(spec.op, c.args, CTX_BODY, c.line, c.file)
                self._close_stage(stage, where, mission_code)
                stage = None
                continue
            if c.op == 'AddObjective':
                if stage['objtype'] is not None:
                    self.E(where, 'second objective in one stage')
                t = str(c.args[0]).lower() if c.args else ''
                if t not in CMD.OBJECTIVES:
                    self.E(where, f'unknown objective type "{t}"')
                stage['objtype'] = t; stage['objmode'] = [str(x) for x in c.args[1:]]
                stage['objcmd'] = len(P.cmds) - stage['first']
                stage['obj_open'] = True
                P.cmd(spec.op, c.args, CTX_OBJ, c.line, c.file)
                continue
            if c.op == 'CloseObjective':
                if stage['cond']:
                    self.W(where, 'CloseObjective with open condition; condition auto-closed'); self._end_cond(stage)
                if not stage['obj_open']:
                    self.W(where, 'CloseObjective without open objective; ignored')
                    continue
                P.cmd(spec.op, c.args, CTX_OBJ, c.line, c.file)
                stage['obj_open'] = False
                continue
            if c.op == 'AddCondition':
                if stage['cond']:
                    self.W(where, 'AddCondition inside open condition; auto-closed'); self._end_cond(stage)
                t = str(c.args[0]).lower() if c.args else ''
                if t not in CMD.CONDITIONS:
                    self.E(where, f'unknown condition type "{t}"')
                if stage['obj_open']:
                    self.stats['cond_in_objective'] += 1  # level07 destroyboss: condition nested in the objective
                stage['cond'] = dict(type=CMD.CONDITIONS.index(t) + 1 if t in CMD.CONDITIONS else 0,
                                     first=len(P.cmds), arg=int(c.args[1]) if len(c.args) > 1 and isinstance(c.args[1], int) else 0)
                P.cmd(spec.op, c.args, CTX_COND, c.line, c.file)
                continue
            if c.op == 'CloseCondition':
                if not stage['cond']:
                    self.W(where, 'CloseCondition without AddCondition (original script bug); ignored')
                    continue
                P.cmd(spec.op, c.args, CTX_COND, c.line, c.file)
                self._end_cond(stage)
                continue
            # ordinary command inside the stage
            if c.op == 'RESET_TO_HERE':
                stage['flags'] |= SF_CHECKPOINT
            elif c.op == 'ShowStageComplete':
                stage['flags'] |= SF_SHOWCOMPLETE
            elif c.op == 'MustActionTrigger':
                stage['flags'] |= SF_MUSTACTION
            elif c.op == 'TurnGotoDialogOff':
                stage['flags'] |= SF_GOTODIALOGOFF
            elif c.op == 'UseElapsedTime':
                stage['flags'] |= SF_ELAPSED
            elif c.op == 'SetStageMusicAlwaysOn':
                stage['flags'] |= SF_MUSICON
            elif c.op == 'SetStageMessageIndex' and len(c.args) > 1 and str(c.args[1]).lower() == 'end':
                self.stats['msg_at_end'] += 1   # level01/m0i.mfk dialogue: message for after the stage, not the objective
            elif c.op == 'SetStageMessageIndex' and c.args:
                try:
                    stage['msg'] = int(c.args[0])
                except (TypeError, ValueError):
                    self.W(where, f'bad message index {c.args[0]}')
            elif c.op == 'SetHUDIcon' and c.args:
                stage['icon'] = P.s(str(c.args[0]))
            ctx = CTX_COND if stage['cond'] else (CTX_OBJ if stage['obj_open'] else CTX_BODY)
            if ctx == CTX_COND and spec.name not in ('SetCondTime', 'SetCondMinHealth', 'SetCondTargetVehicle',
                                                    'SetFollowDistances', 'SetConditionPosition'):
                # level07: SetObjTargetBoss inside AddCondition("damage") inside the objective;
                # m7i.mfk: ShowStageComplete after an unclosed AddCondition("keepbarrel",2). Kept in place: the runtime
                # reads stage-level parameters from the whole stage, condition parameters from the condition range.
                self.stats['foreign_in_condition'] += 1
            self.check_refs(c, spec)
            locs = self.loc_args(spec, c, scope, stage['objtype'])
            P.cmd(spec.op, c.args, ctx, c.line, c.file, locs)
            if spec.name in ('SetDialogueInfo',) and len(c.args) > 2:
                self.conversation(c.args[2], mission_code, where)
            if spec.name == 'SetCompletionDialog' and c.args:
                self.conversation(c.args[0], mission_code, where)
            if spec.name == 'AddCollectible' and len(c.args) > 2:
                self.stats['collectible_lines'] += 1
        if stage:
            self.W(f'{file}', 'script ends with open stage; closed'); self._close_stage(stage, file, mission_code)
        n = len(P.stages) - first_stage
        if n == 0:
            self.E(file, 'phase without stages')
        P.phases.append((P.s(sname), P.s(file), s_first, s_count, load_range[1], load_range[0], first_stage, n, flags, 0, 0))
        return len(P.phases) - 1

    def _end_cond(self, stage):
        cd = stage['cond']
        if cd:
            n = len(self.pack.cmds) - cd['first']
            stage['conds'].append((cd['type'], 0, n, cd['first'], cd['arg']))
        stage['cond'] = None

    def _close_stage(self, stage, where, mission_code):
        P = self.pack
        if stage['cond']:
            self._end_cond(stage)
        if stage['objtype'] is None:
            self.E(where, 'stage without objective')
        cond_first = len(P.conds)
        P.conds.extend(stage['conds'])
        t = stage['objtype'] or ''
        otype = CMD.OBJECTIVES.index(t) + 1 if t in CMD.OBJECTIVES else 0
        mode = stage['objmode'] if isinstance(stage['objmode'], list) else []
        nav = 0; objarg = 0
        for m in mode:
            ml = m.lower()
            if ml in NAV_MODES:
                nav = NAV_MODES[ml]
            elif ml == 'niether':
                pass   # круг 3 (проверка g07b, P16-07): опечатка в level06/m4i.mfk оригиналом не распознаётся — режим стрелки по умолчанию (both), не neither
            elif ml == 'gamble':
                stage['flags'] |= 0  # gamble race: runtime reads obj_arg
                objarg = P.s('gamble')
            else:
                objarg = P.s(m)
        text = 0
        if stage['msg'] != 0xFFFF:
            t_ = self.get(f'MISSION_OBJECTIVE_{stage["msg"]:02d}')
            if t_ is None:
                self.W(where, f'objective text {stage["msg"]} not in text bible')
            else:
                text = P.s(t_)
        count = len(P.cmds) - stage['first']
        P.stages.append((stage['first'], count, stage['flags'], stage['num'], stage['msg'], text, stage['icon'],
                         stage['lock'][1], stage['lock'][0], otype, len(stage['conds']), nav, cond_first,
                         stage['objcmd'] if stage['objcmd'] is not None else 0xFFFF, objarg))
        self.stats['stages'] += 1

    # ---- level
    def compile(self):
        lv = self.level; P = self.pack
        def rd(name):
            p = self.dir / name
            if not p.exists():
                return None
            calls, w = mfk.parse_file(p, f'level{lv:02d}/{name}')
            for x in w:
                self.W('parse', x)
            return calls
        level = rd('level.mfk'); leveli = rd('leveli.mfk')
        # Some script files on the extracted disc carry another file's content (a 0-byte file and a neighbour
        # swapped names: e.g. level06/m8sdl.mfk holds SelectMission("sr1") while sr1i.mfk is empty). Mission
        # scripts are therefore matched by their SelectMission() id, not by file name.
        self.by_select = {}
        for p in sorted(self.dir.glob('*.mfk')):
            calls, _ = mfk.parse_file(p, f'level{lv:02d}/{p.name}')
            sel = next((str(c.args[0]).lower() for c in calls if c.op == 'SelectMission' and c.args), None)
            if sel:
                self.by_select.setdefault(sel, []).append((p.name, calls))
        level_scope = self.loaded_p3d(level) + self.world_files
        lf, lc = self.emit_flat(level, level_scope)
        init_scope = level_scope
        inf, inc = self.emit_flat(leveli, init_scope)
        rewards_all, w = mfk.parse_file(GAME / 'scripts/missions/rewards.mfk', 'rewards.mfk')
        # rewards.mfk is global: keep this level's BindReward + SetTotalGags, and every SetCarAttributes (menu stats)
        rewards = [c for c in rewards_all if (c.op == 'BindReward' and len(c.args) > 4 and int(c.args[4]) == lv)
                   or (c.op == 'SetTotalGags' and c.args and int(c.args[0]) == lv) or c.op == 'SetCarAttributes']
        rf, rc = self.emit_flat(rewards, [])
        # bonus-mission givers (leveli)
        bonus_npc = {}
        for k in range(inf, inf + inc):
            op = P.cmds[k][0]
            if op == CMD.BY_NAME['addnpccharacterbonusmission'].op:
                a0 = P.cmds[k][3]
                mission = P.strings[P.args[a0 + 3][2]]
                bonus_npc.setdefault(mission.lower(), k)
        schedule = [(str(c.args[0]), c.op == 'AddBonusMission') for c in level if c.op in ('AddMission', 'AddBonusMission')]
        n_story = 0
        for sched, (name, bonus) in enumerate(schedule):
            where = f'level{lv:02d}/{name}'
            flags = 0
            if bonus:
                flags |= MF_BONUS
                if name.startswith('sr'): flags |= MF_STREETRACE
                if name.startswith('gr'): flags |= MF_GAMBLE
                if name.startswith('bm'): flags |= MF_BONUSMISSION
            else:
                n_story += 1
            code = name.lower()
            title_key = f'MISSION_TITLE_L{lv}_M{TITLE_NUM.get(code, int(code[1:]) if code[1:].isdigit() else 0)}'
            info_key = title_key.replace('TITLE', 'INFO')
            title = self.get(title_key) or ''; info = self.get(info_key) or ''
            main_calls = self.script_for(code, f'{name}i.mfk', where)
            if main_calls is None:
                # e.g. level03 "ismovie": the Itchy & Scratchy cinema lives in executable code, no script
                flags |= MF_NOSCRIPT
                P.missions.append((P.s(code), P.s(title or name), P.s(info), flags, sched, 0xFFFF, 0xFFFF, 0xFFFFFFFF, 0, 0))
                self.W(where, 'no script on disc (engine-coded mission); flagged NOSCRIPT')
                continue
            load = self.load_script(f'{name}l.mfk', where)
            intro_load = self.load_script(f'{name}sdl.mfk', where)
            intro_calls = self.script_for(code + 'sd', f'{name}sdi.mfk', where)
            for c in main_calls + (intro_calls or []):
                if c.op == 'SetLevelOver': flags |= MF_LEVELOVER
                if c.op == 'SetGameOver': flags |= MF_GAMEOVER
                if c.op == 'SetForcedCar': flags |= MF_FORCEDCAR
            main_scope = self.loaded_p3d(load) + level_scope
            intro_scope = self.loaded_p3d(intro_load) + level_scope
            intro_ph = 0xFFFF
            if intro_calls:
                il = self.emit_flat(intro_load, intro_scope)
                intro_ph = self.phase(intro_calls, code + 'sd', f'level{lv:02d}/{name}sdi.mfk', intro_scope, il, code, PF_INTRO)
            elif bonus and code in bonus_npc:
                intro_ph = self.synth_bonus_intro(code, leveli, level_scope)
            ml = self.emit_flat(load, main_scope)
            main_ph = self.phase(main_calls, code, f'level{lv:02d}/{name}i.mfk', main_scope, ml, code, 0)
            # transition mission (single timer stage, e.g. L2 m8): web calls it "Level transition"
            ph = P.phases[main_ph]
            if ph[7] == 1 and P.stages[ph[6]][9] == CMD.OBJECTIVES.index('timer') + 1 and code == 'm8':
                flags |= MF_TRANSITION      # end-of-level movie + timer; its title key M8 belongs to street race 1
                title = info = ''
            npc = bonus_npc.get(code, 0xFFFFFFFF)
            P.missions.append((P.s(code), P.s(title or name), P.s(info), flags, sched, intro_ph, main_ph, npc, 0, 0))
            self.stats['missions'] += 1
        P.level = (lf, lc, inf, inc, rf, rc, n_story, 0)
        # UI texts: fixed keys, failure reasons + hints, countdown steps (AddToCountdownSequence args are bible keys)
        keys = list(UI_TEXT_KEYS)
        for r in FAIL_TEXT_KEYS:
            keys.append(f'MISSION_FAILED_{r}'); keys += [f'MISSION_FAILED_{r}_HINT{i}' for i in range(8)]
        cd_op = CMD.BY_NAME['addtocountdownsequence'].op
        for c in P.cmds:
            if c[0] == cd_op and c[1] > 0 and P.args[c[3]][0] in (ARG_STR, ARG_BARE):
                keys.append(P.strings[P.args[c[3]][2]])
        for k in keys:
            v = self.get(k)
            if v is not None:
                P.texts[k] = v.replace('\\', '\n')      # the bible uses a backslash as line break
            elif k.startswith('MISSION_FAILED_') and '_HINT' not in k:
                self.W('text', f'bible key {k} not found')
        # правка 122 (сверка 30.09, запись PS2 26:50): окно «Congratulations! You've unlocked Cletus' Pickup Truck. Go to a phone booth
        # to use it.» — машина-награда бонусной миссии / уличных гонок уровня. Ключей этих предложений в библии не знаем (хеши) —
        # по содержанию: название машины (ключ <МОДЕЛЬ>) внутри предложения, иначе общее «… a new vehicle …». Ключ TEXTS: UNLOCK_<модель>
        congr = [v for v in getattr(self.get, 'values', {}).values() if v.startswith("Congratulations! You've unlocked")]
        generic = next((v for v in congr if 'a new vehicle' in v), None)
        for c in rewards:
            if c.op != 'BindReward' or len(c.args) < 5 or str(c.args[2]).lower() != 'car' or str(c.args[3]).lower() not in ('bonusmission', 'streetrace'):
                continue
            model = str(c.args[0]); disp = self.get(model.upper()) or ''
            v = next((x for x in congr if disp and disp.lower() in x.lower()), generic)
            if v: P.texts['UNLOCK_' + model.lower()] = v
        # every bonus-giver conversation
        for k in bonus_npc.values():
            a0 = P.cmds[k][3]
            conv = P.strings[P.args[a0 + 5][2]]; mission = P.strings[P.args[a0 + 3][2]].lower()
            self.conversation(conv, mission, f'level{lv:02d}/leveli.mfk')
        return P.build(lv)

    def script_for(self, sel, canonical, where):
        """Init script of mission/phase `sel` (e.g. "m1", "m1sd"): by SelectMission id, canonical file name preferred."""
        cands = self.by_select.get(sel, [])
        if not cands:
            p = self.dir / canonical
            if p.exists() and p.stat().st_size == 0:
                self.W(where, f'{canonical} is empty on the extracted disc and no other file selects "{sel}"')
            return None
        pick = next((c for c in cands if c[0].lower() == canonical.lower()), cands[0])
        if pick[0].lower() != canonical.lower():
            self.W(where, f'script "{sel}" found in {pick[0]} instead of {canonical} (file names swapped on the extracted disc)')
            self.stats['swapped_files'] += 1
        return pick[1]

    def load_script(self, name, where):
        p = self.dir / name
        if not p.exists():
            return []
        calls, w = mfk.parse_file(p, f'level{self.level:02d}/{name}')
        if any(c.op == 'SelectMission' for c in calls):
            self.W(where, f'{name} holds a mission script, not a load list (swapped on the disc); load list treated as empty')
            return []
        return calls

    def synth_bonus_intro(self, code, leveli, scope):
        """The original starts a bonus mission with a dialogue with its giver (done in engine code).
        We express it as a normal one-stage phase built from leveli.mfk data."""
        P = self.pack
        npc = next((c for c in leveli if c.op == 'AddNPCCharacterBonusMission' and str(c.args[3]).lower() == code), None)
        pos = next((c for c in leveli if c.op == 'SetBonusMissionDialoguePos' and str(c.args[0]).lower() == code), None)
        if not npc or not pos:
            return 0xFFFF
        file = npc.file
        def mk(op, args, line):
            return mfk.Call(op, [mfk.Str(a) if isinstance(a, str) else a for a in args], line, file)
        player = next((str(c.args[0]) for c in leveli if c.op == 'AddCharacter'), 'homer')
        calls = [mk('SelectMission', [code + 'sd'], npc.line), mk('AddStage', [0], npc.line), mk('AddObjective', ['dialogue'], npc.line),
                 mk('AddNPC', [str(npc.args[0]), str(npc.args[2])], npc.line),
                 mk('SetDialogueInfo', [player, str(npc.args[0]), str(npc.args[5]), 0], npc.line),
                 mk('SetDialoguePositions', [str(x) for x in pos.args[1:]], pos.line)]
        for c in leveli:
            if c.op in ('SetConversationCam', 'AddAmbientPcAnimation', 'AddAmbientNpcAnimation', 'SetCamBestSide', 'ClearAmbientAnimations') \
                    and c.args and str(c.args[-1]).lower() == code:
                if c.op == 'ClearAmbientAnimations':
                    continue
                calls.append(mfk.Call(c.op, c.args[:-1], c.line, file))
        calls += [mk('CloseObjective', [], npc.line), mk('CloseStage', [], npc.line), mk('CloseMission', [], npc.line)]
        return self.phase(calls, code + 'sd', file + f'#{code}', scope, (len(P.cmds), 0), code, PF_INTRO | PF_SYNTHETIC)

# ------------------------------------------------------------------ opcode header generation
def gen_c_tables(out_dir):
    feats = list(CMD.FEATURES)
    h = ['/* GENERATED by tools/missions/compile.py --gen-c from tools/missions/commands.py. Do not edit. */',
         '#ifndef MSN_OPS_H', '#define MSN_OPS_H', '',
         f'#define MSN_OP_COUNT {len(CMD.SPECS) + 1}', '', 'enum MsnOp {', '    MSN_OP_NONE = 0,']
    for s in CMD.SPECS:
        h.append(f'    MSN_OP_{s.name.upper()} = {s.op},')
    h += ['};', '', '/* route: how the runtime treats an opcode */',
          'enum { MSN_ROUTE_RT = 1, MSN_ROUTE_ENG = 2, MSN_ROUTE_BOTH = 3 };',
          '/* flags */', 'enum { MSN_OPF_END = 1, MSN_OPF_REPLAY = 2 };', '', 'enum MsnFeature {']
    for i, f in enumerate(feats):
        h.append(f'    MSN_FEAT_{f.upper()} = {i},')
    h += [f'    MSN_FEAT_COUNT = {len(feats)}', '};', '',
          'typedef struct { const char *name; unsigned char route, flags, feature, pad; } MsnOpInfo;',
          'extern const MsnOpInfo msn_opinfo[MSN_OP_COUNT];',
          'extern const char *const msn_feature_name[MSN_FEAT_COUNT];', '', '#endif', '']
    c = ['/* GENERATED by tools/missions/compile.py --gen-c from tools/missions/commands.py. Do not edit. */',
         '#include "msn_ops.h"', '', 'const MsnOpInfo msn_opinfo[MSN_OP_COUNT] = {', '    { "", 0, 0, 0, 0 },']
    for s in CMD.SPECS:
        route = {'rt': 'MSN_ROUTE_RT', 'eng': 'MSN_ROUTE_ENG', 'both': 'MSN_ROUTE_BOTH'}[s.route]
        fl = ' | '.join([x for x, on in (('MSN_OPF_END', s.when == 'end'), ('MSN_OPF_REPLAY', s.replay)) if on]) or '0'
        c.append(f'    {{ "{s.name}", {route}, {fl}, MSN_FEAT_{s.feature.upper()}, 0 }},')
    c += ['};', '', 'const char *const msn_feature_name[MSN_FEAT_COUNT] = {']
    for f in feats:
        c.append(f'    "{f}",')
    c += ['};', '']
    (out_dir / 'msn_ops.h').write_text('\n'.join(h))
    (out_dir / 'msn_ops.c').write_text('\n'.join(c))

def dump_json(pack, path):
    P = pack
    S = lambda i: P.strings[i]
    def args(k):
        op, n, ctx, a0, line, f = P.cmds[k]
        out = []
        for t, loc, v in P.args[a0:a0 + n]:
            val = S(v) if t in (ARG_STR, ARG_BARE) else v
            if loc:
                L = P.locs[loc - 1]
                val = {'v': val, 'loc': S(L['name']), 'pos': [round(x, 2) for x in L['pos']], 'file': S(L['file'])}
            out.append(val)
        return {'op': CMD.SPECS[op - 1].name, 'args': out, 'ctx': ctx, 'src': f'{S(P.files[f])}:{line}'}
    missions = []
    for m in P.missions:
        md = {'id': S(m[0]), 'title': S(m[1]), 'flags': m[3], 'phases': []}
        for ph_i in (m[5], m[6]):
            if ph_i == 0xFFFF:
                continue
            ph = P.phases[ph_i]
            pd = {'name': S(ph[0]), 'file': S(ph[1]), 'setup': [args(k) for k in range(ph[2], ph[2] + ph[3])],
                  'load': [args(k) for k in range(ph[5], ph[5] + ph[4])], 'stages': []}
            for st in P.stages[ph[6]:ph[6] + ph[7]]:
                pd['stages'].append({'obj': CMD.OBJECTIVES[st[9] - 1] if st[9] else '?', 'flags': st[2], 'num': st[3], 'msg': st[4],
                                     'text': S(st[5]), 'icon': S(st[6]), 'lock': S(st[7]),
                                     'conds': [CMD.CONDITIONS[c[0] - 1] for c in P.conds[st[12]:st[12] + st[10]]],
                                     'cmds': [args(k) for k in range(st[0], st[0] + st[1])]})
            md['phases'].append(pd)
        missions.append(md)
    convs = {f'{S(c[0])}/{S(c[1])}': [S(P.clips[i][0]) for i in range(c[2], c[2] + c[3])] for c in P.convs}
    path.write_text(json.dumps({'missions': missions, 'conversations': convs,
                                'locators': [{'name': S(L['name']), 'type': L['type'], 'pos': L['pos'], 'file': S(L['file']), 'ntrig': L['ntrig']} for L in P.locs]},
                               indent=1, ensure_ascii=False))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--level', type=int, action='append')
    ap.add_argument('--all', action='store_true')
    ap.add_argument('--out', default=str(ROOT / 'build/missions'))
    ap.add_argument('--scratch', default=None, help='where to write JSON dumps and reports')
    ap.add_argument('--gen-c', action='store_true', help='regenerate viewer/mission/msn_ops.[ch]')
    ap.add_argument('-q', '--quiet', action='store_true')
    ap.add_argument('--shr', default=str(ROOT / 'build/missions.shr'),
                    help='level 1 pack is also written here (the file the engine loads); "" = off')
    a = ap.parse_args()
    if a.gen_c:
        gen_c_tables(ROOT / 'viewer/mission'); print('generated viewer/mission/msn_ops.[ch]')
    levels = list(range(1, 8)) if a.all else (a.level or ([] if a.gen_c else [1]))
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    scratch = Path(a.scratch) if a.scratch else None
    if scratch:
        scratch.mkdir(parents=True, exist_ok=True)
    total_err = 0; all_ops = Counter()
    for lv in levels:
        c = Compiler(lv)
        data = c.compile()
        (out / f'level{lv:02d}.msn').write_bytes(data)
        if lv == 1 and a.shr:
            Path(a.shr).write_bytes(data)
        P = c.pack
        all_ops.update(c.used_ops)
        summary = (f'level {lv}: {len(data)} bytes, missions {len(P.missions)}, phases {len(P.phases)}, stages {len(P.stages)}, '
                   f'cmds {len(P.cmds)}, args {len(P.args)}, strings {len(P.strings)}, locators {len(P.locs)} (trig {len(P.trigs)}), '
                   f'convs {len(P.convs)} (lines {len(P.clips)}), distinct commands {len(c.used_ops)}, '
                   f'errors {len(c.err)}, warnings {len(c.warn)}, stats {dict(c.stats)}')
        print(summary)
        if scratch:
            dump_json(P, scratch / f'level{lv:02d}.json')
            (scratch / f'level{lv:02d}.report.txt').write_text(summary + '\n\nERRORS\n' + '\n'.join(c.err) + '\n\nWARNINGS\n' + '\n'.join(c.warn) + '\n')
        if not a.quiet:
            for e in c.err:
                print('  ERROR', e)
            for w in c.warn[:12]:
                print('  warn ', w)
            if len(c.warn) > 12:
                print(f'  ... {len(c.warn) - 12} more warnings' + (f' (see {scratch}/level{lv:02d}.report.txt)' if scratch else ''))
        total_err += len(c.err)
    if levels:
        print(f'distinct commands compiled across levels: {len(all_ops)}; errors {total_err}')
    sys.exit(1 if total_err else 0)

if __name__ == '__main__':
    main()
