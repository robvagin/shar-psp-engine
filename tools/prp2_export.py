"""Стейтпропы (AnimDynaPhys 0x3F0000E) и обломки (BreakableObject 0x3001000) уровня -> подсекция SPR1 внутри PRP2.
Спека: docs/props_spec.md. Движок: viewer/stateprops.c/.h (там же формат в C, размеры структур сверяются static_assert).
Только данные диска Роба. Опоры: hit-and-run-web/tools (MIT: p3d, convert,
interiors), tools/char_export.py (скелет, каналы PTRN), tools/stateprop_dump.py (машины состояний).

Вызывается из psp_export.py:
    sp = prp2_export.StateProps(level, art, pack, tex_of_shader)   # pack(file, mesh) -> [(tex, flags, P, UV, C)]
    sp.city_skip                                                    # имена мешей, которые убрать из города
    sp.textures()                                                   # индексы текстур (до tmap) — добавить в used
    body = sp.section(tmap, prp1_type_names)                        # bytes подсекции SPR1
    prp2 = prp2_export.wrap_prp2(prp1_bytes, body)                  # PRP1 -> PRP2 (заголовок 64 Б + SPR1 в конце)
Проверка отдельно (без записи уровня):  python tools/prp2_export.py [--dump]
"""
import sys, struct, re, math, json, os
from pathlib import Path
import numpy as np
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools')); sys.path.insert(0, str(ROOT / 'tools'))
from p3d import read, walk, string                     # noqa: E402
import uvq                                             # noqa: E402  16-битные UV без знака для GE (правка Роба 43)
from interiors import leaves as col_leaves             # noqa: E402
import char_export as CE                               # noqa: E402
from stateprop_dump import parse_stateprop, atc_table  # noqa: E402
_atc_table_disc = atc_table
def atc_table():
    """сверка g12-data P18-15 (02.11.2026): строка 0 таблицы ATC переписывается так же, как в tools/psp_export.atc_rows (звук smash, частицы и
    обломок eNull, масса 100, трение 1, упругость 1; на диске «WTF» и нули). tools/stateprop_dump.py — не наш файл, правим на входе сюда.
    ATC0_OLD=1 — как было"""
    rows = _atc_table_disc()
    if rows and os.environ.get('ATC0_OLD') != '1': rows[0] = dict(sound='smash', particle='eNull', breakable='eNull', f=(1.0, 100.0, 1.0))
    return rows

MIRROR = np.diag([1.0, 1.0, -1.0, 1.0])
N = lambda c: string(c.data)[0]

# ------------------------------------------------------------------ коды (совпадают с viewer/stateprops.h)
EV = {'DESTROYED': 7, 'HIT': 8, 'FEATHER_TOUCH': 9, 'STOMP': 10, 'VEHICLE_HIT': 11}
OP_REMOVE, OP_COINS, OP_COINS_Z, OP_COLOFF, OP_DESTROYED, OP_COLA, OP_KILLSPEED, OP_RADIATE, OP_COL2OFF, OP_BOLT = range(10)
PRP2_COINS25_OLD = os.environ.get('PRP2_COINS25_OLD') == '1'   # красный контроль: число монет из имени вызова
OP_BREAK = 10   # arg = BreakablesEnum: проиграть обломок этого номера в точке экземпляра (барьеры 0x3F0000F, см. _add_animphys)
def call_op(name):
    """Вызов стейтпропа -> (op, arg) по ИМЕНИ (id 11 у SPAWN_5_COINS_Z и SPAWN_25_COINS_Z совпадает)."""
    m = re.fullmatch(r'SPAWN_(\d+)_COINS?(_Z)?', name)
    # сверка g12-data P16-19 / P11-12 (02.11.2026): у вызова стейтпропа номер 11 оригинал не смотрит на имя — SPAWN_5_COINS_Z и SPAWN_25_COINS_Z
    # (автоматы с колой L1/L2, один проп НЛО) оба сыплют 5 монет против оси Z предмета. Было: число из имени (25). PRP2_COINS25_OLD=1 — как было
    if m and m.group(2) and not PRP2_COINS25_OLD: return OP_COINS_Z, 5
    if m: return (OP_COINS_Z if m.group(2) else OP_COINS), int(m.group(1))
    return {'REMOVE_FROM_WORLD': (OP_REMOVE, 0), 'REMOVE_COLLISION_VOLUME': (OP_COLOFF, 0),
            'OBJECT_DESTROYED': (OP_DESTROYED, 0), 'COLA_DESTROYED': (OP_COLA, 0), 'KILL SPEED': (OP_KILLSPEED, 0),
            'KILL_SPEED': (OP_KILLSPEED, 0), 'RADIATE_FORCE': (OP_RADIATE, 0), 'REMOVE_2ND_VOL': (OP_COL2OFF, 0),
            'FIRE_ENERGY_BOLT': (OP_BOLT, 0)}.get(name)

# пресеты частиц (viewer/stateprops.h SPFX_*): по текстурам/именам внутри системы частиц эффекта
FX_NONE, FX_DUST, FX_WOOD, FX_GLASS, FX_LEAVES, FX_SPARKS, FX_WATER, FX_FIRE, FX_SMOKE, FX_HAY, FX_TOMATO, FX_DEBRIS = range(12)
FX_KEYS = [(FX_FIRE, ('fireseq', 'flame', 'fire')), (FX_WATER, ('water', 'spray')), (FX_GLASS, ('glass', 'klogo')),
           (FX_SPARKS, ('spark', 'arc')), (FX_HAY, ('hay', 'sadsaw', 'straw')), (FX_TOMATO, ('tomacco', 'tomaco', 'tomato')),
           (FX_LEAVES, ('leave', 'leaf', 'oak', 'cypress', 'evers', 'shrub', 'pine', 'needle')),
           (FX_WOOD, ('cratebit', 'crate', 'wood', 'splinter', 'chip')), (FX_SMOKE, ('smoke',)),
           (FX_DEBRIS, ('barrel', 'bits', 'debris', 'piece', 'chunk')), (FX_DUST, ('dust', 'terss', 'dirt'))]
def fx_preset(text):
    t = text.lower().replace('firehydrant', 'hydrant')   # «fire hydrant» — вода, не огонь (было: гидрант давал SPFX_FIRE)
    for k, keys in FX_KEYS:
        if any(s in t for s in keys): return k
    return FX_DUST

# индексы обломков (BreakablesEnum) по имени строки ATC; сопоставлено по содержимому (docs/props_spec.md §1.4)
BREAK_BY_ATC = {'eHydrantBreaking': 3, 'eTomaccoBreaking': 8, 'ePineTreeBreaking': 14, 'eOakTreeBreaking': 15,
                'eKrustyGlassBreaking': 19, 'eCypressTreeBreaking': 20, 'eStopsign': 29, 'eStopsignBreaking': 29,
                # 29.09 (уровни 2 и 5): BreakableObject 16 несёт частицы l2_donotenter_fx, 17 — обломки fx_crossing_geo;
                # у барьера l2/l5_donotenter строка ATC 17 (eBigBarrierBreaking), у знака переезда l2/l5_railcross — 16
                'eBigBarrierBreaking': 16, 'eRailCrossBreaking': 17,
                # №389: verified against native BreakableObject composites, including L6 trees/poles
                'eDeadTreeBreaking': 21, 'eSkeletonBreaking': 22, 'eWillow': 23,
                'eGlobeLight': 25, 'eTreeMorn': 26, 'ePalmTreeSmall': 27, 'ePalmTreeLarge': 28}
# №419 (Роб 09.10: «L5 F3-5 стеклянная труба при ударе отлетает, как металлическая, а должна разбиться»): строки ATC, которых не было
# в таблице, — DynaPhys без обломка становились твёрдым летающим телом. Номер — по содержимому BreakableObject 0x3001000 диска:
# 18 l02z4_glasshatter (l2z4, l5z4: стеклянная игла l2z4/l5z4_spaceneedle), 30/31/32 pumpkin_bits1_ps / _psmed / _pssma (l7r1:
# тыквы l7_pumpkin / pumpkinmed / pumpkinsma), 33 l3_burns_effects (l3r2: l3_burns, eCasinoJump). BREAK419_OLD=1 в окружении — как до правки
if not os.environ.get('BREAK419_OLD'):
    BREAK_BY_ATC.update({'eSpaceNeedleBreaking': 18, 'ePumpkin': 30, 'ePumpkinMed': 31, 'ePumpkinSmall': 32, 'eCasinoJump': 33})

# Правка «кованый забор кладбища» (10.10, видео PS2 14:43 и сверка Стен): l1/l4/l7_fence_Shape (AnimDynaPhys cls 4, physProp 1, без обломка и частиц) —
# у PS2 тяжёлый Plow King пробивает, лёгкие идут вдоль и забор цел. Тип получает класс 11 (в движке SP_CLS_HARDFENCE): удар засчитывает только
# машина тяжелее SP_FENCE_MASS (viewer/stateprops.c). SPR_HARDFENCE_OLD=1 в окружении — прежний класс 4
SP_CLS_HARDFENCE = 11
MAX_SLOTS, MAX_FX, VIS_ALWAYS = 32, 4, 31
# Слоты рисунка (props + эффекты составного рисунка, по порядку данных). Маска видимости — слова по 32 бита, ширина — из данных
# типа: vw = ceil(слотов / 32). Тип до 32 слотов — одно слово, запись как до 29.09 (файлы уровня 1 не меняются); больше
# (обезьяны l2/l5_monkiesgag 47, сигнализация gag_alrm 114, летучие мыши gag_bats 75) — слова 1..vw-1 лежат в таблице PVIS
# (часть 6 SPR1): SpType @52 visWords (0 = одно слово), @56 pvisX — индекс слов 1.. маски PVIS; SpState @76 visX — индекс слов
# 1.. маски состояния; кадры PVIS такого типа — по vw слов на кадр. MAX_SLOTS — слотов в одном слове (кластеры
# tools/animobj_export.py режутся по нему: отсечение по центру экземпляра). Потолок — номер слота u8 (SpPiece.flags 16–23,
# SpBill.slot, SpFx.slot, имена эффектов): SLOT_LIMIT. Движок — viewer/stateprops.c (spSlotOn).
SLOT_LIMIT = 256


def vis_words(T):
    """Слов маски видимости у типа (из числа его слотов)."""
    return max(1, (len(T['slots']) + 31) // 32)


def mask_words(m, vw):
    """Маска (целое любой длины) -> vw слов u32, младшее первым."""
    return [(int(m) >> (32 * k)) & 0xFFFFFFFF for k in range(vw)]
QV = np.dtype([('u', '<i2'), ('v', '<i2'), ('c', '<u4'), ('x', '<i2'), ('y', '<i2'), ('z', '<i2'), ('pad', '<i2')])
HDR = 64   # SpHeader

def level_files(art, level):
    return [art / f'l{level}_terra.p3d'] + sorted(art.glob(f'l{level}r*.p3d')) + sorted(art.glob(f'l{level}z*.p3d'))

def inst_list(ent):
    """[(имя, мировая 4x4 отзеркаленная, вектор-строка)] экземпляров из InstanceList (как col_instances)."""
    out = []
    def rec(c, m, nm):
        if c.id == 0x120103:
            nm, p = string(c.data); m = np.frombuffer(c.data, '<f4', 16, p + 4).reshape(4, 4).astype(np.float64) @ m
        if c.id == 0x120107: out.append((nm, MIRROR @ m @ MIRROR))
        for s in c.children: rec(s, m, nm)
    for c in ent.children:
        if c.id == 0x3000008: rec(c, np.eye(4), '')
    return out

def quat_wxyz(R):
    """3x3 (мир = R @ столбец) -> (w, x, y, z)."""
    x, y, z, w = CE.q_from_m(R); return (w, x, y, z)

# ------------------------------------------------------------------ разбор обёртки (AnimObjectWrapper / BreakableObject)
def composite(w):
    cd = next((c for c in w.children if c.id == 0x4512), None)
    props, effects = [], []
    if cd is None: return props, effects
    for l in cd.children:
        for e in l.children:
            if e.id == 0x4516:
                n, p = string(e.data); trans, joint = struct.unpack_from('<2I', e.data, p); props.append((n, joint, trans))
            elif e.id == 0x4518:
                n, p = string(e.data); joint, = struct.unpack_from('<I', e.data, p); effects.append((n, joint))
    return props, effects

def billboards(w):
    """{имя группы: dict(shader, quads=[(mode, t, rgba, w, h, dist)])}"""
    out = {}
    for g in w.children:
        if g.id != 0x17002: continue
        nm, p = string(g.data, 4); sh, p = string(g.data, p); zt, zw, fog, nq = struct.unpack_from('<4I', g.data, p)
        quads = []
        for q in g.children:
            if q.id != 0x17001: continue
            _, p = string(q.data, 4); mode = q.data[p:p + 4].rstrip(b'\0').decode('latin1'); p += 4
            t = struct.unpack_from('<3f', q.data, p); p += 12; col, = struct.unpack_from('<I', q.data, p); p += 4 + 32
            wd, ht, dist = struct.unpack_from('<3f', q.data, p)
            quads.append((mode, (t[0], t[1], -t[2]), col, wd, ht, dist))
        out[nm] = dict(shader=sh, quads=quads, zwrite=zw)
    return out


def billboard_mode(mode):
    # SPR1 keeps legacy NOAX/AAX values; YAX and LYAX preserve the authored fixed axis.
    return {'NOAX': 0, 'AAX': 1, 'YAX': 2, 'LYAX': 3}.get(mode.rstrip('\0 '), 1)

def bqg_anims(w):
    """анимации квадов свечения (BQG_<группа>) внутри рисунка -> {имя: (тип, кадров, fps, {группа: {канал: данные}})}"""
    from frontend3d_export import anims
    try: return {k: v for k, v in anims(w).items() if k.startswith('BQG_')}
    except Exception: return {}


def bqg_track(an, dn, w, h, col):
    """Правка Роба 76 (29.09: «у карточки и у ключа виден квадрат свечения в небе»): у квадов свечения оригинала своя анимация
    BQG — ширина WDT, высота HGT, цвет CLR, видимость VIS по кадрам (карточка: фиолетовые 6 м пульсируют, а не горят целиком).
    -> [(w, h, ARGB u32)] на кадр (невидимый кадр — альфа 0) или [] (анимации нет — рисуется квадрат из данных, как раньше)"""
    import numpy as np
    from frontend3d_export import sample
    if not an: return []
    ch = an[3].get(dn, {})
    if not any(k in ch for k in ('WDT', 'HGT', 'CLR', 'VIS')): return []
    nfr = max(1, int(round(an[1])))
    rows = []
    for f in range(nfr):
        ww = float(sample(*ch['WDT'], f)[0]) if 'WDT' in ch else w
        hh = float(sample(*ch['HGT'], f)[0]) if 'HGT' in ch else h
        if 'CLR' in ch:
            r, g_, b, a = [int(round(float(x) * 255)) for x in np.clip(sample(*ch['CLR'], f), 0, 1)]
            c = (a << 24) | (r << 16) | (g_ << 8) | b
        else: c = col
        if 'VIS' in ch:
            fr, st = ch['VIS'][0], ch['VIS'][1]
            vis = (int(st) + int(sum(1 for x in fr if x <= f))) % 2 == 1 if len(fr) else bool(st)
            if not vis: c &= 0x00FFFFFF
        rows.append((ww, hh, c & 0xFFFFFFFF))
    return rows


def pvis_tracks(w):
    """PVIS: {drawable: (startState, [кадры переключения])} + число кадров."""
    for an in w.children:
        if an.id != 0x121000: continue
        name, p = string(an.data, 4)
        if an.data[p:p + 4] != b'PVIS': continue
        nfr, = struct.unpack_from('<f', an.data, p + 4); out = {}
        for g in walk(an):
            if g.id != 0x121001: continue
            gn, _ = string(g.data, 4)
            for ch in g.children:
                if ch.id != 0x121108: continue
                st, n = struct.unpack_from('<HI', ch.data, 8)
                out[gn] = (st, list(struct.unpack_from(f'<{n}H', ch.data, 14)))
        return out, int(round(nfr))
    return {}, 0

# ------------------------------------------------------------------ барьеры 0x3F0000F (AnimDynamicPhysicsWrapper)
# AnimDynaPhys без StatePropData — на диске только l2/l5_donotenter (барьер «проезд закрыт», 20 шт. на уровне, 4 огня) и
# l2/l5_railcross (знак переезда, 8 шт., 4 красных огня попеременно). Обёртка 0x3F0000F несёт скелет, составной рисунок,
# клипы PTRN (суставы стоят) и BQG (у каждого квадрата-огня канал цвета CLR 0x121109 и видимости VIS 0x121108), главный
# контроллер 0x48A0 (длина = кадров клипа), объём CollisionObject, PhysicsObject и строку ATC (обломок eBigBarrierBreaking /
# eRailCrossBreaking). До 29.09 конвертер ставил их меш в город неподвижным: без огней, без объёма, не ломались.
# Правила порта (данных нет, сверить с эталоном PS2):
#   * огонь горит на кадре, где яркость цвета канала CLR (max(R,G,B)·A, линейно между ключами) не меньше половины, — вместо
#     плавной яркости (формат SPR1 держит видимость слота по кадрам PVIS, а не цвет): у барьера огни 1–2 и 3–4 мигают
#     попеременно (0,83 с), у переезда — пары огней попеременно (0,63 с на L2, 0,5 с на L5), как ключи CLR;
#   * клип играет по кругу всегда (контроллер 0x48A0 без события запуска);
#   * удар машиной (от SP_HIT_SPEED) или пинок Гомера — обломок строки ATC (BreakableObject 16/17 уровня) и объект уходит из
#     мира (как сбитый DynaPhys c обломком: стоп-знак, гидрант); вернуть на место — общим правилом spRespawnStep.
ANIMPHYS_ON = __import__('os').environ.get('SHR_NO_ANIMPHYS') != '1'   # =1 — как до 29.09 (отрицательный контроль гейта)
ANIMPHYS_HALF = 0.5   # порог яркости огня


def bqg_quad_names(w):
    """{группа билбордов 0x17002: [имена квадратов 0x17001 по порядку]}."""
    out = {}
    for g in w.children:
        if g.id != 0x17002: continue
        out[string(g.data, 4)[0]] = [string(q.data, 4)[0] for q in g.children if q.id == 0x17001]
    return out


def bqg_quad_on(w, nfr):
    """Клипы BQG обёртки -> {имя квадрата: [вкл на кадре 0..nfr-1]} по VIS и яркости CLR (см. правила выше)."""
    out = {}
    for an in w.children:
        if an.id != 0x121000: continue
        _, p = string(an.data, 4)
        if an.data[p:p + 4] != b'BQG\0': continue
        for g in walk(an):
            if g.id != 0x121001: continue
            qn, _ = string(g.data, 4)
            vis = [1] * nfr; bright = None
            for ch in g.children:
                if ch.id == 0x121108:
                    st, n = struct.unpack_from('<HI', ch.data, 8); sw = struct.unpack_from(f'<{n}H', ch.data, 14)
                    vis = [st ^ (sum(1 for s in sw if s <= f) & 1) for f in range(nfr)]
                elif ch.id == 0x121109:
                    n, = struct.unpack_from('<I', ch.data, 8)
                    fr = struct.unpack_from(f'<{n}H', ch.data, 12); cv = struct.unpack_from(f'<{n}I', ch.data, 12 + 2 * n)
                    b = [max((c >> 16) & 255, (c >> 8) & 255, c & 255) / 255.0 * ((c >> 24) & 255) / 255.0 for c in cv]
                    bright = [float(np.interp(f, fr, b)) for f in range(nfr)]
            out[qn] = [int(vis[f] and (bright is None or bright[f] >= ANIMPHYS_HALF)) for f in range(nfr)]
    return out


def bqg_sample(w, frame):
    """Каналы BQG квадратов на кадре frame (линейно между ключами): {имя квадрата: {'w','h','rgba'}} — только то, что
    есть в данных (WDT 0x121100 'WDT', HGT 'HGT', цвет CLR 0x121109). Формат SPR1 держит размер и цвет билборда
    постоянными — это «снимок» кадра для типов, у которых живой размер задаёт клип (луч НЛО уровня 7)."""
    out = {}
    for an in w.children:
        if an.id != 0x121000: continue
        _, p = string(an.data, 4)
        if an.data[p:p + 4] != b'BQG\0': continue
        for g in walk(an):
            if g.id != 0x121001: continue
            qn, _ = string(g.data, 4); d = out.setdefault(qn, {})
            for ch in g.children:
                prm = ch.data[4:8].rstrip(b'\0').decode('latin1')
                if ch.id == 0x121100 and prm in ('WDT', 'HGT'):
                    n, = struct.unpack_from('<I', ch.data, 8)
                    fr = struct.unpack_from(f'<{n}H', ch.data, 12); v = struct.unpack_from(f'<{n}f', ch.data, 12 + 2 * n)
                    d['w' if prm == 'WDT' else 'h'] = float(np.interp(frame, fr, v))
                elif ch.id == 0x121109:
                    n, = struct.unpack_from('<I', ch.data, 8)
                    fr = struct.unpack_from(f'<{n}H', ch.data, 12); cv = struct.unpack_from(f'<{n}I', ch.data, 12 + 2 * n)
                    comp = [[(c >> s) & 255 for c in cv] for s in (24, 16, 8, 0)]
                    a_, r_, g_, b_ = (int(round(float(np.interp(frame, fr, k)))) for k in comp)
                    d['rgba'] = (a_ << 24) | (r_ << 16) | (g_ << 8) | b_
    return out


def anim_frames(w):
    """Кадров и кадров/с клипа обёртки: длина главного контроллера 0x48A0, иначе самый длинный клип."""
    mc = next((c for c in w.children if c.id == 0x48a0), None)
    if mc is not None:
        _, p = string(mc.data); _, ln, fps = struct.unpack_from('<Iff', mc.data, p)
        return int(round(ln)), fps
    best = (0, 30.0)
    for an in w.children:
        if an.id != 0x121000: continue
        _, p = string(an.data, 4); nf, fps = struct.unpack_from('<ff', an.data, p + 4)
        best = max(best, (int(round(nf)), fps))
    return best


def ptrn_info(w):
    for an in w.children:
        if an.id != 0x121000: continue
        name, p = string(an.data, 4)
        if an.data[p:p + 4] == b'PTRN':
            nfr, fps, loop = struct.unpack_from('<ffI', an.data, p + 4); return int(round(nfr)), fps
    return 0, 30.0

def fx_text_for(effect, w, file_roots):
    """Текстовая «подпись» эффекта: имена шейдеров/текстур внутри системы частиц с тем же именем."""
    txt = effect
    for root in [w] + file_roots:
        for c in walk(root):
            if c.id in (0x15800, 0x15801) and effect.lower() in string(c.data, 4)[0].lower():
                txt += ' ' + bytes(b if 32 <= b < 127 else 32 for b in b''.join(x.data for x in walk(c))).decode()
                return txt
    return txt

def sample(fr, vals, f):
    if not len(fr): return None
    a, b, u = CE._key(np.asarray(fr), vals, f)
    return a, b, u

def joint_poses(w):
    """Скелет + PTRN, запечённые по кадрам. -> joints(list dict parent, q(wxyz), t, s, track), keys[frame][track] (t3,q4), fps"""
    try: sk = CE.load_skeleton(w)
    except ValueError as e: raise ValueError(f'skeleton: {e}')
    nfr, fps = ptrn_info(w)
    keep_turn = os.environ.get('ANIM_HALF_TURN_OLD', '0') != '1'
    clips = CE.load_clips(w, sk, key_continuity=keep_turn) if nfr else []
    tracks = clips[0]['tracks'] if clips else [dict(rf=[], rq=[], tf=[], tv=[]) for _ in sk]
    joints, per = [], []
    for j, jt in enumerate(sk):
        tr = tracks[j]; frames = []
        for f in range(max(nfr, 1)):
            q = jt['q']; t = jt['t']
            r = sample(tr['rf'], tr['rq'], f)
            if r: q = CE.qmix(r[0], r[1], r[2], keep_half_turn=keep_turn)
            r = sample(tr['tf'], tr['tv'], f)
            if r: t = r[0] + (r[1] - r[0]) * r[2]
            q = np.asarray(q, np.float64); q = q / np.linalg.norm(q)
            if frames and np.dot(q, frames[-1][1]) < 0: q = -q
            frames.append((np.asarray(t, np.float64), q))
        moving = any(np.abs(t - frames[0][0]).max() > 1e-4 or 1 - abs(np.dot(q, frames[0][1])) > 1e-6 for t, q in frames)
        t0, q0 = frames[0]
        joints.append(dict(name=jt['name'], parent=jt['parent'], q=(q0[3], q0[0], q0[1], q0[2]), t=t0, s=jt['s'], track=-1))
        per.append(frames if moving else None)
    ntrack = 0
    for j, fr in enumerate(per):
        if fr is not None: joints[j]['track'] = ntrack; ntrack += 1
    keys = []
    if ntrack:
        for f in range(nfr):
            for j, fr in enumerate(per):
                if fr is None: continue
                t, q = fr[f]; keys.append((*t, q[3], q[0], q[1], q[2]))
    return joints, keys, (nfr if ntrack else 0), ntrack, fps

def rest_world(joints):
    """Мировые (в осях объекта) 4x4 столбцовые матрицы суставов в позе кадра 0."""
    W = []
    for jt in joints:
        w_, x, y, z = jt['q']; L = np.eye(4); L[:3, :3] = CE.m_from_q((x, y, z, w_)) * jt['s']; L[:3, 3] = jt['t']
        W.append(L if jt['parent'] < 0 else W[jt['parent']] @ L)
    return W

def collision(w, name):
    """Листья CollisionObject -> [(kind 0 box/1 sphere/2 cyl, owner, center, axes 3x3 строки, half3)] в осях сустава-владельца
    (как physics_shapes веба: объём ставится матрицей сустава)."""
    out = []
    def rec(v):
        kids = [k for k in v.children if k.id == 0x7010001]
        if kids:
            for k in kids: rec(k)
            return
        owner, = struct.unpack_from('<I', v.data)
        for s in col_leaves(v, name):
            c = np.asarray(s['center'], np.float64)
            if s['kind'] == 'box':
                A = np.asarray(s['axes'], np.float64); out.append((0, owner, c, A, np.asarray(s['halfExtents'], np.float64)))
            elif s['kind'] == 'sphere':
                out.append((1, owner, c, np.eye(3), np.array([s['radius']] * 3)))
            else:
                a = np.asarray(s['axis'], np.float64); a /= np.linalg.norm(a)
                u = np.cross(a, [1.0, 0, 0] if abs(a[0]) < .9 else [0, 1.0, 0]); u /= np.linalg.norm(u)
                out.append((2, owner, c, np.stack([u, a, np.cross(u, a)]), np.array([s['radius'], s['length'] / 2, s['radius']])))
    for co in w.children:
        if co.id != 0x7010000: continue
        for v in co.children:
            if v.id == 0x7010001: rec(v)
    return out

# ------------------------------------------------------------------ сборка
class StateProps:
    def __init__(self, level, art, pack, tex_of_shader, verbose=True):
        """pack(file Path, mesh name) -> [(tex, flags, P, UV, C)] в осях меша (отзеркалено, как город);
        tex_of_shader(file Path, shader name) -> индекс текстуры (до tmap) или -1."""
        self.level, self.art, self.pack, self.tex_of_shader, self.v = level, art, pack, tex_of_shader, verbose
        self.atc = atc_table()
        self.types, self.pieces_raw, self.inst = [], [], []
        self.city_skip = set()
        self._scan()

    def log(self, *a):
        if self.v: print(*a, flush=True)

    # ---- типы
    def _scan(self):
        files = level_files(self.art, self.level)
        found, other_mesh_refs, sp_mesh_refs = {}, set(), set()
        for f in files:
            root = read(f)
            for ent in root.children:
                if ent.id == 0x3f0000e:
                    name = N(ent); insts, seen = [], set()
                    for n, M in inst_list(ent):
                        k = tuple(np.round(M[3, :3], 2))
                        if k not in seen: seen.add(k); insts.append((n, M))
                    w = next((c for c in ent.children if c.id == 0x3f00010), None)
                    animphys = w is None and ANIMPHYS_ON
                    if w is None and animphys: w = next((c for c in ent.children if c.id == 0x3f0000f), None)   # барьеры L2/L5
                    if w is None: continue
                    if not insts: continue   # шаблоны без экземпляров в городе и так не стоят
                    props, _ = composite(w)
                    sp_mesh_refs |= {n for n, _, _ in props}
                    row = found.setdefault(name, dict(file=f, ent=ent, w=w, inst=[], files={}, animphys=animphys))
                    sp = next((c for c in walk(w) if c.id == 0x8020000), None)
                    key = json.dumps(parse_stateprop(sp), sort_keys=True) if sp else ''
                    if 'sp_key' not in row: row['sp_key'] = key
                    elif row['sp_key'] != key: self.log(f'  ВНИМАНИЕ: {name} в {f.stem} — другая машина состояний, беру из {row["file"].stem}')
                    row['inst'] += insts; row['files'][f.stem] = len(insts)
                else:   # всё остальное, что ставит меши в город: имена мешей, на которые ссылаются другие
                    for c in walk(ent):
                        if c.id == 0x120107:
                            _, p = string(c.data); other_mesh_refs.add(string(c.data, p)[0])
                        elif c.id == 0x10000 and ent.id == 0x3f00000: other_mesh_refs.add(N(c))
        self.city_skip = sp_mesh_refs - other_mesh_refs
        both = sp_mesh_refs & other_mesh_refs
        if both: self.log(f'  меши стейтпропов, которые город использует и сам (оставлены в городе): {sorted(both)}')
        self.animphys_break = {}   # имя барьера 0x3F0000F -> номер обломка (BreakablesEnum): обломок строится в _add_breakables
        for name, row in found.items():
            if row.get('animphys'): self._add_animphys(name, row)
            else: self._add_stateprop(name, row)
        self._add_breakables(files)

    def _pieces(self, f, w, props, joints_world):
        """Меши составного рисунка -> куски; билборды -> список. Слоты: props по порядку, потом эффекты."""
        meshes = {N(c) for c in w.children if c.id == 0x10000}
        bbs = billboards(w); out_p, out_b = [], []
        A = bqg_anims(w)
        for slot, (dn, joint, trans) in enumerate(props):
            if dn in meshes:
                for t, fl, P, UV, C in self.pack(f, dn):
                    out_p.append((slot, joint, t, fl | (1 if trans else 0), P, UV, C))
            elif dn in bbs:
                g = bbs[dn]; r = self.tex_of_shader(f, g['shader'])
                tex, blm = r if isinstance(r, tuple) else (r, 0)
                for mode, t, col, wd, ht, dist in g['quads']:
                    out_b.append(dict(joint=joint, slot=slot, mode=billboard_mode(mode), tex=tex,
                                      t=t, w=wd, h=ht, dist=dist, rgba=col, name=dn, blend=blm,
                                      track=bqg_track(A.get('BQG_' + dn), dn, wd, ht, col)))
            else: self.log(f'    нет рисунка {dn}')
        return out_p, out_b

    def _base_type(self, name, f, w, kind):
        props, effects = composite(w)
        if len(props) + len(effects) > SLOT_LIMIT: raise ValueError(f'{name}: {len(props) + len(effects)} слотов > {SLOT_LIMIT}')
        joints, keys, nfr, ntrack, fps = joint_poses(w)
        JW = rest_world(joints)
        pieces, bills = self._pieces(f, w, props, JW)
        pv, pv_n = pvis_tracks(w)
        slots = [p[0] for p in props] + [e[0] for e in effects]
        pvis_mask, vis_frames = 0, []
        if pv and nfr:
            for dn in pv:
                if dn in slots: pvis_mask |= 1 << slots.index(dn)
            for fr in range(nfr):
                m = 0
                for dn, (st, sw) in pv.items():
                    if dn not in slots: continue
                    on = st ^ (sum(1 for s in sw if s <= fr) & 1)
                    if on: m |= 1 << slots.index(dn)
                vis_frames.append(m)
        # коробка по мешам (поза кадра 0), в осях объекта — центр для монет/частиц/отсечения
        pts = [(np.c_[P, np.ones(len(P))] @ JW[j].T)[:, :3] for _, j, _, _, P, _, _ in pieces if j < len(JW)]
        if pts:
            allp = np.concatenate(pts); lo, hi = allp.min(0), allp.max(0)
        else: lo, hi = np.array([-.5, 0, -.5]), np.array([.5, 1, .5])
        cols = []
        for kind_, owner, c, A, h in collision(w, name):
            M = JW[owner] if owner < len(JW) else np.eye(4)
            R = M[:3, :3]; cw = R @ c + M[:3, 3]; Aw = (R @ A.T).T; s = np.linalg.norm(Aw, axis=1); Aw = Aw / s[:, None]
            cols.append(dict(kind=kind_, c=cw, A=Aw, h=h * s))
        fx_text = {e: fx_text_for(e, w, []) for e, _ in effects}
        return dict(name=name, kind=kind, props=props, effects=effects, slots=slots, joints=joints, keys=keys, nfr=nfr,
                    ntrack=ntrack, fps=fps, pieces=pieces, bills=bills, pvis_mask=pvis_mask, vis_frames=vis_frames,
                    lo=lo, hi=hi, cols=cols, fx_text=fx_text, JW=JW, file=f)

    def _fx_entries(self, T, fcs):
        """FrameControllerData EFX_* -> [(slot, mode, cycles, preset, min, max, speed)]"""
        out = []
        for fc in fcs:
            if not fc['name'].startswith('EFX_'): continue
            eff = fc['name'][4:]
            if eff not in T['slots']: continue
            if fc['min'] == fc['max'] and not fc['cyclic']: continue
            out.append((T['slots'].index(eff), 2 if fc['cyclic'] else 1, fc['cycles'], fx_preset(T['fx_text'].get(eff, eff)),
                        int(fc['min']), int(fc['max']), fc['speed']))
        return out[:MAX_FX]

    def _add_stateprop(self, name, row):
        f, w = row['file'], row['w']
        T = self._base_type(name, f, w, 0)
        sp = parse_stateprop(next(c for c in walk(w) if c.id == 0x8020000))
        eff_root = [read(f)]
        T['fx_text'] = {e: fx_text_for(e, w, eff_root) for e, _ in T['effects']}
        a = next((c for c in row['ent'].children if c.id == 0x3000600), None)
        cls, phys = struct.unpack_from('<II', a.data) if a else (4, 1)
        if cls == 4 and phys == 1 and re.fullmatch(r'l\d_fence_Shape', name) and os.environ.get('GATE_BREAK_OLD') == '1': cls = SP_CLS_HARDFENCE   # №564: в оригинале деревянные ворота фермы cls 4
        T['cls'], T['atc'] = cls, phys
        T['mass'] = self.atc[phys]['f'][1] if phys < len(self.atc) else 100.0
        states = []
        for st in sp['states']:
            vis = 0
            names = dict(st['vis'])
            for i, s in enumerate(T['slots']):
                on = names.get(s, 1 if i < len(T['props']) else 0)
                if on: vis |= 1 << i
            pose = next((fc for fc in st['fc'] if fc['name'].startswith('PTRN_')), None)
            if pose is None: pose = next((fc for fc in st['fc'] if fc['name'].startswith('PVIS_')), None)
            if pose is None or pose['min'] == pose['max']: pm, pmin, pmax, psp, pcy = 0, (pose['min'] if pose else 0), (pose['max'] if pose else 0), 1.0, 0
            else: pm, pmin, pmax, psp, pcy = (2 if pose['cyclic'] else 1), pose['min'], pose['max'], pose['speed'], pose['cycles']
            calls = []
            for cn, cid, fr in st['callbacks']:
                oa = call_op(cn)
                if oa is None: self.log(f'    {name}: неизвестный вызов {cn}({cid}) — пропущен'); continue
                calls.append((oa[0], oa[1], fr))
            calls.sort(key=lambda c: c[2])
            if len(calls) > 16: raise ValueError(f'{name}: >16 вызовов в состоянии')
            states.append(dict(vis=vis, auto=(st['outState'] if st['auto'] else 0xFF), autoTime=st['outFrame'],
                               pose=(pm, int(pmin), int(pmax), psp, pcy), events=[(eid, to) for _, to, eid in st['events']],
                               calls=calls, fx=self._fx_entries(T, st['fc'])))
        T['states'] = states
        tags = []
        for n, M in row['inst']:
            m = re.search(r'powerbox(\d+)$', n); tags.append(int(m.group(1)) if m else 0)
        T['inst'] = [(M, tg) for (n, M), tg in zip(row['inst'], tags)]
        T['files'] = row['files']
        self.types.append(T)

    def _add_animphys(self, name, row):
        """Барьер 0x3F0000F -> тип SPR1 (правила — у bqg_quad_on): s0 стоит, огни мигают по кругу; удар — s1 обломок."""
        f, w = row['file'], row['w']
        T = self._base_type(name, f, w, 0)
        nfr, fps = anim_frames(w)
        on = bqg_quad_on(w, nfr) if nfr else {}
        qnames = bqg_quad_names(w)
        seen = {}
        vis_frames, mask = [0] * nfr, 0
        for b in T['bills']:   # квадрат с каналом огня -> свой слот (видимость слота по кадрам — PVIS)
            k = seen.get(b['name'], 0); seen[b['name']] = k + 1
            qs = qnames.get(b['name'], [])
            qn = qs[k] if k < len(qs) else None
            if qn is None or qn not in on: continue
            slot = len(T['slots']); T['slots'].append(qn); b['slot'] = slot; b['pvis'] = 1; mask |= 1 << slot
            for fr in range(nfr):
                if on[qn][fr]: vis_frames[fr] |= 1 << slot
        if len(T['slots']) > SLOT_LIMIT: raise ValueError(f'{name}: {len(T["slots"])} слотов > {SLOT_LIMIT}')
        if mask:
            T['nfr'], T['ntrack'], T['keys'], T['fps'] = nfr, 0, [], fps
            T['pvis_mask'], T['vis_frames'] = mask, vis_frames
        a = next((c for c in row['ent'].children if c.id == 0x3000600), None)
        cls, phys = struct.unpack_from('<II', a.data) if a else (4, 1)
        if cls == 4 and phys == 1 and re.fullmatch(r'l\d_fence_Shape', name) and os.environ.get('GATE_BREAK_OLD') == '1': cls = SP_CLS_HARDFENCE
        T['cls'], T['atc'] = cls, phys
        T['mass'] = self.atc[phys]['f'][1] if phys < len(self.atc) else 100.0
        brk = BREAK_BY_ATC.get(self.atc[phys]['breakable'] if phys < len(self.atc) else '', None)
        allv = (1 << len(T['slots'])) - 1
        pose = (2, 0, nfr, 1.0, 0) if mask else (0, 0, 0, 1.0, 0)
        idle = dict(vis=allv, auto=0xFF, autoTime=0.0, pose=pose, events=[], calls=[], fx=[])
        states = [idle]
        if brk is not None:   # удар машины / пинок Гомера -> обломок и уход из мира
            idle['events'] = [(EV['VEHICLE_HIT'], 1), (EV['HIT'], 1)]
            states.append(dict(vis=0, auto=0xFF, autoTime=0.0, pose=(0, 0, 0, 1.0, 0), events=[],
                               calls=[(OP_BREAK, brk, 0.0), (OP_COLOFF, 0, 0.0), (OP_REMOVE, 0, 0.0)], fx=[]))
            self.animphys_break[name] = brk
        T['states'] = states
        T['inst'] = [(M, 0) for _, M in row['inst']]
        T['files'] = row['files']
        T['animphys'] = dict(frames=nfr, fps=fps, lights=bin(mask).count('1'), breakable=brk)
        self.types.append(T)
        self.log(f'  барьер 0x3F0000F {name}: огней {bin(mask).count("1")}, {nfr} кадров @{fps:g}, обломок {brk}, x{len(T["inst"])}')

    def _add_breakables(self, files):
        """Обломки для DynaPhys cls 4 уровня (из строк ATC): тип kind 1 с одной «игрой» и пулом экземпляров."""
        want = {}
        for f in files:
            for ent in read(f).children:
                if ent.id != 0x3f00002: continue
                a = next((c for c in ent.children if c.id == 0x3000600), None)
                if not a: continue
                cls, phys = struct.unpack_from('<II', a.data)
                if cls == 2 or phys >= len(self.atc): continue
                b = self.atc[phys]['breakable']
                if b in BREAK_BY_ATC: want.setdefault(BREAK_BY_ATC[b], set()).add(N(ent))
                elif b: self.log(f'  обломок {b} ({N(ent)}) — нет в таблице индексов, пропущен')
        for n, idx in getattr(self, 'animphys_break', {}).items(): want.setdefault(idx, set()).add(n)   # барьеры 0x3F0000F
        self.break_of_dyn = {}   # имя DynaPhys -> номер типа обломка в self.types
        srcs = files + [self.art / f'l0{self.level}_fx.p3d']
        done = {}
        def load(flist, only=None):
            for f in flist:
                if not f.exists(): continue
                for c in read(f).children:
                    if c.id != 0x3001000: continue
                    idx, cnt = struct.unpack_from('<2I', c.data)
                    if idx not in want or idx in done or (only is not None and idx not in only): continue
                    props, effects = composite(c)
                    T = self._base_type(f'break{idx}', f, c, 1)
                    T['fx_text'] = {e: fx_text_for(e, c, []) for e, _ in effects}
                    T['cls'], T['atc'], T['mass'] = 0, 0, 0.0
                    nfr = max(ptrn_info(c)[0], 1); fps = T['fps'] or 30.0
                    life = (nfr - 1) / fps
                    allvis = (1 << len(props)) - 1
                    fx = [(len(props) + i, 1, 0, fx_preset(T['fx_text'][e]), 0, max(nfr - 1, 1), 1.0) for i, (e, _) in enumerate(effects)][:MAX_FX]
                    T['states'] = [dict(vis=0, auto=0xFF, autoTime=0.0, pose=(0, 0, 0, 1.0, 0), events=[], calls=[], fx=[]),
                                   dict(vis=(1 << (len(props) + len(effects))) - 1, auto=0, autoTime=life,
                                        pose=(1 if T['nfr'] else 0, 0, max(T['nfr'] - 1, 0), 1.0, 0), events=[], calls=[], fx=fx)]
                    T['pool'], T['bindex'], T['life'] = max(1, min(cnt, 4)), idx, life
                    T['inst'] = [(np.eye(4), 0)] * T['pool']
                    done[idx] = len(self.types); self.types.append(T)
                    self.log(f'  обломок {idx} {props[0][0] if props else "(без мешей)"}: {len(T["pieces"])} кусков, '
                             f'{T["nfr"]} кадров @{fps:g}, {life:.2f} с, пул {T["pool"]}' + (f' (из {f.name}, файла другого уровня)' if only is not None else ''))
        load(srcs)
        # Правка «обломки с диска для всех типов» (10.10, боты: кладбищенские надгробия L1/L4/L7, лавка L6): BreakableObject с этим номером
        # есть в файлах другого уровня (22 скелеты — l7r7, 28 пальма — l3r1, 8 томако — l1r6), а у уровня своего нет, и тип оставался
        # без обломка (проп просто летел). Номер BreakablesEnum общий для игры — берём первый найденный в art/l*.p3d. -DBREAK_XLEVEL_OLD=1 в окружении — прежнее
        miss = {i for i in want if i not in done}
        if miss and not os.environ.get('BREAK_XLEVEL_OLD'):
            # заимствуем обломок, только если у объекта с тем же именем (без номера уровня) в другом уровне ЭТОТ ЖЕ обломок по ATC
            # (скелет надгробий l1/l4r7 = l7r7, томако у лавки l6 = l1r6/l4r6); пальма 28 у надгробия c — чужой смысл, без обломка
            def norm(n): return re.sub(r'^l\d', 'l#', re.sub(r'r\d_', 'r#_', n.lower()))
            donor = {}   # номер обломка -> нормированные имена DynaPhys других уровней с этим обломком
            for lv in range(1, 8):
                if lv == self.level: continue
                present = set()   # номера BreakableObject, которые у уровня-донора действительно есть в файлах
                for f in level_files(self.art, lv) + [self.art / f'l0{lv}_fx.p3d']:
                    if f.exists(): present |= {struct.unpack_from('<I', c.data)[0] for c in read(f).children if c.id == 0x3001000}
                for f in level_files(self.art, lv):
                    for ent in read(f).children:
                        if ent.id != 0x3f00002: continue
                        a = next((c for c in ent.children if c.id == 0x3000600), None)
                        if not a: continue
                        cls, phys = struct.unpack_from('<II', a.data)
                        if cls == 2 or phys >= len(self.atc): continue
                        b = BREAK_BY_ATC.get(self.atc[phys]['breakable'])
                        if b is not None and b in present: donor.setdefault(b, set()).add(norm(N(ent)))
            for idx in list(miss):
                ok = {n for n in want[idx] if norm(n) in donor.get(idx, ())}
                if ok: want[idx] = ok
                else: want.pop(idx); miss.discard(idx); self.log(f'  обломок {idx} ({sorted(want.get(idx, []))[:1]}) — у такого объекта в других уровнях его нет, без обломка')
            others = sorted(self.art.glob('l[1-7]*.p3d')) + sorted(self.art.glob('l0[1-7]_fx.p3d'))
            if miss: load([f for f in others if f not in srcs], only=miss)
        for idx, names in want.items():
            if idx not in done: self.log(f'  обломок {idx} не найден в файлах уровня'); continue
            for n in names: self.break_of_dyn[n] = done[idx]

    def textures(self):
        return {p[2] for T in self.types for p in T['pieces'] if p[2] >= 0} | {b['tex'] for T in self.types for b in T['bills'] if b['tex'] >= 0}

    # ---- запись
    def section(self, tmap, prp1_type_names=()):
        vblob, iblob = bytearray(), bytearray()
        precs, trecs, jrecs, keys, visw, srecs, erecs, crecs, colrecs, brecs, irecs = [], [], [], [], [], [], [], [], [], [], []
        kv_at = {}   # тип -> (keyOff, visOff): типы-кластеры одного объекта (tools/animobj_export.py) делят кадры клипа
        for ti, T in enumerate(self.types):
            fp, fj, fs, fc, fb, ko, vo = len(precs), len(jrecs), len(srecs), len(colrecs), len(brecs), len(keys), len(visw)
            vw = vis_words(T)
            share = T.get('keys_of')
            if share is not None and vis_words(self.types[share]) != vw: share = None   # кадры PVIS другой ширины — свои
            if share is not None: ko, vo = kv_at[share]
            kv_at[ti] = (ko, vo)
            for slot, joint, t, fl, P, UV, C in T['pieces']:
                if not 0 <= slot < SLOT_LIMIT: raise ValueError(f"{T['name']}: слот {slot} вне 0..{SLOT_LIMIT - 1}")
                lo_, hi_ = P.min(0), P.max(0); c = (lo_ + hi_) / 2; S = max(float((hi_ - lo_).max()) / 2, 1e-3)
                q = np.zeros(len(P), QV)
                q['x'], q['y'], q['z'] = [np.clip(np.round((P[:, j] - c[j]) / S * 32767), -32767, 32767) for j in range(3)]
                q['u'], q['v'] = uvq.tri_i16(UV[:, 0], UV[:, 1])   # GE читает u16 (tools/uvq.py, правка Роба 43)
                q['c'] = C
                # без индексов (шаг 3 аудита): вершины подряд по 3 на треугольник, nv = ni, iOff = 0;
                # флаги 16 = без индексов, 8 = двусторонний (обход у мелких объектов не проверяем — рисуем без отсечения)
                vOff = len(vblob); vblob += q.tobytes(); vblob += b'\0' * (-len(vblob) % 16)
                flags = ((fl | 8 | 16) & (0xFF | (1 << 24))) | ((joint & 0xFF) << 8) | ((slot & 0xFF) << 16)   # слот — биты 16–23
                precs.append(struct.pack('<4f2i4I', *c, S * 32768 / 32767, tmap.get(t, -1), flags, len(q), len(q), vOff, 0))
            for jt in T['joints']:
                jrecs.append(struct.pack('<hhI4f3f3f4x', jt['parent'], jt['track'], 0, *jt['q'], *jt['t'], *jt['s']))
            if share is None:
                for k in T['keys']: keys.append(struct.pack('<7f', *k))
                visw += [struct.pack('<I', w) for m in T['vis_frames'] for w in mask_words(m, vw)]   # vw слов на кадр
            pvx = 0
            if vw > 1:   # слова 1.. маски PVIS (слово 0 — SpType.pvisMask)
                pvx = len(visw); visw += [struct.pack('<I', w) for w in mask_words(T['pvis_mask'], vw)[1:]]
            for st in T['states']:
                pm, pmin, pmax, psp, pcy = st['pose']
                fe, fcall = len(erecs), len(crecs)
                for eid, to in st['events']: erecs.append(struct.pack('<BBH', eid, to, 0))
                for op, arg, tm in st['calls']: crecs.append(struct.pack('<BBHf', op, arg, 0, tm))
                fx = b''.join(struct.pack('<4B2Hf', slot, mode, min(cy, 255), pre, mn, mx, spd) for slot, mode, cy, pre, mn, mx, spd in st['fx'])
                fx += b'\0' * (12 * MAX_FX - len(fx))
                vx = 0
                if vw > 1:   # слова 1.. маски состояния; слово 0 — SpState.vis
                    vx = len(visw); visw += [struct.pack('<I', w) for w in mask_words(st['vis'], vw)[1:]]
                srecs.append(struct.pack('<I4B2HfHHHHf', int(st['vis']) & 0xFFFFFFFF, st['auto'], len(st['events']), len(st['calls']), pm,
                                         pmin, pmax, psp, min(pcy, 65535), fe, fcall, len(st['fx']), st['autoTime']) + fx + struct.pack('<I', vx))
            for cl in T['cols']:
                colrecs.append(struct.pack('<BBH3f9f3f', cl['kind'], 1 if cl is not T['cols'][0] else 0, 0, *cl['c'], *cl['A'].reshape(-1), *cl['h']))
            for b in T['bills']:
                brecs.append(struct.pack('<4Bi6fII', b['joint'], b['slot'], b['mode'], 0, tmap.get(b['tex'], -1), *b['t'],
                                         b['w'], b['h'], b['dist'], b['rgba'], 3 | (b.get('blend', 0) << 8) | (b.get('pvis', 0) << 16)))
            c = (T['lo'] + T['hi']) / 2; hlf = (T['hi'] - T['lo']) / 2
            for b in T['bills']:
                if not 0 <= b['slot'] < SLOT_LIMIT: raise ValueError(f"{T['name']}: слот билборда {b['slot']} вне 0..{SLOT_LIMIT - 1}")
            # @60 было 2 f32 нуля: visWords (0 — одно слово, как до 29.09) и pvisX — те же нулевые байты у типов до 32 слотов
            trecs.append(struct.pack('<10H2H4B4I2f2I3f3f', fp, len(precs) - fp, fj, len(T['joints']), fs, len(T['states']),
                                     fc, len(T['cols']), fb, len(T['bills']), T['kind'], T.get('bindex', 0),
                                     T['cls'], min(T['atc'], 255), int(round(T['fps'] or 30)), T.get('pool', 0),
                                     ko, vo, T['nfr'] | (T['ntrack'] << 16), int(T['pvis_mask']) & 0xFFFFFFFF,
                                     T['mass'], T.get('life', 0.0), vw if vw > 1 else 0, pvx, *c, *hlf))
            for M, tag in T['inst']:
                w_, x, y, z = quat_wxyz(M[:3, :3].T)
                irecs.append(struct.pack('<3f4fHH', M[3, 0], M[3, 1], M[3, 2], w_, x, y, z, ti, tag))
        dyn = bytes(self.break_of_dyn.get(n, 0xFF) for n in prp1_type_names)
        # имена систем частиц эффектов (слоты EFX): 32 Б = имя[28] (с нулём), u16 тип, u8 слот, u8 сустав.
        # Рантайм ищет систему по имени в fx.shr (fxFind) и играет её целиком вместо пресетов SPFX_*.
        names = bytearray()
        for ti, T in enumerate(self.types):
            for e, joint in T['effects']:
                if e not in T['slots'] or len(e) > 27: continue
                names += struct.pack('<28sHBB', e.encode('latin1'), ti, T['slots'].index(e), min(joint, 255))
        self.fx_names = len(names) // 32
        parts = [bytes(vblob), bytes(iblob), b''.join(precs), b''.join(trecs), b''.join(jrecs), b''.join(keys), b''.join(visw),
                 b''.join(srecs), b''.join(erecs), b''.join(crecs), b''.join(colrecs), b''.join(brecs), b''.join(irecs), dyn, bytes(names)]
        head = b'SPR1' + struct.pack('<15I', len(self.types), len(irecs), len(precs), len(jrecs), len(keys), len(visw),
                                     len(srecs), len(erecs), len(crecs), len(colrecs), len(brecs), len(vblob), len(iblob), len(dyn),
                                     self.fx_names)   # @60 (было 0): число имён систем частиц — часть 15 после dyn
        assert len(head) == HDR
        out = bytearray(head)
        for p in parts:
            out += p; out += b'\0' * (-len(out) % 16)
        # хвост BQG1 (правка Роба 76): покадровые ширина/высота/цвет квадов свечения — после 15 частей; старые файлы без хвоста
        bills = [b for T in self.types for b in T['bills']]
        if any(b.get('track') for b in bills):
            idx, rows = [], []
            for b in bills:
                tr = b.get('track') or []
                idx.append(struct.pack('<2I', len(rows), len(tr))); rows += tr
            out += b'BQG1' + struct.pack('<3I', len(bills), len(rows), 0) + b''.join(idx)
            out += b''.join(struct.pack('<2fI', *r) for r in rows); out += b'\0' * (-len(out) % 16)
            self._bqg_bytes = 16 + 8 * len(bills) + 12 * len(rows)
        self.sizes = dict(zip(['vert', 'idx', 'piece', 'type', 'joint', 'key', 'vis', 'state', 'event', 'call', 'col', 'bill', 'inst', 'dyn'],
                              [len(p) for p in parts]))
        self.sizes['fxname'] = len(names)
        self.sizes['bqg'] = getattr(self, '_bqg_bytes', 0)
        return bytes(out)

    def add_wrenches(self, positions):
        """Гаечные ключи (пункт 4 ведущего 29.09): составной рисунок wrench из art/wrench.p3d (меш wrench7Shape, блики
        wrenchShape*, вращение PTRN_wrench) — тип, как карточка у tools/collect_export.card_type: одно состояние с позой по
        кругу, насквозь для машины (cls 2), без событий; экземпляр в точке каждого ключа (ActionEvent Wrench, JMP1).
        Берёт/прячет/возвращает ключ viewer/game.c (сверка экземпляра с ключом — по точке)."""
        return self.add_pickup(self.art / 'wrench.p3d', 'wrench', positions)

    def add_pickup(self, f, name, positions, hidden=False):
        """Подбираемый предмет — составной рисунок name файла f (wrench — art/wrench.p3d; nitro — art/missions/generic/nitro.p3d,
        его грузит сценарий мини-игры ss.mfk) тем же правилом, что ключ: одно состояние с позой по кругу, насквозь (cls 2)."""
        import collect_export as CX
        w = CX.sub_chunk(read(f), name)
        T = self._base_type(name, f, w, 0)
        T['cls'], T['atc'], T['mass'] = 2, 0, 0.0
        T['fx_text'] = {e: e for e, _ in T['effects']}
        n = max(T['nfr'] - 1, 0)
        T['states'] = [dict(vis=(1 << len(T['props'])) - 1, auto=0xFF, autoTime=0.0, pose=(2 if n else 0, 0, n, 1.0, 0),
                            events=[], calls=[], fx=[])]
        if hidden:   # №314: предмет миссии (бочка L7 AddCollectibleStateProp) стоит скрытым (состояние 0), движок включает состояние 1 на её этапе
            T['states'].insert(0, dict(vis=0, auto=0xFF, autoTime=0.0, pose=(0, 0, 0, 1.0, 0), events=[], calls=[], fx=[]))
        T["inst"] = [(CX.inst_matrix(p), 0) for p in positions]
        T['files'] = {name: len(positions)}
        self.types.append(T)
        return T

    def report(self):
        for T in self.types:
            ni = len(T['inst']); nv = sum(len(p[4]) for p in T['pieces'])
            extra = f"пул {T['pool']}" if T['kind'] else f"x{ni} cls {T['cls']} atc {T['atc']} m {T['mass']:g} " + \
                    ' '.join(f'{k}:{v}' for k, v in T.get('files', {}).items())
            self.log(f"  {T['name']:26s} {len(T['pieces']):3d} кусков {nv // 3:5d} треуг. {len(T['joints']):2d} суст. "
                     f"{T['ntrack']:2d} дорожек x {T['nfr']:3d} кадров, {len(T['states'])} сост., {len(T['cols'])} объёмов, "
                     f"{len(T['bills'])} билб. {extra}")

def wrap_prp2(prp1, spr, type_fx=b'', anm=b''):
    """PRP1 (заголовок 32 Б) -> PRP2: заголовок 64 Б (первые 32 — как PRP1, магия PRP2), тело PRP1, SPR1 с 16-выравниванием.
    @32 смещение SPR1 от начала секции, @36 её размер, @40 полный размер секции.
    @52/@56 смещение/размер таблицы ANM1 (tools/animobj_export.py), 0 — нет.
    @44 смещение таблицы частиц типов PRP1 (после SPR1, 0 нет), @48 число типов в ней: по 2 Б на тип —
    [частицы ATC: 0 нет, 1 искры кодом (eSpark), 2 звёзды (eStars, текстуры нет — не рисуем), 3..19 номер
    InstParticleSystem (3 куст, 4 мусор, 5 листья дуба, 6 почта)], [обломок BreakablesEnum (3 гидрант, 15 дуб…), 0xFF нет]."""
    assert prp1[:4] == b'PRP1'
    body = prp1[32:]
    head = bytearray(b'PRP2' + prp1[4:32] + b'\0' * 32)
    out = head + body; out += b'\0' * (-len(out) % 16)
    off = len(out); out += spr
    struct.pack_into('<3I', out, 32, off, len(spr), len(out))
    if type_fx:
        out += b'\0' * (-len(out) % 16); toff = len(out); out += type_fx; out += b'\0' * (-len(out) % 16)
        struct.pack_into('<I', out, 40, len(out)); struct.pack_into('<2I', out, 44, toff, len(type_fx) // 2)
    if anm:   # @52/@56: таблица ANM1 анимированных объектов (tools/animobj_export.py, viewer/animobj.c); @40 — вся секция
        out += b'\0' * (-len(out) % 16); aoff = len(out); out += anm; out += b'\0' * (-len(out) % 16)
        struct.pack_into('<I', out, 40, len(out)); struct.pack_into('<2I', out, 52, aoff, len(anm))
    return bytes(out)

# частицы ATC (колонка «частицы») -> код таблицы частиц типов (номера InstParticleSystem одинаковы на всех уровнях, docs/fx_spec.md §3)
PARTICLE_CODE = {'eSpark': 1, 'eStars': 2, 'eShrub': 3, 'eGarbage': 4, 'eOakTreeLeaves': 5, 'eMail': 6}
# сверка g12-data P11-13 (02.11.2026): оригинал знает девять имён частиц удара (eGarbage, eShrub, eOakTreeLeaves, eMail, ePineTreeNeedles, eStars,
# eParkingMeter, eCarExplosion, ePopsicles); любое другое (eSpark на фонарях и светофорах) — «нет частиц». Было: eSpark = 14 искр на каждый удар
# (придумано по впечатлению от PS2), ePineTreeNeedles / eParkingMeter / ePopsicles в таблицу не попадали. Стало: eSpark -> 0, добавлены коды 7, 24, 26
# (номера систем, которых нет в l0N_fx.p3d: рисует код — окно g11-objects, viewer/main_state.h). PRP2_PARTICLES_OLD=1 — прежняя таблица
if os.environ.get('PRP2_PARTICLES_OLD') != '1':
    del PARTICLE_CODE['eSpark']
    PARTICLE_CODE.update({'ePineTreeNeedles': 7, 'eParkingMeter': 24, 'ePopsicles': 26})
def type_fx_table(prop_types, atc_rows):
    """prop_types: dict с 'mat' (physProp = строка ATC). -> bytes по 2 Б на тип (см. wrap_prp2)."""
    out = bytearray()
    for ty in prop_types:
        m = ty.get('mat', -1)
        row = atc_rows[m] if 0 <= m < len(atc_rows) else None
        part = PARTICLE_CODE.get(row['particle'], 0) if row else 0
        brk = BREAK_BY_ATC.get(row['breakable'], 0xFF) if row else 0xFF
        out += bytes((part, brk))
    return bytes(out)

# ------------------------------------------------------------------ проверка отдельно
def _standalone(dump=False):
    import psp_export as PE
    from convert import Converter, OUT
    art = PE.GAME / 'art'; conv = Converter(); cache, blob = {}, []
    for path in sorted(art.glob('l1*.p3d')):
        try: conv.resources(read(path))
        except Exception as e: print('skip', path.name, e)
    def pack(f, mesh):
        tn = f'psp_sp_tmp_{os.getpid()}'   # номер процесса в имени: параллельные экспорты не затирают общий файл
        conv.export(f, tn, mesh_name=mesh)
        meta = json.loads((OUT / (tn + '.json')).read_text()); raw = (OUT / (tn + '.bin')).read_bytes()
        for ext in ('json', 'bin'): (OUT / f'{tn}.{ext}').unlink()
        return PE.pack_prims(meta, raw, cache, blob)
    def tex_of_shader(f, sh):
        s = conv.shaders.get(sh)
        return (PE.load_tex(conv.textures.get(s['texture'], ''), cache, blob, blend=True), int(s['blend'])) if s else -1   # билборды — полупрозрачные
    import contextlib, io
    with contextlib.redirect_stdout(io.StringIO()) if not dump else contextlib.nullcontext():
        sp = StateProps(1, art, pack, tex_of_shader)
    sp.report()
    tmap = {t: i for i, t in enumerate(sorted(sp.textures()))}
    names = list(PE.prop_entities([p.stem for p in level_files(art, 1)], art)) + ['coinShape_000']
    body = sp.section(tmap, names)
    print(f'SPR1: {len(body) / 1024:.1f} КБ; ' + ', '.join(f'{k} {v}' for k, v in sp.sizes.items()))
    print(f'убрать из города: {len(sp.city_skip)} мешей: {sorted(sp.city_skip)}')
    print(f'текстур: {len(tmap)}; DynaPhys с обломками: {sum(1 for n in names if n in sp.break_of_dyn)} из {len(names)}')
    out = ROOT / 'build' / 'spr1_test.bin'; out.write_bytes(body); print('->', out)
    for T in sp.types:
        if T['name'] in ('l1_vending_Shape', 'l1_crate_Shape'):
            JW = T['JW']
            for (slot, j, t, fl, P, UV, C) in T['pieces'][:8]:
                cen = (np.c_[P, np.ones(len(P))] @ JW[j].T)[:, :3].mean(0)
                print(f"    {T['name']} слот {slot} {T['slots'][slot]} сустав {j} центр {np.round(cen, 2)}")
            for b in T['bills']:
                R = JW[b['joint']][:3, :3]
                print(f"    билборд {b['name']} режим {b['mode']} ось Z сустава {np.round(R[:, 2], 2)} {b['w']:.2f}x{b['h']:.2f} blend {b['blend']}")

if __name__ == '__main__':
    _standalone('--dump' in sys.argv)
