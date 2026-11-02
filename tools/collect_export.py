#!/usr/bin/env python3
"""Карточки, осы и гэги уровня (пункт В5, уровень 1 на 100%) -> пакет CLT1 рядом с EBOOT (viewer/collect.c + main.c).

  python3 tools/collect_export.py [--level 1] [--out build/collect-l1.shr] [--json]

Только данные диска Роба (hit-and-run-web/source/game, чтение). Опоры: hit-and-run-web/tools (MIT: p3d, convert),
tools/prp2_export.py (стейтпропы -> SPR1: скелет, PTRN, PVIS, билборды, окна EFX, коллизия), tools/stateprop_dump.py
(машины состояний StateProp).

Откуда что:
  карточки  — action-локаторы (вид 9) уровня с событием CollectorCard; номер карточки — строка 0 данных локатора (card11..17:
              на диске у l1r3 локатор назван card13, а в данных card14 — берём данные), радиус — сфера его TriggerVolume;
              модель и вращение — составной рисунок card_idle из art/cards.p3d (сустав card_idle_geo крутится и качается,
              121 кадр @30); название на HUD — строка CardTitle Scrooby: (уровень-1)*8 + (номер-1).
  осы       — локаторы w_* из art/missions/level0N/wasps.p3d; какие живут — AddSpawnPointByLocatorScript в leveli.mfk
              (5-й аргумент — радиус полёта от точки, 6-й — дистанция появления); поведение — AddBehaviour (ATTRACTION
              min/max/скорость км/ч, ATTACK_PLAYER дальность/дуга/интервал уклонения; у именных ос -1 = стоят на месте);
              PreallocateActors("beecamera", N) — живых одновременно; модель и 6 состояний — StateProp beecamera из
              art/l0N_fx.p3d (появление 1.25 с, полёт, заряд 2.67 с, заряжена, выстрел, уничтожена: 20+5 монет).
  гэги      — блоки GagBegin..GagEnd в level.mfk (закомментированные пропущены): файл сцены art/nis/gags/<имя>.p3d,
              GagSetPosition (x y z или имя локатора), GagSetTrigger (touch/action, центр или локатор, радиус), GagSetCycle,
              GagSetRandom, GagSetInterior, GagSetSound, GagSetCoins, GagSetSparkle, GagSetPersist, GagSetAnimCollision,
              GagSetIntro/Outro, GagSetCameraShake, GagPlayFMV. Сцены без кожи (жёсткие части на суставах) -> тип SPR1 с
              синтезированными состояниями 0 ждёт (кадр 0), 1 играет (PTRN один раз / по кругу), 2 сыграл (последний кадр).
              Сцены с кожей (персонажи в комнатах) -> CHR1 по коже (viewer/skin.c) — см. SKIN ниже.

Формат CLT1 (little endian, все смещения от начала файла, каждая часть с 16-выравниванием):
  заголовок 80 Б (v3, прежний v1 — 64 Б): 'CLT1', u32 версия (3), u32 уровень, u32 текстур, u32 смещение текстур, u32 смещение SPR1, u32 размер SPR1,
    u32 карточек, u32 смещение, u32 ос, u32 смещение, u32 гэгов, u32 смещение, u32 живых ос (PreallocateActors),
    u32 скинов (CHR1), u32 смещение таблицы скинов; v2 @64: i32 kickFx, i32 shockFx; v3 @72: i32 bolt0, u32 nbolt
  текстуры: записи как в SHR7 (16 Б заголовок + палитра 1024 Б + уровни в swizzle), каждая с кратного 16; в SPR1 номер
    текстуры = 0x5000 + номер в этом пакете (main.c: texAt, диапазон CTEX0)
  SPR1: как в level.shr (viewer/stateprops.h), экземпляры по порядку: карточки, осы, гэги с жёсткой сценой
  карточка 32 Б: f32 x y z r, u16 номер (11..17), u16 название (строка CardTitle), u16 экземпляр SPR1, u16 0, 8 Б нулей
  оса 80 Б: имя[16], f32 x y z, радиус полёта, дистанция появления, attrMin, attrMax, attrSpeed (км/ч, <0 стоит),
    atkRange, atkArc (градусы), atkEvade (с, <0 нет), rotSpeed, u16 экземпляр SPR1, u16 флаги (1 стоит на месте, 2 со щитом), u16 экземпляр щита (AddShield, 0 нет), u16 0, f32 boltSpeed, boltLife (SetProjectileStats)
  гэг 128 Б: имя[16] (файл без .p3d), звук[16], ролик[16] (movies/<имя>.smv, "" нет), i16 экземпляр SPR1 (-1 нет),
    i16 первый скин, u16 скинов, i8 комната (-1 город, 0 школа, 1 Kwik-E-Mart, 2 дом Симпсонов — как interior_export.py),
    u8 цикл (0 single, 1 cycle, 2 reset), u8 random, u8 вид триггера (0 touch, 1 action), u8 sparkle, u8 persist,
    u8 animCollision, u8 монет, u16 0, f32 задержка монет (-1 = в конце), f32 x y z, f32 триггер x y z r, f32 длительность с,
    f32 тряска (сила, задержка мс, длительность с), f32 intro, outro, 2 x f32 0
  скин 64 Б (SKIN): u32 смещение CHR1 от начала файла, u32 размер, u16 гэг, u16 0, 52 Б нулей; CHR1 — формат viewer/skin.h
Координаты — движка (z диска отзеркален)."""
import argparse, json, math, os, re, struct, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
GAME = (ROOT / 'hit-and-run-web/source/game').resolve()
VERSION = 3
CTEX0 = 0x5000
ROOMS = {'SpringfieldElementary': 0, 'KwikEMart': 1, 'SimpsonsHouse': 2, 'DMV': 3, 'Moes': 4, 'AndroidsDungeon': 5,
         'Observatory': 6, 'BartRoom': 7}   # номер комнаты — как NAMES в tools/interior_export.py
CYCLE = {'single': 0, 'cycle': 1, 'reset': 2}
BINDING_R = 40.0   # радиус касания гэга-привязки вокруг InteriorOrigin, м (правка 483)


def mirror(p): return (p[0], p[1], -p[2])


def all_locators(files):
    """{имя: (вид, позиция движка, строки данных, [триггеры])} — первые по файлам, как их видит скрипт."""
    from p3d import read, walk, string
    out = {}
    for f in files:
        try: root = read(f)
        except Exception: continue
        for c in walk(root):
            if c.id != 0x3000005: continue
            name, p = string(c.data)
            kind, n = struct.unpack_from('<II', c.data, p)
            payload = c.data[p + 8:p + 8 + 4 * n]
            pos = mirror(struct.unpack_from('<3f', c.data, p + 8 + 4 * n))
            trig = []
            for t in c.children:
                if t.id != 0x3000006: continue
                _, q = string(t.data); rect, = struct.unpack_from('<I', t.data, q); half = struct.unpack_from('<3f', t.data, q + 4)
                m = struct.unpack_from('<16f', t.data, q + 16)
                trig.append((rect, half, mirror(m[12:15])))
            strs = [s.decode('latin1') for s in payload.split(b'\0') if s and all(32 <= b < 127 for b in s)]
            out.setdefault(name, []).append(dict(kind=kind, pos=pos, strs=strs, trig=trig, file=f.name))
    return out


def mfk_calls(path):
    import missions.mfk as M
    calls, _ = M.parse(path.read_text(errors='replace'), path.name)
    return calls


# ------------------------------------------------------------------ SPR1: вырезка одного составного рисунка из файла
def sub_chunk(root, name, extra_names=()):
    """Chunk с детьми, относящимися к составному рисунку name: скелет, рисунок, коллизия, StateProp, анимации
    PTRN_/PVIS_ скелета, BQG_/EFX_ его частей, меши, группы билбордов, системы частиц этих частей."""
    from p3d import Chunk, string
    import prp2_export as PX
    N = lambda c: string(c.data)[0]
    NV = lambda c: string(c.data, 4)[0]
    comp = next((c for c in root.children if c.id == 0x4512 and N(c) == name), None)
    if comp is None: raise ValueError(f'нет составного рисунка {name}')
    skel = string(comp.data, string(comp.data)[1])[0]   # CompositeDrawable: имя, имя скелета (как convert.py)
    kids = [comp] + [c for c in root.children if c.id == 0x4500 and N(c) == skel]
    kids += [c for c in root.children if c.id == 0x7010000 and N(c) in (name, skel)]
    kids += [c for c in root.children if c.id == 0x8020000 and NV(c) in (name, skel)]
    props, effects = PX.composite(PX_chunk([comp]))
    parts = {n for n, _, _ in props} | {n for n, _ in effects} | set(extra_names)
    for c in root.children:
        if c.id == 0x121000:
            an = NV(c)
            kind, _, rest = an.partition('_')
            if (kind in ('PTRN', 'PVIS') and rest in (name, skel)) or (kind in ('BQG', 'EFX') and rest in parts): kids.append(c)
        elif c.id == 0x10000 and N(c) in parts: kids.append(c)
        elif c.id in (0x17002, 0x15800, 0x15801) and NV(c) in parts: kids.append(c)
    return PX_chunk(kids)


def PX_chunk(children):
    from p3d import Chunk
    return Chunk(0, b'', list(children))


# До 29.09 здесь было правило порта limit_slots: у сцен больше 32 рисунков (gag_sqsh уровней 1/4/7: 17 мешей + 17 групп
# билбордов + частицы) лишние группы билбордов не рисовались — маска видимости была 32-битной. С 29.09 ширина маски SPR1 — из
# данных типа (tools/prp2_export.py vis_words, viewer/stateprops.c spSlotOn): сцены целиком, gag_alrm (114) и gag_bats (75) тоже.


class Builder:
    def __init__(self, level, verbose=True):
        for k in ('SHR_TEX_CAP256', 'SHR_SMALL_MIPS', 'SHR_LAYERS'): os.environ.setdefault(k, '1')   # закон 1 — до импорта psp_export
        os.environ.setdefault('SHR_TMP', str(ROOT / 'build/tmp_collect'))
        import psp_export as PE
        import prp2_export as PX
        from convert import Converter
        self.PE, self.PX = PE, PX
        self.conv = Converter(); self.cache, self.blob = {}, []
        if os.environ.get('GAG_SKIN_OLD') != '1':
            from p3d import read
            self.conv.resources(read(GAME / 'art/chars/global.p3d'))
        self.v = verbose
        sp = PX.StateProps.__new__(PX.StateProps)
        sp.level, sp.art, sp.v = level, GAME / 'art', verbose
        sp.pack, sp.tex_of_shader = self.pack, self.tex_of_shader
        sp.atc = PX.atc_table(); sp.types, sp.pieces_raw, sp.inst, sp.city_skip, sp.break_of_dyn = [], [], [], set(), {}
        self.sp = sp

    def pack(self, f, mesh):
        from p3d import read
        PE, conv = self.PE, self.conv
        tn = f'psp_clt_tmp_{os.getpid()}'   # номер процесса: параллельные экспорты не делят файл
        conv.export(f, tn, mesh_name=mesh)
        meta = json.loads((PE.OUT / f'{tn}.json').read_text()); raw = (PE.OUT / f'{tn}.bin').read_bytes()
        for ext in ('json', 'bin'): (PE.OUT / f'{tn}.{ext}').unlink()
        if os.environ.get('GRANDPA_GLASSES_OLD') != '1' and mesh == 'gpa_high_glassesShape' and 'gpa_high_glassesShape' in meta.get('meshes', {}):
            meta['meshes']['gpa_high_glassesShape'] = [pr for pr in meta['meshes']['gpa_high_glassesShape'] if 'glasses' not in pr.get('shader', '').lower()]
        return PE.pack_prims(meta, raw, self.cache, self.blob)

    def tex_of_shader(self, f, sh):
        s_ = self.conv.shaders.get(sh)
        return (self.PE.load_tex(self.conv.textures.get(s_['texture'], ''), self.cache, self.blob, blend=True), int(s_['blend'])) if s_ else -1

    def base(self, name, f, w):
        from p3d import read
        self.conv.resources(read(f))   # шейдеры/текстуры этого файла (имена шейдеров повторяются в разных файлах)
        T = self.sp._base_type(name, f, w, 0)
        T['cls'], T['atc'], T['mass'] = 2, 0, 0.0   # насквозь для машины (удар обрабатывает collect.c), не твёрдые
        if not T['nfr']:   # сцена без движущихся суставов, но с PVIS (телевизор): кадры — по PVIS
            pv, pv_n = self.PX.pvis_tracks(w)
            if pv and pv_n:
                T['nfr'], T['ntrack'], T['keys'] = pv_n, 0, []
                for fr in range(pv_n):
                    m = 0
                    for dn, (st, sw) in pv.items():
                        if dn in T['slots'] and st ^ (sum(1 for s in sw if s <= fr) & 1): m |= 1 << T['slots'].index(dn)
                    T['vis_frames'].append(m)
                T['pvis_mask'] = sum(1 << T['slots'].index(dn) for dn in pv if dn in T['slots'])
        return T

    def efx(self, T, mn, mx, mode=1):
        out = []
        for i, (e, _) in enumerate(T['effects']):
            slot = T['slots'].index(e)
            out.append((slot, mode, 0, self.PX.fx_preset(T['fx_text'].get(e, e)), mn, mx, 1.0))
        return out[:self.PX.MAX_FX]

    def all_vis(self, T):
        return (1 << len(T['props'])) - 1

    # ---- типы
    def card_type(self, positions):
        f = GAME / 'art/cards.p3d'
        from p3d import read
        w = sub_chunk(read(f), 'card_idle')
        T = self.base('card_idle', f, w)
        T['fx_text'] = {e: e for e, _ in T['effects']}
        n = max(T['nfr'] - 1, 0)
        T['states'] = [dict(vis=self.all_vis(T), auto=0xFF, autoTime=0.0, pose=(2 if n else 0, 0, n, 1.0, 0), events=[], calls=[], fx=[])]
        T['inst'] = [(inst_matrix(p), 0) for p in positions]
        self.sp.types.append(T)
        return T

    def wasp_type(self, level, positions):
        from p3d import read
        f = GAME / f'art/l0{level}_fx.p3d'
        root = read(f)
        w = sub_chunk(root, 'beecamera')
        self.conv.resources(root)
        row = dict(file=f, ent=PX_chunk([]), w=w, inst=[(f'w{i}', inst_matrix(p)) for i, p in enumerate(positions)], files={})
        self.sp._add_stateprop('beecamera', row)
        T = self.sp.types[-1]
        T['cls'], T['atc'], T['mass'] = 4, 0, 1.0   # лёгкая: машина почти не теряет скорость на осе (масса в данных нет)
        return T

    def actor_type(self, f, name, positions):
        """Летающий актёр не-оса (AddFlyingActorByLocator) или его рисунок-поведение (UFO_BEAM_ALWAYS_ON): StateProp name из
        файла f целиком, как у осы, один экземпляр на локаторе. Размер и цвет билбордов — снимок каналов BQG на последнем кадре
        состояния, в котором актёр остаётся (цепочка авто-переходов от s0): SPR1 держит их постоянными (правило порта)."""
        from p3d import read
        root = read(f)
        w = sub_chunk(root, name)
        self.conv.resources(root)
        row = dict(file=f, ent=PX_chunk([]), w=w, inst=[(name, inst_matrix(p)) for p in positions], files={})
        self.sp._add_stateprop(name, row)
        T = self.sp.types[-1]
        st, seen = 0, set()
        while T['states'][st]['auto'] != 0xFF and st not in seen and T['states'][st]['auto'] < len(T['states']):
            seen.add(st); st = T['states'][st]['auto']
        rest = T['states'][st]['pose'][2]   # max кадра позы состояния покоя
        from stateprop_dump import parse_stateprop
        from p3d import walk
        spd = next((k for k in walk(w) if k.id == 0x8020000), None)
        if spd is not None:   # позы нет (суставы стоят) — последний кадр контроллера BQG_ состояния покоя (щит осы: 20..40 по кругу)
            fcs = [fc for fc in parse_stateprop(spd)['states'][st]['fc'] if fc['name'].startswith('BQG_') and fc['max'] > fc['min']]
            if fcs and not rest: rest = int(max(fc['max'] for fc in fcs))
        smp = self.PX.bqg_sample(w, rest); qn = self.PX.bqg_quad_names(w); k_of = {}
        for b in T['bills']:
            k = k_of.get(b['name'], 0); k_of[b['name']] = k + 1
            qs = qn.get(b['name'], []); s = smp.get(qs[k] if k < len(qs) else b['name'], {})
            b['w'], b['h'], b['rgba'] = s.get('w', b['w']), s.get('h', b['h']), s.get('rgba', b['rgba'])
        T['actor'] = dict(rest_state=st, rest_frame=rest)
        return T

    def gag_type(self, stem, pos, cycle, source=None, hide=()):   # hide — №869: слоты, нарисованные моделью CHR1 гэга (не в SPR1)
        from p3d import read
        f = source or GAME / 'art/nis/gags' / (stem + '.p3d')
        root = read(f)
        T = self.base(stem, f, root)
        T['fx_text'] = {e: self.PX.fx_text_for(e, root, []) for e, _ in T['effects']}
        # длительность — по кадрам PTRN сцены (у колонки/банкомата/ацтека суставы стоят, играют частицы и билборды)
        nfr_anim, fps = self.PX.ptrn_info(root); fps = fps or T['fps'] or 30.0
        n = max(T['nfr'], nfr_anim) - 1; n = max(n, 0)
        length = n / fps if n else 0.0
        pn = max(T['nfr'] - 1, 0)   # кадры позы (суставы/PVIS); 0 — поза стоит
        vis = self.all_vis(T); allv = (1 << len(T['slots'])) - 1
        hm_ = sum(1 << T['slots'].index(n) for n in hide if n in T['slots']); vis &= ~hm_; allv &= ~hm_   # №869
        wait = dict(vis=vis, auto=0xFF, autoTime=0.0, pose=(0, 0, 0, 1.0, 0), events=[], calls=[], fx=[])
        play = dict(vis=allv, auto=(0xFF if cycle == 1 else (2 if cycle == 0 else 0)), autoTime=length,
                    pose=((2 if cycle == 1 else 1) if pn else 0, 0, pn, 1.0, 0), events=[], calls=[], fx=self.efx(T, 0, max(n, 1)))
        done = dict(vis=vis, auto=0xFF, autoTime=0.0, pose=(0, pn, pn, 1.0, 0), events=[], calls=[], fx=[])
        T['states'] = [wait, play, done]
        T['inst'] = [(inst_matrix(pos), 0)]
        T['length'] = length
        self.sp.types.append(T)
        return T


def inst_matrix(p):
    import numpy as np
    M = np.eye(4); M[3, :3] = p   # строка переноса (как inst_list в prp2_export)
    return M


def has_skin(path):
    from p3d import read
    return any(c.id == 0x10001 for c in read(path).children)


def parse_gags(calls, locs):
    gags, cur = [], None
    def loc_pos(name):
        L = locs.get(str(name))
        return L[0]['pos'] if L else None
    # Правка Роба 483 (10.10: сценки-гэги в интерьерах Мо / DMV / Android's Dungeon): AddGagBinding(комната, файл, цикл, 1, доп. файл) в level.mfk — сцены без позиции и
    # триггера в скрипте (разбор Codex «без позиции», В140). Позиция по данным интерьера: все они нарисованы в ЛОКАЛЬНЫХ координатах вокруг нуля, а у каждого интерьера на
    # диске есть локатор InteriorOrigin (l2i04 Мо — (50, −20, 200), l2i03 DMV — (0, −20, 200), l3i05 Android — (0, −20, 350), l3i06 обсерватория — (150, −20, 350)) — гэг
    # ставится в InteriorOrigin комнаты, цикл как в привязке, срабатывает при входе в комнату (касание радиусом BINDING_R), несколько привязок одной комнаты — пул
    # «random» (в комнате играет один из них, как GagSetRandom). Правило порта: место и пул на диске прямо не заданы. COLLECT_BINDINGS_OLD=1 — как было (привязки не берутся)
    origin = {}
    for e in locs.get('InteriorOrigin', []):
        m = re.match(r'l\d+i(\d+)', e['file'])
        if m: origin[int(m.group(1))] = e['pos']
    names_b = {k.lower(): v for k, v in ROOMS.items()}; names_b.update(moe1=4, android=5)
    for c in calls:
        a = c.args
        if c.op == 'AddGagBinding' and not os.getenv('COLLECT_BINDINGS_OLD') and len(a) >= 3:
            room = names_b.get(str(a[0]).lower(), -1); pos = origin.get(room)
            if pos is None: print(f'  привязка {a[1]}: у комнаты {a[0]} нет InteriorOrigin'); continue
            gags.append(dict(file=str(a[1]), interior=room, cycle=CYCLE.get(str(a[2]).lower(), 1), random=1, sound='', fmv='', pos=pos, trig=(pos, BINDING_R), trigKind=0,
                             sparkle=0, persist=0, animCol=0, coins=0, condition=0, coinDelay=-1.0, intro=0.0, outro=0.0, shake=(0.0, 0.0, 0.0), line=c.line, binding=1))
            continue
        if c.op == 'GagBegin':
            cur = dict(file=str(a[0]), interior=-1, cycle=0, random=0, sound='', fmv='', pos=None, trig=None, trigKind=0,
                       sparkle=0, persist=0, animCol=0, coins=0, condition=0, coinDelay=-1.0, intro=0.0, outro=0.0, shake=(0.0, 0.0, 0.0), line=c.line)
        elif cur is None: continue
        elif c.op == 'GagSetInterior':
            # Script aliases on the personal disc: moe1 (L2), Android (L3), lowercase dmv/bartroom.
            # Interior ids follow SHI1; COLLECT_INTERIOR_OLD is the pre-fix package control.
            name = str(a[0])
            if os.getenv('COLLECT_INTERIOR_OLD'): cur['interior'] = ROOMS.get(name, -1)
            else:
                names = {k.lower(): v for k, v in ROOMS.items()}
                names.update(moe1=4, android=5)
                if name.lower() not in names: raise ValueError(f'unknown gag interior: {name}')
                cur['interior'] = names[name.lower()]
        elif c.op == 'GagSetCycle': cur['cycle'] = CYCLE[str(a[0]).lower()]
        elif c.op == 'GagSetRandom': cur['random'] = int(a[0])
        elif c.op == 'GagSetSound': cur['sound'] = str(a[0])
        elif c.op == 'GagPlayFMV': cur['fmv'] = str(a[0])
        elif c.op == 'GagCheckCollCards': cur['condition'] = 1
        elif c.op == 'GagCheckMovie': cur['condition'] = 2; cur['fmv'] = str(a[2])
        elif c.op == 'GagSetSparkle': cur['sparkle'] = int(a[0])
        elif c.op == 'GagSetPersist': cur['persist'] = int(a[0])
        elif c.op == 'GagSetAnimCollision': cur['animCol'] = int(a[0])
        elif c.op == 'GagSetCoins': cur['coins'] = int(a[0]); cur['coinDelay'] = float(a[1]) if len(a) > 1 else -1.0
        elif c.op == 'GagSetIntro': cur['intro'] = float(a[0])
        elif c.op == 'GagSetOutro': cur['outro'] = float(a[0])
        elif c.op == 'GagSetCameraShake': cur['shake'] = tuple(float(x) for x in a[:3])
        elif c.op == 'GagSetPosition':
            if len(a) == 1:
                cur['pos'] = loc_pos(a[0]); cur['posLoc'] = str(a[0])
                if cur['pos'] is None: print(f'  гэг {cur["file"]}: нет локатора {a[0]}')
            else: cur['pos'] = mirror(tuple(float(x) for x in a[:3]))
        elif c.op == 'GagSetTrigger':
            cur['trigKind'] = 1 if str(a[0]).lower() == 'action' else 0
            if len(a) == 3:
                p = loc_pos(a[1]); cur['trig'] = (p, float(a[2])) if p else None; cur['trigLoc'] = str(a[1])
                if p is None: print(f'  гэг {cur["file"]}: нет локатора триггера {a[1]}')
            else: cur['trig'] = (mirror(tuple(float(x) for x in a[1:4])), float(a[4]))
        elif c.op == 'GagEnd':
            gags.append(cur); cur = None
    return gags


def behaviour_arg(x):
    """Аргумент AddBehaviour после имени поведения: число (ATTRACTION, ATTACK_PLAYER, EVADE_PLAYER — дистанции, скорости,
    интервалы) или текст. Текст на диске один: AddBehaviour("Planet Express Ship", "UFO_BEAM_ALWAYS_ON", "UfoBeam") уровня 7 —
    имя составного рисунка и StatePropData луча в art/missions/level07/ufobeam.p3d (грузит level.mfk). Храним как есть."""
    try: return float(x)
    except (TypeError, ValueError): return str(x)


def parse_wasps(calls, locs):
    """Осы уровня. Поведения актёров не-ос (летающий корабль уровня 7: AddFlyingActorByLocator + UFO_BEAM_ALWAYS_ON с рисунком
    луча) в CLT1 не входят — отдельная механика уровня 7, не покрыто (правило 8 CLAUDE.md); строка в выводе экспорта."""
    beh = {}
    spawn = []
    prealloc = 3
    for c in calls:
        a = c.args
        if c.op == 'AddSpawnPointByLocatorScript' and len(a) >= 6 and str(a[1]).lower() == 'beecamera':
            spawn.append(dict(name=str(a[0]), loc=str(a[3]), roam=float(a[4]), dist=float(a[5])))
        elif c.op == 'AddBehaviour' and len(a) >= 3:
            beh.setdefault(str(a[0]), {})[str(a[1]).upper()] = [behaviour_arg(x) for x in a[2:]]
        elif c.op == 'PreallocateActors' and str(a[0]).lower() == 'beecamera':
            prealloc = int(a[1])
        elif c.op == 'SetActorRotationSpeed' and str(a[0]).lower() == 'beecamera':
            beh.setdefault('beecamera', {})['ROT'] = [float(a[1])]
    out = []
    for s in spawn:
        L = locs.get(s['loc'])
        if not L: print(f'  оса {s["name"]}: нет локатора {s["loc"]}'); continue
        d = dict(beh.get('beecamera', {})); d.update(beh.get(s['name'], {}))
        at = d.get('ATTRACTION', [4.0, 10.0, 10.0]); ak = d.get('ATTACK_PLAYER', [10.0, 5.0, 10.0])
        if any(isinstance(v, str) for v in at[:3] + ak[:3]): raise ValueError(f'оса {s["name"]}: текст в числовом поведении {at} {ak}')
        out.append(dict(s, pos=L[0]['pos'], attr=at[:3], atk=ak[:3], rot=d.get('ROT', [40.0])[0], static=int(at[2] < 0)))
    wasp_names = {'beecamera'} | {s['name'] for s in spawn}
    flying = {str(c.args[1]) for c in calls if c.op == 'AddFlyingActorByLocator' and len(c.args) >= 3}
    for actor, bb in beh.items():
        if actor in wasp_names: continue
        for kind, args in bb.items():
            if actor in flying and kind in ACTOR_LOOKS: continue   # рисунок летающего актёра — parse_flying
            print(f'  поведение {kind}{tuple(args)} актёра «{actor}» — не оса, в CLT1 не входит (не покрыто: механика уровня)')
    return out, prealloc


# Поведения летающего актёра, которые на диске — только рисунок рядом с ним (StatePropData с именем из 3-го аргумента в
# файлах LoadP3DFile уровня). UFO_BEAM_ALWAYS_ON (уровень 7): луч корабля включён всегда — рисунок UfoBeam стоит на том же
# локаторе и остаётся в своём состоянии покоя (s0 «появление» 1 с -> s1). Остальное поведение корабля (цель-босс миссий 3, 5,
# 6, 7: бочка в луче — SetObjTargetBoss / SetPickupTarget / keepbarrel) — код игры, не данные: не покрыто, docs/РЕЦЕПТ_УРОВНЯ.md §12.
ACTOR_LOOKS = ('UFO_BEAM_ALWAYS_ON',)


def parse_shield(calls):
    """AddShield(актёр, рисунок) leveli.mfk -> имя рисунка щита ос (beecamera) или None."""
    for c in calls:
        if c.op == 'AddShield' and len(c.args) >= 2 and str(c.args[0]).lower() == 'beecamera': return str(c.args[1])
    return None


def parse_flying(calls, level_calls, locs, art_root):
    """Летающие актёры не-осы: AddFlyingActorByLocator(тип, имя, локатор, режим) из leveli.mfk -> [dict(type, name, loc, pos,
    file, looks=[(поведение, рисунок, файл)])]. Модель — StatePropData с именем «тип» в файлах LoadP3DFile уровня (level.mfk),
    координаты — локатор (у уровня 7 l7_spaceship в missions/level07/level.p3d)."""
    from p3d import read, string
    loaded = []
    for c in level_calls:
        if c.op == 'LoadP3DFile' and c.args:
            p = art_root.parent / str(c.args[0]).replace('\\', '/').lower()
            hit = next((q for q in [p, *p.parent.glob('*.p3d')] if q.name.lower() == p.name.lower()), None) if p.parent.exists() else None
            if hit is not None: loaded.append(hit)

    def model(name):
        for f in loaded:
            try: root = read(f)
            except Exception: continue
            if any(k.id == 0x8020000 and string(k.data, 4)[0] == name for k in root.children) and \
               any(k.id == 0x4512 and string(k.data)[0] == name for k in root.children): return f
        return None

    beh = {}
    for c in calls:
        if c.op == 'AddBehaviour' and len(c.args) >= 3: beh.setdefault(str(c.args[0]), []).append((str(c.args[1]).upper(), c.args[2:]))
    out = []
    for c in calls:
        if c.op != 'AddFlyingActorByLocator' or len(c.args) < 3: continue
        typ, name, loc = str(c.args[0]), str(c.args[1]), str(c.args[2])
        if typ.lower() == 'beecamera': continue
        L = locs.get(loc)
        if not L: print(f'  летающий актёр {name}: нет локатора {loc} — пропущен'); continue
        f = model(typ)
        if f is None: print(f'  летающий актёр {name}: нет рисунка {typ} в LoadP3DFile уровня — пропущен'); continue
        looks = []
        for kind, args in beh.get(name, []):
            if kind not in ACTOR_LOOKS or not args: continue
            g = model(str(args[0]))
            if g is None: print(f'  поведение {kind} актёра {name}: нет рисунка {args[0]} — пропущено'); continue
            looks.append((kind, str(args[0]), g))
        out.append(dict(type=typ, name=name, loc=loc, pos=L[0]['pos'], file=f, despawn=str(c.args[3]) if len(c.args) > 3 else '',
                        looks=looks))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--level', type=int, default=1)
    ap.add_argument('--out')
    ap.add_argument('--json', action='store_true', help='рядом .json для чтения')
    a = ap.parse_args()
    lv = a.level
    out = Path(a.out) if a.out else ROOT / f'build/collect-l{lv}.shr'
    art = GAME / 'art'
    files = [art / f'l{lv}_terra.p3d', *sorted(art.glob(f'l{lv}[rzi]*.p3d')), *sorted((art / f'missions/level0{lv}').glob('*.p3d'))]
    locs = all_locators(files)
    B = Builder(lv)

    # ---- карточки
    cards = []
    for nm, Ls in locs.items():
        for L in Ls:
            if L['kind'] == 9 and 'CollectorCard' in L['strs']:
                cid = L['strs'][0]
                m = re.fullmatch(r'card(\d)(\d)', cid)
                if not m: print(f'  карточка {nm}: номер {cid}?'); continue
                r = L['trig'][0][1][0] if L['trig'] else 1.0
                cards.append(dict(locator=nm, id=int(m.group(1) + m.group(2)), level=int(m.group(1)), k=int(m.group(2)),
                                  pos=L['pos'], r=r, file=L['file']))
    cards.sort(key=lambda c: c['id'])
    if len({c['id'] for c in cards}) != len(cards): raise SystemExit('карточки: номера повторяются')
    print(f'карточек {len(cards)}: ' + ', '.join(f"card{c['id']}@{c['file']}" for c in cards))

    # ---- осы
    lvl = GAME / f'scripts/missions/level0{lv}'
    icalls = mfk_calls(lvl / 'leveli.mfk')
    wasps, prealloc = parse_wasps(icalls, locs)
    print(f'ос {len(wasps)} (живых одновременно {prealloc}): стоят на месте {sum(w["static"] for w in wasps)}')

    # ---- летающие актёры не-осы (уровень 7: корабль и его луч)
    lcalls = mfk_calls(lvl / 'level.mfk')
    flying = parse_flying(icalls, lcalls, locs, art)
    for fa in flying:
        print(f"летающий актёр {fa['name']} ({fa['type']}, {fa['file'].name}) на {fa['loc']} {tuple(round(v, 2) for v in fa['pos'])}, "
              f"рисунки: {', '.join(f'{k} {n}' for k, n, _ in fa['looks']) or '-'}")

    # ---- гэги
    gags = parse_gags(lcalls, locs)
    print(f'гэгов {len(gags)}: action {sum(g["trigKind"] for g in gags)}, в комнатах {sum(g["interior"] >= 0 for g in gags)}, '
          f'с монетами {sum(g["coins"] > 0 for g in gags)}')

    # ---- SPR1: карточки, осы, жёсткие гэги
    ct = B.card_type([c['pos'] for c in cards])
    wt = B.wasp_type(lv, [w['pos'] for w in wasps])
    inst = len(cards) + len(wasps)
    skins = []
    for gi, g in enumerate(gags):
        f = art / 'nis/gags' / g['file']
        g['inst'] = -1; g['length'] = 0.0; g['skinned'] = has_skin(f)
        if g['pos'] is None or g['trig'] is None: print(f'  гэг {g["file"]}: нет позиции/триггера — пропущен'); continue
        g['skin0'] = -1; g['nskin'] = 0
        if g['skinned'] and os.environ.get('GAG_SKIN_OLD') != '1':
            import gag_skin_export
            models = gag_skin_export.export(f)
            g['skin0'] = len(skins); g['nskin'] = len(models)
            skins.extend(dict(m, gag=gi) for m in models)
        hide_ = {n for m in (models if g['skinned'] and os.environ.get('GAG_SKIN_OLD') != '1' else []) for n in m.get('rigid', [])}   # №869: детали, ушедшие в CHR1
        T = B.gag_type(Path(g['file']).stem, g['pos'], g['cycle'], hide=hide_)
        g['inst'] = inst; g['length'] = T['length']; inst += 1
    # летающие актёры — экземпляры SPR1 после гэгов, без записей CLT1: collect.c их не ведёт, движок рисует и шагает все
    # экземпляры пакета (main.c collDraw, spUpdate) — корабль стоит на локаторе, луч в своём состоянии покоя
    for fa in flying:
        fa['inst'] = [inst]; B.actor_type(fa['file'], fa['type'], [fa["pos"]]); inst += 1
        for kind, look, g in fa['looks']:
            B.actor_type(g, look, [fa["pos"]]); fa['inst'].append(inst); inst += 1
    # щиты ос (AddShield("beecamera", рисунок) — уровни 5–7): рисунок из l0N_fx.p3d, по экземпляру на осу, запись осы — флаг 2 и
    # номер экземпляра щита; поведение щита (сколько ударов держит) — код игры, правило порта в viewer/collect.c
    shield = parse_shield(icalls)
    if shield and wasps:
        B.actor_type(art / f'l0{lv}_fx.p3d', shield, [w['pos'] for w in wasps])
        for k, w in enumerate(wasps): w['shield'] = inst + k
        inst += len(wasps)
        print(f'щит ос {shield}: {len(wasps)} экземпляров (l0{lv}_fx.p3d)')
    # Player effects are explicitly loaded by level.mfk, one instance each.
    foot_fx = [-1, -1]
    if os.environ.get('FOOT_FX_OLD') != '1':
        for call in lcalls:
            if call.op != 'LoadP3DFile' or not call.args: continue
            rel = str(call.args[0]).replace('\\', '/')
            kind = 0 if rel.endswith('_kickwave.p3d') else 1 if rel.endswith('_electrocuted.p3d') else -1
            if kind < 0: continue
            f = GAME / rel; name = 'kickwave' if kind == 0 else 'electrocuted'
            T = B.gag_type(name, (0., 0., 0.), 2, f)
            T['states'][0]['vis'] = T['states'][2]['vis'] = 0
            if kind == 1: T['states'][1]['auto'] = 0xff  # held by the hurt state, no fabricated duration
            foot_fx[kind] = inst; inst += 1
    bolt0 = -1; nbolt = 0; bolt_stats = (0., 0.)
    if os.environ.get('WASP_BOLT_OLD') != '1':
        stats = next(c for c in icalls if c.op == 'SetProjectileStats' and str(c.args[0]).lower() == 'waspray')
        bolt_stats = tuple(float(v) for v in stats.args[1:3])
        # Port interpretation: SetProjectileStats numeric args = speed (m/s), lifetime (s).
        # PS2 timing still unmeasured; the values themselves are copied verbatim.
        # Port budget: 64 simultaneous bolts, independent of active wasp count.
        nbolt = 64; bolt0 = inst
        B.actor_type(art / f'l0{lv}_fx.p3d', 'waspray', [(0.,0.,0.)]*nbolt)
        inst += nbolt

    B.sp.report()
    used = sorted(B.sp.textures())
    tmap = {t: CTEX0 + i for i, t in enumerate(used)}
    spr = B.sp.section(tmap, ())

    # ---- запись
    def pad16(b): return b + b'\0' * (-len(b) % 16)
    texb = bytearray()
    for t in used: texb += B.blob[t]; texb += b'\0' * (-len(texb) % 16)
    cardb = b''.join(struct.pack('<4fHHHH8x', *c['pos'], c['r'], c['id'], (c['level'] - 1) * 8 + (c['k'] - 1), i, 0) for i, c in enumerate(cards))
    waspb = b''
    for i, w in enumerate(wasps):
        waspb += struct.pack('<16s3f2f3f3ffHHH2x2f', w['name'].encode()[:15], *w['pos'], w['roam'], w['dist'], *w['attr'], *w['atk'],
                             w['rot'], len(cards) + i, w['static'] | (2 if w.get('shield') is not None else 0), w.get('shield') or 0, *bolt_stats)
    assert len(waspb) == 80 * len(wasps), len(waspb)
    gagb = b''
    for g in gags:
        tr = g['trig'] or ((0.0, 0.0, 0.0), 0.0); pos = g['pos'] or (0.0, 0.0, 0.0)
        fmv = Path(g['fmv']).stem if g['fmv'] else ''
        rec = struct.pack('<16s16s16shhHbBBBBBBBHf3f4ff3f2f2f', Path(g['file']).stem.encode()[:15], g['sound'].lower().encode()[:15], fmv.encode()[:15],
                          g['inst'], g.get('skin0', -1), g.get('nskin', 0), g['interior'], g['cycle'], g['random'], g['trigKind'], g['sparkle'], g['persist'], g['animCol'],
                          min(g['coins'], 255), (0 if os.environ.get('TICKET_CONTENT_C3_OLD') else g['condition']), g['coinDelay'], *pos, *tr[0], tr[1], g['length'], *g['shake'], g['intro'], g['outro'], 0, 0)
        assert len(rec) == 128, len(rec)
        gagb += rec
    head_len = 80
    off = head_len
    tex_off = off; off += len(texb); off += -off % 16
    spr_off = off; off += len(spr); off += -off % 16
    card_off = off; off += len(cardb); off += -off % 16
    wasp_off = off; off += len(waspb); off += -off % 16
    gag_off = off; off += len(gagb); off += -off % 16
    skin_off = off
    off += 64 * len(skins)
    skinb = bytearray()
    for m in skins:
        skinb += struct.pack('<IIHH4x12f', off, len(m['blob']), m['gag'], 0, *m['place'].flatten())
        off += len(m['blob'])
    head = b'CLT1' + struct.pack('<19I', VERSION, lv, len(used), tex_off, spr_off, len(spr), len(cards), card_off, len(wasps), wasp_off,
                                 len(gags), gag_off, prealloc, len(skins), skin_off, *(i & 0xffffffff for i in foot_fx), bolt0 & 0xffffffff, nbolt)
    assert len(head) == 80
    body = bytearray(head)
    for o, b in ((tex_off, texb), (spr_off, spr), (card_off, cardb), (wasp_off, waspb), (gag_off, gagb), (skin_off, skinb)):
        body += b'\0' * (o - len(body)); body += b
    body += b'\0' * (-len(body) % 16)
    for m in skins: body += m['blob']
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = out.with_suffix(out.suffix + '.tmp'); tmp.write_bytes(bytes(body)); tmp.replace(out)
    print(f'SPR1 {len(spr) / 1024:.0f} КБ, текстур {len(used)} ({len(texb) / 1024:.0f} КБ); ' + ', '.join(f'{k} {v}' for k, v in B.sp.sizes.items()))
    for g in gags:
        kind = 'кожа' if g['skinned'] else f"SPR1 #{g['inst']}"
        print(f"  гэг {g['file']:14s} {kind:10s} комната {g['interior']:2d} {['single', 'cycle', 'reset'][g['cycle']]:6s} "
              f"{'action' if g['trigKind'] else 'touch '} r={g['trig'][1] if g['trig'] else 0:.2f} монет {g['coins']} звук {g['sound']} "
              f"{'FMV ' + g['fmv'] if g['fmv'] else ''} {g['length']:.2f} с")
    print('->', out, out.stat().st_size, 'Б')
    if a.json:
        out.with_suffix('.json').write_text(json.dumps(dict(cards=cards, wasps=wasps, gags=gags, prealloc=prealloc), ensure_ascii=False, indent=1, default=str))
    return 0


if __name__ == '__main__':
    sys.exit(main())
