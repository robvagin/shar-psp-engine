#!/usr/bin/env python3
"""Комнаты уровня (пункт 11.2): коллизия каждой комнаты l{N}iNN.p3d, старт и выход внутри, двери в городе -> пакет SHI1;
рисование комнат (тот же конвейер, что город: tools/psp_export.py pack_prims / load_tex) -> пакет SHG1 рядом.
  python3 tools/interior_export.py [--level 1] [--no-gfx] [--outdir build]
    -> build/interiors-l1.bin (+ .json для чтения), build/interiors-l1-gfx.bin (рисование, viewer/main.c)
Только чтение диска Роба; тот же разбор коллизии, что у города (tools/psp_export.py col_collect / col_section).
Качество картинки — закон 1 очереди: SHR_TEX_CAP256=1 SHR_SMALL_MIPS=1 SHR_LAYERS=1 по умолчанию (как tools/build_level.sh);
текстуры комнат — до 256 px (родной размер диска, комната в памяти одна).

Формат SHI1 (little endian): 'SHI1', u32 комнат, u32 дверей, u32 версия (2); комната 128 Б: имя[32], старт x y z курс (4f),
выход x y z (3f), u32 смещение COL1, триггер выхода 64 Б; дверь 128 Б: имя комнаты[32], x y z (3f), i32 номер комнаты,
f32 курс «наружу», 12 Б нулей, триггер двери 64 Б; дальше блобы COL1 (формат viewer/col.h). Триггер (TriggerVolume
0x3000006 локатора, разбор — tools/missions/p3dlocs.py): u32 вид (0 сфера, 1 коробка, 0xFFFFFFFF нет), центр 3f, полуоси 3f,
оси 3x3f (строки, единичные). Старые версии (без курса двери — 0, без триггеров — 1: записи 64 Б / двери 48 или 64 Б)
viewer/rooms.c читает и переходит по радиусам. Комнаты лежат в тех же координатах мира, что город, глубоко под ним
(y ≈ −20…−100). Разбор и переходы — viewer/rooms.c: вход — Гомер пешком внутри триггера двери, выход — внутри триггера
выхода (оба взводятся, когда Гомер побывал снаружи), как в оригинале; радиусы — только для пакетов без триггеров.
Данные: старт — локатор InteriorEntryEnd в файле комнаты, курс старта — от InteriorEntryStart к InteriorEntryEnd (куда
Гомер входит); выход — локатор *_exit и его триггер; дверь — локатор вида 7 (Interior) с именем комнаты; ось z его матрицы смотрит в
здание (куда входят), курс «наружу» — обратный; триггер двери — коробка перед дверью (сверено с землёй COL1: у Kwik-E-Mart и школы за дверью земли нет). Номер файла -> имя, как в
скриптах (0 SpringfieldElementary, 1 KwikEMart, 2 SimpsonsHouse, …, тот же порядок, что hit-and-run-web/tools/campaign.py).

Формат SHG1: 'SHG1', u32 комнат, u32 версия (1), u32 0; таблица комнат по 48 Б: имя[32], u32 смещение, u32 размер, 8 Б нулей;
блоб комнаты (смещение кратно 16, грузится одним чтением при входе):
  заголовок 128 Б: 'RMG1', u32 текстур, u32 кусков, u32 смещение текстур, u32 смещение таблицы кусков (от начала блоба),
  u32 цвет фона ABGR (в данных комнаты фона нет: чёрный), u32 фоновый свет ABGR, u32 направленных источников (до 4),
  4 x (f32 x y z направление НА источник, u32 цвет ABGR) — LightGroup комнаты (0x2380: Light 0x13000, LightDirection 0x13001),
  6 f32 габарит, u32 смещение таблицы камер (от начала блоба), u32 камер;
  текстуры — ровно формат SHR7 (заголовок 16 Б + палитра 1024 Б + уровни в swizzle), каждая с кратного 16;
  куски — записи 80 Б как Rec в SHR7 (tex — номер текстуры комнаты или −1, owner 0, off — от начала блоба, nf 0),
  вершины без индексов (флаг 16), 12 Б с цветом 5650, если все вершины куска непрозрачны (флаг 32), иначе 16 Б.
  камеры — записи 512 Б (разбор и поведение — viewer/rooms.c roomCam*): локаторы комнаты вида 12 (StaticCam) и вида 4
  (Spline: рельс 0x3000007 + RailCam 0x300000A), в порядке файла, со своими TriggerVolume (коробки: в какой стоит Гомер —
  та камера и снимает). Смещение: 0 имя[32]; 32 u32 вид (0 статичная, 1 рельс), триггеров (до 4), точек рельса (сколько в данных),
  следить за игроком (StaticCam FollowPlayer); 48 f32 позиция (статичная: локатор); 60 f32 цель (статичная: точка мира или
  смещение от Гомера при слежении); 72 f32 FOV (градусы, как в данных); 76 TargetLag; 80 PositionLag (рельс) /
  TransitionTargetRate (статичная); 84 MinRadius, MaxRadius, TrackDist (рельс); 96 u32 TrackRail, ReverseSense, Behaviour
  (рельс) / Flags (статичная); 108 f32 TargetOffset (рельс); 120 u32 CutInOut, Data (статичная); 128 триггеры 4 x 64 Б (как в
  SHI1); 384 точки рельса 9 x 3 f32; 492 f32 AxisPlay (рельс, движком не используется); 504 u32 0 — точки на месте 384, иначе
  (рельс длиннее 9 точек: Обсерватория l3i06/l6i06, 13) смещение от начала блоба массива ВСЕХ точек рельса (12 Б на точку, после
  таблицы камер, cam_table); 508 4 Б нулей. Все точки и векторы —
  в координатах движка (z отзеркален). Поля StaticCam (10 u32 данных локатора: TargetPosition 3f, FOV, TargetLag,
  FollowPlayer, TransitionTargetRate, Flags, CutInOut, Data) и RailCam (Behaviour, MinRadius, MaxRadius, TrackRail,
  TrackDist, ReverseSense, FOV, TargetOffset 3f, AxisPlay 3f, PositionLag, TargetLag) — по порядку в данных.
Свет стен — цвет вершин из данных (запечённый, как у города); LightGroup — для Гомера (skinLights в viewer/skin.c)."""
import argparse, json, os, re, struct, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
GAME = (ROOT / 'hit-and-run-web/source/game').resolve()   # как у convert.py (worktree: ссылка на общий диск)
NAMES = {0: 'SpringfieldElementary', 1: 'KwikEMart', 2: 'SimpsonsHouse', 3: 'DMV', 4: 'Moes', 5: 'AndroidsDungeon', 6: 'Observatory', 7: 'BartRoom'}
SHI_VERSION = 2
NO_TRIG = struct.pack('<I15f', 0xFFFFFFFF, *[0.0] * 15)
# рисование: треугольники длиннее режем (как город, tools/psp_export.py subdivide_lod) — PSP выбрасывает треугольник
# целиком, если вершина вне гвард-бэнда; в комнате камера пешком в 1.2–4.2 м от Гомера и у самых стен, поэтому мельче,
# чем в городе (14 / 7 м при камере 7 м за машиной)
ROOM_SPLIT, ROOM_SPLIT_GROUND = 3.0, 2.0
ROOM_CHUNK = 8.0      # куски по клеткам 8 x 8 м: отсечение пирамидой в комнате
ROOM_TEX_CAP = 256    # предел стороны текстуры комнаты (на диске комнат больше 256 нет)
ROOM_CLEAR = 0xFF000000
ROOM_GRID = 2.0 ** -10   # шаг общей решётки вершин комнаты (~1 мм, ±32 м), как CITY_GRID у города (tools/psp_export.py)


def locators(path):
    """локаторы 0x3000005: имя, вид, позиция (z отзеркален, как у города), курс (вид 3), имя комнаты (вид 7),
    матрица 3x3 (виды 7 и 8: 9 f32 в конце данных) -> ось z в наших координатах"""
    from p3d import read, walk, string
    out = []
    for c in walk(read(path)):
        if c.id != 0x3000005: continue
        name, p = string(c.data); kind, length = struct.unpack_from('<II', c.data, p)
        payload = c.data[p + 8:p + 8 + length * 4]; x, y, z = struct.unpack_from('<3f', c.data, p + 8 + length * 4)
        it = dict(name=name, kind=kind, pos=[x, y, -z])
        if kind == 3 and len(payload) >= 4: it['heading'] = -struct.unpack_from('<f', payload)[0]
        if kind == 7: it['interior'] = payload.split(b'\0')[0].decode('latin1')
        if kind in (7, 8) and len(payload) >= 36:
            m = struct.unpack_from('<9f', payload, len(payload) - 36)
            it['zaxis'] = [m[6], m[7], -m[8]]   # третья строка матрицы (ось z), зеркало z как у позиций
        out.append(it)
    return out


def triggers(path):
    """имя локатора -> первый триггер (вид, центр, полуоси, оси) в наших координатах — tools/missions/p3dlocs.py"""
    from missions.p3dlocs import read_locators
    L, err = read_locators(path, path.name)
    if err: raise SystemExit(err)
    return {l.name: l.triggers[0] for l in L if l.triggers}


def tstr(t):
    return 'нет' if not t else f"{'коробка' if t[0] else 'сфера'} центр {[round(v, 2) for v in t[1]]} полуоси {[round(v, 2) for v in t[2]]}"


def trig_bytes(t):
    if not t: return NO_TRIG
    kind, c, h, ax = t
    return struct.pack('<I15f', kind, *c, *h, *[v for a in ax for v in a])


def heading(dx, dz):
    import math
    return math.atan2(dx, dz)   # курс движка: вперёд = (sin h, cos h) в xz


CAM_REC, CAM_TRIGS, CAM_PTS = 512, 4, 9   # CAM_PTS — точек рельса на месте в записи; длиннее — хвост (cam_table)


def cam_table(recs, base):
    """Записи room_cams (512 Б + хвост точек у рельса длиннее CAM_PTS) -> таблица камер с началом base (от начала блоба/файла):
    записи подряд по 512 Б, за ними хвосты; в @504 записи с хвостом — смещение её точек. Без хвостов — ровно записи подряд."""
    head, tail = bytearray(), bytearray(); end = base + CAM_REC * len(recs)
    for r in recs:
        r = bytearray(r)
        if len(r) > CAM_REC:
            struct.pack_into('<I', r, 504, end + len(tail)); tail += r[CAM_REC:]
        head += r[:CAM_REC]
    return bytes(head + tail)


def room_cams(path):
    """камеры комнаты: StaticCam (вид 12) и рельсы (вид 4: Spline + RailCam) с триггерами -> (записи 512 Б [+ хвост точек рельса
    длиннее CAM_PTS], сводка); в пакет — через cam_table"""
    from p3d import read, walk, string
    T = {}
    from missions.p3dlocs import read_locators
    L, err = read_locators(path, path.name)
    if err: raise SystemExit(err)
    for l in L: T[l.name] = l.triggers
    mz = lambda v: (v[0], v[1], -v[2])
    recs, info = [], []
    for c in walk(read(path)):
        if c.id != 0x3000005: continue
        name, p = string(c.data); kind, ln = struct.unpack_from('<II', c.data, p)
        if kind not in (4, 12): continue
        payload = c.data[p + 8:p + 8 + ln * 4]; pos = mz(struct.unpack_from('<3f', c.data, p + 8 + ln * 4))
        trig = T.get(name, [])[:CAM_TRIGS]
        if kind == 12:
            if len(payload) < 40: continue
            tx, ty, tz, fov, tlag, follow, trate, flags, cut, data = struct.unpack_from('<4ffIfIII', payload)
            head = struct.pack('<32s4I3f3f6f3I3f2I', name.encode()[:31], 0, len(trig), 0, follow, *pos, *mz((tx, ty, tz)), fov, tlag, trate,
                               0.0, 0.0, 0.0, 0, 0, flags, 0.0, 0.0, 0.0, cut, data)
            pts, axis = [], (0.0, 0.0, 0.0)
            info.append(f"статичная {name}: позиция {[round(v, 2) for v in pos]}, {'следит, смещение' if follow else 'цель'} "
                        f"{[round(v, 2) for v in mz((tx, ty, tz))]}, FOV {fov:g}, триггеров {len(trig)}")
        else:
            sp = next((s_ for s_ in c.children if s_.id == 0x3000007), None)
            rc = next((s_ for s_ in sp.children if s_.id == 0x300000a), None) if sp else None
            if not sp or not rc: continue
            _, q = string(sp.data); n, = struct.unpack_from('<I', sp.data, q)
            pts = [mz(struct.unpack_from('<3f', sp.data, q + 4 + 12 * i)) for i in range(n)]
            if not pts: raise SystemExit(f'{path.name} {name}: рельс без точек')
            _, q = string(rc.data)
            beh, mn, mx, tr, td, rev, fov, ox, oy, oz, ax, ay, az, pl, tl = struct.unpack_from('<I2fIfIf3f3f2f', rc.data, q)
            head = struct.pack('<32s4I3f3f6f3I3f2I', name.encode()[:31], 1, len(trig), len(pts), 0, *pos, 0.0, 0.0, 0.0, fov, tl, pl,
                               mn, mx, td, tr, rev, beh, *mz((ox, oy, oz)), 0, 0)
            axis = (ax, ay, az)
            info.append(f"рельс {name}: {len(pts)} точек {[round(v, 2) for v in pts[0]]}…{[round(v, 2) for v in pts[-1]]}, FOV {fov:g}, "
                        f"радиусы {mn:g}–{mx:g}, TrackRail {tr} ({td:g}), смещение цели {[round(v, 3) for v in mz((ox, oy, oz))]}, "
                        f"запаздывание {pl:g}/{tl:g}, триггеров {len(trig)}")
        assert len(head) == 128, len(head)
        r = bytearray(head) + b''.join(trig_bytes(t) for t in trig) + NO_TRIG * (CAM_TRIGS - len(trig))
        inl = pts if len(pts) <= CAM_PTS else []   # длиннее 9 — все точки в хвосте записи (cam_table ставит смещение @504)
        r += b''.join(struct.pack('<3f', *v) for v in inl) + b'\0' * 12 * (CAM_PTS - len(inl)) + struct.pack('<3f', *axis)
        r += b'\0' * (CAM_REC - len(r))
        if not inl: r += b''.join(struct.pack('<3f', *v) for v in pts)
        recs.append(bytes(r))
    return recs, info


def light_group(path):
    """LightGroup комнаты -> (фоновый ABGR, [(x, y, z направление НА источник, ABGR)] до 4 самых ярких направленных)"""
    from p3d import read, walk, string
    root = read(path); lights = {}
    for c in walk(root):
        if c.id != 0x13000: continue
        name, p = string(c.data); _, kind, argb = struct.unpack_from('<3I', c.data, p)
        b = struct.pack('<I', argb); col = 0xFF000000 | (b[0] << 16) | (b[1] << 8) | b[2]   # ARGB -> ABGR (GE)
        d = next((struct.unpack_from('<3f', s.data) for s in c.children if s.id == 0x13001), None)
        lights[name] = (kind, col, d, b[2] + b[1] + b[0])
    grp = next((c for c in walk(root) if c.id == 0x2380), None)
    names = list(lights)
    if grp is not None:
        _, p = string(grp.data); n, = struct.unpack_from('<I', grp.data, p); p += 4; names = []
        for _ in range(n): nm, p = string(grp.data, p); names.append(nm)
    amb = 0xFF000000; dirs = []
    for nm in names:
        if nm not in lights: continue
        kind, col, d, lum = lights[nm]
        if kind == 0: amb = col
        elif kind == 2 and d: dirs.append((lum, (-d[0], -d[1], d[2]), col))   # к источнику = −направление; зеркало z
    dirs.sort(key=lambda t: -t[0])
    return amb, [(v, c) for _, v, c in dirs[:4]]


# Правка Роба 229 (03.10: «в доме сверху битый потолок — в углу чёрные квадратные дыры»): у комнат диска потолка нет (камеры
# оригинала туда не смотрят, кадр 4:3 ниже — близнец В17, build/twins/o4/v17_house_cam); кадр 16:9 порта видит над стенами пустоту,
# и она была чёрной (фон кадра комнаты ROOM_CLEAR). Фон комнаты — средний цвет верхней полосы её стен (стены — почти вертикальные
# треугольники в верхних 20 % высоты комнаты; цвет — средний цвет их текстуры × цвет вершин, по площади): дыры сливаются со стеной.
# SHR_ROOM_CLEAR_BLACK=1 — прежний чёрный фон
def room_top_colour(prims, blob, texq):
    import numpy as np
    if not prims: return ROOM_CLEAR
    ys = np.concatenate([P[:, 1] for _, _, P, _, _ in prims]); y0, y1 = float(ys.min()), float(ys.max())
    band = y1 - 0.2 * (y1 - y0)
    tex_avg, acc, wsum = {}, np.zeros(3), 0.0
    for t, fl, P, UV, C in prims:
        if t < 0 or t >= len(blob): continue
        if t not in tex_avg:
            lv = texq.decode(blob[t])[0].astype(np.float64); a = lv[..., 3:4] / 255.0
            tex_avg[t] = (lv[..., :3] * a).sum((0, 1)) / max(1e-6, a.sum())
        T = P.reshape(-1, 3, 3); cy = T[:, :, 1].mean(1)
        n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); ar = np.linalg.norm(n, axis=1) / 2
        ny = np.abs(n[:, 1]) / np.maximum(1e-9, 2 * ar)
        sel = (cy >= band) & (ny < 0.3) & (ar > 1e-4)
        if not sel.any(): continue
        vc = np.asarray(C, np.uint32).reshape(-1, 3)
        vr, vg, vb = (vc & 255).mean(1), ((vc >> 8) & 255).mean(1), ((vc >> 16) & 255).mean(1)
        lit = np.stack([vr, vg, vb], 1)[sel] / 255.0   # GU_TFX_MODULATE: текстура x цвет вершин / 255 (viewer/main.c)
        c = np.clip(lit * tex_avg[t][None, :], 0, 255)
        acc += (c * ar[sel][:, None]).sum(0); wsum += float(ar[sel].sum())
    if wsum <= 0: return ROOM_CLEAR
    r, g, b = [int(round(v)) for v in acc / wsum]
    return 0xFF000000 | (b << 16) | (g << 8) | r


# Правка Роба 301 (04.10: «на потолке всё равно дырки — полигоны не достраиваются»): у комнат диска потолок есть не везде —
# в доме Симпсонов только над прихожей (2 треугольника wall_m по 25,9 м², y −16,71, x 503–511); над гостиной, кухней и столовой
# его нет, стены кончаются на −16,81. Камеры оригинала туда почти не смотрят, а камера ходьбы порта (вне триггеров камер:
# кухня, столовая) и кадр 16:9 видят пустоту над стенами — до 26 % кадра фона (снимки a301_before h4–h6); правка 229 лишь
# красила фон (у дома вышел тёмно-серый 0x3d3638 — дыры). Правило для всех комнат: пол комнаты (земля COL1) — клетками
# CEIL_CELL; клетка без горизонтальной поверхности выше пола + CEIL_MIN — дыра; связная дыра закрывается плоскостью на высоте
# верха своих стен (взвешенная по площади медиана верхних точек вертикальных треугольников рядом, выше пола + CEIL_MIN),
# лицом вниз, с отсечением изнанки (сверху камера видит сквозь неё). Материал — как у потолка комнаты, если он есть
# (текстура, развёртка его самого большого треугольника — шов в шов, средний цвет вершин), иначе — стены у верха (текстура
# с наибольшей площадью, плоская развёртка с плотностью текселей стен). SHR_ROOM_CEIL=0 — без достройки (как до правки 301)
CEIL_CELL, CEIL_MIN, CEIL_NEAR = 0.25, 2.3, 0.6   # м: клетка; ниже — мебель и лампы, не потолок; стены «рядом» с дырой


def room_ceiling(prims, TR):
    """-> (новые prims потолка: (tex, F_SHELL, P, UV, C)), сводка"""
    import numpy as np
    if TR is None or not prims: return [], {}
    TR = np.asarray(TR, np.float64); g = TR[TR[:, 3, 1] > .7][:, :3]
    if not len(g): return [], {}
    lo, hi = g.reshape(-1, 3).min(0), g.reshape(-1, 3).max(0)
    xs = np.arange(lo[0] + CEIL_CELL / 2, hi[0], CEIL_CELL); zs = np.arange(lo[2] + CEIL_CELL / 2, hi[2], CEIL_CELL)
    X, Z = np.meshgrid(xs, zs, indexing='ij'); Y0 = np.full(X.shape, np.inf)

    def cover_tri(a, b, c, X, Z):   # барицентры в xz -> (внутри, y плоскости)
        d = (b[2] - c[2]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[2] - c[2])
        if abs(d) < 1e-12: return np.zeros(X.shape, bool), X * 0
        w1 = ((b[2] - c[2]) * (X - c[0]) + (c[0] - b[0]) * (Z - c[2])) / d; w2 = ((c[2] - a[2]) * (X - c[0]) + (a[0] - c[0]) * (Z - c[2])) / d
        w3 = 1 - w1 - w2; return (w1 >= -1e-6) & (w2 >= -1e-6) & (w3 >= -1e-6), w1 * a[1] + w2 * b[1] + w3 * c[1]

    def cells(t):   # окно клеток под треугольником
        i0 = max(0, int(np.floor((t[:, 0].min() - xs[0]) / CEIL_CELL))); i1 = min(len(xs), int(np.ceil((t[:, 0].max() - xs[0]) / CEIL_CELL)) + 1)
        k0 = max(0, int(np.floor((t[:, 2].min() - zs[0]) / CEIL_CELL))); k1 = min(len(zs), int(np.ceil((t[:, 2].max() - zs[0]) / CEIL_CELL)) + 1)
        return slice(i0, i1), slice(k0, k1)
    for t in g:
        si, sk = cells(t); m, y = cover_tri(t[0], t[1], t[2], X[si, sk], Z[si, sk])
        Y0[si, sk] = np.where(m, np.minimum(Y0[si, sk], y), Y0[si, sk])
    flo = np.isfinite(Y0)
    if not flo.any(): return [], {}
    y_room = float(Y0[flo].min())
    T = np.concatenate([P.reshape(-1, 3, 3) for _, _, P, _, _ in prims]).astype(np.float64)
    FL = np.concatenate([np.full(len(P) // 3, fl) for _, fl, P, _, _ in prims]); TX = np.concatenate([np.full(len(P) // 3, t) for t, _, P, _, _ in prims])
    UV = np.concatenate([u.reshape(-1, 3, 2) for _, _, _, u, _ in prims]).astype(np.float64)
    CC = np.concatenate([np.asarray(c, np.uint32).reshape(-1, 3) for _, _, _, _, c in prims])
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); ar = np.linalg.norm(n, axis=1) / 2; ny = n[:, 1] / np.maximum(1e-12, 2 * ar)
    opaque = (FL & 2) == 0
    cover = np.zeros(X.shape, bool)
    for i in np.nonzero((np.abs(ny) > .5) & opaque & (T[:, :, 1].max(1) >= y_room + CEIL_MIN))[0]:
        si, sk = cells(T[i]); m, y = cover_tri(T[i, 0], T[i, 1], T[i, 2], X[si, sk], Z[si, sk])
        cover[si, sk] |= m & (y >= Y0[si, sk] + CEIL_MIN)
    hole = flo & ~cover
    # №301 (16.10, «плоскости дыр на разной высоте, перепады»): центр клетки 0,25 м внутри потолка прихожей объявлял клетку закрытой, хотя её край уходил
    # до 12,5 см в сторону дыры: вдоль стыка капа с потолком (дом: x 503, z 394,7) оставалась щель 15 см, снизу виден фон и перепад плоскостей.
    # Дыра шире на одну клетку над полом: кап заходит под соседний потолок той же высоты и закрывает шов. ROOM_CEIL_SEAM_OLD=1 — прежнее
    if not os.environ.get('ROOM_CEIL_SEAM_OLD'):
        d = hole.copy(); d[1:] |= hole[:-1]; d[:-1] |= hole[1:]; d[:, 1:] |= hole[:, :-1]; d[:, :-1] |= hole[:, 1:]; hole = d & flo
    wall = (np.abs(ny) < .3) & opaque & (T[:, :, 1].max(1) >= y_room + CEIL_MIN)
    wtop, wc, wa = T[wall, :, 1].max(1), T[wall].mean(1), ar[wall]
    # материал: потолок комнаты (лицом вниз, выше пола + CEIL_MIN) или стены у верха
    ceil = (ny < -.9) & opaque & (T[:, :, 1].min(1) >= y_room + CEIL_MIN) & (TX >= 0)
    lab = np.zeros(X.shape, int); ncomp = 0; out, info = [], dict(cells=int(hole.sum()), parts=[])
    from collections import deque
    for s0 in zip(*np.nonzero(hole)):
        if lab[s0]: continue
        ncomp += 1; lab[s0] = ncomp; q = deque([s0]); mem = []
        while q:
            i, k = q.popleft(); mem.append((i, k))
            for a, b in ((i + 1, k), (i - 1, k), (i, k + 1), (i, k - 1)):
                if 0 <= a < len(xs) and 0 <= b < len(zs) and hole[a, b] and not lab[a, b]: lab[a, b] = ncomp; q.append((a, b))
        mi = np.array(mem); cx, cz = xs[mi[:, 0]], zs[mi[:, 1]]
        near = (wc[:, 0] >= cx.min() - CEIL_NEAR) & (wc[:, 0] <= cx.max() + CEIL_NEAR) & (wc[:, 2] >= cz.min() - CEIL_NEAR) & (wc[:, 2] <= cz.max() + CEIL_NEAR)
        if not near.any(): continue
        o = np.argsort(wtop[near]); cw = np.cumsum(wa[near][o]); h = float(wtop[near][o][np.searchsorted(cw, cw[-1] / 2)])
        h = max(h, float(Y0[mi[:, 0], mi[:, 1]].max()) + CEIL_MIN)
        if ceil.any():
            j = np.nonzero(ceil)[0]; big = j[np.argmax(ar[j])]; tex = int(TX[big])
            A = np.c_[T[big][:, [0, 2]], np.ones(3)]; M = np.linalg.solve(A, UV[big])
            uvf = lambda P: np.c_[P[:, [0, 2]], np.ones(len(P))] @ M
            src = j
        else:
            band = wall & (T[:, :, 1].max(1) >= h - CEIL_NEAR) & (TX >= 0)
            if not band.any(): continue
            j = np.nonzero(band)[0]; tw = {}
            for t_, a_ in zip(TX[j], ar[j]): tw[t_] = tw.get(t_, 0) + a_
            tex = int(max(tw, key=tw.get)); j = j[TX[j] == tex]
            e1 = T[j, 1] - T[j, 0]; e2 = T[j, 2] - T[j, 0]; u1 = UV[j, 1] - UV[j, 0]; u2 = UV[j, 2] - UV[j, 0]
            dens = np.sqrt(np.abs(u1[:, 0] * u2[:, 1] - u1[:, 1] * u2[:, 0]) / np.maximum(1e-9, 2 * ar[j]))
            sdens = float(np.median(dens[np.isfinite(dens)])) if len(dens) else 1.0
            uvf = lambda P, s_=sdens: P[:, [0, 2]] * s_
            src = j
        cw_ = CC[src].reshape(-1); w_ = np.repeat(ar[src], 3)
        col = [int(round(float(((cw_ >> sh) & 255) @ w_ / max(1e-9, w_.sum())))) for sh in (0, 8, 16)]
        abgr = 0xFF000000 | (col[2] << 16) | (col[1] << 8) | col[0]
        # клетки -> прямоугольники (ряды по z, слияние одинаковых рядов по x)
        mask = lab == ncomp; rects = []; open_ = {}
        for i in range(len(xs)):
            runs, k = [], 0
            while k < len(zs):
                if mask[i, k]:
                    k1 = k
                    while k1 < len(zs) and mask[i, k1]: k1 += 1
                    runs.append((k, k1)); k = k1
                else: k += 1
            nxt = {}
            for r in runs:
                if r in open_: nxt[r] = open_[r]
                else: nxt[r] = i
            for r, i0 in open_.items():
                if r not in nxt: rects.append((i0, i, r[0], r[1]))
            open_ = nxt
        for r, i0 in open_.items(): rects.append((i0, len(xs), r[0], r[1]))
        P = []
        for i0, i1, k0, k1 in rects:
            x0, x1 = xs[0] - CEIL_CELL / 2 + i0 * CEIL_CELL, xs[0] - CEIL_CELL / 2 + i1 * CEIL_CELL
            z0, z1 = zs[0] - CEIL_CELL / 2 + k0 * CEIL_CELL, zs[0] - CEIL_CELL / 2 + k1 * CEIL_CELL
            a, b, c, d = (x0, h, z0), (x1, h, z0), (x1, h, z1), (x0, h, z1)
            P += [a, b, c, a, c, d]   # лицом вниз: (b−a)×(c−a) по y < 0 (обход CCW к глазу снизу, viewer/main.c)
        P = np.array(P, np.float32)
        out.append((tex, 128, P, uvf(P.astype(np.float64)).astype(np.float32), np.full(len(P), abgr, np.uint32)))
        info['parts'].append(dict(cells=len(mem), m2=round(len(mem) * CEIL_CELL ** 2, 1), y=round(h, 2), rects=len(rects), tex=tex,
                                  colour=f'{abgr:08x}', src='потолок' if ceil.any() else 'стены'))
    return out, info


# №301 (16.10, вопрос дирижёра «пурпур в доме»): у комнат диска нет стен там, куда камеры оригинала не смотрят (они стоят за стенами). У дома Симпсонов камеры
# комнаты ведут только гостиную (21 из 84 узлов пола), на остальном полу ходячая камера порта видит фон сквозь периметр: по границе пола 244 проб (шаг 0,25 м, три
# высоты 0,6/1,4/2,0 м над полом, луч наружу 3 м), у 137 (56 %) никакого треугольника, то есть дыра в меше, не изнанка. Правило для всех комнат: участок границы
# пола без стены закрывается вертикальной плоскостью от пола до потолка (высота капа достройки потолка, иначе медиана верхов стен), лицом в комнату (отсечение,
# флаг 128), материал и плотность текселей — самой большой стены комнаты. ROOM_WALL_FILL=1 — включить (по умолчанию выкл: игрок до недостроенных зон не доходит). Материал-заплатка не сверен с PS2 (кухню у PS2 игрок не видит).
WALL_ST, WALL_RAY, WALL_HS = 0.25, 3.0, (0.6, 1.4, 2.0)


def wall_probe(T, TR, y_room):
    """-> (клетки пола, xs, zs, список проб (i, k, di, dk, дыра?)) по границе пола: луч наружу на трёх высотах, дыра — если ни один не встретил треугольник"""
    import numpy as np
    g = np.asarray(TR, np.float64); g = g[g[:, 3, 1] > .7][:, :3]
    st = WALL_ST; xs = np.arange(g[:, :, 0].min() - st, g[:, :, 0].max() + 2 * st, st); zs = np.arange(g[:, :, 2].min() - st, g[:, :, 2].max() + 2 * st, st)
    X, Z = np.meshgrid(xs, zs, indexing='ij'); flo = np.zeros(X.shape, bool)
    for t in g:
        i0 = max(0, int((t[:, 0].min() - xs[0]) / st)); i1 = min(len(xs), int((t[:, 0].max() - xs[0]) / st) + 2)
        k0 = max(0, int((t[:, 2].min() - zs[0]) / st)); k1 = min(len(zs), int((t[:, 2].max() - zs[0]) / st) + 2)
        a, b, c = t; d = (b[2] - c[2]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[2] - c[2])
        if abs(d) < 1e-12: continue
        Xs, Zs = X[i0:i1, k0:k1], Z[i0:i1, k0:k1]
        w1 = ((b[2] - c[2]) * (Xs - c[0]) + (c[0] - b[0]) * (Zs - c[2])) / d; w2 = ((c[2] - a[2]) * (Xs - c[0]) + (a[0] - c[0]) * (Zs - c[2])) / d
        flo[i0:i1, k0:k1] |= (w1 >= -1e-6) & (w2 >= -1e-6) & (1 - w1 - w2 >= -1e-6)
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); ln = np.linalg.norm(n, axis=1); good = ln > 1e-9; n[good] /= ln[good][:, None]
    out = []
    for i in range(1, len(xs) - 1):
        for k in range(1, len(zs) - 1):
            if not flo[i, k]: continue
            for di, dk in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if flo[i + di, k + dk]: continue
                dvec = np.array([di, 0.0, dk]); den_all = n @ dvec; use = np.nonzero(good & (np.abs(den_all) > .2))[0]
                hit = False
                for h in WALL_HS:
                    o = np.array([xs[i], y_room + h, zs[k]])
                    tt = (np.einsum('ij,ij->i', n[use], T[use, 0] - o)) / den_all[use]
                    m = (tt > 0) & (tt < WALL_RAY)
                    if not m.any(): continue
                    u_ = use[m]; P = o + dvec * tt[m][:, None]; A, B, C = T[u_, 0], T[u_, 1], T[u_, 2]
                    v0, v1, v2 = C - A, B - A, P - A
                    d00 = np.einsum('ij,ij->i', v0, v0); d01 = np.einsum('ij,ij->i', v0, v1); d11 = np.einsum('ij,ij->i', v1, v1)
                    d20 = np.einsum('ij,ij->i', v2, v0); d21 = np.einsum('ij,ij->i', v2, v1); dn = d00 * d11 - d01 * d01
                    ok_ = np.abs(dn) > 1e-12; uu = np.where(ok_, (d11 * d20 - d01 * d21) / np.where(ok_, dn, 1), -1); vv = np.where(ok_, (d00 * d21 - d01 * d20) / np.where(ok_, dn, 1), -1)
                    if np.any(ok_ & (uu >= -1e-6) & (vv >= -1e-6) & (uu + vv <= 1 + 1e-6)): hit = True; break
                out.append((i, k, di, dk, not hit))
    return flo, xs, zs, out


def room_wall_fill(prims, TR, ceil_info):
    """-> (новые prims стен: (tex, F_SHELL, P, UV, C)), сводка"""
    import numpy as np
    if TR is None or not prims or not os.environ.get('ROOM_WALL_FILL'): return [], {}   # по умолчанию выкл (16.10, решение дирижёра: игрок у PS2 и у порта до этих зон не доходит, стоит у арки)
    TRa = np.asarray(TR, np.float64); g = TRa[TRa[:, 3, 1] > .7][:, :3]
    if not len(g): return [], {}
    y_room = float(g[:, :, 1].min())
    T = np.concatenate([P.reshape(-1, 3, 3) for _, _, P, _, _ in prims]).astype(np.float64)
    FL = np.concatenate([np.full(len(P) // 3, fl) for _, fl, P, _, _ in prims]); TX = np.concatenate([np.full(len(P) // 3, t) for t, _, P, _, _ in prims])
    UV = np.concatenate([u.reshape(-1, 3, 2) for _, _, _, u, _ in prims]).astype(np.float64)
    CC = np.concatenate([np.asarray(c, np.uint32).reshape(-1, 3) for _, _, _, _, c in prims])
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); ar = np.linalg.norm(n, axis=1) / 2; ny = n[:, 1] / np.maximum(1e-12, 2 * ar)
    opaque = (FL & 2) == 0
    wall = (np.abs(ny) < .3) & opaque & (T[:, :, 1].max(1) >= y_room + CEIL_MIN) & (TX >= 0)
    if not wall.any(): return [], {}
    cy = [p['y'] for p in (ceil_info or {}).get('parts', []) if p.get('cells', 0) >= 100]
    if cy: H = float(np.median(cy))
    else:
        wtop, wa = T[wall, :, 1].max(1), ar[wall]; o_ = np.argsort(wtop); cw = np.cumsum(wa[o_]); H = float(wtop[o_][np.searchsorted(cw, cw[-1] / 2)])
    flo, xs, zs, probes = wall_probe(T, TR, y_room)
    st = WALL_ST; hole = {}
    for i, k, di, dk, hl in probes:
        if hl: hole[(i, k, di, dk)] = True
    if not hole: return [], dict(probes=len(probes), holes=0)
    # материал: самая большая стена комнаты (текстура и плотность текселей по площади)
    j = np.nonzero(wall)[0]; tw = {}
    for t_, a_ in zip(TX[j], ar[j]): tw[t_] = tw.get(t_, 0) + a_
    tex = int(max(tw, key=tw.get)); j = j[TX[j] == tex]
    e1 = T[j, 1] - T[j, 0]; e2 = T[j, 2] - T[j, 0]; u1 = UV[j, 1] - UV[j, 0]; u2 = UV[j, 2] - UV[j, 0]
    dens = np.sqrt(np.abs(u1[:, 0] * u2[:, 1] - u1[:, 1] * u2[:, 0]) / np.maximum(1e-9, 2 * ar[j])); sd = float(np.median(dens[np.isfinite(dens)])) if len(dens) else 1.0
    cw_ = CC[j].reshape(-1); w_ = np.repeat(ar[j], 3)
    col = [int(round(float(((cw_ >> sh) & 255) @ w_ / max(1e-9, w_.sum())))) for sh in (0, 8, 16)]; abgr = 0xFF000000 | (col[2] << 16) | (col[1] << 8) | col[0]
    # прогоны вдоль линий границы
    lines = {}
    for (i, k, di, dk) in hole:
        if di: lines.setdefault(('x', round(xs[i] + di * st / 2, 3), di), []).append(zs[k])
        else: lines.setdefault(('z', round(zs[k] + dk * st / 2, 3), dk), []).append(xs[i])
    P_all, nrun = [], 0
    for (ax, e, sgn), vals in sorted(lines.items()):
        vals = sorted(vals); a = b = vals[0]; segs = []
        for v in vals[1:]:
            if v - b <= st * 1.01: b = v
            else: segs.append((a, b)); a = b = v
        segs.append((a, b))
        for a, b in segs:
            c0, c1 = a - st / 2, b + st / 2; nrun += 1
            if ax == 'x': q = [(e, y_room, c0), (e, y_room, c1), (e, H, c1), (e, H, c0)]; inward = np.array([-sgn, 0.0, 0.0])
            else: q = [(c0, y_room, e), (c1, y_room, e), (c1, H, e), (c0, H, e)]; inward = np.array([0.0, 0.0, -sgn])
            q = np.array(q, np.float64)
            if np.cross(q[1] - q[0], q[2] - q[0]) @ inward < 0: q = q[[1, 0, 3, 2]]   # лицом в комнату
            P_all += [q[0], q[1], q[2], q[0], q[2], q[3]]
    P = np.array(P_all, np.float32)
    uvf = np.c_[(P[:, 0] + P[:, 2]) * sd, P[:, 1] * sd].astype(np.float32)
    return [(tex, 128, P, uvf, np.full(len(P), abgr, np.uint32))], dict(probes=len(probes), holes=len(hole), runs=nrun, height=round(H, 2), tex=tex, colour=f'{abgr:08x}', tris=len(P) // 3)



# сверка g11b-collect круг 5 (P16-11, СВЕРКА_ПОЛНАЯ/P16): в семи комнатах (l1i01 l4i01 l7i01 Kwik-E-Mart, l2i04 l5i04 Moe's, l3i06 l6i06 Обсерватория) лежит
# общий контроллер кадров MasterController, который гоняет в цикле анимацию текстур шейдера: экран автомата (int_kwik_game.bmp.1..4), помехи телевизора
# (int_moe_static.bmp.1..4), огни обсерватории (int_obv_anilights.bmp.1..5). Данные диска: чанк 0x121000 «TEX_<шейдер>» (число кадров, кадров в секунду)
# и канал 0x121107 (ключевые кадры u16 и имена текстур); между ключами кадр не меняется. Здесь они уходят в хвост блока RMG1: блок ANI1 и подвал RAN1
# (последние 16 байт блоба: 'RAN1', смещение блока, число записей, 0). Запись: u32 текстура-основа (номер в блобе), u32 число ключей, f32 длина (кадров), f32 кадров/с,
# затем ключи (u16 кадр, u16 номер текстуры в блобе). ROOM_ANIM_OLD=1 — без хвоста (как раньше)
ROOM_ANIM_OLD = os.environ.get('ROOM_ANIM_OLD') == '1'
def room_anims(path, meta, conv, PE, cache, blob):
    """-> [(base_tex_id, length, fps, [(frame, tex_id)])] в номерах текстур блоба до отбора; пусто, если анимации текстур нет"""
    from p3d import read, walk, string
    out = []
    try: nodes = list(walk(read(path)))
    except Exception: return out
    texmap = getattr(conv, 'textures', {}) or {}
    def tex_of(name, blend):
        url = texmap.get(name)
        return PE.load_tex(url, cache, blob, blend=blend, sky=False, flat=False) if url else None
    for c in nodes:
        if c.id != 0x121000: continue
        try:
            nm, p = string(c.data, 4)
            if not nm.startswith('TEX_') or c.data[p:p + 3] != b'TEX': continue
            ln, fps = struct.unpack_from('<2f', c.data, p + 4)
            sh = nm[4:]; mat = meta['materials'].get(sh)
            if not mat: continue
            fl = (1 if mat.get('alpha') else 0) | (2 if mat.get('translucent') or mat.get('blend') else 0); blend = bool(fl & 2)
            keys = None
            stack = list(getattr(c, 'children', []))
            while stack:
                k = stack.pop()
                if k.id == 0x121107:
                    d = k.data; n = struct.unpack_from('<I', d, 8)[0]; fr = struct.unpack_from(f'<{n}H', d, 12); q = 12 + 2 * n; names = []
                    for _ in range(n):
                        L = d[q]; names.append(d[q + 1:q + 1 + L].split(b'\0')[0].decode('latin1')); q += 1 + L
                    keys = list(zip(fr, names)); break
                stack += list(getattr(k, 'children', []))
            if not keys or ln <= 0 or fps <= 0: continue
            ids = [(f, tex_of(n_, blend)) for f, n_ in keys]
            base = tex_of(mat.get('texture', ''), blend)
            if base is None or any(t is None for _, t in ids): continue
            out.append((base, float(ln), float(fps), ids))
        except Exception as e:
            print(f'  анимация текстур комнаты пропущена ({e})', flush=True)
    return out

def room_gfx(PE, path, stem, TR=None, BX=None):
    """рисование одной комнаты -> байты блоба RMG1 и сводка. TR, BX — коллизия комнаты (земля и ящики мебели): по ним
    глаза Гомера для обхода «лицом в комнату» (tools/roomorient.py, правка Роба 27)"""
    import numpy as np, texq, uvq
    from p3d import read
    conv = PE.Converter()
    tn = f'psp_room_{stem}_{os.getpid()}'   # номер процесса: параллельные экспорты не делят файл
    conv.export(path, tn)
    meta = json.loads((PE.OUT / f'{tn}.json').read_text()); raw = (PE.OUT / f'{tn}.bin').read_bytes()
    for ext in ('json', 'bin'): (PE.OUT / f'{tn}.{ext}').unlink()
    for m in meta['materials'].values():   # родной размер текстур комнаты (до 256)
        if m.get('textureUrl'): texq.CAPS[m['textureUrl']] = max(texq.CAPS.get(m['textureUrl'], 0), ROOM_TEX_CAP)
    cache, blob, groups, src_tris = {}, [], {}, 0
    prims = PE.pack_prims(meta, raw, cache, blob); orient = {}
    room_clear = room_top_colour(prims, blob, texq) if os.environ.get('SHR_ROOM_CLEAR_BLACK') != '1' else ROOM_CLEAR   # правка 229
    cams, cinfo = room_cams(path)
    if TR is not None and os.environ.get('SHR_ROOM_ORIENT', '1') != '0':   # =0 — как до правки 27 (двусторонне), сравнение
        import roomorient
        prims, orient = roomorient.orient_prims(prims, TR, BX, (cam_table(cams, 128), len(cams)))   # 128 — как в блобе (правка 301)
    ceil = {}
    if TR is not None and os.environ.get('SHR_ROOM_CEIL', '1') != '0':   # правка 301: достройка потолка
        caps, ceil = room_ceiling(prims, TR); prims = prims + caps
        wcaps, wfill = room_wall_fill(prims, TR, ceil); prims = prims + wcaps; ceil['walls'] = wfill   # №301: стены по границе пола без стены
    for t, fl, P, UV, C in prims:
        src_tris += len(P) // 3
        P, UV, C, anc, (fP, fUV, fC, forig) = PE.subdivide_lod(P, UV, C, ROOM_SPLIT, ROOM_SPLIT_GROUND, far_split=PE.FAR_SPLIT)
        keys = np.floor(fP.reshape(-1, 3, 3).mean(1)[:, [0, 2]] / ROOM_CHUNK).astype(int)
        for key in {tuple(k) for k in keys}:
            fm = (keys == key).all(1); nm = fm[anc]; local = np.cumsum(fm) - 1
            groups.setdefault((key, t, fl), []).append(
                (PE.verts(P[np.repeat(nm, 3)], UV[np.repeat(nm, 3)], C[np.repeat(nm, 3)]), local[anc[nm]],
                 PE.verts(fP[np.repeat(fm, 3)], fUV[np.repeat(fm, 3)], fC[np.repeat(fm, 3)]), np.ones(int(fm.sum()), bool)))
    PE.snap_grid(groups, ROOM_GRID)   # точки на общую решётку комнаты, дальше Т-стыки и слои — по итоговым (как у города)
    if os.environ.get('SHR_TJ', '1') != '0': print(f'  Т-стыки: разрезано {PE.split_tjunctions(groups)} (правка Роба 37)', flush=True)
    if os.environ.get('SHR_LAYERS') == '1': PE.mark_layers(groups)   # наклейки в плоскости стен/пола -> флаги 64/256/512
    # 16.10 (Камера, комната Барта L4: «розовая панель» = окно int_br_window_m, прозрачное стекло): стекло — слой с флагом прозрачности 2, mark_layers дал ему сторону (F_LSIDE 512, отсечение изнанки) и
    # обход комнаты (F_SHELL 128), и из комнаты стекло оказывалось изнанкой, не рисовалось (на месте окна виден фон кадра: 398 из 400 проб луча попали в изнанку куска tex 1, флаги 722, куски 84/85).
    # Прозрачные слои комнаты рисуем с обеих сторон (без стороны и обхода, F_DOUBLE): стекло видно из комнаты. ROOM_GLASS_OLD=1 — прежнее
    if os.environ.get('ROOM_GLASS_OLD') != '1':
        ng, nglass, names = {}, 0, set()
        inv = {v: k for k, v in cache.items() if '#' not in k}; murl = {m.get('textureUrl'): mk for mk, m in meta['materials'].items()}
        for (key, t, fl), v in groups.items():
            if fl & 2 and fl & 512:
                fl = (fl & ~(512 | 128)) | 8; nglass += 1; names.add(murl.get(inv.get(t), f'tex{t}'))
            ng.setdefault((key, t, fl), []).extend(v)
        groups.clear(); groups.update(ng)
        print(f'  прозрачные слои комнаты без стороны ({stem}): {nglass} групп, материалы {sorted(names)} (ROOM_GLASS_OLD=1 — прежнее)', flush=True)
    used = sorted({k[1] for k in groups if k[1] >= 0}); tmap = {t: i for i, t in enumerate(used)}
    anims, extra = ([] if ROOM_ANIM_OLD else room_anims(path, meta, conv, PE, cache, blob)), []   # круг 5 (P16-11)
    anims = [a for a in anims if a[0] in tmap]   # анимируется только то, что комната рисует
    for a in anims:
        for _, t in a[3]:
            if t not in tmap and t not in extra: extra.append(t)
    for i, t in enumerate(extra): tmap[t] = len(used) + i
    QV = np.dtype([('u', '<i2'), ('v', '<i2'), ('c', '<u4'), ('x', '<i2'), ('y', '<i2'), ('z', '<i2'), ('pad', '<i2')])
    pieces, vblob, tris, clo, chi = [], bytearray(), 0, np.full(3, 1e9), np.full(3, -1e9)
    for key in sorted(groups, key=lambda k: (k[2], k[1], k[0])):   # прозрачность, текстура, клетка: меньше смен текстур
        vn = np.concatenate([p[0] for p in groups[key]])
        for ch in range(0, len(vn), PE.PIECE_VERTS // 3 * 3):
            vv = vn[ch:ch + PE.PIECE_VERTS // 3 * 3]
            Pq = np.stack([vv['x'], vv['y'], vv['z']], 1).astype(np.float64)
            lo_, hi_ = Pq.min(0), Pq.max(0)
            clo, chi = np.minimum(clo, lo_), np.maximum(chi, hi_)
            q = np.zeros(len(vv), QV)
            c, xyz, sc = PE.grid_quant(Pq, ROOM_GRID)   # общая решётка комнаты: швы кусков бит в бит (правка Роба 37)
            q['x'], q['y'], q['z'] = xyz[:, 0], xyz[:, 1], xyz[:, 2]
            q['u'], q['v'] = uvq.tri_i16(vv['u'], vv['v'])   # GE читает u16 (tools/uvq.py, правка Роба 43)
            q['c'] = vv['c']
            v12 = bool((q['c'] >> 24 == 0xFF).all())
            fl = key[2] | PE.F_NOIDX | (PE.F_V12 if v12 else 0)
            vblob += b'\0' * (-len(vblob) % 16); off = len(vblob); vblob += PE.ni_bytes(q, v12)
            pieces.append((lo_, hi_, tmap.get(key[1], -1), fl, len(q), len(q), c, sc, off)); tris += len(q) // 3
    amb, dirs = light_group(path)
    texb = bytearray()
    for t in used + extra: texb += blob[t]; texb += b'\0' * (-len(texb) % 16)
    tex_off = 128; rec_off = tex_off + len(texb); v_off = rec_off + 80 * len(pieces); v_off += -v_off % 16
    recs = bytearray()
    for lo_, hi_, t, fl, nv, ni, c, S, off in pieces:
        recs += struct.pack('<6fiIIII3ffIII2I', *lo_, *hi_, t, fl, nv, ni, 0, *c, S, v_off + off, 0, 0, 0, 0)
    head = b'RMG1' + struct.pack('<7I', len(used) + len(extra), len(pieces), tex_off, rec_off, room_clear, amb, len(dirs))
    for (x, y, z), col in dirs + [((0.0, 0.0, 0.0), 0)] * (4 - len(dirs)): head += struct.pack('<3fI', x, y, z, col)
    cam_off = v_off + len(vblob); cam_off += -cam_off % 16
    head += struct.pack('<6f2I', *clo, *chi, cam_off, len(cams)); assert len(head) == 128
    out = bytearray(head) + texb + recs; out += b'\0' * (v_off - len(out)); out += vblob; out += b'\0' * (cam_off - len(out))
    out += cam_table(cams, cam_off); out += b'\0' * (-len(out) % 16)
    if anims:   # круг 5 (P16-11): хвост ANI1 и подвал RAN1
        ani_off = len(out); ab = bytearray(b'ANI1' + struct.pack('<I', len(anims)))
        for base, ln, fps, ks in anims:
            ab += struct.pack('<2I2f', tmap[base], len(ks), ln, fps)
            for f_, t in ks: ab += struct.pack('<2H', f_, tmap[t])
        ab += b'\0' * (-len(ab) % 16); out += ab; out += b'RAN1' + struct.pack('<3I', ani_off, len(anims), 0)
    info = dict(cams=cinfo, src_tris=src_tris, tris=tris, pieces=len(pieces), textures=len(used) + len(extra), anims=len(anims), tex_bytes=len(texb), bytes=len(out), orient=orient, ceil=ceil,
                ambient=f'{amb:08x}', lights=[[round(v, 3) for v in d] + [f'{c:08x}'] for d, c in dirs])
    return bytes(out), info


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--level', type=int, default=1); ap.add_argument('--no-gfx', action='store_true', help='только SHI1 (коллизия)')
    ap.add_argument('--outdir', default=str(ROOT / 'build'), help='куда писать оба пакета (tools/ship.sh: {outdir})')
    a = ap.parse_args()
    for k in ('SHR_TEX_CAP256', 'SHR_SMALL_MIPS', 'SHR_LAYERS'): os.environ.setdefault(k, '1')   # закон 1 — до импорта psp_export
    os.environ.setdefault('SHR_TMP', str(ROOT / 'build/tmp_interiors'))   # временные json/bin конвертера — в свой build/
    import psp_export as PE
    from p3d import read
    art = GAME / 'art'; lv = a.level
    rooms, blobs, gfx = [], [], []
    for f in sorted(art.glob(f'l{lv}i*.p3d')):
        m = re.fullmatch(rf'l{lv}i(\d+)', f.stem)
        if not m: continue
        name = NAMES.get(int(m.group(1)), f.stem); L = locators(f); T = triggers(f)
        start = next((l for l in L if l['name'] == 'InteriorEntryEnd'), None); ex = next((l for l in L if l['name'].endswith('_exit')), None)
        st0 = next((l for l in L if l['name'] == 'InteriorEntryStart'), None)
        if start and st0: start = dict(start, heading=heading(start['pos'][0] - st0['pos'][0], start['pos'][2] - st0['pos'][2]))
        TR, BX, CY, SG, rep = PE.col_collect([(f.stem, read(f))])
        col, st = PE.col_section(TR, BX, CY, SG)
        rooms.append(dict(name=name, file=f.name, start=start, exit=ex, exitTrig=T.get(ex['name']) if ex else None,
                          tris=len(TR), boxes=len(BX), cyls=len(CY), colBytes=len(col)))
        blobs.append(col)
        if not a.no_gfx: gfx.append((name, *room_gfx(PE, f, f.stem, TR, BX)))
    doors = []
    for f in [art / f'l{lv}_terra.p3d', *sorted(art.glob(f'l{lv}[rz]*.p3d')), *sorted((art / f'missions/level0{lv}').glob('*.p3d'))]:
        T = triggers(f)
        for l in locators(f):
            if l['kind'] == 7 and l.get('interior'):
                # Disc doors use moe (L2/L5), Android (L3/L6); SHI1 stores canonical room names.
                name = l['interior'].lower()
                if not os.getenv('ROOM_INTERIOR_OLD'):
                    name = {'moe': 'moes', 'moe1': 'moes', 'android': 'androidsdungeon'}.get(name, name)
                idx = next((i for i, r in enumerate(rooms) if r['name'].lower() == name), -1)
                zx = l.get('zaxis')
                doors.append(dict(name=l['interior'], locator=l['name'], pos=l['pos'], room=idx, file=f.name,
                                  heading=heading(-zx[0], -zx[2]) if zx else None, trig=T.get(l['name'])))   # наружу = против оси z двери
    out = Path(a.outdir) / f'interiors-l{lv}.bin'; out.parent.mkdir(parents=True, exist_ok=True)
    head = b'SHI1' + struct.pack('<3I', len(rooms), len(doors), SHI_VERSION)
    body_off = len(head) + 128 * len(rooms) + 128 * len(doors); recs = b''; off = body_off
    for r, b in zip(rooms, blobs):
        s = r['start'] or dict(pos=[0, 0, 0], heading=0.0); e = r['exit'] or dict(pos=s['pos'])
        recs += struct.pack('<32s4f3fI', r['name'].encode()[:31], *s['pos'], s.get('heading', 0.0), *e['pos'], off) + trig_bytes(r['exitTrig']); off += len(b)
    for d in doors:
        recs += struct.pack('<32s3fif12x', d['name'].encode()[:31], *d['pos'], d['room'], d['heading'] if d['heading'] is not None else float('nan')) + trig_bytes(d['trig'])
    out.write_bytes(head + recs + b''.join(blobs))
    out.with_suffix('.json').write_text(json.dumps(dict(rooms=rooms, doors=doors, gfx=[dict(name=n, **i) for n, _, i in gfx]), ensure_ascii=False, indent=1))
    for r in rooms: print(f"{r['name']:22s} {r['file']}: треуг. {r['tris']}, ящиков {r['boxes']}, цилиндров {r['cyls']}; старт {r['start'] and [round(v, 1) for v in r['start']['pos']]} курс {r['start'] and round(r['start'].get('heading', 0.0), 3)}, выход {r['exit'] and [round(v, 1) for v in r['exit']['pos']]}")
    for r in rooms: print(f"  выход {r['name']:20s} триггер {tstr(r['exitTrig'])}")
    for d in doors: print(f"дверь {d['name']:22s} {d['locator']} у {[round(v, 1) for v in d['pos']]} курс наружу {d['heading'] if d['heading'] is None else round(d['heading'], 3)}, триггер {tstr(d['trig'])} ({d['file']})")
    print('->', out, out.stat().st_size, 'Б')
    if gfx:
        gout = Path(a.outdir) / f'interiors-l{lv}-gfx.bin'
        g = bytearray(b'SHG1' + struct.pack('<3I', len(gfx), 1, 0)); tab_end = len(g) + 48 * len(gfx); tab_end += -tab_end % 16
        off = tab_end
        for name, b, _ in gfx: g += struct.pack('<32sII8x', name.encode()[:31], off, len(b)); off += len(b)
        g += b'\0' * (tab_end - len(g))
        for _, b, _ in gfx: g += b
        gout.write_bytes(g)
        for name, _, i in gfx:
            print(f"рисование {name:22s}: треуг. {i['src_tris']} -> {i['tris']} (нарезка {ROOM_SPLIT:g}/{ROOM_SPLIT_GROUND:g} м), кусков {i['pieces']}, "
                  f"текстур {i['textures']} ({i['tex_bytes'] / 1024:.0f} КБ), блоб {i['bytes'] / 1024:.0f} КБ; свет: фон {i['ambient']}, направленных {len(i['lights'])}, камер {len(i['cams'])}")
            for l in i['cams']: print('    камера', l)
            if i.get('orient'): print('    обход лицом в комнату (tools/roomorient.py):', i['orient'])
            if i.get('ceil'): print('    достройка потолка (правка 301):', i['ceil'])
        print('качество: SHR_TEX_CAP256=%s SHR_SMALL_MIPS=%s SHR_LAYERS=%s, текстуры комнат до %d px' %
              (os.environ['SHR_TEX_CAP256'], os.environ['SHR_SMALL_MIPS'], os.environ['SHR_LAYERS'], ROOM_TEX_CAP))
        print('->', gout, gout.stat().st_size, 'Б')
    return 0


if __name__ == '__main__':
    sys.exit(main())
