#!/usr/bin/env python3
"""Правка Роба 106 (повтор 01.10: «швы, ещё больше полосы на дороге везде»): швы кадра PSP — замер и сторож без эмулятора.

clearchk на Маке: город уровня рисуется из тех же точек обзора, как его рисует движок (viewer/main.c: кусок ближе LOD_DIST
50 м — ближняя версия, дальше — дальняя, до DIST_LONG 250 м; камера за машиной, FOVY 62°, 16:9, ближняя плоскость 1.5 м),
а растеризация — как у GE: матрица модели куска 24-бит float (sceGuSetMatrix шлёт старшие 24 бита float32 — GE24),
вершины центр + int16·sc/32768, экран в 12.4 (1/16 пикселя), центры пикселей, правило «верх-лево»
(tools/seams_raster.c), глубина с перспективой. Треугольник с вершиной вне гвард-бэнда GE выбрасывается целиком.
PPSSPP рисует с увеличением и в плавающей точке — щелей в доли пикселя не показывает (tools/seams_emu.py, 30.09: «НЕ ГЕЙТ»).

Шов кадра (пиксели PSP 480×272, по каждому кадру):
  дыры    — пиксель без покрытия непрозрачным городом, по строке или столбцу не шире THIN и с обеих сторон закрыт:
            щель между треугольниками, сквозь неё на PSP — небо (оно рисуется последним в пустую глубину);
  сквозь  — покрытый пиксель, глубина которого с обеих сторон намного ближе: щель с дальней геометрией за ней;
  на земле — из них те, что с обеих сторон — земля (полоса на дороге, а не просвет между ветками);
  полосы глубины (z-бой) — 16-бит глубина GE (DepthRange(65535, 0), GEQUAL, сдвиг слоёв): победил треугольник дальше
            истинно ближнего, или победитель зависит от порядка отрисовки при разных текстурах.
Точки обзора: 12 видов tools/seams_scenario.txt (дальние виды смоука) + 10 точек дорог build/routes.txt (старты разгона
у трамплинов по всему городу); у каждой 3 кадра (машина сдвинута на 0, 0.37, 0.74 м вперёд) — шов «рябит», один кадр — лотерея.

  python tools/seams_raster.py LEVEL.shr [--png DIR]          -> таблица швов по видам и итог
  python tools/seams_raster.py LEVEL.shr --ref REF.shr        -> то же против эталона (тот же замер эталонного файла)
  python tools/seams_raster.py LEVEL.shr --gate [--ref-tsv tools/seams_ref.tsv] [--tol 0.10]
         СТОРОЖ (tools/ship.sh): швы, швы на земле и полосы глубины на земле — каждое не больше эталона × (1 + tol) + 20,
         иначе FAIL, код 1. Эталон — числа принятой сборки в tools/seams_ref.tsv (сам уровень в репозиторий не кладём)
  python tools/seams_raster.py REF.shr --write-ref [--ref-tsv …] -> записать эталон
  python tools/seams_raster.py --plant SRC.shr DST.shr         -> заведомо плохой вход: копия, у которой треугольники
         города ужаты к своему центру на 3 % (щели по всем стыкам) — сторож обязан упасть
  python tools/seams_raster.py --zfight A.shr [B.shr …]        -> только полосы глубины по видам"""
import argparse, ctypes, math, os, struct, subprocess, sys
import shutil
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
W, H = 480, 272
FOVY, ASPECT, NEAR, LOD, DRAW = 62.0, 16.0 / 9.0, 1.5, 50.0, 250.0
CAM_BACK, CAM_UP, CAM_LOOK = 5.5, 2.5, 1.4   # глаз за машиной (FollowCameraData ~ 5.5 м назад, 2.5 м над низом колёс)
THIN = 2
JIT = (0.0, 0.37, 0.74)
ROUTE_PTS = ('jumpzone3', 'jumpzone1', 'glass190', 'jumpzone16', 'jumpzone19', 'glass183', 'jumpzone6', 'jumpzone10',
             'jumpzone9', 'glass186')
REF_TSV = ROOT / 'tools/seams_ref.tsv'
GATE_ABS = 20


def lib():
    src = ROOT / 'tools/seams_raster.c'; out = ROOT / ('build/tmp_conv/seams_raster.dylib' if sys.platform == 'darwin' else 'build/tmp_conv/seams_raster.so')   # Linux/WSL: .so
    out.parent.mkdir(parents=True, exist_ok=True)
    if not out.exists() or out.stat().st_mtime < src.stat().st_mtime:
        cc = os.environ.get('CC') or ('clang' if sys.platform == 'darwin' else (shutil.which('cc') or shutil.which('gcc') or 'cc'))   # на Linux clang обычно нет
        subprocess.run([cc, '-O2', '-shared', '-fPIC', '-o', str(out), str(src), '-lm'], check=True)
    L = ctypes.CDLL(str(out))
    L.raster.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                         ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
    L.raster16.argtypes = [ctypes.c_int] + [ctypes.c_void_p] * 4 + [ctypes.c_int, ctypes.c_int] + [ctypes.c_void_p] * 3
    return L


GE24 = True   # матрица модели в GE — 24-бит float (как на PSP); False — точный float32 (как было в замере до 01.10)


def f24(a):
    """float32 -> то, что видит GE: sceGuSetMatrix шлёт (биты float32) >> 8 — мантисса 15 бит, усечение к нулю"""
    a = np.asarray(a, np.float32)
    return (a.view(np.uint32) & np.uint32(0xFFFFFF00)).view(np.float32)


def load(path):
    """куски города: [(bb lo, bb hi, флаги, ближние (N,3,3) float32, дальние (M,3,3) float32)] — мир как у GE (float32)"""
    b = Path(path).read_bytes(); assert b[:4] == b'SHR7', f'{path}: не SHR7'
    ntex, npc = struct.unpack_from('<II', b, 4); ptab, = struct.unpack_from('<I', b, 76); ncell, ctab = struct.unpack_from('<II', b, 80)
    cells = [struct.unpack_from('<6fIIIII', b, ctab + 48 * i) for i in range(ncell)]
    out = []
    for i in range(npc):
        r = struct.unpack_from('<6fiIIII3ffIII2I', b, ptab + 80 * i)
        fl, nv, ni, own = r[7:11]; o = np.array(r[11:14], np.float32); sc = r[14]; off, nf, ci = r[15:18]
        if own != 0 or ni == 0 or not fl & 16 or fl & 4: continue
        vs = 12 if fl & 32 else 16; base = cells[ci][9] + off
        raw = np.frombuffer(b, np.uint8, (ni + nf) * vs, base).reshape(-1, vs)
        xyz = (raw[:, 6:12] if vs == 12 else raw[:, 8:14]).copy().view('<i2').reshape(-1, 3).astype(np.float32)
        if GE24: o = f24(o)   # GE берёт матрицу модели 24-бит float (sceGuSetMatrix: старшие 24 бита float32) — сдвиг куска усечён
        w = (o[None] + xyz * f24(np.float32(sc) / np.float32(32768.0))).astype(np.float32)
        out.append((np.array(r[0:3]), np.array(r[3:6]), fl, w[:ni].reshape(-1, 3, 3), w[ni:].reshape(-1, 3, 3), r[6]))
    return out


def ground_y(pieces, x, z):
    """верх непрозрачной земли под точкой (наклон до 60°) по ближним версиям"""
    best = None
    for lo, hi, fl, Pn, _, _ in pieces:
        if fl & 3 or not (lo[0] - .1 <= x <= hi[0] + .1 and lo[2] - .1 <= z <= hi[2] + .1): continue
        A, B, C = Pn[:, 0].astype(np.float64), Pn[:, 1].astype(np.float64), Pn[:, 2].astype(np.float64)
        d = (B[:, 2] - C[:, 2]) * (A[:, 0] - C[:, 0]) + (C[:, 0] - B[:, 0]) * (A[:, 2] - C[:, 2])
        ok = np.abs(d) > 1e-9; d[~ok] = 1
        l1 = ((B[:, 2] - C[:, 2]) * (x - C[:, 0]) + (C[:, 0] - B[:, 0]) * (z - C[:, 2])) / d
        l2 = ((C[:, 2] - A[:, 2]) * (x - C[:, 0]) + (A[:, 0] - C[:, 0]) * (z - C[:, 2])) / d
        l3 = 1 - l1 - l2
        n = np.cross(B - A, C - A); ny = np.abs(n[:, 1]) / np.maximum(np.linalg.norm(n, axis=1), 1e-12)
        m = ok & (l1 >= 0) & (l2 >= 0) & (l3 >= 0) & (ny > .5)
        if m.any():
            y = (l1 * A[:, 1] + l2 * B[:, 1] + l3 * C[:, 1])[m].max()
            best = y if best is None else max(best, y)
    return best


def views(pieces):
    """[(имя, глаз, точка взгляда)] — 12 видов seams_scenario + 10 точек дорог routes.txt, по 3 кадра"""
    pts = []
    cur = None
    for ln in (ROOT / 'tools/seams_scenario.txt').read_text().splitlines():
        s = ln.split()
        if not s or s[0].startswith('#'): continue
        if s[0] == 'scene': cur = s[1]
        if s[0] == 'goto': x, z, h = map(float, s[1:4]); pts.append((cur, x, None, z, h))
    rt = ROOT / 'build/routes.txt'
    # №1162: build/routes.txt делает обход ботом (build/sandbox), его у собирающего по README нет, и сторож швов падал «виды замера не совпали с эталоном (12 против 22)».
    # Копия (2,5 КБ, координаты стартов разгона у трамплинов L1, не данные диска) лежит в репозитории. SEAMS_ROUTES1162_OLD=1 — как было
    if not rt.exists() and not os.environ.get('SEAMS_ROUTES1162_OLD'): rt = ROOT / 'tools/seams_routes.txt'
    if rt.exists():
        R = {s[0]: s for s in (ln.split() for ln in rt.read_text().splitlines()) if s and not s[0].startswith('#')}
        for k in ROUTE_PTS:
            if k in R: s = R[k]; pts.append((k, float(s[1]), float(s[2]), float(s[3]), float(s[4])))
    out = []
    for name, x, y, z, h in pts:
        gy = ground_y(pieces, x, z)
        y = gy if gy is not None else (y if y is not None else 0.0)
        f = np.array([math.sin(h), 0, math.cos(h)])
        for j in JIT:
            car = np.array([x, y, z]) + f * j
            out.append((name, car - f * CAM_BACK + [0, CAM_UP, 0], car + [0, CAM_LOOK, 0]))
    return out


def vp_matrix(eye, at):
    """вид + перспектива как sceGumLookAt / sceGumPerspective (float32): мир -> клип (x, y, w) и глубина вдоль взгляда"""
    f = np.asarray(at, np.float64) - eye; f /= np.linalg.norm(f)
    s = np.cross(f, [0, 1, 0]); s /= np.linalg.norm(s); u = np.cross(s, f)
    V = np.eye(4); V[0, :3], V[1, :3], V[2, :3] = s, u, -f; V[:3, 3] = -V[:3, :3] @ eye
    ct = 1 / math.tan(math.radians(FOVY) / 2)
    P = np.zeros((4, 4)); P[0, 0] = ct / ASPECT; P[1, 1] = ct; P[3, 2] = -1
    P[2, 2] = -(DRAW + 10 + NEAR) / (DRAW + 10 - NEAR); P[2, 3] = -2 * (DRAW + 10) * NEAR / (DRAW + 10 - NEAR)
    return (P @ V).astype(np.float32)


def frame(L, pieces, eye, at):
    """кадр: покрытие (H, W) bool, глубина (H, W) float, земля ли ближайшее (H, W) bool"""
    M = vp_matrix(eye, at)
    tri = []
    for lo, hi, fl, Pn, Pf, _ in pieces:
        if fl & 3: continue   # альфа-тест и полупрозрачное не закрывают фон сплошь — в покрытие не идут
        c = np.clip(eye, lo, hi); d2 = float(((c - eye) ** 2).sum())
        if d2 > DRAW * DRAW: continue
        P = Pf if d2 > LOD * LOD and not fl & 2048 else Pn   # флаг 2048 — ближняя на любой дальности (viewer/main.c drawPiece, правки 224/237/251)
        if len(P): tri.append(P)
    if not tri: return np.zeros((H, W), bool), np.zeros((H, W), np.float32), np.zeros((H, W), bool)
    T = np.concatenate(tri); P = T.reshape(-1, 3)
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]).astype(np.float64)
    gnd = (np.abs(n[:, 1]) > .5 * np.maximum(np.linalg.norm(n, axis=1), 1e-12)).astype(np.uint8)
    w = (P @ M[3, :3] + M[3, 3]).reshape(-1, 3)
    cut = (w < NEAR).any(1) & ~(w < NEAR).all(1)   # GU_CLIP_PLANES: GE режет треугольник по ближней плоскости (psp_tricks.md)
    if cut.any():
        extra, eg = [], []
        for t in np.nonzero(cut)[0]:
            poly, pw = [], []
            for i in range(3):
                a, b, wa, wb = T[t, i], T[t, (i + 1) % 3], w[t, i], w[t, (i + 1) % 3]
                if wa >= NEAR: poly.append(a)
                if (wa >= NEAR) != (wb >= NEAR): poly.append(a + (b - a) * ((NEAR - wa) / (wb - wa)))
            for j in range(1, len(poly) - 1): extra.append((poly[0], poly[j], poly[j + 1])); eg.append(gnd[t])
        keep = (w >= NEAR).all(1)
        T = np.concatenate([T[keep], np.asarray(extra, np.float32).reshape(-1, 3, 3)])
        gnd = np.concatenate([gnd[keep], np.asarray(eg, np.uint8)])
        P = T.reshape(-1, 3)
    X = (P @ M[:3, :3].T + M[:3, 3]).astype(np.float32); w = (P @ M[3, :3] + M[3, 3]).astype(np.float32)
    X = X.reshape(-1, 3, 3); w = w.reshape(-1, 3)
    ok = (w > NEAR * .999).all(1)
    sx = 2048 + X[..., 0] / np.where(w > 0, w, 1) * (W / 2); sy = 2048 - X[..., 1] / np.where(w > 0, w, 1) * (H / 2)
    ok &= ((sx >= 0) & (sx < 4096) & (sy >= 0) & (sy < 4096)).all(1)   # гвард-бэнд GE: вершина вне — треугольник выброшен
    x16 = np.round(sx[ok] * 16).astype(np.int64) - (2048 - W // 2) * 16
    y16 = np.round(sy[ok] * 16).astype(np.int64) - (2048 - H // 2) * 16
    vis = ~(((x16 < 0).all(1)) | ((x16 >= W * 16).all(1)) | ((y16 < 0).all(1)) | ((y16 >= H * 16).all(1)))
    xy = np.ascontiguousarray(np.stack([x16[vis], y16[vis]], 2).reshape(-1), np.int32)
    z = np.ascontiguousarray(w[ok][vis].reshape(-1), np.float32); g = np.ascontiguousarray(gnd[ok][vis])
    cov = np.zeros(W * H, np.uint8); dep = np.zeros(W * H, np.float32); gb = np.zeros(W * H, np.uint8)
    L.raster(int(len(z) // 3), xy.ctypes.data, z.ctypes.data, g.ctypes.data, W, H, cov.ctypes.data, dep.ctypes.data, gb.ctypes.data)
    return cov.reshape(H, W).astype(bool), dep.reshape(H, W), gb.reshape(H, W).astype(bool)


LAYER_BIAS = 6   # viewer/main.c: сдвиг глубины слоя (флаг 64) в единицах 16-бит глубины, этажи 256 (+1), 1024 (+2)


def frame16(L, pieces, eye, at):
    """кадр 16-бит глубины GE (DepthRange(65535, 0), GEQUAL, слои со сдвигом) в порядке кусков и в обратном ->
    (покрытие, истинная глубина, победитель по 16 бит в прямом порядке: глубина и текстура, то же в обратном, земля)"""
    M = vp_matrix(eye, at)
    tri, zb_, tx_ = [], [], []
    for lo, hi, fl, Pn, Pf, tex in pieces:
        if fl & 3: continue
        c = np.clip(eye, lo, hi); d2 = float(((c - eye) ** 2).sum())
        if d2 > DRAW * DRAW: continue
        P = Pf if d2 > LOD * LOD and not fl & 2048 else Pn   # флаг 2048 — ближняя на любой дальности (viewer/main.c drawPiece, правки 224/237/251)
        if not len(P): continue
        b = LAYER_BIAS * (1 + bool(fl & 256) + 2 * bool(fl & 1024)) if fl & 64 else 0
        tri.append(P); zb_.append(np.full(len(P), b, np.float32)); tx_.append(np.full(len(P), tex, np.int32))
    if not tri: return None
    T = np.concatenate(tri).astype(np.float32); B = np.concatenate(zb_); TX = np.concatenate(tx_)
    P = T.reshape(-1, 3)
    X = (P @ M[:3, :3].T + M[:3, 3]).astype(np.float32).reshape(-1, 3, 3); w = (P @ M[3, :3] + M[3, 3]).astype(np.float32).reshape(-1, 3)
    ok = (w > NEAR).all(1)   # треугольники у ближней плоскости — без них (их режет GE; для z-боя вдали не важны)
    sx = 2048 + X[..., 0] / np.where(w > 0, w, 1) * (W / 2); sy = 2048 - X[..., 1] / np.where(w > 0, w, 1) * (H / 2)
    ok &= ((sx >= 0) & (sx < 4096) & (sy >= 0) & (sy < 4096)).all(1)
    zc = (P @ M[2, :3] + M[2, 3]).astype(np.float32).reshape(-1, 3)
    z16 = 32767.5 * (1 - zc / np.where(w > 0, w, 1)) + B[:, None]   # ближняя плоскость -> 65535, дальняя -> 0, + сдвиг слоя
    x16 = np.round(sx[ok] * 16).astype(np.int64) - (2048 - W // 2) * 16
    y16 = np.round(sy[ok] * 16).astype(np.int64) - (2048 - H // 2) * 16
    vis = ~(((x16 < 0).all(1)) | ((x16 >= W * 16).all(1)) | ((y16 < 0).all(1)) | ((y16 >= H * 16).all(1)))
    xy = np.ascontiguousarray(np.stack([x16[vis], y16[vis]], 2).reshape(-1), np.int32)
    zf = np.ascontiguousarray(w[ok][vis].reshape(-1), np.float32); zz = np.ascontiguousarray(z16[ok][vis].reshape(-1), np.float32)
    tid = np.ascontiguousarray(TX[ok][vis], np.int32); n = len(tid)
    Tn = T[ok][vis].astype(np.float64); nn = np.cross(Tn[:, 1] - Tn[:, 0], Tn[:, 2] - Tn[:, 0])
    g = np.ascontiguousarray((np.abs(nn[:, 1]) > .5 * np.maximum(np.linalg.norm(nn, axis=1), 1e-12)).astype(np.uint8))
    cov = np.zeros(W * H, np.uint8); dep = np.zeros(W * H, np.float32); gb = np.zeros(W * H, np.uint8)
    L.raster(n, xy.ctypes.data, zf.ctypes.data, g.ctypes.data, W, H, cov.ctypes.data, dep.ctypes.data, gb.ctypes.data)
    res = []
    for rev in (False, True):
        if rev:
            xy = np.ascontiguousarray(xy.reshape(n, 6)[::-1].reshape(-1)); zf = np.ascontiguousarray(zf.reshape(n, 3)[::-1].reshape(-1))
            zz = np.ascontiguousarray(zz.reshape(n, 3)[::-1].reshape(-1)); tid = np.ascontiguousarray(tid[::-1])
        zb = np.zeros(W * H, np.int32); win = np.full(W * H, -1, np.int32); wd = np.zeros(W * H, np.float32)
        L.raster16(n, xy.ctypes.data, zz.ctypes.data, zf.ctypes.data, tid.ctypes.data, W, H, zb.ctypes.data, win.ctypes.data, wd.ctypes.data)
        res.append((wd.reshape(H, W), win.reshape(H, W)))
    return cov.reshape(H, W).astype(bool), dep.reshape(H, W), res[0], res[1], gb.reshape(H, W).astype(bool)


def zfight(cov, dep, fw, bw, gb):
    """полосы 16-бит глубины: (а) сквозь — по 16 бит победил треугольник дальше истинно ближнего больше чем на
    5 см + 1 % дальности (дальняя поверхность проступает); (б) бой — победитель зависит от порядка отрисовки, и у
    двух победителей разные текстуры (одна плоскость двумя рисунками мигает). -> маска (H, W)"""
    (wd, win), (wd2, win2) = fw, bw
    thr = dep + 0.05 + 0.01 * dep
    a = cov & (win >= 0) & ((wd > thr) | (wd2 > thr))
    b = cov & (win >= 0) & (win2 >= 0) & (win != win2)
    return a | b


def measure_z(path, png=None, tag=''):
    L = lib(); pieces = load(path); V = views(pieces); rows = {}
    for i, (name, eye, at) in enumerate(V):
        r = frame16(L, pieces, eye, at)
        z = zfight(*r) if r else np.zeros((H, W), bool)
        rr = rows.setdefault(name, [0, 0]); rr[0] += int(z.sum()); rr[1] += int((z & r[4]).sum()) if r else 0
        if png and i % len(JIT) == 0 and r: save_png(Path(png) / f'{tag}z_{name}.png', r[0], r[1], z)
    return rows


def thin_holes(cov):
    """дыры не шире THIN по строке или столбцу, закрытые с обеих сторон"""
    hole = ~cov
    def along(a):
        out = np.zeros(a.shape, bool)
        for k in range(1, THIN + 1):   # полоса длины k: закрыт, k дыр, закрыт
            m = np.ones((a.shape[0], a.shape[1] - k - 1), bool)
            m &= ~a[:, :-k - 1] & ~a[:, k + 1:]
            for i in range(1, k + 1): m &= a[:, i:a.shape[1] - k - 1 + i]
            for i in range(1, k + 1): out[:, i:a.shape[1] - k - 1 + i] |= m
        return out
    return along(hole) | along(hole.T).T


def on_ground(seam, g):
    """шов, с обеих сторон которого (по строке или столбцу, через соседний пиксель шва) — земля: полоса на асфальте,
    а не просвет неба между ветками или у края крыши"""
    S, G = np.pad(seam, 2), np.pad(g, 2)
    def side(dy, dx):
        n1 = G[2 + dy:G.shape[0] - 2 + dy, 2 + dx:G.shape[1] - 2 + dx]
        s1 = S[2 + dy:S.shape[0] - 2 + dy, 2 + dx:S.shape[1] - 2 + dx]
        n2 = G[2 + 2 * dy:G.shape[0] - 2 + 2 * dy, 2 + 2 * dx:G.shape[1] - 2 + 2 * dx]
        return n1 | (s1 & n2)
    return seam & ((side(0, -1) & side(0, 1)) | (side(-1, 0) & side(1, 0)))


def through(cov, dep):
    """покрытый пиксель, который с обеих сторон (строка или столбец) намного ближе: щель с дальней геометрией за ней"""
    out = np.zeros(cov.shape, bool)
    for a, d in ((cov, dep), (cov.T, dep.T)):
        c, l, r = d[:, 1:-1], d[:, :-2], d[:, 2:]
        m = a[:, 1:-1] & a[:, :-2] & a[:, 2:] & (c > 1.3 * np.maximum(l, r) + 1.0)
        if a is cov: out[:, 1:-1] |= m
        else: out.T[:, 1:-1] |= m
    return out


def measure(path, png=None, tag='', zf=True):
    """-> {вид: [дыры, сквозь, кадров, на земле, полосы глубины, полосы глубины на земле]} — сумма по кадрам вида"""
    L = lib(); pieces = load(path); V = views(pieces); rows = {}
    for i, (name, eye, at) in enumerate(V):
        cov, dep, gb = frame(L, pieces, eye, at)
        th, tr = thin_holes(cov), through(cov, dep)
        road = on_ground(th | tr, cov & gb)
        r = rows.setdefault(name, [0] * 6); r[0] += int(th.sum()); r[1] += int(tr.sum()); r[2] += 1; r[3] += int(road.sum())
        z = None
        if zf:
            f = frame16(L, pieces, eye, at)
            if f: z = zfight(*f); r[4] += int(z.sum()); r[5] += int((z & f[4]).sum())
        if png and i % len(JIT) == 0: save_png(Path(png) / f'{tag}{name}.png', cov, dep, th | tr, z)
    return rows


def save_png(p, cov, dep, seam, zf=None):
    """серый — глубина, тёмно-синий — пусто (небо), пурпур — шов (дыра/сквозь), жёлтый — полоса 16-бит глубины"""
    from PIL import Image
    p.parent.mkdir(parents=True, exist_ok=True)
    g = np.where(cov, 235 - np.clip(np.log1p(dep) * 38, 0, 200), 40).astype(np.uint8)
    im = np.stack([g, g, g], 2)
    im[~cov] = (24, 24, 40)
    if zf is not None: im[zf] = (255, 210, 0)
    im[seam] = (255, 0, 255)
    Image.fromarray(im).resize((W * 2, H * 2), Image.NEAREST).save(p)


def total(rows): return sum(r[0] + r[1] for r in rows.values())


def sums(rows):
    """три числа сторожа: швы кадра (дыры + сквозь), швы на земле, полосы глубины на земле"""
    return total(rows), sum(r[3] for r in rows.values()), sum(r[5] for r in rows.values())


GATE_NAMES = ('швы', 'на земле', 'полосы глубины на земле')


def show(rows, ref=None, label=''):
    print(f'{"вид":12} {"дыры":>7} {"сквозь":>7} {"швы":>7} {"земля":>7} {"z-бой":>7}' + (f' {"эталон":>7} {"земля":>7}' if ref else ''))
    for k, r in rows.items():
        e = ref.get(k) if ref else None
        print(f'{k:12} {r[0]:7d} {r[1]:7d} {r[0] + r[1]:7d} {r[3]:7d} {r[5]:7d}' + (f' {e[0] + e[1] if e else 0:7d} {e[3] if e else 0:7d}' if ref else ''))
    t, g, z = sums(rows)
    print(f'итого {label}: швы {t} пикс. PSP на {sum(r[2] for r in rows.values())} кадрах '
          f'(дыры {sum(r[0] for r in rows.values())}, сквозь {sum(r[1] for r in rows.values())}; на земле {g}; '
          f'полосы 16-бит глубины {sum(r[4] for r in rows.values())}, на земле {z})')


def read_ref(p):
    rows = {}
    for ln in Path(p).read_text().splitlines():
        s = ln.split('\t')
        if len(s) == 7 and not ln.startswith('#'): rows[s[0]] = [int(x) for x in s[1:]]
    return rows


def plant(src, dst, k=0.97):
    """заведомо плохой вход: треугольники ВСЕХ версий кусков города ужаты к своему центру на 3 % — щели по всем стыкам
    (как сборка, вернувшая швы); сторож обязан упасть"""
    b = bytearray(Path(src).read_bytes()); assert b[:4] == b'SHR7'
    ntex, npc = struct.unpack_from('<II', b, 4); ptab, = struct.unpack_from('<I', b, 76); ncell, ctab = struct.unpack_from('<II', b, 80)
    cells = [struct.unpack_from('<6fIIIII', b, ctab + 48 * i) for i in range(ncell)]
    n = 0
    for i in range(npc):
        r = struct.unpack_from('<6fiIIII3ffIII2I', b, ptab + 80 * i)
        fl, ni, own, off, nf, ci = r[7], r[9], r[10], r[15], r[16], r[17]
        if own or not fl & 16 or fl & 4 or ni + nf < 3: continue
        vs = 12 if fl & 32 else 16; o = 6 if vs == 12 else 8; base = cells[ci][9] + off; m = ni + nf
        a = np.frombuffer(bytes(b[base:base + m * vs]), np.uint8).reshape(m, vs).copy()
        xyz = a[:, o:o + 6].copy().view('<i2').reshape(-1, 3, 3).astype(np.float64)
        xyz = xyz.mean(1, keepdims=True) + (xyz - xyz.mean(1, keepdims=True)) * k
        a[:, o:o + 6] = np.round(xyz).clip(-32767, 32767).astype('<i2').reshape(m, 3).view(np.uint8)
        b[base:base + m * vs] = a.tobytes(); n += m // 3
    Path(dst).write_bytes(bytes(b)); print(f'посажено: {n:,} треугольников города ужаты на {100 * (1 - k):.0f} % -> {dst}')


def main():
    if len(sys.argv) == 4 and sys.argv[1] == '--plant': plant(sys.argv[2], sys.argv[3]); return 0
    if len(sys.argv) >= 3 and sys.argv[1] == '--zfight':   # полосы 16-бит глубины по тем же видам: файл [файл ...]
        for f in sys.argv[2:]:
            rows = measure_z(f, str(ROOT / 'build/tmp_conv/seams/z'), Path(f).stem + '_')
            print(f'{Path(f).name}: полосы глубины {sum(r[0] for r in rows.values())} пикс. (на земле {sum(r[1] for r in rows.values())}) — '
                  + ' '.join(f'{k}:{r[1]}' for k, r in rows.items()))
        return 0
    ap = argparse.ArgumentParser()
    ap.add_argument('level'); ap.add_argument('--ref'); ap.add_argument('--png'); ap.add_argument('--gate', action='store_true')
    ap.add_argument('--write-ref', action='store_true'); ap.add_argument('--ref-tsv', default=str(REF_TSV))
    ap.add_argument('--tol', type=float, default=0.10)
    a = ap.parse_args()
    rows = measure(a.level, a.png, 'new_' if a.ref else '')
    if a.write_ref:
        with open(a.ref_tsv, 'w') as f:
            f.write(f'# швы кадра PSP (tools/seams_raster.py, GE24) эталонной сборки уровня 1 {Path(a.level).name}: вид, дыры, сквозь, '
                    f'кадров, на земле, полосы 16-бит глубины, они же на земле. Сторож tools/ship.sh: каждое из трёх чисел '
                    f'(швы, на земле, полосы на земле) не больше эталона × (1 + tol) + {GATE_ABS}\n')
            for k, r in rows.items(): f.write(f'{k}\t' + '\t'.join(map(str, r)) + '\n')
        show(rows, label=a.level); print(f'эталон записан: {a.ref_tsv}'); return 0
    ref = None
    if a.ref: ref = measure(a.ref, a.png, 'old_')
    elif a.gate: ref = read_ref(a.ref_tsv)
    show(rows, ref, a.level)
    if ref is not None and not a.gate:
        t, g, z = sums(ref); print(f'эталон {a.ref}: швы {t}, на земле {g}, полосы глубины на земле {z}')
    if a.gate:
        if set(rows) != set(ref) or any(rows[k][2] != ref[k][2] for k in rows):   # нет build/routes.txt — меньше видов, меньше швов
            print(f'FAIL швы уровня: виды замера не совпали с эталоном ({len(rows)} против {len(ref)}; build/routes.txt?)'); return 1
        bad = []
        for nm, n, e in zip(GATE_NAMES, sums(rows), sums(ref)):
            lim = e * (1 + a.tol) + GATE_ABS
            if n > lim: bad.append(f'{nm} {n} > {lim:.0f}')
        if bad:
            print(f'FAIL швы уровня: {"; ".join(bad)} (эталон {Path(a.ref_tsv).name} × {1 + a.tol:.2f} + {GATE_ABS}) — выпуск остановлен'); return 1
        t, g, z = sums(rows); et, eg, ez = sums(ref)
        print(f'OK швы уровня: швы {t} (эталон {et}), на земле {g} ({eg}), полосы глубины на земле {z} ({ez}); допуск × {1 + a.tol:.2f} + {GATE_ABS}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
