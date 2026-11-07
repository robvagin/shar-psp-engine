#!/usr/bin/env python3
"""Правки Роба 224/237/251 («опять много швов», «швов стало больше», «швы очень мигают»): щели кадра PSP — модель GE
с float24 и разбор причин. Эмулятор их не показывает: PPSSPP хранит матрицы GE как float24, но считает вершины во float32
и склеивает world·view·proj (build/twins/ppsspp/src/GPU/Software/TransformUnit.cpp ComputeTransformState). Железо считает
вершины во float24 (мантисса 15 бит): «once the matrices are applied, the result is normalized to … only 15 mantissa bits»
(разработчик PPSSPP по замерам на PSP, github.com/hrydgard/ppsspp/issues/12058#issuecomment-913225641). Город уровня 1 —
в 500–900 м от начала координат: шаг float24 там 1/64 м, ошибка вершины в осях камеры до ~2 см, у камеры 0,2–0,5 пикселя.

Щель — пиксель, где идеальная модель (float64, ближние версии везде, без отбраковки гвард-бэнда; эталон — тот же файл или
--ref) закрыта непрозрачной поверхностью, а модель GE пуста (на PSP там небо — оно рисуется последним в пустую глубину) или
видит то, что дальше на 5 % + 0,5 м, в полосе не шире 2 пикселей, по обе стороны которой GE рисует ту же поверхность.
Законные узкие просветы (листва, фермы моста) есть и в идеальной модели, пропавшая мелочь дальней версии щелью не считается.
Альфа-тест и полупрозрачное в модели не участвуют (их покрытие зависит от текстуры).

Модель GE (--ge): f32 — как PPSSPP (матрицы float24, вершины во float32 со склейкой матриц; эталон эмулятора);
  sep — float24 по шагам: мир = M·v, вид = V·мир, клип = P·вид, усечение после каждого умножения и сложения;
  cat — float24 со склейкой: W = P·(V·M) один раз на кусок, клип = W·v (у каждого куска своя ошибка).
Движок: по умолчанию как viewer/main.c с правками 224/237/251 — город в осях камеры (вид без переноса, перенос куска =
  центр − глаз во float32); --abs — как до правки (матрица куска и вид с переносом в сотни метров, служебный файл seamsold).
  Дальнюю версию куска модель рисует дальше LOD 50 м, если у куска нет флага 2048 (по правилу 224/237/251 он у всех кусков города).
Классы (--classes, по порядку исключения, кадр за кадром): LOD — исчезает, если все куски рисовать ближней версией; точность —
  исчезает, если считать вершины точно (float64); гвард — исчезает без отбраковки вершины за 4096 пикселей; прочее — остаток.

  python tools/seams_ge.py LEVEL.shr [--ge cat|sep|f32] [--abs] [--ref REF.shr] [--views 12|22] [--png DIR] [--classes]
  python tools/seams_ge.py LEVEL.shr --gate [--max N] …   сторож: щелей на 12 видах × 3 кадра не больше N (иначе FAIL, код 1)
  python tools/seams_ge.py LEVEL.shr --tris               цена: треугольников в круге 250 м — с дальними версиями и ближними везде"""
import argparse, ctypes, math, os, struct, subprocess, sys
import shutil
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import seams_raster as SR   # noqa: E402  (точки обзора, глаз за машиной)

W, H = 480, 272
FOVY, ASPECT, NEAR, LOD, DRAW = 62.0, 16.0 / 9.0, 1.5, 50.0, 250.0
OFFX, OFFY = (2048 - W // 2) * 16, (2048 - H // 2) * 16
SEE_K, SEE_ADD = 1.05, 0.5     # «сквозь»: видна поверхность дальше идеальной на 5 % + 0,5 м
GATE_MAX = int(os.environ.get('SEAMS_GE_MAX', 200))   # 03.10: прежний уровень 902 (cat) / 570 (sep), по правилу 145 / 106


def lib():
    src = ROOT / 'tools/seams_ge.c'; out = ROOT / ('build/tmp_conv/seams_ge.dylib' if sys.platform == 'darwin' else 'build/tmp_conv/seams_ge.so')   # Linux/WSL: .so
    out.parent.mkdir(parents=True, exist_ok=True)
    if not out.exists() or out.stat().st_mtime < src.stat().st_mtime:
        cc = os.environ.get('CC') or ('clang' if sys.platform == 'darwin' else (shutil.which('cc') or shutil.which('gcc') or 'cc'))   # на Linux clang обычно нет
        subprocess.run([cc, '-O2', '-shared', '-fPIC', '-o', str(out), str(src), '-lm'], check=True)
    L = ctypes.CDLL(str(out))
    L.raster_id.argtypes = [ctypes.c_int] + [ctypes.c_void_p] * 3 + [ctypes.c_int, ctypes.c_int] + [ctypes.c_void_p] * 2
    return L


# ------------------------------------------------------------------------------------------------ float24 GE
def t24(x):
    """float64 -> float24 GE (знак, 8 бит порядка, 15 бит мантиссы), усечение к нулю; результат — float64"""
    x = np.asarray(x, np.float64)
    m, e = np.frexp(x)
    return np.ldexp(np.trunc(m * 65536.0) / 65536.0, e)


def up24(x):
    """float32 процессора -> регистр GE (sceGuSetMatrix шлёт биты float32 >> 8)"""
    return t24(np.asarray(x, np.float32).astype(np.float64))


def dot24(row, v):
    """строка матрицы (4,) или (n,4) на вершины v (n,3), усечение после каждого умножения и сложения"""
    row = np.asarray(row, np.float64)
    r = row if row.ndim == 2 else row[None, :]
    acc = t24(r[:, 0] * v[:, 0])
    acc = t24(acc + t24(r[:, 1] * v[:, 1]))
    acc = t24(acc + t24(r[:, 2] * v[:, 2]))
    return t24(acc + r[:, 3])


def mm24(A, B):
    """A (…,4,4) · B (…,4,4) во float24 (порядок k = 0..3, усечение после каждой операции)"""
    A = np.asarray(A, np.float64); B = np.asarray(B, np.float64)
    out = np.zeros(np.broadcast_shapes(A.shape, B.shape))
    for i in range(4):
        for j in range(4):
            acc = t24(A[..., i, 0] * B[..., 0, j])
            for k in range(1, 4): acc = t24(acc + t24(A[..., i, k] * B[..., k, j]))
            out[..., i, j] = acc
    return out


def look_at(eye, at):
    """sceGumLookAt во float32: V = R · T(-eye)"""
    eye = np.asarray(eye, np.float32); at = np.asarray(at, np.float32)
    f = at - eye; f = f / np.float32(np.linalg.norm(f))
    s = np.cross(f, np.array([0, 1, 0], np.float32)).astype(np.float32); s = s / np.float32(np.linalg.norm(s))
    u = np.cross(s, f).astype(np.float32)
    V = np.eye(4, dtype=np.float32); V[0, :3], V[1, :3], V[2, :3] = s, u, -f
    V[:3, 3] = -(V[:3, :3] @ eye)
    return V


def perspective(near=NEAR, far=DRAW + 10):
    ct = np.float32(1 / math.tan(math.radians(FOVY) / 2))
    P = np.zeros((4, 4), np.float32); P[0, 0] = ct / np.float32(ASPECT); P[1, 1] = ct; P[3, 2] = -1
    P[2, 2] = np.float32(-(far + near) / (far - near)); P[2, 3] = np.float32(-2 * far * near / (far - near))
    return P


# ------------------------------------------------------------------------------------------------ уровень
def load(path):
    """непрозрачные куски города -> список dict: bb, флаги, центр (float32 файла), sc, int16 ближних/дальних (n,3,3)"""
    b = Path(path).read_bytes(); assert b[:4] == b'SHR7', f'{path}: не SHR7'
    ntex, npc = struct.unpack_from('<II', b, 4); ptab, = struct.unpack_from('<I', b, 76); ncell, ctab = struct.unpack_from('<II', b, 80)
    cells = [struct.unpack_from('<6fIIIII', b, ctab + 48 * i) for i in range(ncell)]
    out = []
    for i in range(npc):
        r = struct.unpack_from('<6fiIIII3ffIII2I', b, ptab + 80 * i)
        fl, nv, ni, own = r[7:11]; o = np.array(r[11:14], np.float32); sc = np.float32(r[14]); off, nf, ci = r[15:18]
        if own != 0 or ni == 0 or not fl & 16 or fl & 4 or fl & 3: continue
        vs = 12 if fl & 32 else 16; base = cells[ci][9] + off
        raw = np.frombuffer(b, np.uint8, (ni + nf) * vs, base).reshape(-1, vs)
        xyz = (raw[:, 6:12] if vs == 12 else raw[:, 8:14]).copy().view('<i2').reshape(-1, 3).astype(np.float64)
        out.append(dict(i=i, lo=np.array(r[0:3]), hi=np.array(r[3:6]), fl=fl, o=o, sc=sc,
                        qn=xyz[:ni].reshape(-1, 3, 3), qf=xyz[ni:].reshape(-1, 3, 3)))
    return out


def world64(p, q):
    """точный мир вершин куска (float64): центр + v · sc/32768"""
    return p['o'].astype(np.float64)[None, None, :] + q * (float(p['sc']) / 32768.0)


def ground_tri(T):
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
    return np.abs(n[:, 1]) > .5 * np.maximum(np.linalg.norm(n, axis=1), 1e-12)


# ------------------------------------------------------------------------------------------------ кадр
def select(pieces, eye, allnear):
    """куски кадра и версия: (кусок, дальняя ли) — правило движка viewer/main.c cullPiece (бокс до глаза)"""
    out = []
    for p in pieces:
        c = np.clip(eye, p['lo'], p['hi']); d2 = float(((c - eye) ** 2).sum())
        if d2 > DRAW * DRAW: continue
        far = (d2 > LOD * LOD) and not allnear and not p['fl'] & 2048   # флаг 2048 — ближняя на любой дальности (viewer/main.c drawPiece)
        q = p['qf'] if far else p['qn']
        if len(q): out.append((p, far, q))
    return out


def clip_coords(sel, eye, at, ge, rel=False):
    """-> клип-координаты (n,3,4) float64 треугольников кадра, номер куска у треугольника, мир (n,3,3) для земли.
    rel — город в осях камеры (viewer/main.c, правки 224/237/251): вид — только поворот, перенос куска = центр − глаз (float32)"""
    V32, P32 = look_at(eye, at), perspective()
    if not sel: return np.zeros((0, 3, 4)), np.zeros(0, int), np.zeros((0, 3, 3))
    Q = np.concatenate([q for _, _, q in sel]).reshape(-1, 3)           # int16 вершин кадра
    cnt = np.array([len(q) * 3 for _, _, q in sel])
    pk = np.repeat(np.arange(len(sel)), cnt)                             # кусок (номер в sel) у вершины
    O = np.array([p['o'] for p, _, _ in sel], np.float64); S = np.array([p['sc'] for p, _, _ in sel], np.float64)
    owners = np.repeat(np.array([p['i'] for p, _, _ in sel]), cnt // 3)
    w64 = O[pk] + Q * (S[pk] / 32768.0)[:, None]
    if rel and ge != 'ref':   # вид без переноса (глаз в начале), перенос куска — центр − глаз во float32 процессора
        V32 = V32.copy(); V32[:3, 3] = 0
        O = (np.array([p['o'] for p, _, _ in sel], np.float32) - np.asarray(eye, np.float32)[None, :]).astype(np.float64)
    v = Q / 32768.0                                                      # GE: s16 / 32768
    if ge == 'ref':
        PV = P32.astype(np.float64) @ V32.astype(np.float64)
        c = np.c_[w64, np.ones(len(w64))] @ PV.T
    elif ge == 'f32':   # PPSSPP: world·view·proj во float32, вершина — тем же произведением
        V24f, P24f = up24(V32).astype(np.float32), up24(P32).astype(np.float32)
        M = np.tile(np.eye(4, dtype=np.float32), (len(sel), 1, 1)); s24 = up24(S).astype(np.float32)
        M[:, 0, 0] = M[:, 1, 1] = M[:, 2, 2] = s24; M[:, :3, 3] = up24(O).astype(np.float32)
        T = np.einsum('ij,njk->nik', (P24f @ V24f).astype(np.float32), M).astype(np.float32)
        vv = np.c_[v.astype(np.float32), np.ones(len(v), np.float32)]
        c = np.einsum('nij,nj->ni', T[pk], vv).astype(np.float64)
    else:
        V24, P24 = up24(V32), up24(P32)
        M = np.tile(np.eye(4), (len(sel), 1, 1)); M[:, 0, 0] = M[:, 1, 1] = M[:, 2, 2] = up24(S); M[:, :3, 3] = up24(O)
        if ge == 'sep':
            wv = np.stack([dot24(M[pk, i], v) for i in range(3)], 1)
            vw = np.stack([dot24(V24[i], wv) for i in range(3)], 1)
            c = np.stack([dot24(P24[i], vw) for i in range(4)], 1)
        else:   # cat: своя склейка на кусок
            T = mm24(P24[None], mm24(V24[None], M))
            c = np.stack([dot24(T[pk, i], v) for i in range(4)], 1)
    return c.reshape(-1, 3, 4), owners, w64.reshape(-1, 3, 3)


def screen(c, ge):
    """клип -> экран 12.4 (как PPSSPP ClipToScreen: +0.375 и усечение), глубина w"""
    x, y, w = c[..., 0], c[..., 1], c[..., 3]
    jx, jy = (float(v) for v in os.environ.get('SEAMS_JIT', '0,0').split(','))   # правка 507: подпиксельный сдвиг мира (джиттер), пикселей
    if jx or jy:
        x = x + jx * (2.0 / W) * w; y = y - jy * (2.0 / H) * w   # сдвиг экрана на jx,jy пикселей = сдвиг клипа на 2j/размер · w
    if ge in ('sep', 'cat'):
        sx = t24(t24(t24(x / w) * 240.0) + 2048.0); sy = t24(t24(t24(y / w) * -136.0) + 2048.0)
    else:
        sx = x / w * 240.0 + 2048.0; sy = y / w * -136.0 + 2048.0
    return sx, sy, w


def frame(L, pieces, eye, at, ge='cat', allnear=False, guard=True, rel=False):
    """кадр модели -> (номер треугольника (H,W) int32, -1 пусто; глубина (H,W); мир треугольников (n,3,3); земля (n,))"""
    if os.environ.get('SEAMS_QEYE'):   # правка 507: глаз кадра на решётку 1/q м (и в эталоне, и в движке): сдвиг от точного глаза не считается щелью
        q_ = float(os.environ['SEAMS_QEYE']); eye = tuple(np.round(np.asarray(eye, np.float64) * q_) / q_)
    sel = select(pieces, eye, allnear)
    C, own, Wd = clip_coords(sel, eye, at, ge, rel)
    gnd = ground_tri(Wd) if len(Wd) else np.zeros(0, bool)
    zc, wc = C[..., 2], C[..., 3]
    inside = zc >= -wc                                  # перед ближней плоскостью (z/w >= -1)
    full = inside.all(1); part = inside.any(1) & ~full
    far_all = (zc > wc).all(1)                          # все три за дальней плоскостью — отброшен
    keep = full & ~far_all
    T, tid, clipped = [C[keep]], [np.nonzero(keep)[0]], [np.zeros(int(keep.sum()), bool)]
    if part.any():                                      # GE режет по ближней плоскости (только треугольники)
        extra, eid = [], []
        for t in np.nonzero(part)[0]:
            poly = []
            for i in range(3):
                a, b_ = C[t, i], C[t, (i + 1) % 3]; da, db = a[2] + a[3], b_[2] + b_[3]
                if da >= 0: poly.append(a)
                if (da >= 0) != (db >= 0): poly.append(a + (b_ - a) * (da / (da - db)))
            for j in range(1, len(poly) - 1): extra.append((poly[0], poly[j], poly[j + 1])); eid.append(t)
        if extra:
            E = np.asarray(extra)
            if ge in ('sep', 'cat'): E = t24(E)
            T.append(E); tid.append(np.asarray(eid)); clipped.append(np.ones(len(eid), bool))
    T = np.concatenate(T); tid = np.concatenate(tid); clipped = np.concatenate(clipped)
    sx, sy, w = screen(T, ge)
    ok = (w > 1e-6).all(1)
    if guard:   # гвард-бэнд: вершина вне 0..4096 — треугольник выброшен (кроме разрезанных ближней плоскостью)
        ok &= clipped | ((sx >= 0) & (sx < 4096) & (sy >= 0) & (sy < 4096)).all(1)
    x16 = np.floor(sx * 16 + .375).astype(np.int64) - OFFX; y16 = np.floor(sy * 16 + .375).astype(np.int64) - OFFY
    vis = ok & ~(((x16 < 0).all(1)) | ((x16 >= W * 16).all(1)) | ((y16 < 0).all(1)) | ((y16 >= H * 16).all(1)))
    vis &= (np.abs(x16) < 1 << 30).all(1) & (np.abs(y16) < 1 << 30).all(1)
    xy = np.ascontiguousarray(np.stack([x16[vis], y16[vis]], 2).reshape(-1), np.int32)
    z = np.ascontiguousarray(w[vis].reshape(-1), np.float32); ids = np.ascontiguousarray(tid[vis], np.int32)
    idb = np.full(W * H, -1, np.int32); dep = np.zeros(W * H, np.float32)
    L.raster_id(int(len(ids)), xy.ctypes.data, z.ctypes.data, ids.ctypes.data, W, H, idb.ctypes.data, dep.ctypes.data)
    return idb.reshape(H, W), dep.reshape(H, W), Wd, gnd, own


THIN = 2          # пикселей: щель не шире (как tools/seams_raster.py)
NEAR_K = 0.10     # края щели — та же поверхность: глубина GE с обеих сторон в 10 % + 0,5 м от идеальной глубины щели


def seams_of(ref, eng):
    """щель-просвет: пиксель, который в идеальной модели закрыт, а у GE пуст или виден дальше на 5 % + 0,5 м, — в полосе
    не шире THIN, по обе стороны которой (по строке или столбцу) GE рисует ту же поверхность (глубина как у идеальной в
    щели). Пропавшая мелочь дальней версии (ящик на крыше, ветка) щелью не считается: по краям у неё фон или стена дальше"""
    rid, rdep = ref[0], ref[1]; eid, edep = eng[0], eng[1]
    bad = (rid >= 0) & ((eid < 0) | (edep > rdep * SEE_K + SEE_ADD))
    good = (eid >= 0) & (rid >= 0) & (np.abs(edep - rdep) <= NEAR_K * rdep + SEE_ADD)
    out = np.zeros_like(bad)
    for B, G, D, E, O in ((bad, good, rdep, edep, out), (bad.T, good.T, rdep.T, edep.T, out.T)):
        n = B.shape[1]
        for k in range(1, THIN + 1):
            m = G[:, :n - k - 1] & G[:, k + 1:]
            for i in range(1, k + 1):
                m = m & B[:, i:n - k - 1 + i]
                tol = NEAR_K * D[:, i:n - k - 1 + i] + SEE_ADD
                m = m & (np.abs(E[:, :n - k - 1] - D[:, i:n - k - 1 + i]) <= tol) & (np.abs(E[:, k + 1:] - D[:, i:n - k - 1 + i]) <= tol)
            for i in range(1, k + 1): O[:, i:n - k - 1 + i] |= m
    return out


def views(pieces_sr, n):
    V = SR.views(pieces_sr)
    if n == 12: V = [v for v in V if v[0].startswith('v')]
    return V


def measure(path, ge='cat', nviews=12, png=None, classes=False, rel=True, ref_path=None):
    """-> {вид: [щелей, на земле, LOD, точность, гвард, прочее, кадров]}"""
    L = lib(); pieces = load(path); refp = load(ref_path) if ref_path else pieces
    V = views(SR.load(ref_path or path), nviews); rows = {}
    for k, (name, eye, at) in enumerate(V):
        ref = frame(L, refp, eye, at, 'ref', True, False)
        eng = frame(L, pieces, eye, at, ge, False, True, rel)
        S = seams_of(ref, eng)
        rg = ref[3][np.maximum(ref[0], 0)] & (ref[0] >= 0)
        r = rows.setdefault(name, [0] * 7); r[0] += int(S.sum()); r[1] += int((S & rg).sum()); r[6] += 1
        if classes and S.any():
            rem = S
            for slot, (g2, an2, gd2) in ((2, (ge, True, True)), (3, ('ref', True, True)), (4, ('ref', True, False))):
                Sv = seams_of(ref, frame(L, pieces, eye, at, g2, an2, gd2, rel))
                r[slot] += int((rem & ~Sv).sum()); rem = rem & Sv
            r[5] += int(rem.sum())
        if png and k % 3 == 0: save_png(Path(png) / f'{name}.png', ref, eng, S)
    return rows


def save_png(p, ref, eng, S):
    """серое — глубина модели GE, тёмно-синее — пусто (небо), пурпур — просвет"""
    from PIL import Image
    p.parent.mkdir(parents=True, exist_ok=True)
    cov = eng[0] >= 0
    g = np.where(cov, 235 - np.clip(np.log1p(eng[1]) * 38, 0, 200), 40).astype(np.uint8)
    im = np.stack([g, g, g], 2); im[~cov] = (24, 24, 40); im[S] = (255, 0, 255)
    Image.fromarray(im).resize((W * 2, H * 2), Image.NEAREST).save(p)


def tris_cost(path, nviews=22):
    """цена: треугольников в кадре (как cullPiece без пирамиды и PVS — верхняя оценка) обычным правилом и ближними везде"""
    pieces = load(path); V = views(SR.load(path), nviews); a = b = 0
    for name, eye, at in V[::3]:
        a += sum(len(q) for _, _, q in select(pieces, eye, False)); b += sum(len(q) for _, _, q in select(pieces, eye, True))
    n = len(V[::3])
    print(f'треугольников в круге {DRAW:g} м (без пирамиды и PVS), среднее на вид: LOD {a / n:,.0f}, ближние везде {b / n:,.0f} '
          f'(×{b / max(a, 1):.2f})')


def show(rows, label, classes):
    hdr = f'{"вид":12} {"щели":>8} {"земля":>7}' + (f' {"LOD":>7} {"точн.":>7} {"гвард":>7} {"прочее":>7}' if classes else '')
    print(hdr)
    for k, r in rows.items():
        print(f'{k:12} {r[0]:8d} {r[1]:7d}' + (f' {r[2]:7d} {r[3]:7d} {r[4]:7d} {r[5]:7d}' if classes else ''))
    t = [sum(r[i] for r in rows.values()) for i in range(7)]
    print(f'итого {label}: щелей {t[0]} пикс. на {t[6]} кадрах (на земле {t[1]})' +
          (f'; LOD {t[2]}, точность float24 {t[3]}, гвард-бэнд {t[4]}, прочее {t[5]}' if classes else ''))
    return t


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('level'); ap.add_argument('--ge', default='cat', choices=('cat', 'sep', 'f32'))
    ap.add_argument('--abs', action='store_true', help='движок до правки: абсолютные матрицы (служебный файл seamsold)')
    ap.add_argument('--ref', help='эталон геометрии — другой файл уровня (по умолчанию тот же)')
    ap.add_argument('--views', type=int, default=12, choices=(12, 22)); ap.add_argument('--png')
    ap.add_argument('--classes', action='store_true'); ap.add_argument('--gate', action='store_true')
    ap.add_argument('--max', type=int, default=GATE_MAX); ap.add_argument('--tris', action='store_true')
    a = ap.parse_args()
    if a.tris: tris_cost(a.level); return 0
    rows = measure(a.level, a.ge, a.views, a.png, a.classes, not a.abs, a.ref)
    t = show(rows, f'{Path(a.level).name} (GE {a.ge}, движок {"до правки" if a.abs else "в осях камеры"}'
                   f'{", эталон " + Path(a.ref).name if a.ref else ""})', a.classes)
    if a.gate:
        if t[0] > a.max:
            print(f'FAIL щели GE {a.ge}: {t[0]} > {a.max} на {t[6]} кадрах'); return 1
        print(f'OK щели GE {a.ge}: {t[0]} <= {a.max} на {t[6]} кадрах')
    return 0


if __name__ == '__main__':
    sys.exit(main())
