#!/usr/bin/env python3
"""Анимированные объекты уровня: AnimCollision 0x3F00008 и Anim 0x3F0000C -> город / SPR1 / COL1 / ANM1.

Почему (пакет «Объекты уровня и миссий», правка Роба 10 от 28.09): конвертер hit-and-run-web ставит в город только
StaticEntity 0x3F00000, граф сцены 0x120100 и экземпляры InstanceList; составной рисунок (CompositeDrawable 0x4512)
этих двух семейств на диске НИКЕМ не ставится, а их коллизия шла в «не в COL1». На уровне 1 не было: грузовика Duff
у Kwik-E-Mart с подъёмником (action PlayAnim у двери, путь на крышу к карточке card12), ветки у дома Симпсонов,
табличек-звонков, кондиционера школы, 20 платформ/вентиляторов/шестерни/дверей/пружин АЭС (l1z6), щитов l1r6,
а из Anim — мельниц, вращающихся вывесок, рек, бабочек, вывесок Krusty и Itchy&Scratchy, мигалок, стрелок гонок.

Источник — только диск Роба (read-only): скелет 0x4500, PTRN/PVIS 0x121000, меши 0x10000, билборды 0x17002,
объёмы CollisionObject 0x7010000 внутри сущности, action-локаторы 0x3000005 вида 9 (объект, сустав, тип действия,
TriggerVolume). Опоры: prp2_export.py (стейтпропы SPR1: скелет, позы по кадрам, объёмы), p3dlocs.py (локаторы),
hit-and-run-web/tools (MIT).

Что куда:
  * части, которые не двигаются (у объекта нет дорожек PTRN и PVIS) и являются мешами -> город (клетки/PVS/LOD, как все
    меши), поза кадра 0; билборды и всё движущееся -> типы SPR1 (стейтпропы) с одним состоянием: по кругу, если клип
    помечен в данных как циклический и у объекта нет action-локатора, иначе стоит на кадре 0 и кадр ведёт движок
    (viewer/animobj.c по таблице ANM1). Части одного объекта, разнесённые дальше CLUSTER м, и сверх 32 рисунков ->
    отдельные типы-«кластеры» с общим скелетом (SPR1 отсекает экземпляр по его центру; маска видимости 32 бита).
  * объёмы AnimCollision -> ящики/цилиндры COL1 (по ним ездят и ходят, верх проходим, как трамплин пожарки):
    объём на суставе без движения — статичный, в позе кадра 0; на движущемся суставе — «живой»: в COL1 стоит поза
    кадра 0, в сетку записан во все клетки, которые он заметает за весь клип (sweep), а движок каждый кадр
    переставляет его по позе сустава (ANM1). Своих объёмов у типов SPR1 нет (упор машины в COL1, не в SPR1).
Действия (данные action-локатора; ЧТО делает кнопка — в данных только имя действия, поведение — правило порта,
сверить с эталоном PS2, docs/gameplay.md §8 «Анимации по кнопке, по ним можно ходить»):
  1 PlayAnim     △ в объёме триггера -> клип один раз с начала до конца (подъёмник Duff: вверх, пауза, вниз)
  2 AutoPlayAnim вход в объём триггера -> клип один раз
  3 ToggleOnOff  △ -> клип по кругу / пауза на текущем кадре; в начале стоит (кадр 0)
  4 AutomaticDoor игрок в объёме -> клип вперёд до конца (открыта), вне объёма -> назад до кадра 0
  5 Doorbell     кнопка звонка: визуально — цикл клипа из данных; реплика персонажа не перенесена (нет в этом пакете)
  (DestroyObject — щитки m4, это стейтпропы 0x3F0000E, уже в SPR1.)

Формат ANM1 (little endian, в секции PRP2 уровня: смещение @52, размер @56 от начала PRP2, 0 — нет):
  заголовок 32 Б: 'ANM1', u32 версия 1, u32 объектов, u32 объёмов, u32 смещение объектов, u32 смещение объёмов, 8 Б 0
  объект 80 Б: u16 первый экземпляр SPR1, u16 экземпляров, u16 первый объём, u16 объёмов, u8 действие (0 нет, 1..5 выше),
    u8 триггер (0 нет, 1 сфера, 2 ящик), u16 0, u16 кадров клипа, u16 0, f32 кадров/с, f32 центр триггера[3],
    f32 полуразмеры[3] (сфера: радиус в [0]), f32 оси триггера[9] (строки)
  объём 72 Б: u8 вид (0 ящик, 1 цилиндр/капсула), u8 0, u16 объект, u32 номер в массиве COL1 (ящиков или цилиндров),
    u16 сустав, u16 0, f32 центр[3] в осях сустава, f32 оси[9] (ящик: 3 строки; цилиндр: ось в [0..2]),
    f32 полуразмеры[3] (цилиндр: радиус, половина длины, плоские торцы 0/1)
Координаты движка (z диска отзеркален), позы — как viewer/stateprops.c spPose (мир = экземпляр + сустав).

Проверка отдельно (без записи уровня):  python tools/animobj_export.py [--level 1]   -> отчёт по семьям
"""
import sys, struct, math
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools')); sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/missions'))
from p3d import read, walk, string, Chunk      # noqa: E402  (MIT)
from convert import geometry                   # noqa: E402  (MIT)
import prp2_export as PX                       # noqa: E402
import char_export as CE                       # noqa: E402
import p3dlocs                                  # noqa: E402

ANIM_COLL, ANIM = 0x3f00008, 0x3f0000c
FAMILIES = {ANIM_COLL: 'AnimCollision', ANIM: 'Anim'}
ACTIONS = {'PlayAnim': 1, 'AutoPlayAnim': 2, 'ToggleOnOff': 3, 'AutomaticDoor': 4, 'Doorbell': 5,
           'PlayOnce': 6}   # круг 5 (g07b, P05-18): PlayOnce — как PlayAnim, но первое △ навсегда снимает триггер; на диске один: L4 l4r4b «library» switch_1
# сверка g12-data P05-7 (02.11.2026): последнее слово данных action-локатора — признак «следовать за суставом» (триггер выключателя и стоянка едут
# вместе с суставом): 1 у шести локаторов на диске (звонок Quimby DB_quimby на l1r3, l1z4, l4r3, l4z4; платформа Splatfrom7Trans AutoPlayAnim на
# l1z6, l4z6), у остальных 434 — 0. В запись ANM1 (80 Б): H после tk — бит 1 «следует за суставом», H после nfr — номер сустава действия + 1
# (0 — нет данных). Читает движок (viewer/animobj.c, окно g11-objects). AO_FOLLOW_OLD=1 — оба поля 0 (прежний файл)
AO_FOLLOW_OLD = __import__('os').environ.get('AO_FOLLOW_OLD') == '1'
AO_FOLLOW = {}   # имя объекта -> последнее слово данных action-локатора (заполняет action_locators)
CLUSTER = 30.0          # м по xz: части дальше — отдельный экземпляр SPR1 (отсечение по центру экземпляра)
BILL_CENTER_OLD = __import__('os').environ.get('AO_BILL_CENTER_OLD') == '1'   # красный контроль: тип из одних билбордов — в начале координат
BILL_FRAMES_OLD = __import__('os').environ.get('AO_BILL_FRAMES_OLD') == '1'   # красный контроль: квады BQG при стоящих суставах — на кадре 0
N = lambda c: string(c.data)[0]
NV = lambda c: string(c.data, 4)[0]


def gather(root, ent):
    """Сущность + её скелет, клипы PTRN/PVIS, BQG/EFX частей, меши, билборды, системы частиц -> один Chunk (как sub_chunk
    tools/collect_export.py, но составной рисунок и объёмы лежат ВНУТРИ сущности, скелет и клипы — в корне файла)."""
    comp = next(c for c in walk(ent) if c.id == 0x4512)
    name = N(comp); skel = string(comp.data, string(comp.data)[1])[0]
    kids = [comp] + [c for c in root.children if c.id == 0x4500 and N(c) == skel]
    kids += [c for c in ent.children if c.id in (0x7010000, 0x17002)]
    props, effects = PX.composite(Chunk(0, b'', [comp]))
    parts = {n for n, _, _ in props} | {n for n, _ in effects}
    for c in root.children:
        if c.id == 0x121000:
            k, _, rest = NV(c).partition('_')
            if (k in ('PTRN', 'PVIS') and rest in (name, skel, N(ent))) or (k in ('BQG', 'EFX') and rest in parts): kids.append(c)
        elif c.id == 0x10000 and N(c) in parts: kids.append(c)
        elif c.id in (0x17002, 0x15800, 0x15801) and NV(c) in parts and c not in kids: kids.append(c)
    return Chunk(0, b'', kids)


def with_parts(w, keep):
    """Копия w, в составном рисунке которой только рисунки/эффекты с именами из keep (кластер)."""
    def filt(c):
        if c.id in (0x4516, 0x4518): return c if string(c.data)[0] in keep else None
        return Chunk(c.id, c.data, [x for x in (filt(k) for k in c.children) if x is not None])
    return Chunk(0, b'', [filt(c) if c.id == 0x4512 else c for c in w.children])


def ptrn_loop(w):
    for an in w.children:
        if an.id != 0x121000: continue
        _, p = string(an.data, 4)
        if an.data[p:p + 4] == b'PTRN': return struct.unpack_from('<ffI', an.data, p + 4)[2]
    return 0


def pose_world(joints, keys, ntrack, f):
    """Мировые 4x4 (столбцовые, оси объекта = мир) суставов на кадре f — как spPose (без интерполяции между кадрами)."""
    W = []
    for jt in joints:
        if jt['track'] >= 0 and ntrack:
            k = keys[f * ntrack + jt['track']]; t = np.array(k[:3]); q = k[3:]
        else: t = np.asarray(jt['t'], np.float64); q = jt['q']
        w_, x, y, z = q; L = np.eye(4); L[:3, :3] = CE.m_from_q((x, y, z, w_)) * jt['s']; L[:3, 3] = t
        W.append(L if jt['parent'] < 0 else W[jt['parent']] @ L)
    return W


def moving_joints(joints):
    """Суставы, которые двигаются сами или через предка."""
    mv = []
    for j, jt in enumerate(joints):
        mv.append(jt['track'] >= 0 or (jt['parent'] >= 0 and mv[jt['parent']]))
    return mv


def action_locators(files):
    """{имя объекта: (действие, сустав, триггер (isRect, центр, полуразмеры, оси), имя локатора)} из локаторов вида 9."""
    out = {}
    for f in files:
        rel = str(f)
        for c in walk(read(f)):
            if c.id != 0x3000005: continue
            name, p = string(c.data); kind, ln = struct.unpack_from('<II', c.data, p)
            if kind != 9: continue
            strs = [s.decode('latin1') for s in c.data[p + 8:p + 8 + ln * 4].split(b'\0') if s]
            if len(strs) < 3 or strs[2] not in ACTIONS: continue
            locs, _ = p3dlocs.read_locators(f, rel)
            L = next((l for l in locs if l.name == name), None)
            trig = L.triggers[0] if L is not None and L.triggers else None
            pl_ = c.data[p + 8:p + 8 + ln * 4]
            if len(pl_) >= 8: AO_FOLLOW.setdefault(strs[0], struct.unpack_from('<I', pl_, len(pl_) - 4)[0])
            out.setdefault(strs[0], (ACTIONS[strs[2]], strs[1], trig, name))
    return out


class AnimObjects:
    """Собирает семьи уровня. Вызов из psp_export.py:
         ao = AnimObjects(level, art, sprops, conv)   # до section(): добавляет типы в sprops.types
         ao.city_prims()                              # [(tex, flags, P, UV, C)] мешей без движения — в город
         BX, CY, sweep = ao.col_extend(BX, CY)        # объёмы -> COL1 (+ габариты заметания для сетки)
         ao.anm1(first_inst)                          # bytes таблицы ANM1 (first_inst — номер первого экземпляра SPR1 типов ao)
    """
    def __init__(self, level, art, sprops, conv=None, verbose=True):
        self.level, self.art, self.sp, self.conv, self.v = level, Path(art), sprops, conv, verbose
        self.files = PX.level_files(self.art, level)
        self.acts = action_locators(self.files)
        self.objs = []          # dict(name, family, file, action, trig, types=[индексы в sp.types], nfr, fps, vols=[])
        self.city = []          # (tex, flags, P, UV, C) в мире
        self.static_vols = []   # (kind, c, A, h) в мире
        self.report = dict(families={}, city_tris=0, spr_types=0, spr_inst=0, vol_static=0, vol_live=0, skipped=[])
        self._scan()

    def log(self, *a):
        if self.v: print(*a, flush=True)

    def _scan(self):
        for f in self.files:
            root = read(f)
            ents = [e for e in root.children if e.id in FAMILIES]
            if not ents: continue
            if self.conv is not None: self.conv.resources(root)   # шейдеры/текстуры этого файла (имена повторяются в файлах)
            for ent in ents:
                try: self._add(f, root, ent)
                except (ValueError, StopIteration) as e:
                    self.report['skipped'].append(f'{f.stem}/{N(ent)}: {e}'); self.log(f'  пропуск {f.stem}/{N(ent)}: {e}')

    def _add(self, f, root, ent):
        name, fam = N(ent), FAMILIES[ent.id]
        w = gather(root, ent)
        props, effects = PX.composite(w)
        fr = self.report['families'].setdefault(fam, dict(objects=0, drawn=0, parts=0, particles_only=0, vols=0))
        fr['objects'] += 1
        cols = volumes(w, name) if ent.id == ANIM_COLL else []
        if not props and not cols:
            fr['particles_only'] += 1; return   # только системы частиц (пар, огонь факелов) — это fx.shr (tools/fx_export.py)
        joints, keys, nfr, ntrack, fps = PX.joint_poses(w)
        pv, pv_n = PX.pvis_tracks(w)
        loop = ptrn_loop(w)
        act = self.acts.get(name)
        action, joint_name, trig, locname = act if act else (0, '', None, '')
        moving = bool(ntrack) or bool(pv)
        JW = PX.rest_world(joints)
        meshes = {N(c) for c in w.children if c.id == 0x10000}
        bbs = PX.billboards(w)
        fol_ = 0; fol_j = 0
        if action and not AO_FOLLOW_OLD and AO_FOLLOW.get(name) == 1:   # P05-7: триггер следует за суставом action-локатора
            fol_ = 1
            try: fol_j = 1 + [j_['name'] for j_ in CE.load_skeleton(w)].index(joint_name)
            except (ValueError, ImportError): fol_j = 0
        o = dict(name=name, family=fam, file=f.stem, action=action, trig=trig, loc=locname, follow=fol_, fjoint=fol_j, types=[], nfr=nfr, fps=fps or 30.0,
                 loop=loop, vols=[], moving=moving)
        # ---- рисунок
        spr_parts = []
        for dn, joint, trans in props:
            if dn in meshes and not moving:
                for t, fl, P, UV, C in self.sp.pack(f, dn):
                    M = JW[joint] if joint < len(JW) else np.eye(4)
                    Pw = (np.c_[P, np.ones(len(P))] @ M.T)[:, :3].astype(np.float32)
                    self.city.append((t, fl | (1 if trans else 0), Pw, UV, C)); self.report['city_tris'] += len(P) // 3
                fr['parts'] += 1
            elif dn in meshes or dn in bbs:
                spr_parts.append((dn, joint))
            else:
                self.report['skipped'].append(f'{f.stem}/{name}: нет рисунка {dn}')
        if spr_parts or (moving and effects):
            self._spr_types(o, f, w, spr_parts, effects, joints, keys, nfr, ntrack, JW)
            fr['parts'] += len(spr_parts)
        if o['types'] or len(spr_parts) < len(props): fr['drawn'] += 1
        # ---- объёмы
        mv = moving_joints(joints)
        for owner, shape in cols:
            live = owner < len(mv) and mv[owner]
            if live and o['types']:
                o['vols'].append(dict(owner=owner, shape=shape))
            else:
                M = JW[owner] if owner < len(JW) else np.eye(4)
                self.static_vols.append(world_vol(shape, M)); self.report['vol_static'] += 1
            fr['vols'] += 1
        if o['vols']:
            # заметание: габарит xz объёма за весь клип (все кадры)
            for vol in o['vols']:
                lo, hi = np.full(2, 1e9), np.full(2, -1e9)
                for fi in range(max(nfr, 1)):
                    Wf = pose_world(joints, keys, ntrack, fi) if ntrack else JW
                    k, rec = world_vol(vol['shape'], Wf[vol['owner']])
                    b = sphere_xz(k, rec); lo = np.minimum(lo, b[:2]); hi = np.maximum(hi, b[2:])
                vol['sweep'] = (*lo, *hi)
                vol['rec0'] = world_vol(vol['shape'], JW[vol['owner']])
            self.report['vol_live'] += len(o['vols'])
        if o['types'] or o['vols'] or action: self.objs.append(o)
        self.log(f"  {fam:13s} {f.stem}/{name}: частей {len(props)} (в город {len(props) - len(spr_parts)}), SPR1 типов {len(o['types'])}, "
                 f"объёмов {len(cols)} (живых {len(o['vols'])}), кадров {nfr}, цикл {loop}, "
                 f"действие {[k for k, v in ACTIONS.items() if v == action][0] if action else '-'}")

    def _spr_types(self, o, f, w, parts, effects, joints, keys, nfr, ntrack, JW):
        """Части -> типы SPR1 по кластерам (<= CLUSTER м, <= 32 слота). Корень скелета смещён в центр кластера,
        экземпляр стоит в центре кластера (spDrawable отсекает по нему)."""
        # центр каждой части в позе кадра 0
        bbs = PX.billboards(w); meshes = {N(c): c for c in w.children if c.id == 0x10000}
        cen = []
        for dn, joint in parts:
            M = JW[joint] if joint < len(JW) else np.eye(4)
            if dn in meshes:
                P = [q['position'] for pg in meshes[dn].children if pg.id == 0x10002 for q in geometry(pg)[1]]
                p = np.concatenate(P).mean(0) if P else np.zeros(3)
            else:
                p = np.mean([q[1] for q in bbs[dn]['quads']], 0) if bbs[dn]['quads'] else np.zeros(3)
            cen.append((M @ np.append(p, 1.0))[:3])
        clusters = []
        for i, (dn, joint) in enumerate(parts):
            for ci, cl in enumerate(clusters):
                room = PX.MAX_SLOTS - (len(effects) if ci == 0 else 0)
                if len(cl['parts']) < room and float(np.hypot(*(cen[i] - cl['seed'])[[0, 2]])) <= CLUSTER:
                    cl['parts'].append(dn); cl['pts'].append(cen[i]); break
            else:
                clusters.append(dict(seed=cen[i], parts=[dn], pts=[cen[i]]))
        if not clusters:   # только частицы (пружина АЭС): экземпляр — у сустава первого эффекта
            ej = effects[0][1] if effects and effects[0][1] < len(JW) else 0
            clusters.append(dict(seed=JW[ej][:3, 3] if JW else np.zeros(3), parts=[], pts=[]))
        effs = [e for e, _ in effects][:max(0, PX.MAX_SLOTS - len(clusters[0]['parts']))]
        for ci, cl in enumerate(clusters):
            keep = set(cl['parts']) | (set(effs) if ci == 0 else set())
            wc = with_parts(w, keep)
            T = self.sp._base_type(o['name'] + (f'#{ci}' if len(clusters) > 1 else ''), f, wc, 0)
            T['fx_text'] = {e: PX.fx_text_for(e, wc, []) for e, _ in T['effects']}
            T['cls'], T['atc'], T['mass'] = 2, 0, 0.0   # не стейтпроп: без упора SPR1 (объёмы — в COL1), без возврата вне вида
            T['cols'] = []
            # центр экземпляра: середина коробки кусков (или частей) кадра 0
            # Тип только из билбордов (кольцо у выключателя грузовика Duff z2switchbbqgrp, кольца звонков switchN_bb, огни
            # пульта АЭС z6_switchlights): коробка кусков пуста, _base_type даёт заглушку ±0.5 м у начала координат — экземпляр
            # вставал в (0, 0.5, 0) и spDrawable отсекал его по расстоянию: кольца не рисовались нигде (запись PS2 10:02–10:05:
            # красное кольцо пульсирует вокруг выключателя). Центр — середина билбордов кластера (поза кадра 0), коробка — их
            # размах ± половина квада. AO_BILL_CENTER_OLD=1 — прежнее (красный контроль tests/levelmech/animobj_bills.sh)
            if T['pieces'] or (T['bills'] and BILL_CENTER_OLD):
                c = (T['lo'] + T['hi']) / 2
            elif T['bills'] and cl['pts']:
                P_ = np.asarray(cl['pts'], np.float64); c = P_.mean(0)
                hw = max(max(b['w'], b['h']) for b in T['bills']) * .5
                T['lo'], T['hi'] = P_.min(0) - hw, P_.max(0) + hw
            else:
                c = np.mean(cl['pts'], 0) if cl['pts'] else np.asarray(cl['seed'], np.float64)
            c = np.round(c, 3)
            for jt in T['joints']:
                if jt['parent'] < 0: jt['t'] = np.asarray(jt['t'], np.float64) - c
            roots = {jt['track'] for jt in T['joints'] if jt['parent'] < 0 and jt['track'] >= 0} if T['ntrack'] else set()
            if ci and not roots and T['vis_frames'] == self.sp.types[o['types'][0]]['vis_frames']:
                T['keys_of'] = o['types'][0]   # кадры клипа и PVIS — одни на все кластеры объекта (корень без дорожки)
            if T['ntrack']:
                if roots:
                    ks = [list(k) for k in T['keys']]
                    for fi in range(T['nfr']):
                        for tr in roots:
                            k = ks[fi * T['ntrack'] + tr]; k[0] -= c[0]; k[1] -= c[1]; k[2] -= c[2]
                    T['keys'] = [tuple(k) for k in ks]
            T['lo'], T['hi'] = T['lo'] - c, T['hi'] - c
            if not all(np.isfinite(np.r_[jt['t'], jt['q'], jt['s']]).all() for jt in T['joints']) or not np.isfinite(np.asarray(T['keys'], np.float64)).all():
                raise ValueError(f"{o['name']}: нечисловая поза сустава после сдвига в центр {c}")
            # Суставы стоят (PTRN без движения: joint_poses даёт 0 кадров), а квады свечения анимированы BQG_ (кольцо выключателя
            # Duff пульсирует 1.04↔2.09 м и альфой 0.13↔0.47 за 30 кадров, кольца звонков 0.96↔2.1 м): кадр типа ведёт дорожку BQG
            # (viewer/stateprops.c spBillQuad: строка poseF % n), иначе кольцо стоит на кадре 0 — маленькое и почти прозрачное.
            # Как огни барьеров 0x3F0000F (prp2_export _add_animphys). AO_BILL_FRAMES_OLD=1 — прежнее (красный контроль tests/levelmech/animobj_bills.sh)
            if not T['nfr'] and not BILL_FRAMES_OLD:
                nb = max((len(b['track']) for b in T['bills']), default=0)
                if nb > 1: T['nfr'], T['ntrack'], T['keys'] = nb, 0, []
            n = max(T['nfr'] - 1, 0)
            cyc = bool(T['nfr']) and o['loop'] == 1 and o['action'] in (0, 5)
            allv = (1 << len(T['slots'])) - 1
            T['states'] = [dict(vis=allv, auto=0xFF, autoTime=0.0, pose=((2, 0, n, 1.0, 0) if cyc else (0, 0, 0, 1.0, 0)),
                                events=[], calls=[], fx=self._efx(T, n) if cyc else [])]
            M = np.eye(4); M[3, :3] = c
            T['inst'] = [(M, 0)]
            T['files'] = {f.stem: 1}
            o['types'].append(len(self.sp.types)); self.sp.types.append(T)
            o['center'] = c if ci == 0 else o.get('center', c)
            o.setdefault('inst_c', []).append(c)
            self.report['spr_types'] += 1; self.report['spr_inst'] += 1

    @staticmethod
    def _efx(T, n):
        out = []
        for e, _ in T['effects']:
            if e not in T['slots']: continue
            out.append((T['slots'].index(e), 2, 0, PX.fx_preset(T['fx_text'].get(e, e)), 0, max(n, 1), 1.0))
        return out[:PX.MAX_FX]

    # ---- город
    def city_prims(self):
        return list(self.city)

    # ---- коллизия
    def col_extend(self, BX, CY):
        """-> BX, CY с объёмами семей в конце и sweep {('box'|'cyl', номер): (x0, z0, x1, z1)} для сетки COL1."""
        bx, cy, sweep = [list(r) for r in BX], [list(r) for r in CY], {}
        for k, rec in self.static_vols:
            (bx if k == 'box' else cy).append(rec)
        for o in self.objs:
            for vol in o['vols']:
                k, rec = vol['rec0']
                arr = bx if k == 'box' else cy
                vol['col'] = (k, len(arr)); arr.append(rec)
                sweep[(k, len(arr) - 1)] = vol['sweep']
        return np.array(bx, np.float64).reshape(-1, 16), np.array(cy, np.float64).reshape(-1, 12), sweep

    # ---- ANM1
    def type_first_inst(self):
        """номер первого экземпляра SPR1 каждого типа (порядок section(): типы по порядку, экземпляры типа подряд)"""
        out, n = [], 0
        for T in self.sp.types:
            out.append(n); n += len(T['inst'])
        return out

    def anm1(self):
        first = self.type_first_inst()
        orecs, vrecs = [], []
        for oi, o in enumerate(self.objs):
            fi = first[o['types'][0]] if o['types'] else 0xFFFF
            if o['types'] and any(first[t] != fi + k for k, t in enumerate(o['types'])):
                raise ValueError(f"{o['name']}: экземпляры кластеров не подряд")
            fv = len(vrecs)
            for vol in o['vols']:
                k, idx = vol['col']; sh = vol['shape']
                if sh['kind'] == 'box':
                    kind, a9, h3 = 0, np.asarray(sh['axes'], np.float64).reshape(-1), np.asarray(sh['halfExtents'], np.float64)
                elif sh['kind'] == 'cylinder':
                    kind, a9, h3 = 1, np.r_[sh['axis'], 0, 0, 0, 0, 0, 0], np.r_[sh['radius'], sh['length'], float(sh['flatEnds'])]
                else:
                    kind, a9, h3 = 1, np.r_[0, 1, 0, 0, 0, 0, 0, 0, 0], np.r_[sh['radius'], 0, 0]
                vrecs.append(struct.pack('<BBHIHH3f9f3f', kind, 0, oi, idx, vol['owner'], 0, *sh['center'], *a9, *h3))
            trig, tk, tc, th, ta = o['trig'], 0, (0, 0, 0), (0, 0, 0), np.eye(3).reshape(-1)
            if trig is not None:
                isr, c, h, axes = trig; tk = 2 if isr else 1; tc, th = c, h; ta = np.asarray(axes, np.float64).reshape(-1)
            orecs.append(struct.pack('<4HBBHHHf3f3f9f', fi, len(o['types']), fv, len(o['vols']), o['action'], tk, o.get('follow', 0),
                                     o['nfr'], o.get('fjoint', 0), float(o['fps']), *tc, *th, *ta))
        head = b'ANM1' + struct.pack('<5I8x', 1, len(orecs), len(vrecs), 32, 32 + 80 * len(orecs))
        out = head + b''.join(orecs) + b''.join(vrecs)
        assert all(len(r) == 80 for r in orecs) and all(len(r) == 72 for r in vrecs)
        return out + b'\0' * (-len(out) % 16)

    def summary(self):
        r = self.report
        fam = '; '.join(f"{k}: {v['objects']} объектов (рисуем {v['drawn']}, частей {v['parts']}, только частицы {v['particles_only']}, "
                        f"объёмов {v['vols']})" for k, v in r['families'].items())
        acts = ', '.join(f"{o['name']}={[k for k, v in ACTIONS.items() if v == o['action']][0]}" for o in self.objs if o['action'])
        return (f'анимированные объекты: {fam}; в город {r["city_tris"]} треуг.; SPR1 типов {r["spr_types"]}; '
                f'объёмов статичных {r["vol_static"]}, живых {r["vol_live"]}; действия: {acts or "-"}'
                + (f'; пропущено {len(r["skipped"])}: {r["skipped"][:4]}' if r['skipped'] else ''))


def volumes(w, name):
    """Листья CollisionObject сущности -> [(сустав-владелец, лист interiors.leaves в осях сустава, z отзеркален)]
    (обход как prp2_export.collision, но лист целиком: у цилиндра «length» диска — половина длины, как в COL1)."""
    out = []
    def rec(v):
        kids = [k for k in v.children if k.id == 0x7010001]
        if kids:
            for k in kids: rec(k)
            return
        owner, = struct.unpack_from('<I', v.data)
        for s in PX.col_leaves(v, name): out.append((owner, s))
    for co in w.children:
        if co.id != 0x7010000: continue
        for v in co.children:
            if v.id == 0x7010001: rec(v)
    return out


def world_vol(shape, M):
    """лист в осях сустава -> ('box'|'cyl', запись COL1) в мире по столбцовой матрице сустава M — те же правила, что
    psp_export.col_place (там матрица строчная): ящик — оси правой тройкой, цилиндр — ось вверх, halfLen = length."""
    R = M[:3, :3]; c = R @ np.asarray(shape['center'], np.float64) + M[:3, 3]
    if shape['kind'] == 'box':
        A = (R @ np.asarray(shape['axes'], np.float64).T).T; s = np.linalg.norm(A, axis=1)
        h = np.asarray(shape['halfExtents'], np.float64) * s; A = A / s[:, None]
        ax = A[0]; ay = A[1] - ax * (ax @ A[1]); ay /= np.linalg.norm(ay); az = np.cross(ax, ay)
        return 'box', [*c, *ax, *ay, *az, *h, 0.0]
    rs = float(max(np.linalg.norm(R, axis=0)))
    if shape['kind'] == 'cylinder':
        a = R @ np.asarray(shape['axis'], np.float64); s = np.linalg.norm(a); a /= s
        if a[1] < 0: a = -a
        return 'cyl', [*c, *a, shape['radius'] * rs, shape['length'] * s, float(shape['flatEnds']), 0, 0, 0]
    return 'cyl', [*c, 0.0, 1.0, 0.0, shape['radius'] * rs, 0.0, 0.0, 0, 0, 0]


def sphere_xz(k, rec):
    """xz-габарит описанной сферы записи COL1: живой объём между кадрами клипа поворачивается (вентилятор, шестерня) —
    его габарит на промежуточном угле шире, чем на соседних кадрах; сфера покрывает любой поворот, центр между кадрами
    идёт по отрезку — объединение сфер соседних кадров его покрывает"""
    rec = np.asarray(rec, np.float64)
    r = float(np.linalg.norm(rec[12:15])) if k == 'box' else float(rec[6] + rec[7])
    return np.array([rec[0] - r, rec[2] - r, rec[0] + r, rec[2] + r])


def xz_bounds(k, rec):
    """xz-габарит записи COL1 (как psp_export.col_xz_bounds)"""
    rec = np.asarray(rec, np.float64)
    if k == 'box':
        e = np.abs(rec[3:12].reshape(3, 3)) * rec[12:15, None]; ex, ez = e[:, 0].sum(), e[:, 2].sum()
        return np.array([rec[0] - ex, rec[2] - ez, rec[0] + ex, rec[2] + ez])
    r, hl = rec[6], rec[7]
    cx, cz = abs(rec[3]) * hl + r, abs(rec[5]) * hl + r
    return np.array([rec[0] - cx, rec[2] - cz, rec[0] + cx, rec[2] + cz])


def family_counts(level, art):
    """Счёт по диску (для host-проверки): {семья: (объектов, частей составного рисунка, объёмов CollisionObject)}."""
    out = {}
    for f in PX.level_files(Path(art), level):
        root = read(f)
        for ent in root.children:
            if ent.id not in FAMILIES: continue
            w = gather(root, ent); props, _ = PX.composite(w)
            nv = len(volumes(w, N(ent))) if ent.id == ANIM_COLL else 0
            a, b, c = out.get(FAMILIES[ent.id], (0, 0, 0)); out[FAMILIES[ent.id]] = (a + 1, b + len(props), c + nv)
    return out


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser(); ap.add_argument('--level', type=int, default=1); a = ap.parse_args()
    GAME = (ROOT / 'hit-and-run-web/source/game').resolve()
    print(family_counts(a.level, GAME / 'art'))
    for k, v in action_locators(PX.level_files(GAME / 'art', a.level)).items(): print(' ', k, v[:2], v[3])
