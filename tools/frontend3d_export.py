#!/usr/bin/env python3
"""Гостиная главного меню оригинала (Scrooby 3dFE.pag) -> build/fe.shr, формат FE1 (viewer/frontend3d.c).

Источник — диск Роба (только чтение), страница 3dFE.pag из frontend.p3d и её объекты pure3d/*.p3d:
  camset.p3d   FE_LivingRoom (сцена), FE_Camera (камера), FE_CameraController (1036 кадров, 30 кадр/с)
  homer.p3d    FE_Homer: Гомер меню (скин, скелет 41 сустав, цикл 61 кадр) -> вложенный CHR1 (viewer/skin.h)
  TVscreen.p3d экран телевизора: помехи (текстурная анимация 11 кадров)
  racecar.p3d  машинка на треке (цикл 132 кадра)
  glowvcr/glowbook/glowsofa/glowmini/glowhomr/glowtv.p3d — подсветки Glow0..5 (цикл 23 кадра); номер = номер
               строки пункта в MainMenu.pag: 0 LOAD GAME, 1 SCRAP BOOK, 2 OPTIONS, 3 MINI GAME, 4 NEW GAME, 5 RESUME
Опора: hit-and-run-web/tools/p3d.py (MIT), tools/char_export.py (скелет, клипы, CHR1), tools/texq.py (текстуры как
у уровня: T8, точная палитра/k-means, мипы до 1 px, swizzle), donut (GPL: режимы смешивания 1 альфа / 2 сложение).

  python tools/frontend3d_export.py                 # -> build/fe.shr (+ сводка чисел)
  python tools/frontend3d_export.py --out X.shr
  python tools/frontend3d_export.py --check X.shr   # гейт: числа файла = данным p3d, повторная сборка побайтно та же

Кадр меню: камера оригинала стоит в «главном виде» (поза кадра 0) на нескольких удержаниях таймлайна; берём первый кадр
самого длинного удержания, где камера = кадр 0 (380..459) — там дверца VCR закрыта, альбом закрыт, диафрагма и гоночная
f1 камсета скрыты (видимость из данных). Перелёты камеры к предметам (правка Роба 28.09): отрезки таймлайна, которые
начинаются в главном виде; пункт отрезка — из данных: в центре кадра прилёта (луч в треугольники комнаты) предмет, который
из главного вида закрыт подсветкой Glow этого пункта (подсветки — плоскости перед предметами, совпадают с ними только из
главного вида). Выходит: 4 NEW GAME 1→40 (газета), 2 OPTIONS 80→110→140 (диван), 5 RESUME 140→170→200 (ТВ), 0 LOAD
200→230→260 (VCR, дверца открывается 225–235), 1 SCRAP 320→350→380 (альбом), 3 MINI 880→910 (трек, обратного нет).
Треугольники режутся у ближней плоскости и во всех кадрах перелётов (через FLIGHT_STEP), не только в кадре меню.
Порядок отрисовки — страница 3dFE.pag: TVScreen, CamAndSet, RaceCar, Glow5, Glow2, Homer, Glow4, Glow3, Glow1, Glow0 —
смешиваемое комнаты и подсветки ТВ/дивана рисуются до Гомера (флаг 32), остальные подсветки после.
Скин: MatrixList — u32 на вершину, индекс палитры для веса 0 в старшем байте (donut SkinModel.cpp).

Координаты как у level.shr / hit-and-run-web: z игры отзеркален (F = diag(1,1,-1)). Освещение LIT-материалов
(ключевой + заполняющий направленные, фоновый) запекается в цвет вершин так же, как считает фиксированный конвейер
(амбиент*AMBI + свет*DIFF*max(0,N·L)); Гомер освещается на GE теми же светами.

ФОРМАТ FE1 (little-endian; смещения от начала файла; секции выровнены на 16)
  заголовок 128 Б:
    0 'FE1\\0'  4 version=1  8 size
   12 nTex  16 offTex     FeTex 64 Б: u16 w,h; u8 nlev,pad[3]; u32 offClut (256 x ABGR8888); u32 offLev[8] (T8, swizzle,
                          высота добита до кратной 8, строка >= 16 Б); u16 bufw[8]; u32 pad
   20 nPiece 24 offPiece  FePiece 64 Б: u16 tex (0xffff нет), texTrack (0xffff нет), flags, node (матричная дорожка,
                          0xffff = мир), vis (дорожка видимости, 0xffff нет), pad; u32 nverts, offVerts; f32 origin[3],
                          scale; f32 bbox[6] (в осях узла); u32 srcTris (исходные треугольники до нарезки у камеры)
                          flags: 1 альфа-тест, 2 смешивание альфой, 4 сложение, 8 без записи глубины, 16 не виден из
                          камеры меню (закрыт непрозрачным или вне кадра; видимость одной точки глаза, как PVS города),
                          32 объект страницы 3dFE.pag до Гомера (смешиваемое рисуется до него; у билбордов тоже)
                          вершины 16 Б: s16 u,v (uv*256, TexScale 128); u32 ABGR; s16 x,y,z (/32768*scale+origin); pad
   28 nTrack 32 offTrack  FeTrack 32 Б: u32 type (1 матрица 12 f32/кадр: 3 строки x (R|t), 2 видимость u8/кадр,
                          3 текстура u16/кадр, 4 подсветка 6 f32/кадр: w,h,r,g,b,a); u32 nframes; f32 fps;
                          u32 flags (1 цикл, 2 часы таймлайна камеры, иначе свои часы); u32 off; pad[3]
   36 nBill  40 offBill   FeBill 80 Б: u16 track (0xffff статичная), tex; u16 flags (как у куска); u8 mode (0 NOAX
                          плоскость узла, 1 YAX вокруг оси Y узла, 2 к камере), glow (0..5 пункт меню, 0xff всегда);
                          f32 w,h; u32 ABGR; f32 m[12] (узел: 3 строки R|t); pad
   44 nFrames 48 offCam   камера по кадрам таймлайна: 12 f32 (pos3, look3, up3, fovX при 4:3, near, far)
   52 menuFrame 56 fps(f32) 60 offHomer 64 sizeHomer (CHR1, viewer/skin.h; треугольники GU_TRIANGLES)
   68 f32 homerPlace[12] (3 строки R|t)  116 offLights: key dir3 (к свету) + ABGR, fill dir3 + ABGR, ambient ABGR,
      f32 scale (множитель света, запечён и в вершины)  120 srcTris всего  124 offTexNames: имена текстур, 32 Б на текстуру
      (для гейта; движок не читает)
  хвост (последние 16 Б, если есть): 'FLT1', u32 offFlights, u32 n, u32 0; перелёт 16 Б: u8 пункт (номер Glow),
      u8 flags (1 есть обратный), u16 0, u16 go0, go1 (туда: кадры таймлайна), back0, back1 (обратно), f32 угол цели
Отрицательные контроли гейта (переменные среды): FE_FLOOR_HID_OLD=1 (правка 293: пол в отсечении меню), FE_SKIN_BYTES_OLD=1, FE_GLOW_ORDER_OLD=1, FE_FLIGHT_SHIFT=N, FE_NO_FLIGHTS=1,
  FE_NEAR_OLD=1 (near перелётов как в данных, правка 133; гейт tools/fe_depth_check.py)
"""
import argparse, io, math, os, struct, sys
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools')); sys.path.insert(0, str(ROOT / 'tools'))
from p3d import read, walk, string        # noqa: E402  (MIT)
import char_export as ce                  # noqa: E402
import texq                               # noqa: E402
import uvq                                # noqa: E402  16-битные UV без знака (правка Роба 43)

PU = ROOT / 'hit-and-run-web/source/game/art/frontend/scrooby/resource/pure3d'
OUT = ROOT / 'build/fe.shr'
F4 = np.diag([1., 1., -1., 1.])
GLOWS = ['glowvcr', 'glowbook', 'glowsofa', 'glowmini', 'glowhomr', 'glowtv']   # Glow0..5 (3dFE.pag) = пункты 0..5
FILES = ['camset', 'homer', 'TVscreen', 'racecar'] + GLOWS
LSCALE = float(os.environ.get('FE_LSCALE', '1'))   # множитель света (1 = как в данных)
SKIN_BYTES_OLD = os.environ.get('FE_SKIN_BYTES_OLD') == '1'   # отрицательный контроль гейта: старый порядок байтов MatrixList
NEAR_SPLIT = 0.25          # треугольник, пересекающий плоскость z_cam = NEAR_SPLIT в кадре меню, режем (GE не режет по ближней)
MAX_PX = 1200.             # ...и если в кадре меню он длиннее 1200 px (защитная полоса GE)
CELL = 2.0                 # куски: одна текстура/режим, клетка 2 м (отсечение пирамидой)
T_MAT, T_VIS, T_TEX, T_BILL = 1, 2, 3, 4
TR_LOOP, TR_TIMELINE = 1, 2
F_ATEST, F_BLEND, F_ADD, F_NOZW, F_MENUHID = 1, 2, 4, 8, 16   # 16: не виден из камеры меню (движок пропускает в кадре меню)
F_PREHOMER = 32            # объект страницы 3dFE.pag стоит ДО Гомера (порядок отрисовки Scrooby): смешиваемое рисуется до него
GLOW_ORDER_OLD = os.environ.get('FE_GLOW_ORDER_OLD') == '1'   # отрицательный контроль: без F_PREHOMER (всё смешиваемое после Гомера)
NO_FLIGHTS = os.environ.get('FE_NO_FLIGHTS') == '1'           # отрицательный контроль: файл без таблицы перелётов
NEAR_OLD = os.environ.get('FE_NEAR_OLD') == '1'               # отрицательный контроль (правка 133): near перелётов из данных
FLIGHT_SHIFT = int(os.environ.get('FE_FLIGHT_SHIFT', '0'))    # отрицательный контроль: перелёты сдвинуты на N пунктов
# правка Роба 293 (04.10, «поменяй построение полигонов пола, чтобы при наезде пол наконец не ломался»): пол гостиной — один слой
# без наложений и без кусков-двойников «не виден из кадра меню» (F_MENUHID): его треугольники в отсечение одной точки глаза не
# идут, движок рисует весь пол и в кадре меню, и в перелёте — в первом кадре движения куски пола больше не включаются (тур TEST 9:
# floor 4 -> 6); весь пол — ОДИН кусок (одно начало и масштаб 16-битных вершин: у общих рёбер разных кусков округление давало
# волосяные наложения, у прежнего файла 0,07 мм). Нарезка у камеры (ближняя плоскость, длина на экране) — как у всех.
# Гейт tests/fe_guard/floor_layer.py
FLOOR_TEX = ('floor3.bmp',)                                  # текстура пола гостиной camset (FE_LivingRoom)
FLOOR_HID_OLD = os.environ.get('FE_FLOOR_HID_OLD') == '1'    # отрицательный контроль: пол режется отсечением меню, как раньше
FLIGHT_MAX_DEG = 5.        # цель перелёта = предмет под подсветкой пункта, если из главного вида они в пределах 5°
FLIGHT_STEP = 2            # кадры перелётов, у которых режем треугольники (как у кадра меню), — через каждые 2


# ---------------------------------------------------------------- разбор p3d
def sname(c): return string(c.data)[0]


def shaders_of(root):
    out = {}
    for c in root.children:
        if c.id != 0x11000: continue
        name, p = string(c.data); props = {}
        for s in c.children:
            key = s.data[:4].rstrip(b'\0').decode('ascii')
            if s.id == 0x11002: props[key] = string(s.data, 4)[0].lower()
            elif s.id == 0x11003: props[key] = struct.unpack_from('<I', s.data, 4)[0]
            elif s.id == 0x11004: props[key] = struct.unpack_from('<f', s.data, 4)[0]
            elif s.id == 0x11005: props[key] = struct.unpack_from('<I', s.data, 4)[0]
        out[name] = props
    return out


def argb(u):                                          # Pure3D u32 цвет ARGB -> r,g,b,a 0..1
    return np.array([(u >> 16) & 255, (u >> 8) & 255, u & 255, (u >> 24) & 255], np.float64) / 255.


def prim_groups(mesh):
    """0x10000/0x10001 -> [(shader, P, N|None, UV, C rgba|None, tris, skin|None)] в осях меша (игра, не отзеркалено)"""
    out = []
    for pg in mesh.children:
        if pg.id != 0x10002: continue
        shader, p = string(pg.data, 4)
        prim, = struct.unpack_from('<I', pg.data, p)
        d = {c.id: c.data for c in pg.children}
        assert 0x10012 not in d, 'PS2-native в меню не ожидается'
        n, = struct.unpack_from('<I', d[0x10005]); P = np.frombuffer(d[0x10005], '<f4', n * 3, 4).reshape(-1, 3).astype(np.float64)
        UV = np.frombuffer(d[0x10007], '<f4', n * 2, 8).reshape(-1, 2).astype(np.float64) if 0x10007 in d else np.zeros((n, 2))
        N = np.frombuffer(d[0x10006], '<f4', n * 3, 4).reshape(-1, 3).astype(np.float64) if 0x10006 in d else None
        C = None
        if 0x10008 in d:
            bgra = np.frombuffer(d[0x10008], np.uint8, n * 4, 4).reshape(-1, 4)
            C = bgra[:, [2, 1, 0, 3]].astype(np.float64) / 255.
        idx = np.frombuffer(d[0x1000a], '<u4', offset=4).astype(np.int64) if 0x1000a in d else np.arange(n)
        if prim == 1:
            T = np.stack([idx[:-2], idx[1:-1], idx[2:]], 1); T[1::2] = T[1::2][:, [1, 0, 2]]
        elif prim == 0: T = idx[:len(idx) // 3 * 3].reshape(-1, 3)
        else: raise ValueError(f'{shader}: primitive {prim}')
        a = np.linalg.norm(np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]]), axis=1)
        T = T[a > 1e-8]
        skin = None
        if 0x1000b in d:
            pal = np.frombuffer(d[0x1000d], '<u4', offset=4).astype(np.int64)
            mi = np.frombuffer(d[0x1000b], np.uint8, n * 4, 4).reshape(-1, 4).astype(np.int64)
            # MatrixList: u32 на вершину, индекс палитры для веса 0 — СТАРШИЙ байт (donut SkinModel.cpp: i0 = m >> 24,
            # i1 = m >> 16, i2 = m >> 8). Данные подтверждают: у вершины с весами (1, 0, 0) байты (0, 0, 0, k).
            # Прежде брали байты в порядке памяти (младший = вес 0) — кости перепутаны, Гомер меню «по кускам»
            if not SKIN_BYTES_OLD: mi = mi[:, ::-1]
            w3 = np.frombuffer(d[0x1000c], '<f4', n * 3, 4).reshape(-1, 3).astype(np.float64) if 0x1000c in d else np.c_[np.ones(n), np.zeros((n, 2))]
            W = np.c_[w3, np.maximum(0., 1. - w3.sum(1))]
            skin = (pal[np.minimum(mi, len(pal) - 1)], W)
        out.append((shader, P, N, UV, C, T, skin))
    return out


def ntris(mesh): return sum(len(g[5]) for g in prim_groups(mesh))


def chan(c):
    """канал анимации -> (param, frames[int], values[n, k]); кватернионы (w,x,y,z) игры"""
    b = c.data; key = b[4:8].decode('latin1').strip('\0')
    if c.id in (0x121102, 0x121103):
        axis, = struct.unpack_from('<H', b, 8); const = np.frombuffer(b, '<f4', 3, 10).astype(np.float64)
        n, = struct.unpack_from('<I', b, 22); fr = np.frombuffer(b, '<u2', n, 26).astype(int)
        dof = 1 if c.id == 0x121102 else 2
        vals = np.frombuffer(b, '<f4', n * dof, 26 + 2 * n).reshape(n, dof)
        v = np.tile(const, (n, 1)); v[:, [axis] if dof == 1 else [a for a in range(3) if a != axis]] = vals
        return key, fr, v
    if c.id == 0x121108:                                   # видимость: начальное состояние + кадры переключений
        st, = struct.unpack_from('<H', b, 8); n, = struct.unpack_from('<I', b, 10)
        return key, np.frombuffer(b, '<u2', n, 14).astype(int), st
    if c.id == 0x121107:                                   # сущность (текстура): имена по кадрам
        n, = struct.unpack_from('<I', b, 8); fr = np.frombuffer(b, '<u2', n, 12).astype(int); q = 12 + 2 * n; names = []
        for _ in range(n): s, q = string(b, q); names.append(s.lower())
        return key, fr, names
    n, = struct.unpack_from('<I', b, 8); fr = np.frombuffer(b, '<u2', n, 12).astype(int); o = 12 + 2 * n
    if c.id == 0x121111: return key, fr, np.frombuffer(b, '<i2', n * 4, o).reshape(n, 4) / 32767.
    if c.id == 0x121105: return key, fr, np.frombuffer(b, '<f4', n * 4, o).reshape(n, 4).astype(np.float64)
    if c.id == 0x121109: return key, fr, np.array([argb(int(u)) for u in np.frombuffer(b, '<u4', n, o)])
    k = {0x121104: 3, 0x121100: 1, 0x121101: 2}[c.id]
    return key, fr, np.frombuffer(b, '<f4', n * k, o).reshape(n, k).astype(np.float64)


def anims(root):
    """-> {имя анимации: (тип, кадров, fps, {группа: {канал: (fr, v)}})}"""
    out = {}
    for an in root.children:
        if an.id != 0x121000: continue
        name, p = string(an.data, 4); typ = an.data[p:p + 4].rstrip(b'\0').decode('latin1')
        frames, fps, loop = struct.unpack_from('<ffI', an.data, p + 4)
        groups = {}
        for g in walk(an):
            if g.id != 0x121001: continue
            gn, _ = string(g.data, 4)
            groups[gn] = {k: (f, v) for k, f, v in (chan(c) for c in g.children)}
        out[name] = (typ, int(round(frames)), fps, groups)
    return out


def sample(fr, v, f):
    """линейно между ключами; до первого/после последнего — крайнее значение"""
    if len(fr) == 1 or f <= fr[0]: return np.array(v[0], np.float64)
    if f >= fr[-1]: return np.array(v[-1], np.float64)
    i = int(np.searchsorted(fr, f, 'right')) - 1; u = (f - fr[i]) / (fr[i + 1] - fr[i])
    return v[i] * (1 - u) + v[i + 1] * u


def qsample(fr, v, f):
    if len(fr) == 1 or f <= fr[0]: q = v[0]
    elif f >= fr[-1]: q = v[-1]
    else:
        i = int(np.searchsorted(fr, f, 'right')) - 1; u = (f - fr[i]) / (fr[i + 1] - fr[i])
        a, b = v[i], v[i + 1]
        if np.dot(a, b) < 0: b = -b
        q = a * (1 - u) + b * u
    return q / np.linalg.norm(q)


def vis_at(fr, st, f):
    return (st + int(np.searchsorted(fr, f, 'right'))) & 1


def row_from_qt(qwxyz, t):
    """Pure3D (строки, p' = p·M): кватернион (w,x,y,z) + перенос -> 4x4 в осях игры"""
    w, x, y, z = qwxyz
    R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w)],
                  [2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w)],
                  [2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)]])
    M = np.eye(4); M[:3, :3] = R; M[3, :3] = t; return M


def col(M):
    """строчная 4x4 игры -> столбцовая 4x4 в отзеркаленных осях (как level.shr, char_export)"""
    return (F4 @ M @ F4).T


def mat_at(ch, f, rest):
    """локальная матрица узла на кадре f из каналов TRAN/ROT (чего нет — из покоя)"""
    t = sample(*ch['TRAN'], f) if 'TRAN' in ch else rest[3, :3]
    q = qsample(*ch['ROT'], f) if 'ROT' in ch else None
    if q is None: M = rest.copy(); M[3, :3] = t; return M
    return row_from_qt(q, t)


# ---------------------------------------------------------------- сцена
class Scene:
    def __init__(self):
        self.tex = {}          # имя -> PIL RGBA
        self.tex_order = []
        self.shaders = {}
        self.tris = []         # dict(P, UV, C, tex, flags, node, vis, texTrack, src)
        self.tracks = []       # (type, nframes, fps, flags, bytes)
        self.bills = []
        self.stats = {}
        self.skipped = []      # (что, треугольников, почему)
        self.pre = 0           # F_PREHOMER для объектов текущего файла (порядок 3dFE.pag)
        self.flights = []      # перелёты камеры к предметам пунктов (flights())

    def add_track(self, typ, nframes, fps, flags, data):
        self.tracks.append((typ, nframes, fps, flags, data)); return len(self.tracks) - 1


def load_textures(sc, root):
    for c in root.children:
        if c.id != 0x19000: continue
        name = sname(c).lower()
        img = next(v for v in c.children if v.id == 0x19001)
        raw = next(v for v in img.children if v.id == 0x19002).data[4:]
        assert raw[:4] == b'\x89PNG', name
        if name in sc.tex: continue
        sc.tex[name] = Image.open(io.BytesIO(raw)).convert('RGBA'); sc.tex_order.append(name)


def material(sc, shader):
    s = sc.shaders.get(shader, {})
    fl = (F_ATEST if s.get('ATST') else 0) | (F_BLEND if s.get('BLMD') == 1 else 0) | (F_ADD if s.get('BLMD') == 2 else 0)
    if (fl & (F_BLEND | F_ADD)) and not (fl & F_ATEST): fl |= F_NOZW
    return s, s.get('TEX', ''), fl


def light_bake(P, N, s, lights):
    """цвет вершины LIT-материала: амбиент*AMBI + сумма свет*DIFF*max(0, N·L) (+EMIS), в осях мира (отзеркаленных)"""
    amb = argb(s.get('AMBI', 0xffffffff))[:3]; dif = argb(s.get('DIFF', 0xffffffff))[:3]; emi = argb(s.get('EMIS', 0xff000000))[:3]
    c = np.tile(emi + lights['ambient'] * amb * LSCALE, (len(P), 1))
    for L, colr in lights['dir']:
        c += np.maximum(0., N @ L)[:, None] * (colr * dif * LSCALE)[None]
    return np.clip(c, 0., 1.)


def emit(sc, shader, P, N, UV, C, T, Mw, node, vis, lights, tex_track=0xffff):
    """треугольники прим-группы в мир (Mw — строчная 4x4 игры; для узлов с дорожкой — в осях узла)"""
    s, tname, fl = material(sc, shader)
    fl |= sc.pre
    Pm = (np.c_[P, np.ones(len(P))] @ Mw)[:, :3]; Pm[:, 2] *= -1          # мир, отзеркалено
    uv = UV.copy(); uv[:, 1] = 1. - uv[:, 1]                               # Pure3D: v вверх; GE: v=0 — первая строка PNG
    if s.get('LIT') and N is not None:
        Nm = N @ Mw[:3, :3]; Nm[:, 2] *= -1
        Nm /= np.maximum(np.linalg.norm(Nm, axis=1, keepdims=True), 1e-9)
        rgb = light_bake(Pm, Nm, s, lights); a = np.ones(len(P))
    elif C is not None: rgb, a = C[:, :3], C[:, 3]
    else: rgb, a = np.ones((len(P), 3)), np.ones(len(P))
    if not (fl & (F_BLEND | F_ADD)): a = np.ones(len(P))               # альфа вершин — только у смешиваемых
    col_ = np.c_[rgb, a]
    T2 = T[:, [0, 2, 1]]                                                    # зеркало z меняет обход
    sc.tris.append(dict(P=Pm[T2], UV=uv[T2], C=col_[T2], tex=tname, flags=fl, node=node, vis=vis, texTrack=tex_track,
                        shader=shader))
    return len(T)


def flatten(sg, cb):
    """обход дерева сцены: cb(kind, chunk, мировая строчная матрица, путь имён)"""
    def rec(c, M, path):
        if c.id == 0x120103:
            name, p = string(c.data); L = np.frombuffer(c.data, '<f4', 16, p + 4).reshape(4, 4).astype(np.float64)
            M = L @ M; path = path + (('T', name, L),)
        elif c.id == 0x120104:
            name, p = string(c.data); path = path + (('V', name, None),)
        elif c.id == 0x120107:
            cb(c, M, path)
        for s in c.children: rec(s, M, path)
    rec(sg, np.eye(4), ())


def scene_file(sc, fn, root, lights, timeline, loop_clock):
    """сцена одного файла: меши, композиты (пропы на суставах, скины в позе), билборды; анимированные узлы -> дорожки"""
    A = anims(root)
    meshes = {sname(c): c for c in root.children if c.id == 0x10000}
    skins = {sname(c): c for c in root.children if c.id == 0x10001}
    comps = {sname(c): c for c in root.children if c.id == 0x4512}
    skels = {sname(c): c for c in root.children if c.id == 0x4500}
    bqgs = {string(c.data, 4)[0]: c for c in root.children if c.id == 0x17002}
    strn = {g: (nm, a) for nm, a in A.items() if a[0] == 'STRN' for g in a[3]}
    svis = {g: (nm, a) for nm, a in A.items() if a[0] == 'SVIS' for g in a[3]}
    used = set()
    tl_frames, menu = timeline
    node_tracks = {}

    def node_track(path):
        """ближайший анимированный предок -> (индекс матричной дорожки, строчная матрица предка-родителя) или None"""
        for i in range(len(path) - 1, -1, -1):
            k, name, L = path[i]
            if k == 'T' and name in strn:
                if name not in node_tracks:
                    an, (typ, nfr, fps, groups) = strn[name]; ch = groups[name]
                    par = np.eye(4)
                    for kk, nn, LL in path[:i]:
                        if kk == 'T': par = LL @ par
                    own = fn != 'camset'                                  # у файла свой цикл (машинка), у камсета — таймлайн
                    nf = nfr if own else tl_frames
                    mats = []
                    for f in range(nf):
                        Mf = mat_at(ch, f, L) @ par
                        mats.append(col(Mf)[:3, :4].astype('<f4').tobytes())
                    # сверка соглашений: ключ покоя = матрица узла в дереве сцены
                    f0 = int(ch['TRAN'][0][0]) if 'TRAN' in ch else 0
                    assert np.allclose(mat_at(ch, f0, L), L, atol=2e-3) or name in ('locator6', 'locator1'), (fn, name)
                    tr = sc.add_track(T_MAT, nf, fps, (TR_LOOP if own else TR_TIMELINE), b''.join(mats))
                    node_tracks[name] = (tr, par, i)
                return node_tracks[name]
        return None

    def vis_track(path):
        for k, name, _ in reversed(path):
            if k == 'V' and name in svis:
                an, (typ, nfr, fps, groups) = svis[name]; fr, st = groups[name]['VIS']
                return name, fr, st
        return None

    def cb(c, M, path):
        name, p = string(c.data); dn, p = string(c.data, p)
        vt = vis_track(path)
        if fn == 'camset' and vt and not vis_at(vt[1], vt[2], menu):
            n = ntris(meshes[dn]) if dn in meshes else 0
            sc.skipped.append((f'{fn}:{dn}', n, f'скрыт на кадре меню {menu} (видимость {vt[0]})')); used.add(dn); return
        nt = node_track(path)
        if nt is not None:
            tr, par, i = nt; Ml = np.eye(4)
            for k, nn, L in path[i + 1:]:
                if k == 'T': Ml = L @ Ml
            Mw, node = Ml, tr                                       # вершины в осях узла, узел ведёт дорожка
        else: Mw, node = M, 0xffff
        if dn in meshes:
            used.add(dn)
            for shader, P, N, UV, C, T, _ in prim_groups(meshes[dn]):
                n = emit(sc, shader, P, N, UV, C, T, Mw, node, 0xffff, lights, tex_track(fn, shader))
                sc.stats[fn] = sc.stats.get(fn, 0) + n
        elif dn in comps: composite(comps[dn], M, dn)
        elif dn in bqgs:
            used.add(dn); billboard(bqgs[dn], M)
        else: sc.skipped.append((f'{fn}:{dn}', 0, 'нет такого рисуемого'))

    def composite(cc, M, dn):
        n, p = string(cc.data); skn, _ = string(cc.data, p)
        sk = skels[skn]; joints = []
        for j in sk.children:
            if j.id != 0x4501: continue
            jn, q = string(j.data); par, = struct.unpack_from('<I', j.data, q)
            L = np.frombuffer(j.data, '<f4', 16, q + 24).reshape(4, 4).astype(np.float64)
            joints.append([jn, par, L])
        pt = next((a for a in A.values() if a[0] == 'PTRN' and joints[0][0] in a[3]), None)
        pvis = {g: ch['VIS'] for a in A.values() if a[0] == 'PVIS' for g, ch in a[3].items()}
        f = menu if fn == 'camset' else 0
        W = []
        for j, (jn, par, L) in enumerate(joints):
            if pt and jn in pt[3]: L = mat_at(pt[3][jn], f, L)
            W.append(L if par >= j else L @ W[par])
        used.add(dn)
        for s in walk(cc):
            if s.id == 0x4516:                                     # проп на суставе
                pn, q = string(s.data); _, jj = struct.unpack_from('<II', s.data, q)
                if pn in meshes:
                    used.add(pn)
                    for shader, P, N, UV, C, T, _ in prim_groups(meshes[pn]):
                        k = emit(sc, shader, P, N, UV, C, T, W[jj] @ M, 0xffff, 0xffff, lights, tex_track(fn, shader))
                        sc.stats[fn] = sc.stats.get(fn, 0) + k
            elif s.id == 0x4515:                                   # скин: поза кадра меню (CPU), если виден
                sn, _ = string(s.data)
                used.add(sn); sk_ = skins[sn]; nsk = ntris(sk_)
                shape_vis = 1
                if fn == 'camset' and sn in pvis: shape_vis = vis_at(pvis[sn][0], pvis[sn][1], f)
                hasexp = any(pg.id == 0x10018 for pg in sk_.children)
                if not shape_vis:
                    sc.skipped.append((f'{fn}:{sn}', nsk, f'скин скрыт на кадре меню {f} (PVIS)')); continue
                if hasexp:
                    sc.skipped.append((f'{fn}:{sn}', nsk, 'скин с морфом (выражение) — не перенесён')); continue
                inv = [np.linalg.inv(w0) for w0 in rest_world(joints)]
                for shader, P, N, UV, C, T, skin in prim_groups(sk_):
                    bi, bw = skin
                    Pp = np.zeros_like(P); Np = np.zeros_like(N) if N is not None else None
                    for k in range(4):
                        Mk = np.array([inv[b] @ W[b] for b in bi[:, k]])
                        ph = np.c_[P, np.ones(len(P))]
                        Pp += bw[:, k:k + 1] * np.einsum('ni,nij->nj', ph, Mk)[:, :3]
                        if N is not None: Np += bw[:, k:k + 1] * np.einsum('ni,nij->nj', N, Mk[:, :3, :3])
                    k = emit(sc, shader, Pp, Np, UV, C, T, M, 0xffff, 0xffff, lights)
                    sc.stats[fn] = sc.stats.get(fn, 0) + k

    def billboard(g, M):
        v, = struct.unpack_from('<I', g.data); gname, p = string(g.data, 4); shader, p = string(g.data, p)
        zt, zw, fog, nq = struct.unpack_from('<4I', g.data, p)
        _, tname, fl = material(sc, shader)
        fl |= sc.pre
        if not zw: fl |= F_NOZW
        glow = GLOWS.index(fn) if fn in GLOWS else 0xff
        for q in g.children:
            if q.id != 0x17001: continue
            qv, = struct.unpack_from('<I', q.data); qn, pp = string(q.data, 4); mode = q.data[pp:pp + 4].rstrip(b'\0').decode(); pp += 4
            tx, ty, tz, colr = struct.unpack_from('<3fI', q.data, pp); uvs = struct.unpack_from('<8f', q.data, pp + 16)
            w, h, dist = struct.unpack_from('<3f', q.data, pp + 48)
            assert np.allclose(uvs, [0, 0, 1, 0, 1, 1, 0, 1]), qn
            Mq = np.eye(4); Mq[3, :3] = (tx, ty, tz); Mq = Mq @ M
            tr = 0xffff
            an = A.get('BQG_' + gname)
            if an:
                typ, nfr, fps, groups = an; ch = groups[gname]; rows = []
                for f in range(nfr):
                    ww = sample(*ch['WDT'], f)[0] if 'WDT' in ch else w
                    hh = sample(*ch['HGT'], f)[0] if 'HGT' in ch else h
                    cc = sample(*ch['CLR'], f) if 'CLR' in ch else argb(colr)
                    rows.append(struct.pack('<6f', ww, hh, *cc))
                tr = sc.add_track(T_BILL, nfr, fps, TR_LOOP, b''.join(rows))
            m = {'NOAX': 0, 'YAX': 1}.get(mode, 2)
            sc.bills.append(dict(track=tr, tex=tname, flags=fl, mode=m, glow=glow, w=w, h=h, col=argb(colr),
                                 m=col(Mq)[:3, :4], name=f'{fn}:{qn}'))

    def rest_world(joints):
        W = []
        for j, (jn, par, L) in enumerate(joints):
            W.append(L if par >= j else L @ W[par])
        return W

    for sg in (c for c in root.children if c.id == 0x120100):
        flatten(sg, cb)
    for dn, m in meshes.items():
        if dn not in used: sc.skipped.append((f'{fn}:{dn}', ntris(m), 'не в дереве сцены'))
    for dn, m in skins.items():
        if dn not in used: sc.skipped.append((f'{fn}:{dn}', ntris(m), 'скин не в дереве сцены'))


TEX_TRACKS = {}


def tex_track(fn, shader):
    return TEX_TRACKS.get((fn, shader), 0xffff)


# ---------------------------------------------------------------- камера
def camera(root):
    A = anims(root)
    typ, nfr, fps, groups = A['CAM_FE_Camera']; ch = groups['CAM_FE_Camera']
    rows = []; poses = []
    for f in range(nfr):
        pos = sample(*ch['TRAN'], f); look = sample(*ch['LOOK'], f); up = sample(*ch['UP'], f)
        look = look / np.linalg.norm(look); up = up / np.linalg.norm(up)
        fov = sample(*ch['FOV'], f)[0]; nc = sample(*ch['NCLP'], f)[0]; fc = sample(*ch['FCLP'], f)[0]
        pm, lm, um = pos * [1, 1, -1], look * [1, 1, -1], up * [1, 1, -1]
        rows.append(struct.pack('<12f', *pm, *lm, *um, fov, nc, fc)); poses.append(np.r_[pm, lm, um, fov, nc, fc])
    poses = np.array(poses)
    same = np.all(np.abs(poses[:, :9] - poses[0, :9]) < 2e-3, axis=1)   # поза кадра 0 (главный вид), до 2 мм / 0.1°
    holds = []; s = None
    for f in range(nfr + 1):
        if f < nfr and same[f]:
            if s is None: s = f
        elif s is not None: holds.append((s, f - 1)); s = None
    holds = [h for h in holds if h[1] > h[0]]
    best = max(holds, key=lambda h: h[1] - h[0])
    return nfr, fps, b''.join(rows), poses, best[0], holds


def cam_lerp(poses, tl):
    """камера между кадрами данных — линейно по всем 12 числам, как движок (viewer/frontend3d.c feDraw)"""
    f0 = int(tl); f1 = min(f0 + 1, len(poses) - 1); u = tl - f0
    return poses[f0] + (poses[f1] - poses[f0]) * u


def near_eff(sc, poses, menu, GX=96, GY=54):
    """правка Роба 133, правило порта: ближняя плоскость перелётов. В данных (FE_Camera NCLP) у перелётов near
    0.01–0.066 при far 10–100; у PS2 GS глубина 24/32 бит, у PSP 16 бит (шаг ~d^2/(65535*near), docs/psp_tricks.md §4),
    и при near 0.01 ковёр и пол (1–2 см друг над другом) дерутся по глубине: на NEW GAME (кадры 32–35) пол мигает
    прозрачным. near поднимаем до near кадра меню, но не дальше 0.8 ближайшего видимого попадания луча (сетка GX x GY,
    кадр f и полукадры f±0.5 — движок тянет камеру линейно), чтобы ближняя плоскость ничего не срезала. far — как в
    данных. Кадры с near >= near меню не трогаем. -> (новые позы, число изменённых кадров)"""
    nm = float(poses[menu, 10]); out = poses.copy()
    lo = [f for f in range(len(poses)) if poses[f, 10] < nm]
    if NEAR_OLD or not lo: return out, 0
    reach = nm / .8                                              # попадания дальше не ограничивают near
    # мировые треугольники: статичные + узловые (дорожка таймлайна — по кадрам, свой цикл — все кадры цикла)
    stat = np.concatenate([t['P'] for t in sc.tris if t['node'] == 0xffff])
    node = {}
    for t in sc.tris:
        if t['node'] != 0xffff: node.setdefault(t['node'], []).append(t['P'])

    def mats(tr):
        typ, nf, fps_, fl, data = sc.tracks[tr]
        return np.frombuffer(data, '<f4').reshape(nf, 3, 4).astype(np.float64), bool(fl & TR_TIMELINE)
    nodes = {tr: (np.concatenate(ps),) + mats(tr) for tr, ps in node.items()}
    gx = (np.arange(GX) + .5) / GX * 2 - 1; gy = 1 - (np.arange(GY) + .5) / GY * 2
    SX, SY = np.meshgrid(gx, gy)
    changed = 0
    for f in lo:
        hit = np.inf
        for tl in (f - .5, f, f + .5):
            if tl < 0 or tl > len(poses) - 1: continue
            c = cam_lerp(poses, tl)
            pos, fwd, right, upv, tv, th = view_of(c)
            T = [stat]
            for P, M, tlk in nodes.values():
                ks = [min(int(round(tl)), len(M) - 1)] if tlk else range(len(M))
                T += [P @ M[k][:, :3].T + M[k][:, 3] for k in ks]
            T = np.concatenate(T)
            near_ = np.linalg.norm(T - pos, axis=2).min(1) < reach      # вершина рядом (грубо) ...
            cen = T.mean(1); rad = np.linalg.norm(T - cen[:, None], axis=2).max(1)
            near_ |= np.linalg.norm(cen - pos, axis=1) - rad < reach     # ... или сфера треугольника
            Tn = T[near_]
            if not len(Tn): continue
            D = fwd + right * (SX * th)[..., None] + upv * (SY * tv)[..., None]
            D = (D / np.linalg.norm(D, axis=2, keepdims=True)).reshape(-1, 3)
            for k in range(0, len(Tn), 256):                             # Мёллер–Трумбор, лучи x треугольники кусками
                Tk = Tn[k:k + 256]; e1 = Tk[:, 1] - Tk[:, 0]; e2 = Tk[:, 2] - Tk[:, 0]
                pv = np.cross(D[:, None], e2[None]); det = (e1[None] * pv).sum(2)
                ok = np.abs(det) > 1e-12; inv = np.where(ok, 1. / np.where(ok, det, 1.), 0.)
                tvv = pos - Tk[:, 0]; u = (tvv[None] * pv).sum(2) * inv; qv = np.cross(tvv, e1)
                v = (D @ qv.T) * inv; t = (e2 * qv).sum(1)[None] * inv
                m = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 1e-5)
                if m.any(): hit = min(hit, float((np.where(m, t, np.inf) * (D @ fwd)[:, None]).min()))   # глубина по оси
        ne = max(float(poses[f, 10]), min(nm, .8 * hit))
        if ne != out[f, 10]: out[f, 10] = ne; changed += 1
    return out, changed


def page_order():
    """объекты Pure3d страницы 3dFE.pag сверху вниз (порядок отрисовки Scrooby): TVScreen, CamAndSet, RaceCar, Glow5,
    Glow2, Homer, Glow4, Glow3, Glow1, Glow0 -> {файл: стоит до Гомера}. Подсветки ТВ и дивана рисуются ДО Гомера,
    поэтому Гомер (непрозрачный, пишет глубину) закрывает подсветку дивана; остальные — после него"""
    import ui_export as U
    prj = U.project('frontend.p3d')
    res = [e['resource'].lower() for _, e in U.iter_leaves(prj.pages['3dFE.pag']) if e['kind'] == 'p3d']
    h = res.index('homer')
    return {fn: res.index(fn.lower()) < h for fn in FILES}, res


def ray_hit(tris, o, d):
    """ближайшее пересечение луча с треугольниками (N,3,3) -> точка или None (Мёллер–Трумбор)"""
    e1 = tris[:, 1] - tris[:, 0]; e2 = tris[:, 2] - tris[:, 0]; pv = np.cross(d, e2); det = (e1 * pv).sum(1)
    ok = np.abs(det) > 1e-12; inv = np.where(ok, 1. / np.where(ok, det, 1.), 0.)
    tv = o - tris[:, 0]; u = (tv * pv).sum(1) * inv; qv = np.cross(tv, e1); v = (qv @ d) * inv; t = (e2 * qv).sum(1) * inv
    m = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 0.01)
    return o + d * t[m].min() if m.any() else None


def flight_segments(poses, menu):
    """отрезки таймлайна FE_CameraController, которые начинаются в главном виде (поза кадра меню) и уводят камеру к
    предмету: (старт, прилёт, обратно_до | None). Прилёт: у петли «главный вид -> предмет -> главный вид» — её середина
    (обе половины зеркальны по времени, скорость в середине ~0); у отрезка без возврата — первый кадр удержания
    (скорость 0) или склейки (скачок камеры > 1 м за кадр)"""
    n = len(poses)
    home = np.all(np.abs(poses[:, :9] - poses[menu, :9]) < 2e-3, axis=1)
    v = np.r_[0., np.linalg.norm(np.diff(poses[:, :3], axis=0), axis=1)]
    out = []
    for s in range(n - 1):
        if not (home[s] and not home[s + 1] and v[s + 1] < 1.): continue
        f = s + 1
        while f + 1 < n and not home[f + 1] and v[f + 1] < 1. and not (v[f + 1:f + 4] < 1e-4).all(): f += 1
        if f + 1 < n and home[f + 1] and v[f + 1] < 1.:          # петля: вернулась в главный вид без склейки
            r = f + 1; a = (s + r) // 2
            sym = max(float(np.abs(poses[s + k, :9] - poses[r - k, :9]).max()) for k in range(r - s + 1))
            out.append((s, a, r if sym < 2e-2 else None))
        else: out.append((s, f, None))
    return out


def flights(sc, poses, menu):
    """таблица перелётов по данным: для каждого отрезка — что в центре кадра камеры в момент прилёта (луч в
    треугольники комнаты) и какая подсветка Glow0..5 закрывает эту точку, если смотреть из главного вида (подсветки —
    плоскости перед предметами, совпадают с ними только из главного вида). Пункт = номер Glow. Если отрезков на пункт
    несколько, берём тот, у которого в данных есть обратный перелёт (диван: 80–110–140, а не 770–800 со склейкой)"""
    tris = np.concatenate([t['P'] for t in sc.tris if t['node'] == 0xffff])
    P0 = poses[menu, :3]
    glows = {}
    for b in sc.bills:
        if b['glow'] != 0xff: glows.setdefault(b['glow'], []).append(b['m'][:, 3])
    gdir = {g: (lambda d: d / np.linalg.norm(d))(np.mean(p, 0) - P0) for g, p in glows.items()}
    cands = {}
    for s, a, r in flight_segments(poses, menu):
        c = poses[a]; p = ray_hit(tris, c[:3], c[3:6] / np.linalg.norm(c[3:6]))
        if p is None: continue
        d = (p - P0) / np.linalg.norm(p - P0)
        ang, g = min((math.degrees(math.acos(float(np.clip(d @ gd, -1, 1)))), g) for g, gd in gdir.items())
        if ang <= FLIGHT_MAX_DEG: cands.setdefault(g, []).append((r is None, ang, s, a, r))
    out = []
    for g in sorted(cands):
        noback, ang, s, a, r = min(cands[g])
        out.append(dict(item=g, go=(s, a), back=(a, r) if r is not None else None, deg=ang))
    if FLIGHT_SHIFT:
        items = [f['item'] for f in out]
        for f, it in zip(out, items[FLIGHT_SHIFT % len(items):] + items[:FLIGHT_SHIFT % len(items)]): f['item'] = it
    return out


# ---------------------------------------------------------------- Гомер меню -> CHR1 (треугольники)
def chr_batches(parts, shd, imgs, tex_index, textures, eye_shader=None):
    """части [(shader, P, N, UV, T, bi, bw)] в осях игры (P/N не отзеркалены) -> батчи GU_TRIANGLES, союз костей <= 8
    (жадно, как char_export). Батч шейдера eye_shader помечен флагом 1 (глаза: текстура по дорожке TEX_*)"""
    batches = []; ntri = 0
    pscale = ce.pos16_scale([P for _, P, *_r in parts])           # Модели-96: позиции s16 с масштабом модели в заголовке (CHARPOS16_OLD=1 — f32)
    for shader, P, N, UV, T, bi, bw in parts:
        tname = shd.get(shader, {}).get('TEX', '')
        if tname not in tex_index: tex_index[tname] = len(textures); textures.append((tname, imgs[tname]))
        P = P * [1, 1, -1]; N = N * [1, 1, -1]; T = T[:, [0, 2, 1]]
        ntri += len(T)
        open_ = []
        for t in T:
            used = {int(bi[v, k]) for v in t for k in range(4) if bw[v, k] > 1e-4}
            best = None
            for bt in open_:
                u = bt['bones'] | used
                if len(u) <= ce.MAXB and (best is None or len(u) < len(best['bones'] | used)): best = bt
            if best is None:
                best = dict(tex=tex_index[tname], bones=set(), tris=[]); open_.append(best)
            best['bones'] |= used; best['tris'].append(t)
        for bt in open_:
            bones = sorted(bt['bones']); slot = {b: i for i, b in enumerate(bones)}; nb = len(bones)
            uvo, nro, poo, stride = ce.vertex_layout(nb, pscale is not None)
            seq = [v for t in bt['tris'] for v in t]
            buf = bytearray(stride * len(seq))
            for k, v in enumerate(seq):
                o = k * stride; w = np.zeros(nb)
                for b, x in zip(bi[v], bw[v]):
                    if x > 1e-4: w[slot[int(b)]] += x
                w /= w.sum(); q = ce.quantize_weights(w, nb)
                buf[o:o + nb] = bytes(int(x) for x in q)
                struct.pack_into('<hh', buf, o + uvo, int(round(UV[v, 0] * ce.UV_SCALE)), int(round((1 - UV[v, 1]) * ce.UV_SCALE)))
                if pscale is None: struct.pack_into('<bbb', buf, o + nro, *[int(round(max(-1, min(1, c)) * 127)) for c in N[v]])
                else: struct.pack_into('<hhh', buf, o + nro, *[int(round(max(-1, min(1, c)) * 32767 / (pscale * 32768))) for c in N[v]])
                if pscale is None: struct.pack_into('<fff', buf, o + poo, *P[v])
                else: struct.pack_into('<hhh', buf, o + poo, *[int(round(float(c) / pscale)) for c in P[v]])
            vt = ce.GU_WEIGHTS(nb) | ce.GU_WEIGHT_8BIT | ce.GU_TEXTURE_16BIT | (ce.GU_NORMAL_8BIT | ce.GU_VERTEX_32BITF if pscale is None else ce.GU_NORMAL_16BIT | ce.GU_VERTEX_16BIT)
            batches.append(dict(tex=bt['tex'], bones=bones, nb=nb, stride=stride, nverts=len(seq), data=bytes(buf), vtype=vt, pos=P, pscale=pscale,
                                flags=1 if eye_shader is not None and shader == eye_shader else 0))
    return batches, ntri


def chr1_blob(joints, batches, textures, clips, eyes=None, clip_flags=None, root=0):
    """запись CHR1 (как char_export.write_chr, примитив GU_TRIANGLES). eyes — ключи дорожки TEX_* [(кадр, номер текстуры)]:
    таблица мигания CHR1, blinkFps = 0 (случайное мигание skin.c выключено; кадр текстуры — по времени клипа, frontend3d.c).
    clip_flags(c, cl) -> флаги клипа (по умолчанию 1, как у Гомера меню). root — сустав корня-движения (skin.c не двигает его
    переносом); 0xffffffff — нет такого: перенос корня играет в позе (сценка №404 ходит сама: Мэгги ползёт 3,5 м)"""
    nj, nbt, ntx, ncl = len(joints), len(batches), len(textures), len(clips)
    nbl = len(eyes) if eyes else 0
    bl = ce.Blob(); bl.b += bytes(ce.HDR)
    offJ = bl.add(bytes(ce.JNT * nj)); offB = bl.add(bytes(ce.BAT * nbt)); offT = bl.add(bytes(ce.TEXR * ntx)); offC = bl.add(bytes(ce.CLIP * ncl))
    offBl = bl.add(bytes(8 * nbl) if nbl else bytes(8))
    for k, (fr, ti) in enumerate(eyes or []): struct.pack_into('<HHI', bl.b, offBl + 8 * k, fr, ti, 0)
    for j, jt in enumerate(joints):
        inv = jt['inv']; cols = [inv[:3, c] for c in range(4)]
        rec = ce.cname(jt['name'], 32) + struct.pack('<iI', jt['parent'], 0) + struct.pack('<4f', *jt['q']) + struct.pack('<3f', *jt['t']) \
            + struct.pack('<3f', *jt['s']) + struct.pack('<12f', *np.concatenate(cols))
        bl.b[offJ + ce.JNT * j: offJ + ce.JNT * j + len(rec)] = rec
    for i, (name, (w, h, clut, idx)) in enumerate(textures):
        oc = bl.add(clut.tobytes(), 16); op = bl.add(ce.swizzle(idx, w, h), 16)
        rec = ce.cname(name, 24) + struct.pack('<HHIIII', w, h, ce.GU_PSM_T8, oc, op, 1)
        bl.b[offT + ce.TEXR * i: offT + ce.TEXR * i + len(rec)] = rec
    tv = 0
    for i, bt in enumerate(batches):
        ov = bl.add(bt['data'], 16); tv += bt['nverts']
        rec = struct.pack('<IIII', bt['vtype'], 3, bt['nverts'], ov) + struct.pack('<HH', bt['tex'], bt['nb']) + bytes(bt['bones']) + bytes(8 - bt['nb']) \
            + struct.pack('<II', bt['stride'], bt.get('flags', 0))
        bl.b[offB + ce.BAT * i: offB + ce.BAT * i + len(rec)] = rec
    nkeys = 0; shared = {}
    def keys(data):                                   # Модели-96: упаковка ключей (tools/animpack.py, ANIMPACK96_OLD=1 — как было), одинаковые массивы — один раз
        if ce.ANIMPACK.OLD: return bl.add(data, 4)
        if data not in shared: shared[data] = bl.add(data, 4)
        return shared[data]
    for c, cl in enumerate(clips):
        offTr = bl.add(bytes(ce.TRK * nj)); has_root = 0
        for j, tr in enumerate(cl['tracks']):
            nr, nt = len(tr['rf']), len(tr['tf']); orr = ot = 0; flags = 0
            if nr:
                mode, data = ce.ANIMPACK.pack_rot(tr['rf'], tr['rq']); orr = keys(data); nkeys += nr; flags |= mode
            if nt:
                mode, data = ce.ANIMPACK.pack_trans(tr['tf'], tr['tv']); ot = keys(data); nkeys += nt; flags |= mode << 2
                if j == 0: has_root = 2
            struct.pack_into('<HHIII', bl.b, offTr + ce.TRK * j, nr, nt, orr, ot, flags)
        dur = (cl['frames'] - 1) / cl['fps']
        fl = clip_flags(c, cl) if clip_flags else 1
        rec = ce.cname(cl['name'], 48) + struct.pack('<fffII', cl['frames'], cl['fps'], dur, fl | has_root, offTr)
        bl.b[offC + ce.CLIP * c: offC + ce.CLIP * c + len(rec)] = rec
    allp = np.concatenate([b['pos'] for b in batches]); bbox = list(allp.min(0)) + list(allp.max(0))
    eb = next((i for i, b in enumerate(batches) if b.get('flags', 0) & 1), 0xffffffff) if nbl else 0xffffffff
    bl.at(16)
    struct.pack_into('<4s13I7f3I', bl.b, 0, b'CHR1', 1, len(bl.b), nj, nbt, ntx, ncl, nbl, offJ, offB, offT, offC, offBl, root,
                     *bbox, 0., eb, tv, nkeys)
    ps = next((b.get('pscale') for b in batches if b.get('pscale')), None)
    if ps: struct.pack_into('<f', bl.b, 96, ps)                      # Модели-96: pad[0] — масштаб s16-позиций
    return bytes(bl.b), tv


def anchor_place(root, name):
    """мировая матрица узла сцены (0x120100), на котором висит рисуемое name"""
    place = None
    for sg in (c for c in root.children if c.id == 0x120100):
        def cb(c, M, path):
            nonlocal place
            if place is None and string(c.data, string(c.data)[1])[0] == name: place = M
        flatten(sg, cb)
    return place


def homer_chr(root, sc, gag_root=None):
    joints = ce.load_skeleton(root)
    clips = ce.load_clips(root, joints)
    if gag_root is not None:                                 # №404: клип 1 — движение сценки Gag0 (gaghomer) на скелете Гомера меню
        clips += ce.load_clips(gag_root, joints)             # (суставы по именам: 41 Гомера ⊂ 47 gaghomer)
    shd = shaders_of(root)
    imgs = ce.load_images(root)
    tex_index, textures = {}, []
    parts = []
    for sk in (c for c in root.children if c.id == 0x10001):
        for shader, P, N, UV, C, T, skin in prim_groups(sk):
            parts.append((shader, P, N, UV, T) + tuple(skin))
    batches, ntri = chr_batches(parts, shd, imgs, tex_index, textures)
    blob, tv = chr1_blob(joints, batches, textures, clips)
    place = anchor_place(root, 'Motion_Root')                # место: FE_Homer — узел над композитом Motion_Root
    sc.homer = dict(blob=blob, place=col(place)[:3, :4], tris=ntri, batches=len(batches), verts=tv, clips=[c['name'] for c in clips],
                    frames=[c['frames'] for c in clips], joints=len(joints))


# №404 (Роб 08.10: «пасхалки меню — прохожие заглядывают в окно, Мэгги ползает»): сценки гостиной, страница 3dFEGags.pag
# (frontend.p3d). Из данных: на странице 8 слоёв Homer, GrandPa, Moleman, Frink, Barney, Nick, Snake, Maggie, в каждом
# один объект Gag0..Gag7 (gaghomer, gaggrand, gagmole, gagfrink, gagbarne, gagnick, gagsnake, maggie); видим по умолчанию
# только слой Homer, остальные скрыты — какой слой показать, решает код игры. Каждый ресурс страницы (0x18101) рисуется
# камерой FE_Camera (камера гостиной) со своим мультиконтроллером. В экране MainMenu.scr (и LoadGame, ScrapBook...) страница
# 3dFEGags.pag стоит ПЕРЕД 3dFE.pag — сценка рисуется до комнаты. Файл сценки: композит (кожи 0x4515 + пропы 0x4516 на
# суставах: глаза, зубы, язык Гомера, бант, глаза, ладошки Мэгги), скелет композита, клип PTRN_<скелет>, дорожка
# TEX_<шейдер> (кадры текстуры глаз по времени клипа), место — узел сцены над композитом. Не рисуется: камера fe_camShape
# (дорожка CAM_fe_camShape в мультиконтроллере Барни — камера, которой страница не пользуется: у страницы FE_Camera),
# пустой композит iris_joint у Мэгги (ни кож, ни пропов; клип PTRN_iris_joint двигает только его).
# Когда и в каком порядке они играют, в данных нет (код игры; вопрос наблюдателю В100) — расписание в viewer/frontend3d.c,
# правило порта. FE_GAGS_OLD=1 — без сценок (красный контроль гейта tests/menu/gags.sh)
GAGS_OLD = os.environ.get('FE_GAGS_OLD') == '1'
GAG_LOOP, GAG_PAGEVIS, GAG_ONHOMER = 1, 2, 4   # флаги записи GAG1: клип с флагом цикла в данных; слой страницы видим по умолчанию;
# сценка Гомера меню: своей модели в fe.shr нет (offChr = sizeChr = 0) — её движение играет Гомер меню (клип 1 его CHR1).
# Из исполняемого файла диска (SLES_518.97, CGuiScreenMainMenu, 0x1b9414–0x1b94fc): для Gag0 код берёт объект «Homer»
# страницы 3dFE и ставит его мультиконтроллеру анимацию из Gag0 с отрезком кадров (таблица 0x42bc98) и смешиванием 10 кадров;
# по данным место gaghomer — в 1,8 см от FE_Homer (тот же Гомер на диване). Отрезки — в viewer/frontend3d.c FE_HOMER_GAG_SEG
GAG_SAME_M = .1


def gag_page():
    """[(ресурс, слой видим по умолчанию)] в порядке страницы 3dFEGags.pag (Gag0..Gag7)"""
    import ui_export as U
    prj = U.project('frontend.p3d'); out = []
    for lay in prj.pages['3dFEGags.pag']:
        for ch in lay.get('children', []):
            if ch.get('kind') == 'p3d': out.append((ch['resource'].lower(), bool(lay.get('visible'))))
    return out


def gag_chr(root):
    from p3d import Chunk
    comps = [c for c in root.children if c.id == 0x4512]
    cc = max(comps, key=lambda c: sum(1 for k in walk(c) if k.id in (0x4515, 0x4516)))   # композит с геометрией
    cname, p = string(cc.data); skn, _ = string(cc.data, p)
    skel = next(c for c in root.children if c.id == 0x4500 and string(c.data)[0] == skn)
    joints = ce.load_skeleton(Chunk(0, b'', [skel]))                                      # только скелет композита
    anim = [c for c in root.children if c.id == 0x121000 and string(c.data, 4)[0] == 'PTRN_' + skn]
    clips = ce.load_clips(Chunk(0, b'', anim), joints)
    shd = shaders_of(root); imgs = ce.load_images(root)
    meshes = {string(c.data)[0]: c for c in root.children if c.id in (0x10000, 0x10001)}
    eye = ce.load_blink(root)
    parts = []; nprop = 0
    for s in walk(cc):
        if s.id == 0x4515:                                   # кожа
            for shader, P, N, UV, C, T, skin in prim_groups(meshes[string(s.data)[0]]):
                parts.append((shader, P, N, UV, T) + tuple(skin))
        elif s.id == 0x4516:                                 # проп на суставе jj: вершины в осях сустава -> поза привязки
            pn, q = string(s.data); _, jj = struct.unpack_from('<II', s.data, q); nprop += 1
            Wb = joints[jj]['W']                              # столбцовая, отзеркаленные оси (char_export)
            for shader, P, N, UV, C, T, _ in prim_groups(meshes[pn]):
                Pm = np.c_[P * [1, 1, -1], np.ones(len(P))] @ Wb.T
                Pg = Pm[:, :3] * [1, 1, -1]                   # обратно в оси игры: chr_batches отзеркалит сам
                if N is None: N = np.zeros_like(P)
                Ng = (N * [1, 1, -1]) @ Wb[:3, :3].T
                Ng = Ng / np.maximum(np.linalg.norm(Ng, axis=1, keepdims=True), 1e-9) * [1, 1, -1]
                n = len(P)
                parts.append((shader, Pg, Ng, UV, T, np.tile([[jj, 0, 0, 0]], (n, 1)), np.tile([[1., 0, 0, 0]], (n, 1))))
    tex_index, textures = {}, []
    batches, ntri = chr_batches(parts, shd, imgs, tex_index, textures, eye['shader'] if eye else None)
    keys = []
    if eye and any(b['flags'] & 1 for b in batches):
        for fr, tn in zip(eye['frames'], eye['textures']):
            if tn not in tex_index: tex_index[tn] = len(textures); textures.append((tn, imgs[tn]))
            keys.append((fr, tex_index[tn]))
    blob, tv = chr1_blob(joints, batches, textures, clips, keys, clip_flags=lambda c, cl: cl['loop'] & 1, root=0xffffffff)
    place = anchor_place(root, cname)
    return dict(blob=blob, place=col(place)[:3, :4], tris=ntri, batches=len(batches), verts=tv, joints=len(joints), props=nprop,
                clip=clips[0]['name'], frames=clips[0]['frames'], fps=clips[0]['fps'], loop=clips[0]['loop'] & 1, eyes=len(keys),
                textures=[t for t, _ in textures])


def gag_chrs(homer_place=None):
    out = []
    for k, (res, vis) in enumerate([] if GAGS_OLD else gag_page()):
        root = read(PU / f'{res}.p3d')
        g = gag_chr(root)
        on = k == 0 and homer_place is not None and float(np.linalg.norm(g['place'][:, 3] - homer_place[:, 3])) < GAG_SAME_M
        if on: g.update(blob=b'', props=0, eyes=0)          # движение играет Гомер меню (клип 1), модель сценки не нужна
        out.append(dict(name=res, pagevis=vis, onhomer=on, **g))
    return out


# ---------------------------------------------------------------- нарезка у камеры меню, куски, запись
def view_of(pose, aspect=16. / 9.):
    pos, look, up = pose[0:3], pose[3:6], pose[6:9]
    fwd = look / np.linalg.norm(look); right = np.cross(fwd, up); right /= np.linalg.norm(right); upv = np.cross(right, fwd)
    tv = math.tan(pose[9] / 2) / (4. / 3.)                       # fov — горизонтальный при 4:3; Hor+: вертикаль та же
    return pos, fwd, right, upv, tv, tv * aspect


def views_of(poses, frames):
    """виды камеры (pos, fwd, right, up, tv, th) кадров -> массивы по видам (V, ...) для need_split"""
    vs = [view_of(poses[f]) for f in frames]
    return tuple(np.array([v[i] for v in vs]) for i in range(6))


def need_split(T, VA):
    """T (N,3,3) -> маска: треугольник хоть в одном виде VA виден (не за одной гранью пирамиды с запасом 10%) и
    пересекает ближнюю плоскость NEAR_SPLIT или на экране длиннее MAX_PX"""
    pos, fwd, right, upv, tv, th = VA; need = np.zeros(len(T), bool)
    for k in range(len(pos)):
        rel = T - pos[k]; z = rel @ fwd[k]; x = rel @ right[k]; y = rel @ upv[k]
        out = (np.all(1.1 * th[k] * z - x < 0, 1) | np.all(1.1 * th[k] * z + x < 0, 1) |
               np.all(1.1 * tv[k] * z - y < 0, 1) | np.all(1.1 * tv[k] * z + y < 0, 1))
        vis = ~out & (z.max(1) > NEAR_SPLIT)
        zs = np.maximum(z, 1e-3); xs = x / zs / th[k] * 240; ys = y / zs / tv[k] * 136
        need |= vis & ((z.min(1) < NEAR_SPLIT) | (np.maximum(np.ptp(xs, 1), np.ptp(ys, 1)) > MAX_PX))
    return need


def split_near(P, UV, C, VA):
    """треугольник, который в кадре меню или в кадрах перелётов виден (не за одной гранью пирамиды) и пересекает
    ближнюю плоскость NEAR_SPLIT или длиннее MAX_PX на экране, делим пополам по длинному ребру (до 8 раз): GE не режет по
    ближней плоскости"""
    out = []; stack = [(P, UV, C, 0)]; added = 0
    while stack:
        p, u, c, d = stack.pop()
        need = bool(need_split(p[None], VA)[0])
        if need and d < 8:
            e = [np.linalg.norm(p[(i + 1) % 3] - p[i]) for i in range(3)]; i = int(np.argmax(e)); j = (i + 1) % 3; k = (i + 2) % 3
            mp, mu, mc = (p[i] + p[j]) / 2, (u[i] + u[j]) / 2, (c[i] + c[j]) / 2
            stack.append((np.array([mp, p[j], p[k]]), np.array([mu, u[j], u[k]]), np.array([mc, c[j], c[k]]), d + 1))
            stack.append((np.array([p[i], mp, p[k]]), np.array([u[i], mu, u[k]]), np.array([c[i], mc, c[k]]), d + 1))
            added += 1
        else: out.append((p, u, c))
    return out, added


def menu_hidden(tris, view, near, W=960, H=544):
    """какие треугольники не видны из камеры меню: вне кадра или за непрозрачным (без альфа-теста и смешивания, не на
    дорожке). Буфер ближайшего 1/z заслонителей в 2x разрешении кадра; треугольник виден, если хоть в одном своём
    пикселе он не дальше заслонителя (запас 0.2% по 1/z); без пикселей (мельче точки) — по рамке +1 px с запасом"""
    pos, fwd, right, upv, tv, th = view
    zb = np.zeros((H, W)); prj = []

    def cover(sx, sy, iz3, x0, x1, y0, y1):
        d = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0])
        if abs(d) < 1e-9: return None, None
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
        l1 = ((gx - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (gy - sy[0])) / d
        l2 = ((sx[1] - sx[0]) * (gy - sy[0]) - (gx - sx[0]) * (sy[1] - sy[0])) / d
        l0 = 1 - l1 - l2
        return (l0 >= 0) & (l1 >= 0) & (l2 >= 0), l0 * iz3[0] + l1 * iz3[1] + l2 * iz3[2]
    for P, fl, node in tris:
        rel = P - pos; z = rel @ fwd
        if z.min() <= near: prj.append(None); continue
        sx = W / 2 + (rel @ right) / z / th * W / 2; sy = H / 2 - (rel @ upv) / z / tv * H / 2
        box = (max(0, int(sx.min())), min(W - 1, int(math.ceil(sx.max()))), max(0, int(sy.min())), min(H - 1, int(math.ceil(sy.max()))))
        prj.append((sx, sy, 1. / z, box))
        if fl & (F_ATEST | F_BLEND | F_ADD) or node != 0xffff or box[0] > box[1] or box[2] > box[3]: continue
        m, iz = cover(sx, sy, 1. / z, *box)
        if m is None: continue
        sub = zb[box[2]:box[3] + 1, box[0]:box[1] + 1]; np.maximum(sub, np.where(m, iz, 0.), out=sub)
    out = []
    for (P, fl, node), pr in zip(tris, prj):
        if node != 0xffff or pr is None: out.append(False); continue
        sx, sy, iz3, (x0, x1, y0, y1) = pr
        if x0 > x1 or y0 > y1: out.append(True); continue                     # вне кадра
        m, iz = cover(sx, sy, iz3, x0, x1, y0, y1)
        sub = zb[y0:y1 + 1, x0:x1 + 1]
        if m is not None and m.any(): out.append(not bool((m & (iz >= sub * (1 - 2e-3))).any())); continue
        X0, X1, Y0, Y1 = max(0, x0 - 1), min(W - 1, x1 + 1), max(0, y0 - 1), min(H - 1, y1 + 1)
        out.append(bool(zb[Y0:Y1 + 1, X0:X1 + 1].min() > iz3.max() * (1 + 2e-3)))
    return out


def pack_verts(P, UV, C, origin, scale):
    q = np.zeros(len(P), np.dtype([('u', '<i2'), ('v', '<i2'), ('c', '<u4'), ('x', '<i2'), ('y', '<i2'), ('z', '<i2'), ('p', '<i2')]))
    assert np.abs(UV).max() < 128, 'uv вне ±128'
    q['u'], q['v'] = uvq.tri_i16(UV[:, 0], UV[:, 1])   # GE читает u16: целый сдвиг на треугольник (tools/uvq.py, правка Роба 43)
    c8 = np.clip(np.round(C * 255), 0, 255).astype(np.uint32)
    q['c'] = c8[:, 0] | (c8[:, 1] << 8) | (c8[:, 2] << 16) | (c8[:, 3] << 24)
    xyz = np.clip(np.round((P - origin) / scale * 32767), -32767, 32767)
    q['x'], q['y'], q['z'] = xyz[:, 0], xyz[:, 1], xyz[:, 2]
    return q.tobytes()


def build(out_path):
    texq.CAP, texq.CLUT4, texq.PRESWIZZLE, texq.SMALL_MIPS = 256, False, True, True   # закон 1: как уровень, родной размер
    roots = {fn: read(PU / f'{fn}.p3d') for fn in FILES}
    sc = Scene()
    for fn in FILES:
        load_textures(sc, roots[fn])
        for k, v in shaders_of(roots[fn]).items(): sc.shaders.setdefault(k, v)
    # свет камсета (FE_Lights): направленные -> направление К свету, отзеркалено
    lights = dict(dir=[], ambient=np.zeros(3), raw=[])
    for c in roots['camset'].children:
        if c.id != 0x13000: continue
        n, p = string(c.data); ver, typ, colr = struct.unpack_from('<3I', c.data, p)
        rgb = argb(colr)[:3]
        if typ == 0: lights['ambient'] = rgb; lights['raw'].append(('amb', None, colr))
        else:
            d = next(np.frombuffer(s.data, '<f4', 3).astype(np.float64) for s in c.children if s.id == 0x13001)
            L = -d * [1, 1, -1]; L /= np.linalg.norm(L); lights['dir'].append((L, rgb)); lights['raw'].append((n, L, colr))
    nfr, fps, camrows, poses, menu, holds = camera(roots['camset'])
    view = view_of(poses[menu])
    # текстурная анимация экрана ТВ (TEX_FE_static_m): дорожка номеров текстур
    for fn in FILES:
        for nm, (typ, nf, afps, groups) in anims(roots[fn]).items():
            if typ != 'TEX': continue
            for g, ch in groups.items():
                fr, names = ch['TEX']
                seq = [names[int(np.searchsorted(fr, f, 'right')) - 1] for f in range(nf)]
                sc.tex_anim = seq
                TEX_TRACKS[(fn, g[4:])] = sc.add_track(T_TEX, nf, afps, TR_LOOP, b''.join(struct.pack('<H', 0) for _ in seq))
                sc.tex_track_names = (TEX_TRACKS[(fn, g[4:])], seq)
    pre, sc.page = page_order()
    for fn in FILES:
        if fn == 'homer': continue
        sc.pre = F_PREHOMER if pre[fn] and not GLOW_ORDER_OLD else 0
        scene_file(sc, fn, roots[fn], lights, (nfr, menu), None)
    sc.pre = 0
    homer_chr(roots['homer'], sc, None if GAGS_OLD else read(PU / 'gaghomer.p3d'))
    sc.flights = [] if NO_FLIGHTS else flights(sc, poses, menu)
    poses, nnear = near_eff(sc, poses, menu)                   # правка Роба 133: near перелётов под 16-битную глубину
    camrows = b''.join(struct.pack('<12f', *r) for r in poses)
    print(f'near перелётов: поднято на {nnear} кадрах' + (' (FE_NEAR_OLD: как в данных)' if NEAR_OLD else ''))
    fl_frames = sorted({menu} | {f for fl in sc.flights for a, b in [fl['go']] + ([fl['back']] if fl['back'] else [])
                                 for f in list(range(a, b + 1, FLIGHT_STEP)) + [b]})
    VA = views_of(poses, fl_frames)
    # текстуры: только используемые (кусками, билбордами, анимацией)
    used = {t['tex'] for t in sc.tris} | {b['tex'] for b in sc.bills} | set(sc.tex_anim)
    names = [n for n in sc.tex_order if n in used]
    missing = sorted(u for u in used if u and u not in sc.tex)
    assert not missing, missing
    tidx = {n: i for i, n in enumerate(names)}
    # дорожка ТВ: имена -> номера
    tr, seq = sc.tex_track_names
    t = sc.tracks[tr]; sc.tracks[tr] = (t[0], t[1], t[2], t[3], b''.join(struct.pack('<H', tidx[n]) for n in seq))
    # треугольники после нарезки у камеры меню; видимость из кадра меню (как PVS города, одна точка глаза)
    flat = []; added = 0; src_total = 0
    for t in sc.tris:
        n = len(t['P']); src_total += n
        need = need_split(t['P'], VA) if t['node'] == 0xffff else np.zeros(n, bool)
        for k in range(n):
            if need[k]:
                tri, a = split_near(t['P'][k], t['UV'][k], t['C'][k], VA); added += a
            else: tri = [(t['P'][k], t['UV'][k], t['C'][k])]
            cen = t['P'][k].mean(0)
            base = (tidx.get(t['tex'], 0xffff), t['texTrack'], t['flags'], t['node'], t['vis'])
            cell = (int(math.floor(cen[0] / CELL)), int(math.floor(cen[1] / CELL)), int(math.floor(cen[2] / CELL)))
            for j, (pp, u, c) in enumerate(tri): flat.append((base, cell, pp, u, c, 1 if j == 0 else 0))
    hidden = menu_hidden([(f[2], f[0][2], f[0][3]) for f in flat], view, poses[menu][10])
    floor_t = {tidx[n] for n in FLOOR_TEX if n in tidx}
    if not FLOOR_HID_OLD:                                     # правка 293: пол — без F_MENUHID (один слой и в меню, и в перелёте)
        hidden = [h and f[0][0] not in floor_t for f, h in zip(flat, hidden)]
    # куски: (текстура, анимация текстуры, режим (+16 скрыт в кадре меню), узел, видимость) x клетка CELL
    groups = {}
    for (base, cell, pp, u, c, first), hid in zip(flat, hidden):
        if not FLOOR_HID_OLD and base[0] in floor_t: cell = (0, 0, 0)   # правка 293: весь пол — один кусок (одна сетка вершин:
        key = (base[0], base[1], base[2] | (F_MENUHID if hid else 0), base[3], base[4]) + cell   # общие рёбра без щелей и наложений)
        g = groups.setdefault(key, dict(P=[], UV=[], C=[], src=0)); g['src'] += first
        g['P'].append(pp); g['UV'].append(u); g['C'].append(c)
    sc.hidden = int(sum(hidden))
    blob = bytearray(128)

    def add(data, a=16):
        while len(blob) % a: blob.append(0)
        o = len(blob); blob.extend(data); return o
    # текстуры
    trec = bytearray()
    tex_bytes = 0
    for n in names:
        data, levels, pal, fmt = texq.build(sc.tex[n], 256, 'kmeans', False, True)
        w, h, nlev = struct.unpack_from('<HHH', data, 0)
        oc = add(data[16:16 + 1024]); body = data[16 + 1024:]
        offs, bufw = [], []; lw, lh, o = w, h, 0
        for l in range(nlev):
            rb = max(lw, 16); sz = rb * ((lh + 7) & ~7); sz += -sz % 16
            offs.append(o); bufw.append(rb); o += sz; lw, lh = max(1, lw // 2), max(1, lh // 2)
        assert o == len(body), (n, o, len(body))
        ob = add(body); tex_bytes += len(data)
        offs = [ob + x for x in offs] + [0] * (8 - nlev); bufw = bufw + [0] * (8 - nlev)
        trec += struct.pack('<HHB3xI8I8HI', w, h, nlev, oc, *offs, *bufw, 0)
    offTex = add(bytes(trec))
    offNames = add(b''.join(n.encode('latin1')[:31].ljust(32, b'\0') for n in names))
    # дорожки
    tr_rec = bytearray()
    for typ, nf, tfps, fl, data in sc.tracks:
        o = add(data); tr_rec += struct.pack('<IIfII3I', typ, nf, tfps, fl, o, 0, 0, 0)
    offTrack = add(bytes(tr_rec))
    # куски
    prec = bytearray(); nverts = 0; ptris = 0
    for key in sorted(groups):
        g = groups[key]; P = np.array(g['P']).reshape(-1, 3); UV = np.array(g['UV']).reshape(-1, 2); C = np.array(g['C']).reshape(-1, 4)
        lo, hi = P.min(0), P.max(0); origin = (lo + hi) / 2; scale = max(float((hi - lo).max() / 2), 1e-3) * 1.0001
        ov = add(pack_verts(P, UV, C, origin, scale)); nverts += len(P); ptris += len(P) // 3
        tex, ttr, fl, node, vis = key[:5]
        prec += struct.pack('<6H2I4f6fI', tex, ttr, fl, node, vis, 0, len(P), ov, *origin, scale, *lo, *hi, g['src'])
    offPiece = add(bytes(prec))
    # билборды
    brec = bytearray()
    for b in sc.bills:
        c8 = np.clip(np.round(b['col'] * 255), 0, 255).astype(int)
        abgr = int(c8[0] | (c8[1] << 8) | (c8[2] << 16) | (c8[3] << 24))
        brec += struct.pack('<3H2B2fI12f12x', b['track'], tidx.get(b['tex'], 0xffff), b['flags'], b['mode'], b['glow'], b['w'], b['h'], abgr,
                            *b['m'].flatten())
    offBill = add(bytes(brec))
    offCam = add(camrows)
    offHomer = add(sc.homer['blob'], 64)
    lrec = bytearray()
    dirs = [r for r in lights['raw'] if r[1] is not None]
    for n, L, colr in dirs[:2]: lrec += struct.pack('<3fI', *L, 0xff000000 | abgr_of(colr))
    amb = next(r[2] for r in lights['raw'] if r[1] is None)
    lrec += struct.pack('<If', 0xff000000 | abgr_of(amb), LSCALE)
    offLights = add(bytes(lrec))
    # перелёты камеры (хвост файла, FE1 v1 совместим): записи 16 Б, в последних 16 Б файла 'FLT1', смещение, число
    frec = b''.join(struct.pack('<BBHHHHHf', f['item'], 1 if f['back'] else 0, 0, f['go'][0], f['go'][1],
                                *(f['back'] if f['back'] else (0, 0)), f['deg']) for f in sc.flights)
    offFlt = add(frec) if frec else 0
    while len(blob) % 16: blob.append(0)
    # №404: сценки меню — таблица 'GAG1', u32 n, 8 Б пусто; запись 96 Б (порядок Gag0..Gag7 страницы 3dFEGags.pag):
    # имя 16, u32 offChr, sizeChr, f32 кадров, f32 fps, f32 place[12] (3 строки R|t), u32 flags (1 клип с флагом цикла,
    # 2 слой страницы видим по умолчанию, 4 сценка Гомера меню: модели нет, offChr = sizeChr = 0), u32 0, 8 Б пусто. Смещение таблицы — в 4-м слове хвоста FLT1 (прежний движок его не читает)
    gags = gag_chrs(sc.homer['place']); offGag = 0
    if gags:
        offs = [add(g['blob'], 64) if g['blob'] else 0 for g in gags]
        grec = struct.pack('<4sI8x', b'GAG1', len(gags)) + b''.join(
            struct.pack('<16sIIff12fII8x', g['name'].encode()[:16], o, len(g['blob']), g['frames'], g['fps'], *g['place'].flatten(),
                        (GAG_LOOP if g['loop'] else 0) | (GAG_PAGEVIS if g['pagevis'] else 0) | (GAG_ONHOMER if g['onhomer'] else 0), 0)
            for g, o in zip(gags, offs))
        offGag = add(grec)
        print('сценки меню: ' + ', '.join(f"{g['name']} {len(g['blob']) // 1024} КБ {int(g['frames'])} к, пропов {g['props']}, глаз {g['eyes']}"
                                          for g in gags))
    sc.gags = gags
    while len(blob) % 16: blob.append(0)
    if frec or offGag: blob.extend(struct.pack('<4sIII', b'FLT1', offFlt, len(sc.flights), offGag))
    hdr = struct.pack('<4s11I', b'FE1\0', 1, len(blob), len(names), offTex, len(groups), offPiece, len(sc.tracks), offTrack,
                      len(sc.bills), offBill, nfr)
    hdr += struct.pack('<IIfII12fIII', offCam, menu, fps, offHomer, len(sc.homer['blob']), *sc.homer['place'].flatten(), offLights, src_total, offNames)
    hdr += bytes(128 - len(hdr))
    blob[:128] = hdr
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Path(out_path).write_bytes(bytes(blob))
    sc.summary = dict(size=len(blob), tex=len(names), tex_bytes=tex_bytes, pieces=len(groups), tris_src=src_total, tris_out=ptris,
                      split_added=added, hidden=sc.hidden, verts=nverts, tracks=len(sc.tracks), bills=len(sc.bills), frames=nfr, fps=fps, menu=menu,
                      holds=holds, homer=dict((k, v) for k, v in sc.homer.items() if k not in ('blob', 'place')),
                      homer_bytes=len(sc.homer['blob']), stats=sc.stats, skipped=sc.skipped)
    return sc


def abgr_of(u):
    r, g, b = (u >> 16) & 255, (u >> 8) & 255, u & 255
    return r | (g << 8) | (b << 16)


# ---------------------------------------------------------------- гейт
def source_counts():
    """числа прямо из p3d (своим обходом, без экспортёра): треугольники мешей/скинов x число ссылок на них (рисуемые
    дерева сцены + пропы/скины композитов, на которые оно ссылается; без ссылок — 1 раз), текстуры, кадры анимаций"""
    tris, texn, frames = {}, set(), {}
    for fn in FILES:
        r = read(PU / f'{fn}.p3d'); refs = {}
        comps = {sname(c): c for c in r.children if c.id == 0x4512}
        for c in walk(r):
            if c.id != 0x120107: continue
            n, p = string(c.data); dn, _ = string(c.data, p); refs[dn] = refs.get(dn, 0) + 1
            if dn in comps:
                for s in walk(comps[dn]):
                    if s.id in (0x4515, 0x4516): k = sname(s); refs[k] = refs.get(k, 0) + 1
        tris[fn] = sum(ntris(c) * max(1, refs.get(sname(c), 0)) for c in r.children if c.id in (0x10000, 0x10001))
        texn |= {sname(c).lower() for c in r.children if c.id == 0x19000}
        for an in r.children:
            if an.id == 0x121000:
                n, p = string(an.data, 4); frames[f'{fn}:{n}'] = struct.unpack_from('<f', an.data, p + 4)[0]
    return tris, texn, frames


def read_fe(path):
    b = Path(path).read_bytes()
    h = struct.unpack_from('<4s11I', b, 0); h2 = struct.unpack_from('<IIfII12fIII', b, 48)
    magic, ver, size, ntex, otex, npc, opc, ntr, otr, nbl, obl, nfr = h
    ocam, menu, fps, oho, sho = h2[:5]; olig, src = h2[17], h2[18]
    pcs = [struct.unpack_from('<6H2I4f6fI', b, opc + 64 * i) for i in range(npc)]
    trs = [struct.unpack_from('<IIfII3I', b, otr + 32 * i) for i in range(ntr)]
    onm = h2[19]
    names = [b[onm + 32 * i:onm + 32 * i + 32].rstrip(b'\0').decode('latin1') for i in range(ntex)] if onm else []
    hb = b[oho:oho + sho]; hnt, hot = struct.unpack_from('<I', hb, 20)[0], struct.unpack_from('<I', hb, 40)[0]
    hnames = [hb[hot + 48 * i:hot + 48 * i + 24].rstrip(b'\0').decode('latin1') for i in range(hnt)]
    gags = []                                                    # №404: таблица GAG1 (4-е слово хвоста FLT1)
    if len(b) >= 16 and b[-16:-12] == b'FLT1':
        og, = struct.unpack_from('<I', b, len(b) - 4)
        if og and b[og:og + 4] == b'GAG1':
            n, = struct.unpack_from('<I', b, og + 4)
            for i in range(n):
                r = struct.unpack_from('<16sIIff12fII', b, og + 16 + 96 * i)
                gags.append(dict(name=r[0].rstrip(b'\0').decode(), off=r[1], size=r[2], frames=r[3], fps=r[4],
                                 place=np.array(r[5:17]).reshape(3, 4), flags=r[17]))
    return dict(b=b, magic=magic, size=size, ntex=ntex, npc=npc, ntr=ntr, nbl=nbl, nfr=nfr, menu=menu, fps=fps, pcs=pcs, trs=trs,
                homer=hb, src=src, names=names, hnames=hnames, gags=gags)


def homer_bones_check(fe):
    """кости с весом у каждой вершины Гомера в CHR1 файла против p3d, прочитанного НЕ через prim_groups: u32 MatrixList,
    индекс палитры веса k = (m >> (24 - 8k)) & 255 (donut SkinModel.cpp), веса WeightList 3 + остаток. Вершины
    сравниваются по позиции (мм, z отзеркален). -> (число вершин, где набор костей расходится, суставов с весом)"""
    import tempfile
    b = fe['b']; oho, sho = struct.unpack_from('<II', b, 60)
    fd_, tmp = tempfile.mkstemp(suffix='.chr'); os.close(fd_); Path(tmp).write_bytes(b[oho:oho + sho])
    try: m = ce.read_chr(Path(tmp))
    finally: os.unlink(tmp)
    got = {}
    for bt in m['batches']:
        for p, w in zip(bt['pos'], bt['w']):
            k = tuple(np.round(p * 1000).astype(int)); got.setdefault(k, set()).update(bt['bones'][s] for s in range(len(w)) if w[s] > .02)
    want = {}
    for sk in (c for c in read(PU / 'homer.p3d').children if c.id == 0x10001):
        for pg in sk.children:
            if pg.id != 0x10002: continue
            d = {c.id: c.data for c in pg.children}
            n, = struct.unpack_from('<I', d[0x10005]); P = np.frombuffer(d[0x10005], '<f4', n * 3, 4).reshape(-1, 3) * [1, 1, -1]
            ml = np.frombuffer(d[0x1000b], '<u4', n, 4).astype(np.int64); pal = np.frombuffer(d[0x1000d], '<u4', offset=4)
            w3 = np.frombuffer(d[0x1000c], '<f4', n * 3, 4).reshape(-1, 3) if 0x1000c in d else np.c_[np.ones(n), np.zeros((n, 2))]
            W = np.c_[w3, 1. - w3.sum(1)]
            for i in range(n):
                idx = [(ml[i] >> (24 - 8 * k)) & 255 for k in range(4)]
                k_ = tuple(np.round(P[i] * 1000).astype(int))
                want.setdefault(k_, set()).update(int(pal[min(idx[s], len(pal) - 1)]) for s in range(4) if W[i, s] > .02)
    bad = sum(1 for k, v in want.items() if got.get(k) != v)
    return bad, len(set().union(*got.values()))


def flights_check(fe):
    """таблица FLT1 файла: у каждого перелёта в центре кадра прилёта — предмет, который из главного вида закрыт
    подсветкой его пункта (ближайшая подсветка, <= FLIGHT_MAX_DEG); пункты не повторяются; NEW GAME и OPTIONS (с
    обратным) есть. -> (ошибки, строки отчёта)"""
    b = fe['b']; errs, rep = [], []
    if len(b) < 16 or b[-16:-12] != b'FLT1': return ['в fe.shr нет таблицы перелётов FLT1'], ['перелёты: нет']
    off, n = struct.unpack_from('<II', b, len(b) - 12)
    recs = [struct.unpack_from('<BBHHHHHf', b, off + 16 * i) for i in range(n)]
    ocam, menu = struct.unpack_from('<II', b, 48)
    C = np.frombuffer(b, '<f4', 12 * fe['nfr'], ocam).reshape(-1, 12).astype(np.float64)
    VT = np.dtype([('u', '<i2'), ('v', '<i2'), ('c', '<u4'), ('x', '<i2'), ('y', '<i2'), ('z', '<i2'), ('p', '<i2')])
    T = []
    for pc in fe['pcs']:
        if pc[3] != 0xffff: continue
        q = np.frombuffer(b, VT, pc[6], pc[7])
        T.append((np.c_[q['x'], q['y'], q['z']].astype(np.float64) / 32768. * pc[11] + np.array(pc[8:11])).reshape(-1, 3, 3))
    T = np.concatenate(T)
    nbl, obl = struct.unpack_from('<II', b, 36); G = {}
    for i in range(nbl):
        glow, = struct.unpack_from('<B', b, obl + 80 * i + 7)
        if glow != 0xff: G.setdefault(glow, []).append(np.array(struct.unpack_from('<12f', b, obl + 80 * i + 20)).reshape(3, 4)[:, 3])
    P0 = C[menu, :3]; unit = lambda v: v / np.linalg.norm(v)
    items = []
    for item, flags, _, g0, g1, b0, b1, deg in recs:
        p = ray_hit(T, C[g1, :3], unit(C[g1, 3:6]))
        if p is None: errs.append(f'перелёт {item}: в центре кадра прилёта {g1} пусто'); continue
        angs = sorted((math.degrees(math.acos(float(np.clip(unit(p - P0) @ unit(np.mean(G[g], 0) - P0), -1, 1)))), g) for g in G)
        home0 = bool(np.all(np.abs(C[g0, :9] - C[menu, :9]) < 2e-3)); home1 = not (flags & 1) or bool(np.all(np.abs(C[b1, :9] - C[menu, :9]) < 2e-3))
        rep.append(f"перелёт пункт {item} ({GLOWS[item]}): {g0}→{g1}" + (f", обратно {b0}→{b1}" if flags & 1 else ', без обратного')
                   + f"; в центре прилёта — под подсветкой {GLOWS[angs[0][1]]} ({angs[0][0]:.1f}°)")
        if angs[0][1] != item or angs[0][0] > FLIGHT_MAX_DEG: errs.append(f'перелёт пункта {item} ведёт к {GLOWS[angs[0][1]]}')
        if not home0 or not home1: errs.append(f'перелёт пункта {item} не из главного вида / не в него')
        if flags & 1 and b0 != g1: errs.append(f'перелёт пункта {item}: обратный не с кадра прилёта')
        items.append(item)
    if len(set(items)) != len(items): errs.append(f'пункты перелётов повторяются: {items}')
    if 4 not in items: errs.append('нет перелёта NEW GAME (4)')
    if not any(r[0] == 2 and r[1] & 1 for r in recs): errs.append('нет перелёта OPTIONS (2) с обратным')
    return errs, rep


def check(path):
    """гейт (а): 1) треугольники файла (исходные, до нарезки) + пропущенные с причиной = треугольникам p3d;
    нарезанные = куски; 2) кадры таймлайна и циклов = данным; 3) текстуры = использованным; 4) пересборка побайтно та же"""
    fe = read_fe(path); errs = []
    if fe['magic'] != b'FE1\0' or fe['size'] != len(fe['b']): errs.append('заголовок')
    src_tris, texn, frames = source_counts()
    got = sum(p[-1] for p in fe['pcs'])
    hb = fe['homer']; hnj, hnb = struct.unpack_from('<II', hb, 12)
    htv, = struct.unpack_from('<I', hb, 120)
    tmp = Path(str(path) + '.check')
    sc = build(tmp); s = sc.summary
    skipped = sum(n for _, n, _ in s['skipped'])
    total_src = sum(src_tris.values())
    hom = s['homer']['tris']
    lines = []
    lines.append(f"треугольники p3d {total_src} = в кусках {got} + Гомер {hom} + пропущено {skipped}")
    if got + hom + skipped != total_src: errs.append(f'треугольники: {got}+{hom}+{skipped} != {total_src}')
    if got != fe['src']: errs.append(f'srcTris заголовка {fe["src"]} != сумма кусков {got}')
    ntri_out = sum(p[6] for p in fe['pcs']) // 3
    lines.append(f"после нарезки у камеры {ntri_out} (+{ntri_out - got})")
    if fe['nfr'] != int(frames['camset:CAM_FE_Camera']): errs.append(f"кадры камеры {fe['nfr']} != {frames['camset:CAM_FE_Camera']}")
    want = {T_MAT: [], T_BILL: [], T_TEX: []}
    for t in fe['trs']: want.setdefault(t[0], []).append(t[1])
    loops = sorted(int(v) for k, v in frames.items() if not k.startswith('camset:'))
    lines.append(f"кадры: камера {fe['nfr']}, дорожки {sorted(t[1] for t in fe['trs'])}, циклы p3d {loops}")
    for t in fe['trs']:
        if t[1] not in [int(v) for v in frames.values()]: errs.append(f'дорожка {t[1]} кадров нет в данных')
    got_t = set(fe['names']) | set(fe['hnames'])
    lines.append(f"текстуры: в файле {fe['ntex']} + у Гомера {len(fe['hnames'])} = {len(got_t)} имён; в p3d {len(texn)} имён")
    if got_t != texn: errs.append(f'текстуры: нет {sorted(texn - got_t)[:4]}, лишние {sorted(got_t - texn)[:4]}')
    hj = sum(1 for c in walk(read(PU / 'homer.p3d')) if c.id == 0x4501)
    hoc, = struct.unpack_from('<I', hb, 44); hfr, = struct.unpack_from('<f', hb, hoc + 48)
    lines.append(f"Гомер: суставов {hnj} (в p3d {hj}), клип {hfr:g} кадров (в p3d {frames['homer:PTRN_Motion_Root']:g})")
    if hnj != hj or hfr != frames['homer:PTRN_Motion_Root']: errs.append('Гомер: суставы/кадры')
    # камера по кадрам = каналам CAM_FE_Camera в p3d (прямо из ключей, отзеркалено)
    typ, nfr, cfps, groups = anims(read(PU / 'camset.p3d'))['CAM_FE_Camera']; ch = groups['CAM_FE_Camera']
    ocam, = struct.unpack_from('<I', fe['b'], 48); worst = 0.
    for f in (0, fe['menu'], 230, 721, nfr - 1):
        got = np.frombuffer(fe['b'], '<f4', 12, ocam + 48 * f).astype(np.float64)
        want = np.r_[sample(*ch['TRAN'], f) * [1, 1, -1], (lambda v: v / np.linalg.norm(v))(sample(*ch['LOOK'], f)) * [1, 1, -1]]
        worst = max(worst, float(np.abs(got[:6] - want).max()))
    lines.append(f"камера: кадры 0/{fe['menu']}/230/721/{nfr - 1} = ключам p3d (откл. {worst:.1e})")
    if worst > 1e-4: errs.append(f'камера отличается от p3d ({worst:.1e})')
    # скрытые из кадра меню куски не меняют кадр: превью без них = превью со всеми (та же математика, что движок)
    im0 = preview(path, None, 4, None, False, 0., True).astype(int); im1 = preview(path, None, 4, None, False, 0., False).astype(int)
    diff = np.abs(im0 - im1).max(2); bad = int((diff > 8).sum())
    lines.append(f"кадр меню без скрытых = со всеми: отличающихся пикселей {bad} из {diff.size}")
    if bad > diff.size // 2000: errs.append(f'отсечение по видимости меняет кадр ({bad} пикс.)')
    # правка Роба 3: Гомер цельный — кости вершин CHR1 = кости по данным p3d, прочитанным своим разбором MatrixList
    # (u32 на вершину, индекс палитры веса k — байт (m >> (24 - 8k)) & 255, как donut SkinModel.cpp)
    bad, nj_used = homer_bones_check(fe)
    lines.append(f"Гомер: кости вершин = данным p3d (MatrixList, старший байт = вес 0): расхождений {bad}, суставов с весом {nj_used}")
    if bad: errs.append(f'Гомер разобран: у {bad} вершин кости не те, что в p3d')
    # правка Роба 2: порядок страницы 3dFE.pag — подсветка дивана до Гомера, Гомер её закрывает; видик и альбом видны
    st = {}; preview(path, None, 2, stats_out=st)
    none = preview(path, None, 99).astype(int)
    vis = {g: int((np.abs(preview(path, None, g).astype(int) - none).max(2) > 40).sum()) for g in (0, 1, 2)}
    lines.append(f"подсветка дивана поверх Гомера: {st['glow_on_homer']} пикс. (Гомер {st['homer_px']} пикс.); видно подсветок: "
                 f"видик {vis[0]}, альбом {vis[1]}, диван {vis[2]} пикс.")
    if st['homer_px'] < 500: errs.append(f"Гомера в кадре меню нет ({st['homer_px']} пикс.)")
    if st['glow_on_homer'] > 0: errs.append(f"подсветка дивана светится сквозь Гомера ({st['glow_on_homer']} пикс.)")
    if min(vis.values()) < 200: errs.append(f'подсветка не видна: {vis}')
    # правка Роба 1: перелёты — у каждого в центре кадра прилёта предмет под подсветкой своего пункта
    fl, rep = flights_check(fe)
    lines += rep
    errs += fl
    same = tmp.read_bytes() == fe['b']
    lines.append(f"пересборка побайтно {'та же' if same else 'ДРУГАЯ'} ({len(fe['b'])} Б)")
    if not same: errs.append('пересборка отличается')
    tmp.unlink()
    print(('OK' if not errs else 'FAIL ' + '; '.join(errs)) + f' · frontend3d_export --check {Path(path).name}')
    for l in lines: print('  ' + l)
    return 0 if not errs else 1

# ---------------------------------------------------------------- превью на процессоре (та же математика, что frontend3d.c)
def unswizzle(buf, rb, h):
    hp = (h + 7) & ~7
    a = np.frombuffer(buf, np.uint8, rb * hp).reshape(hp // 8, rb // 16, 8, 16).transpose(0, 2, 1, 3).reshape(hp, rb)
    return a[:h]


def homer_posed(fe, t=0., clip=0):
    """Гомер меню из CHR1 файла, поза клипа на t с (та же математика, что skin.c/GE: char_export.read_chr/palette/skin)
    -> [(P мир (n,3), N мир, UV, текстура RGBA 0..1)] треугольниками"""
    import tempfile
    b = fe['b']; oho, sho = struct.unpack_from('<II', b, 60)
    place = np.array(struct.unpack_from('<12f', b, 68)).reshape(3, 4)
    fd_, tmp = tempfile.mkstemp(suffix='.chr'); os.close(fd_); Path(tmp).write_bytes(b[oho:oho + sho])
    try: m = ce.read_chr(Path(tmp))
    finally: os.unlink(tmp)
    loc = ce.sample_local(m, clip, t) if m['clips'] else [(j['q'], j['t']) for j in m['joints']]
    pal, _ = ce.palette(m, loc)
    out = []
    for P, N, UV, ti, flags in ce.skin(m, pal):
        n = len(P) // 3 * 3
        out.append((P[:n] @ place[:, :3].T + place[:, 3], N[:n] @ place[:, :3].T, UV[:n], m['tex'][ti]['rgba'].astype(np.float64) / 255.))
    return out


def gag_posed(fe, i, t=0.):
    """№404: сценка i таблицы GAG1, поза клипа на t с; текстура глаз (батчи с флагом 1) — ключ дорожки TEX_* на кадре
    t*fps, как frontend3d.c -> [(P мир, N мир, UV, текстура RGBA 0..1)]"""
    import tempfile
    g = fe['gags'][i]; b = fe['b']
    fd_, tmp = tempfile.mkstemp(suffix='.chr'); os.close(fd_); Path(tmp).write_bytes(b[g['off']:g['off'] + g['size']])
    try: m = ce.read_chr(Path(tmp))
    finally: os.unlink(tmp)
    loc = ce.sample_local(m, 0, t)                               # корень без переноса — дорисуем перенос корня (rootJoint нет)
    cl = m['clips'][0]; tr = cl['tracks'][0]
    if tr['tf'] is not None and len(tr['tf']):
        a, b2, u = ce._key(tr['tf'], tr['tv'], min(max(t * cl['fps'], 0.), cl['frames'] - 1)); loc[0] = (loc[0][0], a + (b2 - a) * u)
    pal, _ = ce.palette(m, loc)
    eye = None
    if m['blink']:
        f = t * g['fps']; eye = m['blink'][0][1]
        for fr, tx in m['blink']:
            if fr <= f: eye = tx
    place = g['place']; out = []
    for P, N, UV, ti, flags in ce.skin(m, pal):
        if eye is not None and flags & 1: ti = eye
        n = len(P) // 3 * 3
        out.append((P[:n] @ place[:, :3].T + place[:, 3], N[:n] @ place[:, :3].T, UV[:n], m['tex'][ti]['rgba'].astype(np.float64) / 255.))
    return out


def gag_replaces_homer(fe, i):
    """сценка Гомера меню (флаг GAG_ONHOMER): её движение играет Гомер меню (клип 1 CHR1), своей модели нет"""
    return bool(fe['gags'][i]['flags'] & GAG_ONHOMER)


def preview(path, out, sel=4, frame=None, flipv=False, t=0., hidden=False, homer=True, stats_out=None, gag=None, gag_t=0.):
    """кадр 480x272 из fe.shr в порядке движка (viewer/frontend3d.c feDraw): непрозрачные куски; смешиваемое и
    подсветки до Гомера (F_PREHOMER) от дальних к ближним; Гомер (CHR1, та же поза и скиннинг); остальное смешиваемое.
    Вне главного вида подсветки не рисуются, скрытые из кадра меню — рисуются. stats_out (dict): треугольники,
    glow_px — пикселей, где подсветка пункта прошла тест глубины, glow_on_homer — из них поверх Гомера"""
    fe = read_fe(path); b = fe['b']
    h = struct.unpack_from('<4s11I', b, 0); ntex, otex, npc, opc, ntr, otr, nbl, obl, nfr = h[3:12]
    ocam, menu = struct.unpack_from('<II', b, 48)
    f = menu if frame is None else frame
    cm = np.frombuffer(b, '<f4', 12, ocam + 48 * menu).astype(np.float64)
    home = bool(np.all(np.abs(np.frombuffer(b, '<f4', 12, ocam + 48 * f)[:9] - cm[:9]) <= 2e-3))
    if not home: sel = -1
    texs = []
    for i in range(ntex):
        w, hh, nlev = struct.unpack_from('<HHB', b, otex + 64 * i); oc, = struct.unpack_from('<I', b, otex + 64 * i + 8)
        ol, = struct.unpack_from('<I', b, otex + 64 * i + 12); bw, = struct.unpack_from('<H', b, otex + 64 * i + 44)
        pal = np.frombuffer(b, np.uint8, 1024, oc).reshape(256, 4)
        texs.append(pal[unswizzle(b[ol:], bw, hh)[:, :w]].astype(np.float64) / 255.)
    trs = fe['trs']

    def track(ti, fr_):
        typ, n, fps, fl, off = trs[ti][:5]
        k = int(min(fr_, n - 1)) if fl & TR_TIMELINE else int(fr_) % n
        return typ, off, k
    cam = np.frombuffer(b, '<f4', 12, ocam + 48 * f).astype(np.float64)
    pos, look, up, fovx, nc, fc = cam[0:3], cam[3:6], cam[6:9], cam[9], cam[10], cam[11]
    W_, H_ = 480, 272
    tv = math.tan(fovx / 2) / (4. / 3.); th = tv * W_ / H_
    fwd = look / np.linalg.norm(look); right = np.cross(fwd, up); right /= np.linalg.norm(right); upv = np.cross(right, fwd)
    img = np.zeros((H_, W_, 3)); zb = np.zeros((H_, W_)); own = np.zeros((H_, W_), bool); ogag = np.zeros((H_, W_), bool)
    stats = dict(tris=0, glow_px=0, glow_on_homer=0, homer_px=0)

    def raster(P, UV, C, tex, fl, tag=None):
        rel = P - pos; z = rel @ fwd
        if z.min() <= nc: return
        sx = W_ / 2 + (rel @ right) / z / th * W_ / 2; sy = H_ / 2 - (rel @ upv) / z / tv * H_ / 2
        x0, x1 = max(0, int(sx.min())), min(W_ - 1, int(math.ceil(sx.max()))); y0, y1 = max(0, int(sy.min())), min(H_ - 1, int(math.ceil(sy.max())))
        if x0 > x1 or y0 > y1: return
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
        d = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0])
        if abs(d) < 1e-9: return
        l1 = ((gx - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (gy - sy[0])) / d
        l2 = ((sx[1] - sx[0]) * (gy - sy[0]) - (gx - sx[0]) * (sy[1] - sy[0])) / d
        l0 = 1 - l1 - l2; m = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
        if not m.any(): return
        iw = np.stack([l0 / z[0], l1 / z[1], l2 / z[2]], -1); zz = iw.sum(-1)
        ww = iw / zz[..., None]
        sub = zb[y0:y1 + 1, x0:x1 + 1]
        m &= zz >= sub
        if not m.any(): return
        uv = ww @ UV; cc = ww @ C
        if isinstance(tex, np.ndarray) or tex != 0xffff:
            T = tex if isinstance(tex, np.ndarray) else texs[tex]; th_, tw_ = T.shape[:2]
            v = (1 - uv[..., 1]) if flipv else uv[..., 1]
            tx = (np.floor(uv[..., 0] * tw_).astype(int)) % tw_; ty = (np.floor(v * th_).astype(int)) % th_
            src = T[ty, tx] * cc
        else: src = cc
        if fl & F_ATEST: m &= src[..., 3] > 64 / 255.
        dst = img[y0:y1 + 1, x0:x1 + 1]
        if fl & F_ADD: res = np.minimum(dst + src[..., :3], 1)
        elif fl & F_BLEND: a = src[..., 3:4]; res = src[..., :3] * a + dst * (1 - a)
        else: res = src[..., :3]
        dst[m] = res[m]
        if not (fl & F_NOZW): sub[m] = zz[m]
        ow = own[y0:y1 + 1, x0:x1 + 1]; og = ogag[y0:y1 + 1, x0:x1 + 1]
        if not (fl & (F_BLEND | F_ADD)): ow[m] = tag == 'homer'; og[m] = tag == 'gag'
        elif tag == 'glow': stats['glow_px'] += int(m.sum()); stats['glow_on_homer'] += int((m & ow).sum())
        stats['tris'] += 1

    order = []
    VT = np.dtype([('u', '<i2'), ('v', '<i2'), ('c', '<u4'), ('x', '<i2'), ('y', '<i2'), ('z', '<i2'), ('p', '<i2')])
    for pc in fe['pcs']:
        tex, ttr, fl, node, vis, _, nv, ov = pc[:8]; origin = np.array(pc[8:11]); scale = pc[11]
        if vis != 0xffff or (fl & F_MENUHID and not hidden and home): continue
        q = np.frombuffer(b, VT, nv, ov)
        P = np.c_[q['x'], q['y'], q['z']].astype(np.float64) / 32768. * scale + origin
        if node != 0xffff:
            typ, off, k = track(node, f if trs[node][3] & TR_TIMELINE else t * trs[node][2])
            M = np.frombuffer(b, '<f4', 12, off + 48 * k).reshape(3, 4).astype(np.float64); P = P @ M[:, :3].T + M[:, 3]
        if ttr != 0xffff:
            typ, off, k = track(ttr, t * trs[ttr][2]); tex, = struct.unpack_from('<H', b, off + 2 * k)
        UV = np.c_[q['u'], q['v']].astype(np.float64) / 256.
        c = q['c'].astype(np.uint64); C = np.c_[c & 255, (c >> 8) & 255, (c >> 16) & 255, (c >> 24) & 255].astype(np.float64) / 255.
        dist = (P.mean(0) - pos) @ fwd                             # как движок: глубина центра по оси камеры
        grp = (1 if fl & F_PREHOMER else 3) if fl & (F_BLEND | F_ADD) else 0
        order.append((grp, -dist if grp else 0, P, UV, C, tex, fl, None))
    for i in range(nbl):
        tr_, tex, fl, mode, glow = struct.unpack_from('<3H2B', b, obl + 80 * i)
        w, hh, abgr = struct.unpack_from('<2fI', b, obl + 80 * i + 8); M = np.array(struct.unpack_from('<12f', b, obl + 80 * i + 20)).reshape(3, 4)
        if glow != 0xff and glow != sel: continue
        colr = np.array([(abgr >> s_) & 255 for s_ in (0, 8, 16, 24)], np.float64) / 255.
        if tr_ != 0xffff:
            typ, off, k = track(tr_, t * trs[tr_][2]); vals = struct.unpack_from('<6f', b, off + 24 * k); w, hh = vals[:2]; colr = np.array(vals[2:])
        c0 = M[:, 3].copy()
        if glow != 0xff: c0 += (pos - c0) / np.linalg.norm(pos - c0) * .05   # FE_GLOW_LIFT движка (5 см к камере)
        if mode == 0: ax, ay = M[:, 0], M[:, 1]
        elif mode == 1:
            ay = M[:, 1] / np.linalg.norm(M[:, 1]); tc = pos - c0; ax = np.cross(ay, tc); ax /= np.linalg.norm(ax)
        else: ax, ay = right, upv
        P = np.array([c0 - ax * w / 2 + ay * hh / 2, c0 + ax * w / 2 + ay * hh / 2, c0 + ax * w / 2 - ay * hh / 2, c0 - ax * w / 2 - ay * hh / 2])
        UV = np.array([[0, 0], [1, 0], [1, 1], [0, 1]], np.float64)
        C = np.tile(colr, (4, 1))
        dist = (c0 - pos) @ fwd
        for tri in ([0, 1, 2], [0, 2, 3]):
            order.append((1 if fl & F_PREHOMER else 3, -dist, P[tri], UV[tri], C[tri], tex, fl, 'glow' if glow != 0xff else None))
    hclip, ht = 0, t
    if gag is not None and gag_replaces_homer(fe, gag): hclip, ht, gag, homer = 1, gag_t, None, True   # Гомер играет клип сценки
    if homer or gag is not None:                                    # Гомер: непрозрачный, свет GE (ключевой, заполняющий, фон)
        olig, = struct.unpack_from('<I', b, 116)
        key = np.array(struct.unpack_from('<3f', b, olig)); kc, = struct.unpack_from('<I', b, olig + 12)
        fil = np.array(struct.unpack_from('<3f', b, olig + 16)); fc_, = struct.unpack_from('<I', b, olig + 28)
        ac, = struct.unpack_from('<I', b, olig + 32)
        rgb = lambda u: np.array([u & 255, (u >> 8) & 255, (u >> 16) & 255], np.float64) / 255.
        figs = ([(P, N, UV, T, 'homer') for P, N, UV, T in homer_posed(fe, ht, hclip)] if homer else []) + \
               ([(P, N, UV, T, 'gag') for P, N, UV, T in gag_posed(fe, gag, gag_t)] if gag is not None else [])
        for P, N, UV, T, tag in figs:
            Nn = N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
            L = rgb(ac)[None] + np.maximum(0, Nn @ key)[:, None] * rgb(kc) + np.maximum(0, Nn @ fil)[:, None] * rgb(fc_)
            C = np.c_[np.minimum(L, 1), np.ones(len(P))]
            order.append((2 if tag == 'homer' else 0, 0, P, UV, C, T, 0, tag))   # сценка — до комнаты (3dFEGags.pag перед 3dFE.pag)
    order.sort(key=lambda o: (o[0], o[1]))
    for o in order:
        P, UV, C, tex, fl, tag = o[2:]
        for k in range(0, len(P), 3): raster(P[k:k + 3], UV[k:k + 3], C[k:k + 3], tex, fl, tag)
    stats['homer_px'] = int(own.sum()); stats['gag_px'] = int(ogag.sum())
    stats['homer_mask'] = own.copy(); stats['gag_mask'] = ogag.copy()
    stats['gag_box'] = (lambda ys, xs: (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())) if len(xs) else None)(*np.nonzero(ogag))
    if stats_out is not None: stats_out.update(stats)
    res = (np.clip(img, 0, 1) * 255).astype(np.uint8)
    if out: Image.fromarray(res).save(out); print(f'превью {out}: кадр {f}, пункт {sel}, треугольников {stats["tris"]}')
    return res



def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=str(OUT)); ap.add_argument('--check')
    ap.add_argument('--preview', nargs=2, metavar=('FE', 'PNG')); ap.add_argument('--sel', type=int, default=4)
    ap.add_argument('--frame', type=int); ap.add_argument('--flipv', action='store_true'); ap.add_argument('--t', type=float, default=0.)
    ap.add_argument('--hidden', action='store_true', help='рисовать и скрытые из кадра меню (сверка)')
    a = ap.parse_args()
    if a.check: sys.exit(check(a.check))
    if a.preview: preview(a.preview[0], a.preview[1], a.sel, a.frame, a.flipv, a.t, a.hidden); return
    sc = build(a.out); s = sc.summary
    print(f"fe.shr {s['size'] / 1024:.0f} КБ: текстур {s['tex']} ({s['tex_bytes'] / 1024:.0f} КБ), кусков {s['pieces']}, "
          f"треугольников {s['tris_src']} (+{s['split_added']} нарезка у камеры = {s['tris_out']}, из них не видно из кадра меню {s['hidden']}), дорожек {s['tracks']}, "
          f"билбордов {s['bills']}, таймлайн {s['frames']} кадров @ {s['fps']:g}, кадр меню {s['menu']}")
    print(f"  Гомер: {s['homer']} ({s['homer_bytes'] / 1024:.0f} КБ)")
    print(f"  по файлам: {s['stats']}")
    print(f"  удержания камеры в главном виде: {s['holds']}")
    print(f"  порядок страницы 3dFE.pag: {' '.join(sc.page)} (до Гомера: F_PREHOMER{' ВЫКЛ' if GLOW_ORDER_OLD else ''})")
    for f in sc.flights:
        print(f"  перелёт пункт {f['item']} ({GLOWS[f['item']]}): кадры {f['go'][0]}→{f['go'][1]}"
              + (f", обратно {f['back'][0]}→{f['back'][1]}" if f['back'] else ', обратного в данных нет') + f", цель {f['deg']:.1f}° от подсветки")
    for w, n, why in s['skipped']: print(f'  не перенесено: {w} ({n} треуг.): {why}')


if __name__ == '__main__':
    main()
