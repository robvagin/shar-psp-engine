#!/usr/bin/env python3
"""Трафик и пешеходы уровня (этап 5) -> build/traffic.shr (формат TRF1, читает viewer/traffic.c; структуры — viewer/traffic.h).

  python tools/traffic_export.py            # уровень 1 -> build/traffic.shr
  python tools/traffic_export.py --level 1 --out /tmp/t.shr

Источник — только диск Роба (hit-and-run-web/source/game, никуда не публикуется):
  * дорожная сеть: art/l<N>_terra.p3d — Intersection 0x3000004 (центр, радиус, trafficBehaviour),
    Road 0x3000003 (от/до перекрёстка, maxCars, скорость км/ч, shortcut), RoadSegment 0x3000002 + RoadDataSegment
    0x3000009 (4 угла полосы: край 0-3 — начало, 1-2 — конец, полос lanes). Разбор — как hit-and-run-web
    tools/world_resources.py navigation() (MIT). Дорога односторонняя (у улицы две дороги рядом), полосы — ось каждой
    полосы ломаной по сегментам (lerp(p0,p3,(k+.5)/n) -> lerp(p1,p2,(k+.5)/n)).
  * машины трафика: scripts/missions/level0N/leveli.mfk — CreateTrafficGroup(0) / AddTrafficModel(имя, число[, noPark]);
    модели art/cars/<имя>.p3d; кузов в позе покоя со светотенью из нормалей (psp_export.car_prims), колёса w0..w3 —
    отдельные куски в осях своего колеса (движок крутит); LOD <= 400 треуг. для > 30 м — fast-simplification (MIT,
    план оптимизации п.2 и аудит шаг 9), UV/цвет — от выживших исходных треугольников. Остов huskA (level.mfk грузит
    его для трафика) — последняя модель.
  * пешеходы: CreatePedGroup / AddPed (leveli.mfk), модели art/chars/<имя[:6]>_m.p3d, анимации — общий риг npd
    (art/chars/npd_a.p3d): ходьба, бег, стойка, flail, get_up, dodge, cringe, fall_land. Скиннинг на GE — формат CHR1
    viewer/skin.h (tools/char_export.py этапа 4): все пешеходы в ОДНОМ блобе, клипы и текстуры (char_swatches_lit,
    eyeball) — общие, смещения внутри блоба от его начала (SkinModel.base = начало блоба).
  * пути пешеходов: Path 0x300000B (110 замкнутых ломаных), зоны групп — локаторы kind 13 (тонкие «ворота» с
    TriggerVolume, данные = номер группы): игрок в воротах -> активная группа.
  * припаркованные машины: CarStart-локаторы (Locator2 0x3000005, тип 3) в art/l<N>z*.p3d и art/l<N>r*.p3d, данные —
    два слова (биты float курса, флаг); флаг 1 = место стоящей машины (ур. 1: 93 места, Z1PC3, R7PC12…). Курс в
    координатах level.shr = −курс игры (z зеркалим; как hit-and-run-web tools/campaign.py locators(), MIT).
    Модели для стоянки — AddTrafficModel с третьим аргументом 1 (ур. 1: glastruc, schoolbu).
    ПРАВИЛО ПОРТА (в данных нет, какие места заняты и чем): заняты все места; модель = parkModels[FNV-1a(имя места) % n]
    (стабильно между сборками; viewer/traffic.h trafficParkModel — тот же хеш для проверки).
Координаты — как level.shr (z игры зеркалим), метры."""
import argparse, os, json, re, struct, sys
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
import uvq                                   # noqa: E402  16-битные UV без знака (правка Роба 43)
import psp_export as pe                      # noqa: E402  car_prims, load_tex, ni_bytes, verts (формат level.shr)
import level_light                           # noqa: E402  свет уровня: группа sun, блок LGT1 (29.09, рецепт §12)
import char_export as ce                     # noqa: E402  скиннинг CHR1 (этап 4)
from convert import Converter, GAME, OUT, mat, floats   # noqa: E402
from p3d import read, walk, string           # noqa: E402

LOD_TRIS = 400          # аудит, шаг 9: трафик дальше 30 м — упрощённая версия <= 400 треуг.
LOD_TARGET = 300
# правка 354 [PS2 код SLES 0x27a140]: машины, у которых оригинал не рисует куски на суставах w0..w3 (бит k — w<k>); то же —
# tools/wheel_clear.HIDDEN и viewer/carwheels.h. TRF_WHEELS_OLD=1 — кубы-заглушки в пакете, как до правки (красный контроль)
HIDDEN_WHEELS = {'frink_v': 0xF, 'honor_v': 0xF, 'hbike_v': 0xF, 'witchcar': 0xF, 'ship': 0xF, 'mono_v': 0xF, 'rocke_v': 0xC}
WHEELS_OLD = bool(os.environ.get('TRF_WHEELS_OLD'))
DRIVER_OLD = bool(os.environ.get('TRF_DRIVER_OLD'))   # №666: водитель запечён в модель — без отметки owner 4 (прежнее)
PED_CLIPS = ['npd_loco_walk', 'npd_loco_run', 'npd_loco_idle_rest', 'npd_flail', 'npd_get_up', 'npd_dodge',
             'npd_cringe', 'npd_fall_land']
HDR = 256
QV = np.dtype([('u', '<i2'), ('v', '<i2'), ('c', '<u4'), ('x', '<i2'), ('y', '<i2'), ('z', '<i2'), ('pad', '<i2')])
F_NOIDX, F_V12, F_DOUBLE = pe.F_NOIDX, pe.F_V12, pe.F_DOUBLE


def al(b, a=16):
    b += b'\0' * (-len(b) % a)
    return b


# ------------------------------------------------------------------ скрипты уровня
def level_script(level):
    """-> ([(модель, число, noPark)], {группа: [(пешеход, число)]})"""
    s = (GAME / f'scripts/missions/level0{level}/leveli.mfk').read_text(errors='replace')
    s = re.sub(r'//[^\n]*', '', s)
    traffic, peds, tg, pg = [], {}, None, None
    for op, args in re.findall(r'(\w+)\s*\(([^)]*)\)', s):
        a = [x.strip().strip('"') for x in args.split(',')] if args.strip() else []
        if op == 'CreateTrafficGroup': tg = int(a[0])
        elif op == 'CloseTrafficGroup': tg = None
        elif op == 'AddTrafficModel' and tg == 0: traffic.append((a[0], int(a[1]), len(a) > 2 and a[2] == '1'))
        elif op == 'CreatePedGroup': pg = int(a[0]); peds[pg] = []
        elif op == 'ClosePedGroup': pg = None
        elif op == 'AddPed' and pg is not None: peds[pg].append((a[0], int(a[1])))
    return traffic, peds


# ------------------------------------------------------------------ дорожная сеть
def road_network(level):
    root = read(GAME / f'art/l{level}_terra.p3d')
    defs = {string(c.data)[0]: c for c in root.children if c.id == 0x3000009}
    junc, jidx = [], {}
    for c in root.children:
        if c.id != 0x3000004: continue
        name, p = string(c.data); x, y, z, r, beh = struct.unpack_from('<4fI', c.data, p)
        if name in jidx: continue            # дубли одного имени (вариант того же места) — берём первый
        jidx[name] = len(junc); junc.append(dict(name=name, pos=(x, y, -z), r=r, beh=beh))
    roads = []
    for road in root.children:
        if road.id != 0x3000003: continue
        name, p = string(road.data); _kind, = struct.unpack_from('<I', road.data, p)
        start, p = string(road.data, p + 4); end, p = string(road.data, p)
        max_cars, speed, _intel, shortcut = struct.unpack_from('<IBBB', road.data, p)
        segs = []
        for c in road.children:
            if c.id != 0x3000002: continue
            _label, q = string(c.data); dn, q = string(c.data, q); d = defs[dn]; _, r_ = string(d.data)
            _k, lanes, _sh = struct.unpack_from('<III', d.data, r_)
            pts = np.concatenate([np.zeros((1, 3)), floats(d.data, 9, r_ + 12).reshape(3, 3)])
            pts = np.c_[pts, np.ones(4)] @ mat(c.data, q + 64) @ mat(c.data, q); pts[:, 2] *= -1
            segs.append((pts[:, :3], lanes))
        # порядок сегментов: цепочкой от перекрёстка начала (как world_resources.navigation)
        at = np.array(junc[jidx[start]]['pos']); order, rest = [], list(range(len(segs)))
        while rest:
            k = min(rest, key=lambda n: np.linalg.norm((segs[n][0][0] + segs[n][0][3]) / 2 - at))
            rest.remove(k); order.append(k); at = (segs[k][0][1] + segs[k][0][2]) / 2
        segs = [segs[k] for k in order]
        lanes = max(l for _, l in segs)
        lane_pts = []
        for k in range(lanes):
            pl = []
            for pts, l in segs:
                f = (min(k, l - 1) + .5) / l
                a, b = pts[0] + (pts[3] - pts[0]) * f, pts[1] + (pts[2] - pts[1]) * f
                if not pl or np.linalg.norm(pl[-1] - a) > .05: pl.append(a)
                pl.append(b)
            pl = np.array(pl); s = np.r_[0, np.cumsum(np.linalg.norm(np.diff(pl, axis=0), axis=1))]
            lane_pts.append(np.c_[pl, s])
        width = float(np.mean([np.linalg.norm(pts[3] - pts[0]) / l for pts, l in segs]))
        roads.append(dict(name=name, s=jidx[start], e=jidx[end], maxCars=max_cars, speed=speed, shortcut=shortcut,
                          lanes=lanes, pts=lane_pts, width=width))
    # точки полос разной длины (полос 1..2, число точек у полос одной дороги может отличаться) -> выравниваем
    # к общему числу: ломаную каждой полосы передискретизируем по длине до max точек
    for r in roads:
        n = max(len(p) for p in r['pts'])
        for k, p in enumerate(r['pts']):
            if len(p) == n: continue
            t = np.linspace(0, p[-1, 3], n)
            q = np.stack([np.interp(t, p[:, 3], p[:, j]) for j in range(3)], 1)
            r['pts'][k] = np.c_[q, t]
    # обратная дорога (та же пара перекрёстков наоборот) и списки выездов
    for i, r in enumerate(roads):
        r['rev'] = next((j for j, o in enumerate(roads) if o['s'] == r['e'] and o['e'] == r['s'] and j != i), 0xFFFF)
    for j in junc:
        j['out'] = []; j['nin'] = 0
    for i, r in enumerate(roads):
        if r['shortcut'] or r['maxCars'] == 0: continue
        junc[r['s']]['out'].append(i); junc[r['e']]['nin'] += 1
    return junc, roads


def ped_paths_zones(level):
    root = read(GAME / f'art/l{level}_terra.p3d')
    paths = []
    for c in root.children:
        if c.id != 0x300000B: continue
        n, = struct.unpack_from('<I', c.data); P = np.frombuffer(c.data, '<f4', n * 3, 4).reshape(-1, 3).astype(np.float64).copy()
        P[:, 2] *= -1
        if np.linalg.norm(P[0] - P[-1]) < .01: P = P[:-1]          # замкнутая: последняя = первая
        paths.append(P)
    zones = []
    for c in walk(root):
        if c.id != 0x3000005: continue
        name, p = string(c.data); kind, ln = struct.unpack_from('<II', c.data, p)
        if kind != 13 or ln < 1: continue
        group, = struct.unpack_from('<I', c.data, p + 8)
        for t in c.children:
            if t.id != 0x3000006: continue
            _tn, q = string(t.data); _rect, = struct.unpack_from('<I', t.data, q)
            h = struct.unpack_from('<3f', t.data, q + 4); M = np.frombuffer(t.data, '<f4', 16, q + 16).reshape(4, 4).astype(np.float64)
            cpos = M[3, :3].copy(); cpos[2] *= -1
            ax = M[0, :3].copy(); ax[2] *= -1; az = M[2, :3].copy(); az[2] *= -1
            sx, sz = np.linalg.norm(ax), np.linalg.norm(az)
            zones.append(dict(name=name, c=cpos, ax=ax / max(sx, 1e-9), az=az / max(sz, 1e-9),
                              h=(h[0] * sx, h[1] * np.linalg.norm(M[1, :3]), h[2] * sz), group=group))
    return paths, zones


def fnv1a(s):
    h = 0x811C9DC5
    for b in s.encode('latin1'):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def parked_spots(level):
    """-> [(имя, x, y, z, курс)] мест стоящих машин (CarStart, флаг 1) в координатах level.shr, в порядке файлов"""
    files = sorted((GAME / 'art').glob(f'l{level}z*.p3d')) + sorted((GAME / 'art').glob(f'l{level}r*.p3d'))
    out, seen = [], set()
    for f in files:
        for c in walk(read(f)):
            if c.id != 0x3000005: continue
            name, p = string(c.data); kind, ln = struct.unpack_from('<II', c.data, p)
            if kind != 3 or ln < 2: continue
            hg, flag = struct.unpack_from('<fI', c.data, p + 8)
            if flag != 1: continue
            x, y, z = struct.unpack_from('<3f', c.data, p + 8 + ln * 4)
            if name in seen: continue                 # одно имя в двух файлах зоны — одно место
            seen.add(name); out.append((name, x, y, -z, -hg))
    return out


# ------------------------------------------------------------------ правка 139: зоны стоящих машин по загрузчикам
# Данные диска: в art/l<N>_terra.p3d локаторы вида 5 («loader1»…) со строкой файлов зон и TriggerVolume. Разбор строки —
# как donut (GPL, src/Level.cpp DynaLoadData): «имя;» — загрузить, «имя:» — выгрузить, «@»/«$» — комнаты (порт их не
# трогает). Уличный загрузчик называет КАЖДЫЙ файл зон уровня (загрузить или выгрузить) — после него набор загруженных
# файлов известен целиком; такие и берём (у комнатных SimpsonsHouse/KwikEMart/… — частичный список, пропускаем).
# Проверка прочтения — tools/zones_check.py (AddTeleportDest level.mfk: список файлов точки ⊆ набора ближайшего загрузчика).
ZONE_MAX_FILES = 16   # маска места — uint16 (TrPark.zmask); больше файлов — правило выключено для уровня (маски 0)


def zone_files(level):
    """-> имена файлов зон (без .p3d) в порядке parked_spots: бит i маски = файл i"""
    files = sorted((GAME / 'art').glob(f'l{level}z*.p3d')) + sorted((GAME / 'art').glob(f'l{level}r*.p3d'))
    return [f.stem.lower() for f in files]


def parse_dyna(s):
    """строка загрузчика -> (загрузить, выгрузить) — множества имён файлов без .p3d; комнаты (@, $) отбрасываются"""
    load, unload, cur = set(), set(), ''
    for ch in s:
        if ch in ';:@$':
            f = cur[:-4].lower() if cur.lower().endswith('.p3d') else cur.lower()
            if ch == ';': load.add(f)
            elif ch == ':': unload.add(f)
            cur = ''
        else: cur += ch
    return load, unload


def spot_masks(level, names):
    """-> {имя места: маска файлов зон, где лежит его CarStart} (одно имя в двух файлах — оба бита)"""
    files = zone_files(level); out = {}
    if len(files) > ZONE_MAX_FILES: return {}
    for i, stem in enumerate(files):
        for c in walk(read(GAME / 'art' / f'{stem}.p3d')):
            if c.id != 0x3000005: continue
            name, p = string(c.data)
            if name in names: out[name] = out.get(name, 0) | (1 << i)
    return out


def dyna_loaders(level, swap=False):
    """-> [dict(name, c, ax, az, h, mask)] уличных загрузчиков: ворота (как зоны пешеходов) и маска загруженных файлов.
    swap=True — НАМЕРЕННО неверное прочтение («;» и «:» наоборот), только для красного контроля гейтов"""
    files = zone_files(level)
    if len(files) > ZONE_MAX_FILES: return []
    out = []
    for c in walk(read(GAME / f'art/l{level}_terra.p3d')):
        if c.id != 0x3000005: continue
        name, p = string(c.data); kind, ln = struct.unpack_from('<II', c.data, p)
        if kind != 5 or ln < 1: continue
        load, unload = parse_dyna(c.data[p + 8:p + 8 + ln * 4].split(b'\0')[0].decode('latin1'))
        if swap: load, unload = unload, load
        if (load | unload) & set(files) != set(files): continue          # комнатный / частичный — не уличный
        mask = sum(1 << i for i, f in enumerate(files) if f in load)
        for t in c.children:
            if t.id != 0x3000006: continue
            _tn, q = string(t.data)
            h = struct.unpack_from('<3f', t.data, q + 4); M = np.frombuffer(t.data, '<f4', 16, q + 16).reshape(4, 4).astype(np.float64)
            cpos = M[3, :3].copy(); cpos[2] *= -1
            ax = M[0, :3].copy(); ax[2] *= -1; az = M[2, :3].copy(); az[2] *= -1
            sx, sz = np.linalg.norm(ax), np.linalg.norm(az)
            out.append(dict(name=name, c=cpos, ax=ax / max(sx, 1e-9), az=az / max(sz, 1e-9),
                            h=(h[0] * sx, h[1] * np.linalg.norm(M[1, :3]), h[2] * sz), mask=mask))
    return out


# ------------------------------------------------------------------ машины трафика
def simplify_soup(P, UV, C, T, F, target):
    """Суп треугольников (P n*3 x 3, uv, цвет, текстура/флаги по треугольнику) -> упрощённый суп (цель target треуг.).
    Правка Роба 56 (29.09: «на машинах отваливаются текстуры: школьный автобус, обычные серые машины»): прежнее
    упрощение сваривало только по позиции — окна и надписи автобуса, решётка пикапа (свои куски рисунка) схлопывались в
    корпус, а сдвинутые углы оставляли старые UV. Правило для всех моделей:
      * вершина = (позиция, UV, текстура, флаги): каждый кусок рисунка — своя область с границей;
      * fast-simplification (MIT) с preserve_border — контуры кусков рисунка неподвижны (окна, надписи, фары остаются,
        швов-трещин нет), упрощается только середина куска;
      * UV и цвет новой вершины — с исходной поверхности в той точке, куда она встала (ближайшая точка треугольников
        вокруг слитых в неё вершин, барицентрически).
    Цель — не жёсткий потолок: контуры не упрощаются, поэтому у машины с мелким рисунком треугольников остаётся больше."""
    import fast_simplification as fs
    key = np.c_[np.round(P / 1e-3), np.round(UV * 4096), np.repeat(T, 3), np.repeat(F, 3)].astype(np.int64)
    uniq, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True); inv = inv.reshape(-1)
    pts = P[first].astype(np.float64)
    tri = inv.reshape(-1, 3)
    good = (tri[:, 0] != tri[:, 1]) & (tri[:, 1] != tri[:, 2]) & (tri[:, 0] != tri[:, 2])
    idx = np.nonzero(good)[0]; tri = tri[good]
    if len(tri) <= target: return P, UV, C, T, F
    best = None
    for red in np.linspace(1 - target / len(tri), .95, 12):
        _p, _t, coll = fs.simplify(pts, tri.astype(np.int32), target_reduction=float(red), return_collapses=True, agg=4,
                                   preserve_border=True)
        np_, nt_, mp = fs.replay_simplification(pts.astype(np.float32), tri.astype(np.int32), coll)
        rt = mp[tri]
        keep = (rt[:, 0] != rt[:, 1]) & (rt[:, 1] != rt[:, 2]) & (rt[:, 0] != rt[:, 2])
        srt = np.sort(rt, 1); _, first_ = np.unique(srt[keep], axis=0, return_index=True)
        sel = np.nonzero(keep)[0][np.sort(first_)]
        if best is None or len(sel) < len(best[3]): best = (np_, mp, rt, sel)
        if len(sel) <= target: break
    np_, mp, rt, sel = best
    # UV/цвет новой вершины: ближайшая точка на исходных треугольниках, у которых есть угол, слитый в эту вершину
    corner = idx[:, None] * 3 + np.arange(3)
    cols = np.stack([(C >> s) & 0xFF for s in (0, 8, 16, 24)], -1).astype(np.float64)
    newUV = np.zeros((len(np_), 2)); newC = np.zeros((len(np_), 4))
    for v in np.unique(rt[sel]):
        rows = np.nonzero((rt == v).any(1))[0]
        A, B_, Cc = (P[corner[rows, k]].astype(np.float64) for k in range(3))
        q = np_[v].astype(np.float64)
        w = _bary_closest(q, A, B_, Cc)
        d = ((w[:, :1] * A + w[:, 1:2] * B_ + w[:, 2:] * Cc - q) ** 2).sum(1)
        r = rows[int(d.argmin())]; wr = w[int(d.argmin())]
        newUV[v] = wr @ UV[corner[r]].astype(np.float64); newC[v] = wr @ cols[corner[r]]
    src = idx[sel]; ntri = rt[sel].reshape(-1)
    c8 = np.clip(np.round(newC[ntri]), 0, 255).astype(np.uint32)
    outC = (c8[:, 0] | c8[:, 1] << 8 | c8[:, 2] << 16 | c8[:, 3] << 24).astype(np.uint32)
    return np_[ntri].astype(np.float32), newUV[ntri].astype(np.float32), outC, T[src], F[src]


def _bary_closest(p, A, B, C):
    """барицентрические веса ближайшей к p точки каждого треугольника (A, B, C: n x 3) — Ericson, Real-Time Collision
    Detection 5.1.5"""
    ab, ac, ap = B - A, C - A, p - A
    d1, d2 = (ab * ap).sum(1), (ac * ap).sum(1)
    bp = p - B; d3, d4 = (ab * bp).sum(1), (ac * bp).sum(1)
    cp = p - C; d5, d6 = (ab * cp).sum(1), (ac * cp).sum(1)
    va = d3 * d6 - d5 * d4; vb = d5 * d2 - d1 * d6; vc = d1 * d4 - d3 * d2
    den = np.where(np.abs(va + vb + vc) < 1e-18, 1e-18, va + vb + vc)
    v = vb / den; w = vc / den; out = np.c_[1 - v - w, v, w]
    # края и вершины (области Вороного)
    m = (d1 <= 0) & (d2 <= 0); out[m] = (1, 0, 0)
    m2 = (d3 >= 0) & (d4 <= d3) & ~m; out[m2] = (0, 1, 0)
    m3 = (d6 >= 0) & (d5 <= d6) & ~m & ~m2; out[m3] = (0, 0, 1)
    done = m | m2 | m3
    e = (vc <= 0) & (d1 >= 0) & (d3 <= 0) & ~done; t = d1 / np.where(d1 - d3 == 0, 1, d1 - d3)
    out[e] = np.c_[1 - t, t, 0 * t][e]; done |= e
    e = (vb <= 0) & (d2 >= 0) & (d6 <= 0) & ~done; t = d2 / np.where(d2 - d6 == 0, 1, d2 - d6)
    out[e] = np.c_[1 - t, 0 * t, t][e]; done |= e
    e = (va <= 0) & ((d4 - d3) >= 0) & ((d5 - d6) >= 0) & ~done
    t = (d4 - d3) / np.where((d4 - d3) + (d5 - d6) == 0, 1, (d4 - d3) + (d5 - d6))
    out[e] = np.c_[0 * t, 1 - t, t][e]
    return out


def pack_piece(P, UV, C, tex, flags, owner_tag, blob, bbox_extra=None, owner=1):   # owner: 1 кузов/колесо, 3 блик (car_flares), 4 запечённый водитель (№666)
    """-> запись TrPiece (как Rec level.shr), вершины 16 Б (или 12 Б, если альфа 255) без индексов, двусторонние."""
    lo, hi = P.min(0).astype(np.float64), P.max(0).astype(np.float64)
    c = (lo + hi) / 2; S = max(float((hi - lo).max()) / 2, 1e-3)
    q = np.zeros(len(P), QV)
    q['x'], q['y'], q['z'] = [np.clip(np.round((P[:, j] - c[j]) / S * 32767), -32767, 32767) for j in range(3)]
    q['u'], q['v'] = uvq.tri_i16(UV[:, 0], UV[:, 1])   # GE читает u16 (tools/uvq.py, правка Роба 43)
    q['c'] = C
    v12 = bool((q['c'] >> 24 == 0xFF).all())
    fl = flags | F_NOIDX | F_DOUBLE | (F_V12 if v12 else 0)
    off = len(blob); blob += pe.ni_bytes(q, v12); blob += b'\0' * (-len(blob) % 16)
    return struct.pack('<6fiIIII3ffIII2I', *lo, *hi, tex, fl, len(q), len(q), owner, *c, S * 32768 / 32767, off, 0, 0, *owner_tag)


def car_flares(conv, p3d, cache, texblob):
    """блики стоп-сигналов и заднего хода модели (BillboardQuadGroup brake*/rev* на суставах, как psp_export.car_export) -> [(запись TrPiece, текстура)]"""
    import car_dump as cd
    root = read(p3d)
    comp = next((c for c in root.children if c.id == 0x4512), None)
    if comp is None: return []
    _, q = string(comp.data); skname, _ = string(comp.data, q)
    sk = next((c for c in root.children if c.id == 0x4500 and string(c.data)[0] == skname), None)
    if sk is None: return []
    J = cd.skeleton(sk); jn = {j[0]: i for i, j in enumerate(J)}
    sh = cd.shaders(root); out = []
    for g, shn, qn, mode, t, col, w, h, dist, zt, zw in cd.billboards(root):
        jname = g[:-5] if g.endswith('Shape') else g
        if jname not in jn: continue
        kind = 0 if jname.startswith('brake') else 1 if jname.startswith('rev') else -1
        if kind < 0: continue
        url = conv.textures.get(sh.get(shn, ('', {}))[1].get('TEX', [''])[0].lower())
        ti = pe.load_tex(url, cache, texblob, blend=True) if url else -1
        a, r, gg, b = (col >> 24) & 255, (col >> 16) & 255, (col >> 8) & 255, col & 255
        abgr = (a << 24) | (b << 16) | (gg << 8) | r
        x, y, z = (J[jn[jname]][3][3, :3] + np.array([t[0], t[1], t[2]])).tolist()
        z = -z   # оси модели пакета, как SHR у машины игрока (psp_export: jpos и смещение бликов с -z): нос в -Z, зад (стоп-сигналы) в +Z
        fl = F_NOIDX | (kind << 8) | ({'YAX': 0, 'LYAX': 1, 'AAX': 2}.get(mode, 0) << 10)
        out.append((struct.pack('<6fiIIII3ffIII2I', float(w), float(h), float(dist), 0., 0., 0., ti, fl, 0, 0, 3, x, y, z, 1.0, 0, abgr, 0, 0, 0), ti))
    return out


def seat_joint(carp):
    """№666: позиция сустава dl (сиденье водителя) в осях SHR (z зеркален, как psp_export.car_measure) или None, если сустава нет"""
    import car_dump as cd
    root = read(carp)
    comp = next(c for c in root.children if c.id == 0x4512)
    _, q = string(comp.data); skname, _ = string(comp.data, q)
    J = cd.skeleton(next(c for c in root.children if c.id == 0x4500 and string(c.data)[0] == skname))
    for j in J:
        if j[0] == 'dl': return np.array([j[3][3, 0], j[3][3, 1], -j[3][3, 2]])
    return None


def car_model(conv, name, cache, texblob, vblob, pieces, tmp='trf_car', light=None):
    # tmp — имя временного файла конвертера в hit-and-run-web/public/assets (у машин миссий своё: экспорты не мешают друг другу);
    # light — свет уровня (tools/level_light.py), которым запекается светотень машины
    p = GAME / 'art' / 'cars' / f'{name.lower()}.p3d'
    tmp = f'{tmp}_{os.getpid()}'   # номер процесса: параллельные экспорты (разные сессии) не делят файл
    conv.export(p, tmp, vehicle=True)
    meta = json.loads((OUT / f'{tmp}.json').read_text()); raw = (OUT / f'{tmp}.bin').read_bytes()
    for ext in ('json', 'bin'): (OUT / f'{tmp}.{ext}').unlink()
    objs = {o['name']: np.array(o['matrix'], np.float64).reshape(4, 4) for o in meta['objects']}
    baked = pe.car_prims(meta, raw, cache, texblob, True, light, tire=False)    # поза покоя, мир модели; светотень — свет уровня (§12). №1061: колёса без своей текстуры — дальняя версия (суп) та же, что была
    local = pe.car_prims(meta, raw, cache, texblob, False, light)   # оси сустава (колёса)
    wheel = {}
    for nm_, M in objs.items():
        if re.fullmatch(r'w[0-3]', nm_): wheel[int(nm_[1])] = M[3, :3].copy()
    dl = seat_joint(p)   # №666: сустав сиденья водителя dl (как psp_export.car_info), y в осях модели
    # правка 354 [PS2 код SLES 0x27a140]: колёса-заглушки машин из tools/wheel_clear.HIDDEN оригинал не рисует — ни в ближней версии,
    # ни в дальней (раньше LOD склеивал кубы honor_v в суп); подъём/радиус меряем по всем кускам, как прежде. TRF_WHEELS_OLD=1 — прежнее
    hide = 0 if WHEELS_OLD else HIDDEN_WHEELS.get(name.lower(), 0)
    shown = lambda n_: not (re.fullmatch(r'w[0-3]', n_) and hide >> int(n_[1]) & 1)
    allP = np.concatenate([P for _, _, _, P, _, _ in baked])
    lift = -float(allP[:, 1].min())
    body = [b for b in baked if not re.fullmatch(r'w[0-3]', b[0])]
    bodyP = np.concatenate([P for _, _, _, P, _, _ in body])
    wr = max((float(np.abs(P[:, 1]).max()) for n_, _, _, P, _, _ in local if re.fullmatch(r'w[0-3]', n_)), default=.37)
    first = len(pieces)
    # ближняя версия: кузов одним куском на (текстура, флаги), колёса — отдельными кусками в осях колеса
    # №666: водитель машины — запечён в модель (материал char_swatches*: цветные пятна персонажей, фигура за рулём); его куски ближней версии — owner 4:
    # движок рисует их у ИИ-машин и не рисует у машины игрока (угнанная: водителя выбрасывают, иначе двойник с Гомером). TRF_DRIVER_OLD=1 (экспорт) — как прежде, owner 0
    drv_tex = set()
    if not DRIVER_OLD:
        for obj_ in meta['objects']:
            for prim_ in meta['meshes'].get(obj_['mesh'], []):
                if prim_['shader'].lower().startswith('char_swatches'):
                    mat_ = meta['materials'].get(prim_['shader'], {}); fl_ = (1 if mat_.get('alpha') else 0) | (2 if mat_.get('translucent') or mat_.get('blend') else 0)
                    drv_tex.add(pe.load_tex(mat_.get('textureUrl', ''), cache, texblob, blend=bool(fl_ & 2)))
    grp = {}
    for n_, t, fl, P, UV, C in body: grp.setdefault((t, fl, t in drv_tex), []).append((P, UV, C))
    for (t, fl, drv_), parts in sorted(grp.items(), key=lambda kv: (kv[0][1] & 2, kv[0][0])):   # непрозрачное раньше стекла
        P = np.concatenate([a for a, _, _ in parts]); UV = np.concatenate([b for _, b, _ in parts]); C = np.concatenate([c for _, _, c in parts])
        pieces.append(dict(rec=pack_piece(P, UV, C, t, fl, (0, 0), vblob, owner=4 if drv_ else 1), tris=len(P) // 3, tex=t))
    for n_, t, fl, P, UV, C in local:
        if not re.fullmatch(r'w[0-3]', n_) or not shown(n_): continue
        k = int(n_[1]); R = objs[n_][:3, :3]
        Pr = (P.astype(np.float64) @ R).astype(np.float32)       # поворот сустава (обычно единичный), без сдвига
        pieces.append(dict(rec=pack_piece(Pr, UV, C, t, fl, (k + 1, 0), vblob), tris=len(P) // 3, tex=t))
    # №465 (стоп-сигналы у трафика и машин миссий, как у машины игрока): блики brake*/rev* — куски без вершин (owner 3, ni 0), параметры в полях записи: bb[0..2] = ширина,
    # высота, вынос к камере; flags = 16 | вид << 8 (0 стоп-сигнал, 1 задний ход) | режим << 10 (0 YAX, 1 LYAX, 2 AAX); tex — текстура пакета (блик, смешивание); nf = ABGR;
    # ox, oy, oz — место в осях модели. Движок рисует их, когда машина тормозит (viewer/traffic.c trafficDrawModel, main_render.h trDrawPiece/mcDrawPiece);
    # старый движок кусок без вершин не рисует (ничего не ломается). TRF_BRAKELIGHT_OLD=1 — без бликов в пакете
    if os.environ.get('TRF_BRAKELIGHT_OLD') != '1':
        for rec, ftex in car_flares(conv, p, cache, texblob):
            pieces.append(dict(rec=rec, tris=0, tex=ftex))
    near = len(pieces) - first
    # дальняя версия (> 30 м): всё в позе покоя одним супом -> <= 400 треуг., колёса не крутятся
    soup = [b for b in baked if shown(b[0])]
    P = np.concatenate([b[3] for b in soup]); UV = np.concatenate([b[4] for b in soup]); C = np.concatenate([b[5] for b in soup])
    T = np.concatenate([np.full(len(b[3]) // 3, b[1]) for b in soup]); Fl = np.concatenate([np.full(len(b[3]) // 3, b[2]) for b in soup])
    ntri = len(P) // 3
    target = LOD_TARGET if ntri > LOD_TRIS else ntri
    sP, sUV, sC, sT, sF = simplify_soup(P, UV, C, T, Fl, target) if ntri > LOD_TRIS else (P, UV, C, T, Fl)
    lod_first = len(pieces); lod_tris = 0
    for (t, fl) in sorted({(int(a), int(b)) for a, b in zip(sT, sF)}, key=lambda k_: (k_[1] & 2, k_[0])):
        m = (sT == t) & (sF == fl); mm = np.repeat(m, 3)
        pieces.append(dict(rec=pack_piece(sP[mm], sUV[mm], sC[mm], t, fl, (0, 1), vblob), tris=int(m.sum()), tex=t))
        lod_tris += int(m.sum())
    nlod = len(pieces) - lod_first
    lo, hi = bodyP.min(0), bodyP.max(0)
    wz = [wheel[k][2] for k in wheel]; wx = [abs(wheel[k][0]) for k in wheel]
    front = min(wz) if wz else lo[2] + 1.0; rear = max(wz) if wz else hi[2] - 1.0      # модель смотрит в -Z
    geo = dict(halfW=float(max(-lo[0], hi[0])), halfL=float((hi[2] - lo[2]) / 2), bodyOfs=float(-(lo[2] + hi[2]) / 2),
               wheelX=float(np.mean(wx)) if wx else .9, wheelF=float(-front), wheelR=float(rear), wheelRad=float(wr),
               bodyLo=float(max(.15, lo[1] + lift)), bodyHi=float(hi[1] + lift))
    wl = [(wheel[k] if k in wheel else np.zeros(3)) for k in range(4)]
    # правка Роба 198 (03.10): камера за машиной — своя у каждой машины (FollowCameraData 0x3000100 модели, ближний вариант, как у
    # машины уровня в CAR1 v2): у школьного автобуса 8,7 м, у грузовика glastruc 8,15, у famil_v 7,2. Угнанная машина трафика брала
    # камеру famil_v. В TrModel — бывшие pad/pad2 (0 — старый пакет: движок оставляет камеру famil_v)
    cams = sorted(struct.unpack_from('<I6f', c.data) for c in read(p).children if c.id == 0x3000100)
    geo['camElev'], geo['camMag'] = (float(cams[0][2]), float(cams[0][3])) if cams else (0.0, 0.0)
    print(f'  {name}: {ntri} треуг. ({near} кусков), LOD {lod_tris} треуг. ({nlod} кусков), lift {lift:.2f}, '
          f'кузов {2 * geo["halfW"]:.2f} x {2 * geo["halfL"]:.2f} x {geo["bodyHi"]:.2f} м, колесо R {wr:.2f}')
    return dict(name=name, dl=(dl.tolist() if dl is not None else [0.0, 99.0, 0.0]), first=first, n=near, lodFirst=lod_first, nLod=nlod, lift=lift, geo=geo, wheel=wl, tris=ntri, lodTris=lod_tris)


# ------------------------------------------------------------------ пешеходы: общий блоб CHR1
class Blob(ce.Blob):
    pass


def ped_blob(names):
    """-> (bytes блоба, [(имя, смещение заголовка, рост)]). Клипы/текстуры общие; ключи клипов — без повторов."""
    anims = read(GAME / 'art/chars/npd_a.p3d')
    gimg = ce.load_images(read(GAME / 'art/chars/global.p3d'))
    bl = Blob(); tex_off = {}; key_off = {}; clip_sets = {}; out = []
    def tex(tn):
        if tn not in tex_off:
            w, h, clut, idx = gimg[tn]
            oc = bl.add(clut.tobytes(), 16); op = bl.add(ce.swizzle(idx, w, h), 16); tex_off[tn] = (w, h, oc, op)
        return tex_off[tn]
    def keys(data):
        if data not in key_off: key_off[data] = bl.add(data, 4)
        return key_off[data]
    for name in names:
        mroot = read(GAME / f'art/chars/{name[:6]}_m.p3d')
        joints = ce.load_skeleton(mroot); shaders = ce.load_shaders(mroot)
        groups = ce.load_strips(mroot)   # и скин без родного VIF (busm2) — char_export.generic_strip (правка 180: кость от старшего байта)
        # имена суставов: у olady1 'Ass_joint' вместо 'Ass_Joint' — анимации ищем без учёта регистра
        lower = [dict(j, name=j['name']) for j in joints]
        canon = {j['name'].lower(): j['name'] for j in joints}
        used = []; tex_index = {}; own = None; src = {}
        for shader, _ in groups:
            tn = shaders.get(shader, {}).get('TEX', '')
            if tn in ('char_swatches.bmp', 'char_swatches_lit.bmp1'): tn = 'char_swatches_lit.bmp'
            if tn not in gimg:   # нет в общем global.p3d — из файла модели (зомби уровня 7: zombie5_dress.bmp в zfem5_m.p3d)
                own = ce.load_images(mroot) if own is None else own
                if tn not in own: raise ValueError(f'{name}: texture {tn} not in global.p3d nor {name[:6]}_m.p3d')
                src[tn] = f'{name[:6]}_m:{tn}'; gimg.setdefault(src[tn], own[tn])
            if tn not in used: used.append(tn)
            tex_index[tn] = used.index(tn)
        batches = ce.build_batches(groups, shaders, tex_index, None)
        batches = [dict(bt, **ce.batch_vertices(bt)) for bt in batches]
        nj = len(joints); sig = tuple(j['name'].lower() for j in joints)
        # клипы: один набор записей на порядок суставов
        if sig not in clip_sets:
            aj = [dict(j, name=j['name']) for j in joints]
            # load_clips сопоставляет имена точно -> подменяем имена суставов на имена из анимаций
            anim_names = {}
            for an in anims.children:
                if an.id != 0x121000: continue
                for g in walk(an):
                    if g.id == 0x121001: anim_names.setdefault(string(g.data, 4)[0].lower(), string(g.data, 4)[0])
            for j in aj: j['name'] = anim_names.get(j['name'].lower(), j['name'])
            clips = [c for c in ce.load_clips(anims, aj) if c['name'] in PED_CLIPS]
            clips.sort(key=lambda c: PED_CLIPS.index(c['name']))
            recs = []
            for cl in clips:
                offTr = bl.add(bytes(16 * nj)); has_root = 0
                for j, tr in enumerate(cl['tracks']):
                    nr, nt = len(tr['rf']), len(tr['tf']); orr = ot = 0
                    if nr:
                        fr = struct.pack(f'<{nr}H', *tr['rf']); fr += bytes(ce.align(len(fr), 4) - len(fr))
                        orr = keys(fr + np.clip(np.round(tr['rq'] * 32767), -32767, 32767).astype('<i2').tobytes())
                    if nt:
                        fr = struct.pack(f'<{nt}H', *tr['tf']); fr += bytes(ce.align(len(fr), 4) - len(fr))
                        ot = keys(fr + tr['tv'].astype('<f4').tobytes())
                        if j == 0: has_root = 2
                    struct.pack_into('<HHII', bl.b, offTr + 16 * j, nr, nt, orr, ot)
                dur = (cl['frames'] - 1) / cl['fps']
                recs.append(ce.cname(cl['name'], 48) + struct.pack('<fffII', cl['frames'], cl['fps'], dur, (cl['loop'] & 1) | has_root, offTr) + bytes(12))
            clip_sets[sig] = (bl.add(b''.join(recs), 16), len(recs))
        offC, ncl = clip_sets[sig]
        # заголовок + суставы + партии + записи текстур
        offH = bl.add(bytes(ce.HDR), 16)
        offJ = bl.add(bytes(ce.JNT * nj), 16)
        for j, jt in enumerate(joints):
            inv = jt['inv']; cols = [inv[:3, c] for c in range(4)]
            rec = ce.cname(jt['name'], 32) + struct.pack('<iI', jt['parent'], 0) + struct.pack('<4f', *jt['q']) + \
                struct.pack('<3f', *jt['t']) + struct.pack('<3f', *jt['s']) + struct.pack('<12f', *np.concatenate(cols))
            bl.b[offJ + ce.JNT * j: offJ + ce.JNT * j + len(rec)] = rec
        offT = bl.add(bytes(ce.TEXR * len(used)), 16)
        for i, tn in enumerate(used):
            w, h, oc, op = tex(src.get(tn, tn))
            rec = ce.cname(tn, 24) + struct.pack('<HHIIII', w, h, ce.GU_PSM_T8, oc, op, 1)
            bl.b[offT + ce.TEXR * i: offT + ce.TEXR * i + len(rec)] = rec
        offB = bl.add(bytes(ce.BAT * len(batches)), 16); tv = 0
        for i, bt in enumerate(batches):
            ov = bl.add(bt['data'], 16); tv += bt['nverts']
            bones = bytes(bt['bones']) + bytes(8 - bt['nb'])
            rec = struct.pack('<IIII', bt['vtype'], ce.GU_TRIANGLE_STRIP, bt['nverts'], ov) + struct.pack('<HH', bt['tex'], bt['nb']) + bones \
                + struct.pack('<II', bt['stride'], 2 if bt['alpha'] else 0)
            bl.b[offB + ce.BAT * i: offB + ce.BAT * i + len(rec)] = rec
        offBl = bl.add(bytes(8), 16)
        allp = np.concatenate([s['pos'] for bt in batches for s in bt['strips']])
        bbox = list(allp.min(0)) + list(allp.max(0))
        struct.pack_into('<4s13I7f3I', bl.b, offH, b'CHR1', 1, 0, nj, len(batches), len(used), ncl, 0, offJ, offB, offT, offC, offBl, 0,
                         *bbox, 0., 0xFFFFFFFF, tv, 0)
        out.append((name, offH, float(bbox[4] - bbox[1])))
        print(f'  {name}: {nj} суставов, {len(batches)} партий GE, {tv} вершин, клипов {ncl}, рост {bbox[4] - bbox[1]:.2f} м')
    bl.at(64)
    # размер блоба в каждом заголовке (skin.h: size — «файл»; у нас — весь блоб)
    for _, offH, _ in out: struct.pack_into('<I', bl.b, offH + 8, len(bl.b))
    return bytes(bl.b), out


# ------------------------------------------------------------------ сборка файла
def main(level, out_path):
    traffic, pedgroups = level_script(level)
    junc, roads = road_network(level)
    paths, zones = ped_paths_zones(level)
    print(f'дороги: {len(junc)} перекрёстков, {len(roads)} дорог ({sum(1 for r in roads if not r["shortcut"] and r["maxCars"])} с трафиком), '
          f'{sum(len(r["pts"][0]) * r["lanes"] for r in roads)} точек полос; пути пешеходов {len(paths)}, зон групп {len(zones)}')
    print(f'трафик (группа 0): {traffic}')
    print(f'группы пешеходов: {pedgroups}')

    conv = Converter(); cache = {}; texblob = []; vblob = bytearray(); pieces = []
    models = []
    names = [t[0] for t in traffic]
    light = level_light.level_light(level)   # машины трафика и стоящие освещаются группой sun уровня (tools/level_light, §12)
    print(f'машины трафика… (свет уровня {level}: фон ×{tuple(round(x, 3) for x in light["ambS"])}, '
          f'направленный ×{tuple(round(x, 3) for x in light["dirS"])})', flush=True)
    for n in names + ['huskA']:
        models.append(car_model(conv, n, cache, texblob, vblob, pieces, light=light))
    used = sorted({p['tex'] for p in pieces if p['tex'] >= 0}); tmap = {t: i for i, t in enumerate(used)}
    recs = bytearray()
    for p in pieces:
        r = bytearray(p['rec']); t, = struct.unpack_from('<i', r, 24); struct.pack_into('<i', r, 24, tmap.get(t, -1)); recs += r
    slots = []
    for n, cnt, park in traffic:
        slots += [(names.index(n), 1 if park else 0)] * cnt
    # стоящие машины: модели с третьим аргументом 1; место -> модель по FNV-1a имени (правило порта, см. шапку)
    park_models = [names.index(n) for n, _, park in traffic if park]
    spots = parked_spots(level) if park_models else []
    # Правка Роба 337 (05.10: «сектор L1 J14-2 перегружен припаркованными грузовиками; парковки часто — срезки и трамплины; в
    # оригинале столько не стояло»; помощник 302: на записи PS2 2:50 и 10:02 стоянка Kwik-E-Mart пуста, а порт занимал все места).
    # ПРАВИЛО ПОРТА (какие места заняты, в данных нет; вопрос близнецу В58): занята примерно треть мест — стабильно по FNV-1a имени
    # места (TRAFFIC_PARK_PCT, по умолчанию 35 %), и ни одной машины ближе RAMP_CLEAR м к трамплину (цели ramp ботов
    # build/bots/targets-l<N>.tsv). TRAFFIC_PARK_ALL=1 — прежнее: заняты все места
    # №490 (Роб 10.10: «припаркованные вдоль улиц в оригинале РЕДКО»): по умолчанию занято около 12 % мест (каждое восьмое, равномерно по
    # хешу имени места, детерминированно), было 35 %. Места миссий и сюжета — отдельные записи, сюда не входят. TRAFFIC_PARK_PCT_OLD=1 — 35 %
    if spots and os.environ.get('TRAFFIC_PARK_ALL') != '1':
        pct, RAMP_CLEAR = int(os.environ.get('TRAFFIC_PARK_PCT', '35' if os.environ.get('TRAFFIC_PARK_PCT_OLD') == '1' else '12')), 18.0
        ramps = []
        tf = Path(__file__).resolve().parents[1] / f'build/bots/targets-l{level}.tsv'
        if tf.exists():
            for ln in tf.read_text().splitlines():
                f_ = ln.split('\t')
                if len(f_) >= 3 and f_[0] == 'ramp':
                    xyz = f_[2].split(); ramps.append((float(xyz[0]), float(xyz[2])))
        keep = [sp for sp in spots if not any((sp[1] - rx) ** 2 + (sp[3] - rz) ** 2 < RAMP_CLEAR ** 2 for rx, rz in ramps)
                and fnv1a(sp[0] + '#park') % 100 < pct]
        print(f'стоящие машины (правка 337): из {len(spots)} мест занято {len(keep)} ({pct} %, у {len(ramps)} трамплинов пусто)')
        spots = keep
    masks = spot_masks(level, {nm for nm, *_ in spots}) if spots else {}   # правка 139: файл(ы) зоны места
    krec = bytearray()
    for nm, x, y, z, h in spots:
        krec += struct.pack('<4f12sHH', x, y, z, h, nm.encode('latin1')[:11], park_models[fnv1a(nm) % len(park_models)], masks.get(nm, 0))
    loaders = dyna_loaders(level) if spots else []
    lrec = b''.join(struct.pack('<12fI3I', *L['c'], *L['ax'], *L['az'], *L['h'], L['mask'], 0, 0, 0) for L in loaders)
    print(f'зоны стоящих (правка 139): {len(loaders)} ворот уличных загрузчиков, {sum(1 for v in masks.values() if v)} мест с файлом зоны')
    print(f'стоящие машины: {len(spots)} мест, модели {[names[m] for m in park_models]}')

    print('пешеходы…', flush=True)
    ped_names = []
    for g in sorted(pedgroups):
        for n, _ in pedgroups[g]:
            if n not in ped_names: ped_names.append(n)
    pblob, pinfo = ped_blob(ped_names)

    # --- секции
    body = bytearray()
    def sec(data, a=16):
        nonlocal body
        body = al(body, a); off = HDR + len(body); body += data; return off
    jrec = bytearray(); outl = []
    for j in junc:
        jrec += struct.pack('<4fIHHHHI', *j['pos'], j['r'], j['beh'], len(outl), len(j['out']), j['nin'], 0, 0); outl += j['out']
    rrec = bytearray(); prec = bytearray(); npt = 0
    for r in roads:
        flags = (1 if r['shortcut'] else 0) | (2 if r['maxCars'] == 0 else 0)
        n = len(r['pts'][0])
        rrec += struct.pack('<4H4BIHHfff', r['s'], r['e'], r['rev'], r['lanes'], flags, min(r['maxCars'], 255), r['speed'], 0,
                            npt, n, 0, float(r['pts'][0][-1, 3]), r['width'], 0.)
        for k in range(r['lanes']):
            prec += r['pts'][k].astype('<f4').tobytes(); npt += n
    mrec = bytearray()
    for m in models:
        g = m['geo']
        mrec += struct.pack('<16s4H10f12ff3I', m['name'].encode()[:15], m['first'], m['n'], m['lodFirst'], m['nLod'],
                            m['lift'], g['wheelRad'], g['halfW'], g['halfL'], g['bodyOfs'], g['wheelX'], g['wheelF'], g['wheelR'],
                            g['bodyLo'], g['bodyHi'], *np.concatenate(m['wheel']).tolist(), g.get('camElev', 0.), m['tris'], m['lodTris'], 0)
        struct.pack_into('<f', mrec, len(mrec) - 4, g.get('camMag', 0.))   # правка 198: бывший pad2 — дистанция камеры
    trec = bytearray(); toffs = []
    for t in used:
        toffs.append(len(trec)); trec += texblob[t]; trec = al(trec)
    srec = b''.join(struct.pack('<HH', m, f) for m, f in slots)
    parec = bytearray(); ptrec = bytearray(); npp = 0
    for P in paths:
        L = float(np.linalg.norm(np.diff(np.r_[P, P[:1]], axis=0), axis=1).sum()); c = P.mean(0)
        rad = float(np.linalg.norm((P - c)[:, [0, 2]], axis=1).max())
        parec += struct.pack('<II4f2I', npp, len(P), L, c[0], c[2], rad, 0, 0); ptrec += P.astype('<f4').tobytes(); npp += len(P)
    zrec = bytearray()
    for z in zones:
        zrec += struct.pack('<12fI3I', *z['c'], *z['ax'], *z['az'], *z['h'], z['group'], 0, 0, 0)
    grec = bytearray()
    for g in range(max(pedgroups) + 1 if pedgroups else 0):
        lst = pedgroups.get(g, [])[:4]
        mi = [ped_names.index(n) for n, _ in lst] + [0xFF] * (4 - len(lst)); cn = [c for _, c in lst] + [0] * (4 - len(lst))
        grec += struct.pack('<B3x4B4B4x', len(lst), *mi, *cn)
    pmrec = b''.join(struct.pack('<16sIf2I', n[:15].encode(), off, h, 0, 0) for n, off, h in pinfo)

    oJ = sec(jrec); oR = sec(rrec); oP = sec(prec); oO = sec(struct.pack(f'<{len(outl)}H', *outl) if outl else b'')
    oM = sec(mrec); oPc = sec(recs); oV = sec(bytes(vblob)); oT = sec(bytes(trec)); oS = sec(srec)
    oPa = sec(parec); oPp = sec(ptrec); oZ = sec(zrec); oG = sec(grec); oPm = sec(pmrec); oB = sec(pblob, 64)
    oK = sec(krec); oL = sec(lrec)
    fields = [len(junc), oJ, len(roads), oR, npt, oP, len(outl), oO, len(models), oM, len(pieces), oPc, oV, len(vblob),
              len(used), oT, len(trec), len(slots), oS, len(paths), oPa, npp, oPp, len(zones), oZ, len(grec) // 16, oG,
              len(pinfo), oPm, oB, len(pblob), len(models) - 1, len(spots), oK, len(loaders), oL]
    oLg = sec(level_light.block(light))   # LGT1 (свет, которым запечены машины): смещение — поле @156 заголовка (0 — старый файл)
    fields.append(oLg)
    fields.append(sec(b''.join(struct.pack('<3f', *m['dl']) for m in models)))   # №666: TRF1 @160 offSeat — сустав dl (x, y, z; y = 99 — нет) по моделям, 12 Б на модель (0 — старый файл)
    total = HDR + len(al(body, 16))
    head = struct.pack('<4sII', b'TRF1', 1, total) + struct.pack(f'<{len(fields)}I', *fields)
    head += b'\0' * (HDR - len(head))
    data = head + al(body, 16)
    # смещения текстур: после заголовка каждой (16 Б) + CLUT 1024 Б — как в level.shr (читает traffic.c)
    dst = Path(out_path); dst.parent.mkdir(parents=True, exist_ok=True); dst.write_bytes(data)
    print(f'{dst.name}: {len(data) / 1024:.0f} КБ — дороги {len(rrec) + len(prec) + len(jrec)} Б, машины {len(vblob) / 1024:.0f} КБ + '
          f'текстуры {len(trec) / 1024:.0f} КБ ({len(used)}), пешеходы {len(pblob) / 1024:.0f} КБ ({len(pinfo)} моделей), '
          f'пути {len(ptrec) / 1024:.0f} КБ')


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description='SHAR traffic + pedestrians -> TRF1')
    ap.add_argument('--level', type=int, default=1)
    ap.add_argument('--out', default=str(ROOT / 'build' / 'traffic.shr'))
    a = ap.parse_args()
    main(a.level, a.out)
