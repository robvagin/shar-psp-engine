#!/usr/bin/env python3
"""SHAR character -> PSP .chr (format CHR1), stream "homer".

Reads the owner's own disc files (hit-and-run-web/source/game, never published):
  art/chars/<name>_m.p3d   skin (PS2 VIF strips), skeleton, shaders, eye-blink texture animation
  art/chars/<name>_a.p3d   ALL animation clips of the character (67 for Homer)
  art/chars/global.p3d     shared textures (char_swatches*, eyeball*.bmp.0..3)
and writes build/chars/<name>.chr, loaded on the PSP by viewer/skin.c (format documented in skin.h).

  python tools/char_export.py                         # Homer -> build/chars/homer.chr
  python tools/char_export.py --name bart             # any character with <name>_m.p3d/<name>_a.p3d
  python tools/char_export.py --preview DIR           # + PNG renders made FROM THE .chr (same math as skin.c)
  python tools/char_export.py --max-bones 4 --out build/chars/homer_b4.chr   # 4 weights/vertex variant (GE cost test)
  python tools/char_export.py --name bart --npc       # mission NPC (viewer/msn_view.c npcFiles): only the clips it can play

Decisions (numbers printed at the end of every run):
  * GE hardware skinning: strips are grouped into draw batches of <= 8 bones (GU_WEIGHTS(n), 8-bit weights,
    128 = 1.0), the CPU only builds ~32 bone matrices per frame.
  * The PS2 weights are CHAINED RATIOS: w0 = s0, w1 = w0*s1, w2 = w1*s2, w3 = w2*s3 (sums are exactly 1.0 for
    every vertex of homer/bart/lisa/marge/apu). hit-and-run-web normalises the raw values instead, which is wrong.
  * Original PS2 strip order is kept; identical packed vertices share indices within the same bone palette.
  * Textures are the original 8-bit indexed images -> CLUT8 with the exact palette (no quantisation), swizzled.
  * Animations: original sparse keys (u16 frame numbers), quaternions quantised to 4 x s16, translations f32.
Coordinates are hit-and-run-web's (game z mirrored), the same as level.shr. The model faces +Z (the car model faces -Z),
so for main.c's heading h (forward = (sin h, 0, cos h)) Homer is drawn rotated by h, the car by h + pi.
"""
import argparse, math, os, struct, sys
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
from p3d import read, walk, string      # noqa: E402  (MIT, bounds-checked Pure3D reader)
from vif import unpack                  # noqa: E402  (MIT, PS2 VIF UNPACK reader)
sys.path.insert(0, str(ROOT / 'tools'))
import animpack as ANIMPACK             # noqa: E402  (Модели-96: упаковка ключей, ANIMPACK96_OLD=1 — как было)

GAME = ROOT / 'hit-and-run-web/source/game'
F = np.diag([1., 1., -1., 1.])          # game (left-handed) <-> hit-and-run-web coordinates

# GE constants (pspgu.h)
GU_TRIANGLE_STRIP = 4
GU_TEXTURE_16BIT = 2 << 0
GU_NORMAL_8BIT = 1 << 5
GU_VERTEX_32BITF = 3 << 7
GU_WEIGHT_8BIT = 1 << 9
GU_INDEX_16BIT = 2 << 11  # pspgu.h GU_INDEX_SHIFT(2)
def GU_WEIGHTS(n): return ((n - 1) & 7) << 14
GU_PSM_T8 = 5
UV_SCALE = 256.0                        # u16 uv = uv * 256 (GE reads unsigned; batch shifted into [0,1), tools/uvq.py), sceGuTexScale(128,128) like level.shr
MAXB = 8                                # GE bone matrices per draw (--max-bones: fewer weights per vertex, more draws)

HDR, JNT, BAT, TEXR, CLIP, TRK = 128, 128, 48, 48, 80, 16


def align(n, a): return (n + a - 1) & ~(a - 1)


POS16_OLD = bool(os.environ.get('CHARPOS16_OLD'))   # Модели-96: 1 — позиции вершин f32, как было (по умолчанию s16 с масштабом на модель)
GU_VERTEX_16BIT = 2 << 7
GU_NORMAL_16BIT = 2 << 5
POS16_MARGIN = 0.10                                  # м: запас диапазона s16 под раздутие контура


def vertex_layout(nb, pos16=False):
    """GE vertex: nb u8 weights, s16 uv, s8 normal, f32 position (each part aligned to its own size).
    pos16 (Модели-96): позиция s16 x3 и нормаль s16 x3 (по 6 Б) вместо f32 x3 (12 Б) и s8 x3 (3 Б).
    GE читает s16 как целое / 32768 (так же level.shr, main_render.h S = 1/32768): метры = целое * scale, палитра костей множит столбцы на scale * 32768
    (header pad[0] = scale, метры на единицу); та же палитра растягивает нормаль в F = scale * 32768 раз, поэтому нормаль хранится поделённой на F (s16 -> |n| = 1 после кости,
    свет не зависит от того, нормирует ли GE нормаль)"""
    uv = align(nb, 2); nrm = uv + 4
    if pos16:
        pos = align(nrm + 6, 2); stride = align(pos + 6, 2)
    else:
        pos = align(nrm + 3, 4); stride = align(pos + 12, 4)
    return uv, nrm, pos, stride


def pos16_scale(strips_pos):
    """Масштаб s16-позиций модели: степень двойки (в палитре костей умножение точное), наименьшая, при которой весь ящик влезает в ±32767.
    strips_pos — список массивов Nx3 (позиции в системе покоя модели). None — если s16 выключен."""
    if POS16_OLD: return None
    m = max(float(np.abs(p).max()) for p in strips_pos if len(p)) + POS16_MARGIN       # запас: контур раздувает вершины (viewer/skin.c toonHull)
    if not m > 0: return None
    s = 2.0 ** math.ceil(math.log2(m / 32767.0))
    while m / s > 32767.0: s *= 2
    return max(s, 2.0 ** -15)       # F = s * 32768 >= 1: нормаль s16 делится на F, при F < 1 не влезла бы


# ------------------------------------------------------------------ source parsing
def q_from_m(R):
    t = R[0, 0] + R[1, 1] + R[2, 2]
    if t > 0:
        s = math.sqrt(t + 1) * 2; w = s / 4; x = (R[2, 1] - R[1, 2]) / s; y = (R[0, 2] - R[2, 0]) / s; z = (R[1, 0] - R[0, 1]) / s
    elif R[0, 0] > R[1, 1] and R[0, 0] > R[2, 2]:
        s = math.sqrt(1 + R[0, 0] - R[1, 1] - R[2, 2]) * 2; w = (R[2, 1] - R[1, 2]) / s; x = s / 4; y = (R[0, 1] + R[1, 0]) / s; z = (R[0, 2] + R[2, 0]) / s
    elif R[1, 1] > R[2, 2]:
        s = math.sqrt(1 + R[1, 1] - R[0, 0] - R[2, 2]) * 2; w = (R[0, 2] - R[2, 0]) / s; x = (R[0, 1] + R[1, 0]) / s; y = s / 4; z = (R[1, 2] + R[2, 1]) / s
    else:
        s = math.sqrt(1 + R[2, 2] - R[0, 0] - R[1, 1]) * 2; w = (R[1, 0] - R[0, 1]) / s; x = (R[0, 2] + R[2, 0]) / s; y = (R[1, 2] + R[2, 1]) / s; z = s / 4
    q = np.array([x, y, z, w]); return q / np.linalg.norm(q)


def m_from_q(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def load_skeleton(root):
    joints = []
    for c in walk(root):
        if c.id != 0x4501: continue
        name, p = string(c.data); parent, = struct.unpack_from('<I', c.data, p)
        M = np.frombuffer(c.data, '<f4', 16, p + 24).reshape(4, 4).astype(np.float64)
        L = (F @ M @ F).T                                   # column-vector local rest matrix
        S = np.linalg.norm(L[:3, :3], axis=0)
        R = L[:3, :3] / S
        if np.linalg.det(R) < 0: raise ValueError(f'mirrored joint {name}')
        joints.append(dict(name=name, parent=-1 if parent == len(joints) else parent, L=L, q=q_from_m(R), t=L[:3, 3].copy(), s=S))
    for j, jt in enumerate(joints):
        if jt['parent'] >= j: raise ValueError('joints are not parent-first')
        jt['W'] = jt['L'] if jt['parent'] < 0 else joints[jt['parent']]['W'] @ jt['L']
        jt['inv'] = np.linalg.inv(jt['W'])
    return joints


def load_images(root):
    """Pure3D P3DI images -> (w, h, CLUT 256 x RGBA8, indices uint8[h*w]) exactly as stored (no quantisation)."""
    out = {}
    for c in walk(root):
        if c.id != 0x19000: continue
        name = string(c.data)[0].lower()
        img = next(v for v in c.children if v.id == 0x19001)
        raw = next(v for v in img.children if v.id == 0x19002).data[4:]
        if raw[:4] == b'\x89PNG':                   # global.p3d: indexed PNGs -> palette + indices as stored
            import io
            from PIL import Image
            im = Image.open(io.BytesIO(raw))
            if im.mode != 'P': im = im.convert('RGBA').quantize(256)      # not seen in character files
            idx = np.array(im, np.uint8).flatten(); w, h = im.size
            rgb = np.array(im.getpalette() or [0] * 768, np.uint8)[:768].reshape(-1, 3)
            clut = np.zeros((256, 4), np.uint8); clut[:len(rgb), :3] = rgb; clut[:, 3] = 255
            tr = im.info.get('transparency')
            if isinstance(tr, (bytes, bytearray)): clut[:len(tr), 3] = np.frombuffer(tr, np.uint8)
            elif isinstance(tr, int): clut[tr, 3] = 0
            out[name] = (w, h, clut, idx); continue
        if raw[:4] != b'P3DI': continue
        fmt, w, h, bpp, psize, pxsize = struct.unpack_from('<6I', raw, 4)
        pal = np.frombuffer(raw, np.uint8, psize, 28).reshape(-1, 4).copy()
        pal[:, 3] = np.minimum(pal[:, 3].astype(np.uint16) * 255 // 128, 255)
        px = np.frombuffer(raw, np.uint8, pxsize, 28 + psize)
        if bpp == 8:
            order = np.arange(256); order = (order & ~24) | ((order & 8) << 1) | ((order & 16) >> 1)   # PS2 CSM1
            pal = pal[order]; idx = px[:w * h].copy()
        elif bpp == 4:
            idx = np.stack([px & 15, px >> 4], 1).flatten()[:w * h].astype(np.uint8)
        else: continue
        clut = np.zeros((256, 4), np.uint8); clut[:len(pal)] = pal[:256]
        out[name] = (w, h, clut, idx)
    for name, (w, h, clut, idx) in list(out.items()):  # GE T8: >= 16 texels per row for the swizzle / buffer width
        if w < 16: out[name] = (16, h, clut, np.repeat(idx.reshape(h, w), 16 // w, axis=1).flatten())
    return out


def load_shaders(root):
    out = {}
    for c in walk(root):
        if c.id != 0x11000: continue
        name, p = string(c.data); props = {}
        for prop in c.children:
            key = prop.data[:4].rstrip(b'\0').decode('ascii')
            if prop.id == 0x11002: props[key] = string(prop.data, 4)[0].lower()
            elif prop.id == 0x11003: props[key] = struct.unpack_from('<I', prop.data, 4)[0]
        out[name] = props
    return out


GENERIC_BONES_LSB = False   # красный tests/chars/generic_skin_check.py --negative: номера костей от младшего байта (прежнее чтение)


def generic_strip(pg, pal):
    """Правка Роба 180 (близнец Б2: оригинал грузит art/chars/louie_m.p3d — Луи, бонус-гонка gr1 у школы): у этой модели группы кожи
    без родных полос PS2 (0x10012) — обычные списки Pure3D (как читает hit-and-run-web/tools/convert.py, MIT): 0x10005 вершины,
    0x10006 нормали, 0x10007 UV (с 8-го байта), 0x1000b номера костей (u32: u8 x4 от старшего байта, индекс палитры 0x1000d), 0x1000c веса (3 + четвёртый
    = 1 - сумма). Примитив 1 — одна длинная полоса со склейками вырожденными треугольниками (индексов нет). -> [strip dict]"""
    d = {c.id: c.data for c in pg.children}
    if 0x10005 not in d: return []
    n, = struct.unpack_from('<I', d[0x10005])
    pos = np.frombuffer(d[0x10005], '<f4', n * 3, 4).reshape(-1, 3).astype(np.float64)
    nrm = np.frombuffer(d[0x10006], '<f4', n * 3, 4).reshape(-1, 3).astype(np.float64) if 0x10006 in d else np.tile([0., 1., 0.], (n, 1))
    uv = np.frombuffer(d[0x10007], '<f4', n * 2, 8).reshape(-1, 2).astype(np.float64) if 0x10007 in d else np.zeros((n, 2))
    if 0x1000b in d:
        # u32 на вершину, первая кость — в старшем байте (у louie_m байт 3 пробегает всю палитру 0..26, байт 0 — 0/14/22 при весе
        # первой кости 0,4–1,0): в памяти байты в обратном порядке весам
        bi = np.frombuffer(d[0x1000b], np.uint8, n * 4, 4).reshape(-1, 4)
        bi = (bi if GENERIC_BONES_LSB else bi[:, ::-1]).astype(np.int64)
        bones = pal[np.minimum(bi, len(pal) - 1)]
    else: bones = np.zeros((n, 4), np.int64)
    if 0x1000c in d:
        w3 = np.maximum(np.frombuffer(d[0x1000c], '<f4', n * 3, 4).reshape(-1, 3).astype(np.float64), 0.)
        w = np.concatenate([w3, np.maximum(1. - w3.sum(axis=1, keepdims=True), 0.)], axis=1)
        w /= np.maximum(w.sum(axis=1, keepdims=True), 1e-9)
    else: w = np.zeros((n, 4)); w[:, 0] = 1.
    pos[:, 2] *= -1; nrm[:, 2] *= -1
    nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-9)
    return [dict(pos=pos, nrm=nrm, uv=uv, bones=bones, w=w)]


def primitive_strips(strips, primitive, shader):
    """Keep strips intact; triangle-list faces become individual three-vertex strips.

    batch_vertices joins them only with degenerate vertices, preserving winding,
    weights and UVs. Native Dolph hair is 94 faces, not five connected strips.
    """
    if primitive == 1: return strips
    if primitive != 0: raise ValueError(f'{shader}: unsupported primitive {primitive}')
    out = []
    for strip in strips:
        n = len(strip['pos'])
        if n % 3: raise ValueError(f'{shader}: incomplete triangle list ({n} vertices)')
        for i in range(0, n, 3):
            out.append({k: v[i:i + 3] for k, v in strip.items() if isinstance(v, np.ndarray) and len(v) == n})
    return out


def load_strips(root):
    """Skin primitive groups -> list of (shader, [strip dict]) keeping the original PS2 strip order."""
    groups = []
    for skin in (c for c in walk(root) if c.id == 0x10001):
        for pg in skin.children:
            if pg.id != 0x10002: continue
            shader, p = string(pg.data, 4)
            prim, = struct.unpack_from('<I', pg.data, p)
            if prim not in (0, 1): raise ValueError(f'{shader}: unsupported primitive {prim}')
            palc = next((c for c in pg.children if c.id == 0x1000d), None)
            pal = np.frombuffer(palc.data, '<u4', offset=4).astype(np.int64) if palc else np.zeros(1, np.int64)
            native = next((c for c in pg.children if c.id == 0x10012), None)
            if native is None:   # правка 180 (louie_m.p3d): обычная группа Pure3D без родных полос PS2 — одна полоса из списков
                strips = generic_strip(pg, pal)
                indices = next((c for c in pg.children if c.id == 0x1000a), None)
                if indices is not None:
                    idx = np.frombuffer(indices.data, '<u4', offset=4).astype(np.int64)
                    for strip in strips:
                        n = len(strip['pos'])
                        if len(idx) and int(idx.max()) >= n: raise ValueError(f'{shader}: index outside vertices')
                        for k, v in list(strip.items()):
                            if isinstance(v, np.ndarray) and len(v) == n: strip[k] = v[idx]
                groups.append((shader, primitive_strips(strips, prim, shader))); continue
            strips = []; cur = {}
            for cmd, n, v, b, imm, data in unpack(native.data):
                a = imm & 0x3ff
                if a == 0:
                    if 'pos' in cur: strips.append(cur)
                    cur = {}
                elif a == 2 and v == 2 and b == 32: cur['uv'] = np.frombuffer(data, '<f4', n * 2).reshape(-1, 2).astype(np.float64)
                elif a == 4 and v == 3 and b == 32: cur['pos'] = np.frombuffer(data, '<f4', n * 3).reshape(-1, 3).astype(np.float64)
                elif a == 3 and v == 3: cur['nrm'] = (np.frombuffer(data, '<f4', n * 3) if b == 32 else np.frombuffer(data, '<i2', n * 3) / 32767.).reshape(-1, 3).astype(np.float64)
                elif a == 5 and v == 4 and b == 8: cur['si'] = np.frombuffer(data, np.uint8).reshape(-1, 4)[:n].astype(np.int64)
                elif a == 6 and v == 4 and b == 16: cur['sw'] = np.frombuffer(data, '<i2').reshape(-1, 4)[:n] / 32767.
            if 'pos' in cur: strips.append(cur)
            for s in strips:
                n = len(s['pos'])
                s['pos'][:, 2] *= -1; s['nrm'][:, 2] *= -1
                s['nrm'] /= np.maximum(np.linalg.norm(s['nrm'], axis=1, keepdims=True), 1e-9)
                si = s.get('si', np.zeros((n, 4), np.int64))
                bones = pal[np.minimum(si // 4, len(pal) - 1)]
                if 'sw' in s:           # chained ratios -> weights
                    r = np.maximum(s['sw'], 0); w = np.cumprod(r, axis=1)
                else:
                    w = np.zeros((n, 4)); w[:, 0] = 1
                s['bones'] = bones; s['w'] = w
            groups.append((shader, primitive_strips(strips, prim, shader)))
    return groups


def load_blink(root):
    """Texture animation (eye blink): list of (frame, texture name), fps, target shader."""
    for an in (c for c in walk(root) if c.id == 0x121000):
        name, p = string(an.data, 4)
        if an.data[p:p + 3] != b'TEX': continue
        frames, fps = struct.unpack_from('<ff', an.data, p + 4)
        for ch in walk(an):
            if ch.id != 0x121107: continue
            n, = struct.unpack_from('<I', ch.data, 8)
            fr = struct.unpack_from(f'<{n}H', ch.data, 12); q = 12 + 2 * n; names = []
            for _ in range(n):
                s, q = string(ch.data, q); names.append(s.lower())
            return dict(shader=name[4:], frames=list(fr), textures=names, fps=fps, length=frames)
    return None


def load_clips(root, joints, *, key_continuity=False):
    names = [j['name'] for j in joints]; clips = []
    for an in root.children:
        if an.id != 0x121000: continue
        name, p = string(an.data, 4)
        if an.data[p:p + 4] != b'PTRN': continue
        frames, fps, loop = struct.unpack_from('<ffI', an.data, p + 4)
        tracks = [dict(rf=[], rq=[], tf=[], tv=[]) for _ in joints]
        for g in walk(an):
            if g.id != 0x121001: continue
            bone, _ = string(g.data, 4)
            if bone not in names: continue
            tr = tracks[names.index(bone)]
            for ch in g.children:
                b = ch.data
                if ch.id in (0x121111, 0x121105):
                    n, = struct.unpack_from('<I', b, 8); fr = np.frombuffer(b, '<u2', n, 12)
                    if ch.id == 0x121111: v = np.frombuffer(b, '<i2', n * 4, 12 + 2 * n).reshape(-1, 4) / 32767.
                    else: v = np.frombuffer(b, '<f4', n * 4, 12 + 2 * n).reshape(-1, 4).astype(np.float64)
                    q = v[:, [1, 2, 3, 0]].copy(); q[:, :2] *= -1                    # (w,x,y,z) game -> (x,y,z,w) mirrored
                    q /= np.linalg.norm(q, axis=1, keepdims=True)
                    for k in range(1, n):                                             # shortest path between keys
                        dot = float(np.dot(q[k], q[k - 1]))
                        # At a half turn both arcs are equally short. Baked props must continue
                        # the incoming motion instead of reversing at the ambiguous key.
                        tie = key_continuity and abs(dot) <= 1e-7
                        reverse = k > 1 and np.dot(q[k], q[k - 1] - q[k - 2]) < 0
                        if (reverse if tie else dot < 0): q[k] = -q[k]
                    tr['rf'] = fr.astype(int).tolist(); tr['rq'] = q
                elif ch.id == 0x121104:
                    n, = struct.unpack_from('<I', b, 8); fr = np.frombuffer(b, '<u2', n, 12)
                    v = np.frombuffer(b, '<f4', n * 3, 12 + 2 * n).reshape(-1, 3).astype(np.float64).copy(); v[:, 2] *= -1
                    tr['tf'] = fr.astype(int).tolist(); tr['tv'] = v
                elif ch.id in (0x121102, 0x121103):
                    axis, = struct.unpack_from('<H', b, 8); const = np.frombuffer(b, '<f4', 3, 10).astype(np.float64)
                    n, = struct.unpack_from('<I', b, 22); fr = np.frombuffer(b, '<u2', n, 26)
                    dof = 1 if ch.id == 0x121102 else 2
                    vals = np.frombuffer(b, '<f4', n * dof, 26 + 2 * n).reshape(n, dof)
                    v = np.tile(const, (n, 1)); axes = [axis] if dof == 1 else [a for a in range(3) if a != axis]
                    v[:, axes] = vals; v[:, 2] *= -1
                    tr['tf'] = fr.astype(int).tolist(); tr['tv'] = v
        clips.append(dict(name=name, frames=frames, fps=fps, loop=loop, tracks=tracks))
    return clips


# ------------------------------------------------------------------ batching
def vbones(s, i): return {int(b) for b, w in zip(s['bones'][i], s['w'][i]) if w > 1e-4}


def split_strip(s):
    """A strip whose vertices use more than 8 bones is cut into pieces (2 shared vertices, parity kept by a
    duplicated first vertex when a piece starts at an odd position). Homer has none; Lisa and some NPCs do."""
    n = len(s['pos']); used = set()
    for i in range(n): used |= vbones(s, i)
    if len(used) <= MAXB: return [s]
    pieces = []; a = 0
    while a < n - 2:
        u = vbones(s, a) | vbones(s, a + 1); b = a + 1
        while b + 1 < n and len(u | vbones(s, b + 1)) <= MAXB: b += 1; u |= vbones(s, b)
        if b < a + 2: raise ValueError('one triangle needs more than 8 bones (GE limit)')
        idx = list(range(a, b + 1))
        if a & 1: idx = [a] + idx                       # keep the original winding parity
        pieces.append({k: v[idx] for k, v in s.items() if isinstance(v, np.ndarray) and len(v) == n})
        if b >= n - 1: break
        a = b - 1
    return pieces


def build_batches(groups, shaders, tex_index, blink):
    """Strips of one texture are packed greedily into batches whose bone union is <= 8."""
    batches = []
    for shader, strips in groups:
        tname = shaders.get(shader, {}).get('TEX', '')
        if tname not in tex_index and tname in ('char_swatches.bmp', 'char_swatches_lit.bmp1'): tname = 'char_swatches_lit.bmp'
        if tname not in tex_index: raise ValueError(f'unresolved texture {tname} for {shader}')
        open_ = []
        for s in [p for st in strips for p in split_strip(st)]:
            used = set(int(b) for b, w in zip(s['bones'].flatten(), s['w'].flatten()) if w > 1e-4)
            if len(used) > MAXB: raise ValueError(f'strip with {len(used)} bones')
            best = None
            for bt in open_:
                u = bt['bones'] | used
                if len(u) <= MAXB and (best is None or len(u) - len(bt['bones']) < len(best['bones'] | used) - len(best['bones'])): best = bt
            if best is None:
                props = shaders.get(shader, {})
                best = dict(tex=tex_index[tname], bones=set(), strips=[], eyes=bool(blink and shader == blink['shader']),
                            alpha=bool(props.get('ATST', 0) or props.get('BLMD', 0))); open_.append(best)
            best['bones'] |= used; best['strips'].append(s)
        batches += open_
    return batches


def quantize_weights(w, nb):
    """float weights (sum 1) -> nb uint8 summing to exactly 128 (largest remainder)."""
    x = w * 128; q = np.floor(x).astype(int); rest = 128 - q.sum()
    if rest > 0:
        for i in np.argsort(-(x - q))[:rest]: q[i] += 1
    return q


def batch_vertices(bt, scale=None):
    """scale — масштаб s16-позиций модели (pos16_scale); None — позиции f32"""
    bones = sorted(bt['bones']); slot = {b: i for i, b in enumerate(bones)}; nb = len(bones)
    uvo, nro, poo, stride = vertex_layout(nb, scale is not None)
    seq = []                                                     # strips joined with degenerate vertices, parity kept
    for s in bt['strips']:
        n = len(s['pos']); idx = [(s, i) for i in range(n)]
        if seq:
            join = [seq[-1], idx[0]]
            if len(seq) & 1: join.append(idx[0])
            seq += join
        seq += idx
    buf = bytearray(stride * len(seq)); maxerr = 0.
    # GE читает 16-битные UV без знака (tools/uvq.py, правка Роба 43): вся партия-полоса (вершины общие) сдвигается на
    # целое число повторов так, чтобы наименьшие u и v были в [0, 1) — иначе треугольник с u < 0 и u >= 0 рисуется пятном
    allu = np.array([[s['uv'][i][0], 1 - s['uv'][i][1]] for s, i in seq], np.float64) if seq else np.zeros((0, 2))
    ku, kv = (-np.floor(allu.min(0))).tolist() if len(allu) else (0., 0.)
    for k, (s, i) in enumerate(seq):
        o = k * stride; w = np.zeros(nb)
        for b, x in zip(s['bones'][i], s['w'][i]):
            if x > 1e-4: w[slot[int(b)]] += x
        w /= w.sum(); q = quantize_weights(w, nb); maxerr = max(maxerr, np.abs(q / 128 - w).max())
        buf[o:o + nb] = bytes(int(v) for v in q)
        u, v = s['uv'][i]; v = 1 - v                             # Pure3D uv: v up; GE: v = 0 is the first texture row
        struct.pack_into('<HH', buf, o + uvo, min(65535, max(0, int(round((u + ku) * UV_SCALE)))), min(65535, max(0, int(round((v + kv) * UV_SCALE)))))
        if scale is None: struct.pack_into('<bbb', buf, o + nro, *[int(round(max(-1, min(1, c)) * 127)) for c in s['nrm'][i]])
        else: struct.pack_into('<hhh', buf, o + nro, *[int(round(max(-1, min(1, c)) * 32767 / (scale * 32768))) for c in s['nrm'][i]])
        if scale is None: struct.pack_into('<fff', buf, o + poo, *s['pos'][i])
        else: struct.pack_into('<hhh', buf, o + poo, *[int(round(float(c) / scale)) for c in s['pos'][i]])
    vtype = GU_WEIGHTS(nb) | GU_WEIGHT_8BIT | GU_TEXTURE_16BIT | (GU_NORMAL_8BIT | GU_VERTEX_32BITF if scale is None else GU_NORMAL_16BIT | GU_VERTEX_16BIT)
    return dict(bones=bones, nb=nb, stride=stride, nverts=len(seq), data=bytes(buf), vtype=vtype, werr=maxerr)


def swizzle(idx, w, h):
    """T8 swizzle: 16-byte x 8-row blocks (as viewer/main.c swizzleLevel)."""
    a = idx.reshape(h, w); bw = w // 16
    return a.reshape(h // 8, 8, bw, 16).transpose(0, 2, 1, 3).flatten().tobytes()


# ------------------------------------------------------------------ writing
class Blob:
    def __init__(self): self.b = bytearray()
    def at(self, a=16):
        while len(self.b) % a: self.b.append(0)
        return len(self.b)
    def add(self, data, a=16):
        o = self.at(a); self.b += data; return o


def cname(s, n): return s.encode('latin1')[:n - 1].ljust(n, b'\0')


def indexed_vertices(data, stride):
    """Share identical packed vertices within one bone palette, preserving strip order."""
    seen = {}; unique = bytearray(); indices = []
    for off in range(0, len(data), stride):
        key = data[off:off+stride]
        if key not in seen:
            seen[key] = len(seen); unique.extend(key)
        indices.append(seen[key])
    if len(seen)>65535 or len(unique)+2*len(indices)>=len(data):
        return data, None, len(indices)
    return bytes(unique), np.asarray(indices,dtype='<u2').tobytes(), len(seen)


def footbake(path):
    """Модели-96: поправка стоп (viewer/skin.c skinFootLift) считается здесь, на Маке, и ложится в запись клипа (tools/footbake.c, той же функцией, что у игры):
    в игре только чтение, без обхода вершин на PSP при первом появлении клипа. FOOTBAKE_OFF=1 — не запекать (движок посчитает на месте, как в сборке 100); нет cc — предупреждение, файл без числа"""
    if os.environ.get('FOOTBAKE_OFF'): return
    import subprocess
    sh = Path(__file__).resolve().parent / 'footbake.sh'
    try: r = subprocess.run([str(sh), str(path)], capture_output=True, text=True)
    except OSError as e: print('footbake: не запустить (%s): %s без поправки стоп' % (e, path), file=sys.stderr); return
    if r.returncode: print('footbake: ОШИБКА %s: %s' % (path, (r.stderr or r.stdout).strip()[:300]), file=sys.stderr)


def write_chr(path, joints, batches, textures, clips, blink, blink_batch, compact=False, pack_rot=True, pos_scale=None):
    """pack_rot=False — вращения без упаковки «три меньших» (гэги-сцены: рычаг в десятки метров, 0,14° дают сантиметры)"""
    nj, nbt, ntx, ncl = len(joints), len(batches), len(textures), len(clips)
    bl = Blob(); bl.b += bytes(HDR)
    offJ = bl.add(bytes(JNT * nj)); offB = bl.add(bytes(BAT * nbt)); offT = bl.add(bytes(TEXR * ntx)); offC = bl.add(bytes(CLIP * ncl))
    nbl = len(blink['frames']) if blink else 0
    offBl = bl.add(bytes(8 * max(nbl, 1)))
    for j, jt in enumerate(joints):
        inv = jt['inv']; cols = [inv[:3, c] for c in range(4)]
        rec = cname(jt['name'], 32) + struct.pack('<iI', jt['parent'], 0) + struct.pack('<4f', *jt['q']) + struct.pack('<3f', *jt['t']) \
            + struct.pack('<3f', *jt['s']) + struct.pack('<12f', *np.concatenate(cols))
        bl.b[offJ + JNT * j: offJ + JNT * j + len(rec)] = rec
    for i, (name, (w, h, clut, idx)) in enumerate(textures):
        oc = bl.add(clut.tobytes(), 16); op = bl.add(swizzle(idx, w, h), 16)
        rec = cname(name, 24) + struct.pack('<HHIIII', w, h, GU_PSM_T8, oc, op, 1)
        bl.b[offT + TEXR * i: offT + TEXR * i + len(rec)] = rec
    tv = 0; version = 1
    for i, bt in enumerate(batches):
        data, idx, stored = indexed_vertices(bt['data'], bt['stride']) if compact else (bt['data'], None, bt['nverts'])
        ov = bl.add(data, 16); tv += bt['nverts']
        oi = bl.add(idx, 16) if idx is not None else 0
        if oi: version = 2
        bones = bytes(bt['bones']) + bytes(8 - bt['nb'])
        rec = struct.pack('<IIII', bt['vtype'] | (GU_INDEX_16BIT if oi else 0), GU_TRIANGLE_STRIP, bt['nverts'], ov) + struct.pack('<HH', bt['tex'], bt['nb']) + bones \
            + struct.pack('<II', bt['stride'], (1 if bt['eyes'] else 0) | (2 if bt['alpha'] else 0) | (4 if oi else 0))
        rec += struct.pack('<III', oi, stored if oi else 0, 0)
        bl.b[offB + BAT * i: offB + BAT * i + len(rec)] = rec
    nkeys = 0; shared = {}
    def keys(data):                                   # Модели-96: одинаковые массивы ключей (в том числе из разных клипов) лежат один раз
        if ANIMPACK.OLD: return bl.add(data, 4)
        if data not in shared: shared[data] = bl.add(data, 4)
        return shared[data]
    for c, cl in enumerate(clips):
        offTr = bl.add(bytes(TRK * nj)); has_root = 0
        for j, tr in enumerate(cl['tracks']):
            nr, nt = len(tr['rf']), len(tr['tf']); orr = ot = 0; flags = 0
            if nr and pack_rot and ANIMPACK.is_rest_rot(tr['rq'], joints[j]['q']): nr = 0      # все ключи = поза покоя: дорожка не нужна
            if nr:
                mode, data = ANIMPACK.pack_rot(tr['rf'], tr['rq'], pack_rot)
                orr = keys(data); nkeys += nr; flags |= mode
            if nt:
                mode, data = ANIMPACK.pack_trans(tr['tf'], tr['tv'])
                ot = keys(data); nkeys += nt; flags |= mode << 2
                if j == 0: has_root = 2
            struct.pack_into('<HHIII', bl.b, offTr + TRK * j, nr, nt, orr, ot, flags)
        dur = (cl['frames'] - 1) / cl['fps']
        rec = cname(cl['name'], 48) + struct.pack('<fffII', cl['frames'], cl['fps'], dur, (cl['loop'] & 1) | has_root, offTr)
        bl.b[offC + CLIP * c: offC + CLIP * c + len(rec)] = rec
    if blink:
        for k, (fr, tn) in enumerate(zip(blink['frames'], blink['tex'])): struct.pack_into('<HHI', bl.b, offBl + 8 * k, fr, tn, 0)
    allp = np.concatenate([s['pos'] for bt in batches for s in bt['strips']])
    bbox = list(allp.min(0)) + list(allp.max(0))
    offF = bl.add(bytes(4 * max(ncl, 1)), 16)   # Модели-96: таблица поправок стоп на клип — заполняет tools/footbake.c (заголовок pad[1] — смещение, pad[2] — метка)
    bl.at(16)
    struct.pack_into('<4s13I7f3I', bl.b, 0, b'CHR1', version, len(bl.b), nj, nbt, ntx, ncl, nbl, offJ, offB, offT, offC, offBl, 0,
                     *bbox, blink['fps'] if blink else 0., blink_batch, tv, nkeys)
    struct.pack_into('<I', bl.b, 100, offF)
    if pos_scale: struct.pack_into('<f', bl.b, 96, pos_scale)       # Модели-96: pad[0] — масштаб s16-позиций (0 — позиции f32)
    path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(bytes(bl.b))
    footbake(path)
    return len(bl.b), tv, nkeys


# ------------------------------------------------------------------ reader + runtime mirror (same math as viewer/skin.c)
def read_chr(path, offH=0):
    """offH — смещение заголовка CHR1 внутри файла (общий блоб пешеходов traffic: все смещения от начала блоба)"""
    b = path.read_bytes()
    (magic, ver, size, nj, nbt, ntx, ncl, nbl, offJ, offB, offT, offC, offBl, rootJ, *rest) = struct.unpack_from('<4s13I7f3I', b, offH)
    assert magic == b'CHR1' and size == len(b)
    pscale, = struct.unpack_from('<f', b, offH + 96)            # Модели-96: масштаб s16-позиций (0 — f32)
    m = dict(joints=[], batches=[], tex=[], clips=[], blink=[], bbox=rest[:6], blinkFps=rest[6], blinkBatch=rest[7], posScale=pscale)
    for j in range(nj):
        o = offJ + JNT * j
        name = b[o:o + 32].split(b'\0')[0].decode(); parent, = struct.unpack_from('<i', b, o + 32)
        q = np.array(struct.unpack_from('<4f', b, o + 40)); t = np.array(struct.unpack_from('<3f', b, o + 56)); s = np.array(struct.unpack_from('<3f', b, o + 68))
        inv = np.array(struct.unpack_from('<12f', b, o + 80)).reshape(4, 3).T                 # 3x4
        m['joints'].append(dict(name=name, parent=parent, q=q, t=t, s=s, inv=inv))
    for i in range(ntx):
        o = offT + TEXR * i; name = b[o:o + 24].split(b'\0')[0].decode(); w, h, psm, oc, op, swz = struct.unpack_from('<HHIIII', b, o + 24)
        clut = np.frombuffer(b, np.uint8, 1024, oc).reshape(256, 4)
        sw = np.frombuffer(b, np.uint8, w * h, op)
        idx = sw.reshape(h // 8, w // 16, 8, 16).transpose(0, 2, 1, 3).reshape(h, w)
        m['tex'].append(dict(name=name, w=w, h=h, rgba=clut[idx]))
    for i in range(nbt):
        o = offB + BAT * i; vtype, prim, nv, ov, tex, nb = struct.unpack_from('<IIIIHH', b, o)
        bones = list(b[o + 20:o + 20 + nb]); stride, flags = struct.unpack_from('<II', b, o + 28)
        uvo, nro, poo, st = vertex_layout(nb, bool(pscale)); assert st == stride and ((vtype >> 7) & 3) == (2 if pscale else 3) and ((vtype >> 5) & 3) == (2 if pscale else 1)
        stored = struct.unpack_from('<I', b, o+40)[0] if flags & 4 else nv
        raw = np.frombuffer(b, np.uint8, stored * stride, ov).reshape(stored, stride)
        if flags & 4:
            oi = struct.unpack_from('<I', b, o+36)[0]
            indices = np.frombuffer(b, '<u2', nv, oi)
            assert ver==2 and int(indices.max())<stored
            raw = raw[indices]
        w = raw[:, :nb].astype(np.float64) / 128
        uv = raw[:, uvo:uvo + 4].copy().view('<u2').astype(np.float64) / UV_SCALE   # как читает GE: без знака
        nrm = raw[:, nro:nro + 6].copy().view('<i2').astype(np.float64) * (pscale * 32768) / 32767 if pscale else raw[:, nro:nro + 3].copy().view(np.int8).astype(np.float64) / 127
        pos = raw[:, poo:poo + 6].copy().view('<i2').astype(np.float64) * pscale if pscale else raw[:, poo:poo + 12].copy().view('<f4').astype(np.float64)
        m['batches'].append(dict(tex=tex, bones=bones, w=w, uv=uv, nrm=nrm, pos=pos, flags=flags))
    for c in range(ncl):
        o = offC + CLIP * c; name = b[o:o + 48].split(b'\0')[0].decode(); frames, fps, dur, flags, offTr = struct.unpack_from('<fffII', b, o + 48)
        tracks = []
        for j in range(nj):
            nr, nt, orr, ot, tflags = struct.unpack_from('<HHIII', b, offTr + TRK * j)
            tr = dict(rf=None, tf=None)
            if nr: tr['rf'], tr['rq'] = ANIMPACK.decode_rot(tflags & 3, b, orr, nr)
            if nt: tr['tf'], tr['tv'] = ANIMPACK.decode_trans((tflags >> 2) & 3, b, ot, nt)
            tracks.append(tr)
        m['clips'].append(dict(name=name, frames=frames, fps=fps, dur=dur, flags=flags, tracks=tracks))
    for k in range(nbl): m['blink'].append(struct.unpack_from('<HH', b, offBl + 8 * k))
    return m


def _key(fr, vals, f):
    n = len(fr)
    if n == 1 or f <= fr[0]: return vals[0], vals[0], 0.
    if f >= fr[-1]: return vals[-1], vals[-1], 0.
    k = int(np.searchsorted(fr, f, side='right')) - 1
    return vals[k], vals[k + 1], (f - fr[k]) / (fr[k + 1] - fr[k])


def qmix(a, b, u, *, keep_half_turn=False):
    """skin.c qmix: shortest path; nlerp when the keys are close (dot > 0.95), slerp otherwise."""
    d = float(np.dot(a, b)); sb = 1.
    if keep_half_turn and abs(d) <= 1e-7: d = 0.
    elif d < 0: d = -d; sb = -1.
    if d > 0.95: ka, kb = 1 - u, u
    else:
        th = math.acos(min(d, 1.)); s = 1 / math.sin(th); ka = math.sin((1 - u) * th) * s; kb = math.sin(u * th) * s
    q = ka * np.asarray(a) + kb * sb * np.asarray(b); return q / np.linalg.norm(q)


def sample_local(m, c, t):
    """(q, t) per joint for clip c at time t (seconds, clamped). Root translation is left at rest (in place)."""
    cl = m['clips'][c]; f = min(max(t * cl['fps'], 0.), cl['frames'] - 1); out = []
    for j, jt in enumerate(m['joints']):
        tr = cl['tracks'][j]; q = jt['q']; p = jt['t']
        if tr['rf'] is not None:
            a, b2, u = _key(tr['rf'], tr['rq'], f); q = qmix(a, b2, u)
        if tr['tf'] is not None and j != 0:
            a, b2, u = _key(tr['tf'], tr['tv'], f); p = a + (b2 - a) * u
        out.append((q, p))
    return out


def palette(m, local):
    world = []; pal = []
    for j, jt in enumerate(m['joints']):
        q, p = local[j]; L = np.eye(4); L[:3, :3] = m_from_q(q) * jt['s']; L[:3, 3] = p
        W = L if jt['parent'] < 0 else world[jt['parent']] @ L; world.append(W)
        inv = np.eye(4); inv[:3, :] = jt['inv']; pal.append((W @ inv)[:3, :])
    return pal, world


def skin(m, pal):
    """GE skinning emulation: per batch sum_i w_i * (bone_i * v) -> positions, normals, uvs, tex, strip lengths."""
    out = []
    for bt in m['batches']:
        P = np.zeros_like(bt['pos']); N = np.zeros_like(bt['nrm'])
        for s, j in enumerate(bt['bones']):
            B = pal[j]; w = bt['w'][:, s:s + 1]
            P += w * (bt['pos'] @ B[:, :3].T + B[:, 3]); N += w * (bt['nrm'] @ B[:, :3].T)
        out.append((P, N, bt['uv'], bt['tex'], bt['flags']))
    return out


def render(m, skinned, eye, target, size=(320, 400), fov=34., tex_override=None):
    """Tiny z-buffered rasterizer (strips, like GU_TRIANGLE_STRIP, no culling), 1 directional light + ambient."""
    W, H = size; img = np.zeros((H, W, 3)); img[:] = (0.62, 0.7, 0.78); zb = np.full((H, W), np.inf)
    fwd = np.array(target, float) - eye; fwd /= np.linalg.norm(fwd); rt = np.cross(fwd, [0, 1, 0]); rt /= np.linalg.norm(rt); up = np.cross(rt, fwd)
    fl = (H / 2) / math.tan(math.radians(fov / 2)); light = np.array([0.4, 0.8, 0.45]); light /= np.linalg.norm(light)
    # ground grid
    yy, xx = np.mgrid[0:H, 0:W]
    dx = (xx - W / 2 + .5) / fl; dy = -(yy - H / 2 + .5) / fl
    dirs = fwd[None, None] + dx[..., None] * rt + dy[..., None] * up
    with np.errstate(divide='ignore', invalid='ignore'):
        tg = -eye[1] / dirs[..., 1]
    hit = tg > 0; gp = eye + dirs * tg[..., None]
    chk = ((np.floor(gp[..., 0] * 2) + np.floor(gp[..., 2] * 2)) % 2 == 0)
    img[hit] = np.where(chk[hit][:, None], [0.45, 0.55, 0.4], [0.5, 0.62, 0.45])
    zb[hit] = (tg * (dirs @ fwd))[hit]
    for P, N, UV, t, flags in skinned:
        tx = m['tex'][tex_override if (tex_override is not None and flags & 1) else t]
        rel = P - eye; z = rel @ fwd; sx = W / 2 + fl * (rel @ rt) / z; sy = H / 2 - fl * (rel @ up) / z
        light_v = 0.45 + 0.65 * np.maximum(0, (N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)) @ light)
        for i in range(len(P) - 2):
            a, b, c = i, i + 1, i + 2
            x0, y0, x1, y1, x2, y2 = sx[a], sy[a], sx[b], sy[b], sx[c], sy[c]
            den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
            if abs(den) < 1e-9 or min(z[a], z[b], z[c]) < 0.05: continue
            xa, xb = int(max(0, math.floor(min(x0, x1, x2)))), int(min(W - 1, math.ceil(max(x0, x1, x2))))
            ya, yb = int(max(0, math.floor(min(y0, y1, y2)))), int(min(H - 1, math.ceil(max(y0, y1, y2))))
            if xa > xb or ya > yb: continue
            gy, gx = np.mgrid[ya:yb + 1, xa:xb + 1] + .5
            l0 = ((y1 - y2) * (gx - x2) + (x2 - x1) * (gy - y2)) / den; l1 = ((y2 - y0) * (gx - x2) + (x0 - x2) * (gy - y2)) / den; l2 = 1 - l0 - l1
            ins = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
            if not ins.any(): continue
            zz = l0 * z[a] + l1 * z[b] + l2 * z[c]
            sub = zb[ya:yb + 1, xa:xb + 1]; ok = ins & (zz < sub)
            if not ok.any(): continue
            u = l0 * UV[a, 0] + l1 * UV[b, 0] + l2 * UV[c, 0]; v = l0 * UV[a, 1] + l1 * UV[b, 1] + l2 * UV[c, 1]
            ti = (np.floor(v * tx['h']).astype(int) % tx['h'], np.floor(u * tx['w']).astype(int) % tx['w'])
            texel = tx['rgba'][ti]; col = texel[..., :3] / 255.
            if flags & 2: ok = ok & (texel[..., 3] > 0x40)                # alpha test like main.c (GREATER 0x40)
            if not ok.any(): continue
            li = l0 * light_v[a] + l1 * light_v[b] + l2 * light_v[c]
            sub[ok] = zz[ok]; reg = img[ya:yb + 1, xa:xb + 1]; reg[ok] = np.minimum(col[ok] * li[ok][:, None], 1)
    return (img * 255).astype(np.uint8)


def preview(chrpath, outdir):
    from PIL import Image, ImageDraw
    m = read_chr(chrpath); outdir.mkdir(parents=True, exist_ok=True)
    names = [c['name'] for c in m['clips']]
    def find(s):
        for i, n in enumerate(names):
            if n.endswith(s): return i
        return None
    bb = m['bbox']; cy = (bb[1] + bb[4]) / 2; h = bb[4] - bb[1]
    views = {'front': np.array([0., cy + .2, 4.6]), 'side': np.array([4.6, cy + .2, 0.]), 'back34': np.array([-3.2, cy + 1.1, -3.4])}
    rows = [('bind', None, [0.])]
    for s, ts in [('loco_idle_rest', [0., .5, 1.]), ('loco_walk', [0., .25, .5, .75]), ('loco_run', [0., .13, .27, .4]),
                  ('jump_idle_in_air', [0., .3]), ('get_out_of_car', [0., .2, .4]), ('get_into_car', [0., .3, .57]),
                  ('in_car_idle', [0.]), ('jump_kick', [.3]), ('idle_dancing', [1., 2.])]:
        c = find(s)
        if c is not None: rows.append((names[c], c, ts))
    sheets = []
    for vname, eye in views.items():
        tiles = []
        for label, c, ts in rows:
            for t in ts:
                local = [(j['q'], j['t']) for j in m['joints']] if c is None else sample_local(m, c, t)
                pal, _ = palette(m, local); sk = skin(m, pal)
                im = Image.fromarray(render(m, sk, eye, [0, cy, 0], size=(200, 250)))
                ImageDraw.Draw(im).text((4, 4), f'{label[:28]} t={t:.2f}', fill=(0, 0, 0))
                tiles.append(im)
        cols = 8; rws = (len(tiles) + cols - 1) // cols
        sheet = Image.new('RGB', (cols * 200, rws * 250), (255, 255, 255))
        for i, im in enumerate(tiles): sheet.paste(im, ((i % cols) * 200, (i // cols) * 250))
        p = outdir / f'preview_{vname}.png'; sheet.save(p); sheets.append(p)
    # eye blink frames, close-up
    tiles = []
    hj = next((i for i, j in enumerate(m['joints']) if j['name'] == 'Head'), None)
    if m['blink'] and hj is not None:
        pal, world = palette(m, [(j['q'], j['t']) for j in m['joints']]); sk = skin(m, pal)
        hp = world[hj][:3, 3]
        for fr, tx in m['blink']:
            im = Image.fromarray(render(m, sk, hp + np.array([0.25, 0.2, 1.0]), hp + np.array([0, .12, 0]), size=(160, 160), fov=25, tex_override=tx))
            ImageDraw.Draw(im).text((4, 4), f'blink f{fr} tex{tx}', fill=(0, 0, 0)); tiles.append(im)
        sheet = Image.new('RGB', (160 * len(tiles), 160))
        for i, im in enumerate(tiles): sheet.paste(im, (i * 160, 0))
        p = outdir / 'preview_blink.png'; sheet.save(p); sheets.append(p)
    return sheets


# ------------------------------------------------------------------ main
# ---- правка 143(а): NPC-модели несут только то, что может проиграть движок --------------------------------------------
# Персонаж миссии (viewer/msn_view.c npcFiles) играет ровно три вещи: стойку и шаг (msn_view.c skinFindClip "loco_idle_rest",
# "loco_walk" — список ниже вынимается из исходника, не вписан руками) и жест реплики разговора — клип из
# AddAmbientNpcAnimation скриптов диска (game_msn.c talk.anim, skinFindClip по суффиксу). Список жестов выводится из
# scripts/missions/level*/*.mfk всех уровней (там только dialogue_* и "none"). Остальные клипы рига (npd_a / bart_a …)
# движок не открывает. homer.shr (игрок, tools/homer_export.py) это не касается.
NPC_CODE = ROOT / 'viewer/msn_view.c'
NPC_SCRIPT_OPS = ('AddAmbientNpcAnimation', 'AddNpcAnimation')
DROP_FOR_TEST = set()


def npc_code_clips(src=NPC_CODE):
    import re
    return sorted(set(re.findall(r'skinFindClip\([^,()]+,\s*"([A-Za-z0-9_]+)"\)', src.read_text(encoding='utf-8'))))


def npc_script_clips(game=GAME):
    sys.path.insert(0, str(ROOT / 'tools/missions'))
    import mfk   # noqa: E402
    out = set()
    for f in sorted((game / 'scripts/missions').glob('level*/*.mfk')):
        calls, _ = mfk.parse_file(f)
        for c in calls:
            if c.op in NPC_SCRIPT_OPS and c.args:
                a = str(c.args[0]).lower()
                if a and a != 'none': out.add(a)
    return sorted(out)


def npc_clip_names():
    return sorted(set(npc_code_clips()) | set(npc_script_clips()))


# Аудит 09.10 п. 20 (В61): водители машин миссий. Диск: 5-й аргумент AddStageVehicle (scripts/missions/level*/*.mfk) — персонаж
# за рулём; поза водителя у всех ригов одна — art/chars/*.cho «in car animations borrowed from ndr»: in_car_idle_driver ->
# ndr_in_car_idle (art/chars/ndr_a.p3d). Пакет персонажа-водителя (--npc) получает клипы ndr_a, которые называет код (msn_view.c)
NDR_ANIMS = 'art/chars/ndr_a.p3d'


def npc_script_drivers(game=GAME):
    sys.path.insert(0, str(ROOT / 'tools/missions'))
    import mfk   # noqa: E402
    out = set()
    for f in sorted((game / 'scripts/missions').glob('level*/*.mfk')):
        calls, _ = mfk.parse_file(f)
        for c in calls:
            if c.op == 'AddStageVehicle' and len(c.args) >= 5:
                a = str(c.args[4]).lower()
                if a and a != 'none': out.add(a)
    return sorted(out)


def clip_matches(clip, want):
    """skinFindClip (viewer/skin.c): exact name, or '<prefix>_' + want."""
    return clip == want or (len(clip) > len(want) and clip.endswith('_' + want))


def export(name, model, anims, out, keep=None):
    mroot = read(GAME / model); aroot = read(GAME / anims)
    images = {}; images.update(load_images(read(GAME / 'art/chars/global.p3d'))); images.update(load_images(mroot))
    joints = load_skeleton(mroot); shaders = load_shaders(mroot); groups = load_strips(mroot); blink = load_blink(mroot)
    clips = load_clips(aroot, joints)
    if keep is not None and name.lower() in npc_script_drivers():   # водитель машины миссии: поза ndr (п. 20)
        have = {cl['name'] for cl in clips}
        clips += [cl for cl in load_clips(read(GAME / NDR_ANIMS), joints) if cl['name'] not in have]
    if keep is not None:
        drop = [w for w in keep if w in DROP_FOR_TEST]
        keep = [w for w in keep if w not in drop]
        n0 = len(clips); clips = [cl for cl in clips if any(clip_matches(cl['name'], w) for w in keep)]
        print(f'  --npc: kept {len(clips)} of {n0} clips (wanted {len(keep)}: {keep}){" DROPPED FOR TEST " + str(drop) if drop else ""}')
    # textures actually used: each group's texture + blink frames
    used = []
    def tex_id(tn):
        if tn not in images: raise ValueError(f'missing texture {tn}')
        if tn not in used: used.append(tn)
        return used.index(tn)
    tex_index = {}
    for shader, _ in groups:
        tn = shaders.get(shader, {}).get('TEX', '')
        if tn not in images and tn in ('char_swatches.bmp', 'char_swatches_lit.bmp1'): tn = 'char_swatches_lit.bmp'
        tex_index[tn] = tex_id(tn)
    if blink: blink['tex'] = [tex_id(t) for t in blink['textures']]
    batches = build_batches(groups, shaders, tex_index, blink)
    pscale = pos16_scale([s['pos'] for bt in batches for s in bt['strips']])
    batches = [dict(bt, **batch_vertices(bt, pscale)) for bt in batches]
    blink_batch = next((i for i, bt in enumerate(batches) if bt['eyes']), 0xffffffff)
    textures = [(t, images[t]) for t in used]
    size, tv, nkeys = write_chr(out, joints, batches, textures, clips, blink, blink_batch, compact=not os.environ.get('CHAR_INDEXED_OLD'), pos_scale=pscale)
    # report
    src_v = sum(len(s['pos']) for _, st in groups for s in st); src_s = sum(len(st) for _, st in groups)
    tris = sum(len(s['pos']) - 2 for _, st in groups for s in st)
    print(f'{name}: {len(joints)} joints, {src_s} PS2 strips / {src_v} verts (<= {tris} strip tris incl. degenerate) '
          f'-> {len(batches)} GE batches / {tv} verts')
    for i, bt in enumerate(batches):
        print(f'  batch {i:2d}: tex {bt["tex"]} bones {bt["nb"]} {[joints[b]["name"] for b in bt["bones"]]} verts {bt["nverts"]} '
              f'stride {bt["stride"]}{" (eyes)" if bt["eyes"] else ""}{" (alpha test)" if bt["alpha"] else ""}  max weight quant err {bt["werr"]:.4f}')
    maxang = 0.
    for cl in clips:
        for tr in cl['tracks']:
            if len(tr['rf']) > 1:
                d = np.abs(np.sum(tr['rq'][1:] * tr['rq'][:-1], 1)); maxang = max(maxang, math.degrees(2 * math.acos(min(1, d.min()))))
    print(f'  {len(clips)} clips, {nkeys} keys, largest rotation between neighbouring keys {maxang:.1f} deg; '
          f'{len(textures)} textures {[t for t, _ in textures]}; blink {blink["frames"] if blink else None}')
    print(f'  wrote {out.relative_to(ROOT) if out.is_relative_to(ROOT) else out}: {size:,} bytes')
    for cl in clips:
        f = cl['frames']; print(f'    {cl["name"]:44s} {f:4.0f} frames @ {cl["fps"]:.0f} fps = {(f - 1) / cl["fps"]:.2f} s')
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--name', default='homer')
    ap.add_argument('--model'); ap.add_argument('--anims')
    ap.add_argument('--out'); ap.add_argument('--preview')
    ap.add_argument('--max-bones', type=int, default=8, help='bones per GE batch = weights per vertex (2..8)')
    ap.add_argument('--npc', action='store_true', help='mission NPC: keep only clips named by msn_view.c and the disc scripts')
    ap.add_argument('--drop-clip', action='append', default=[], help='TEST ONLY (gate red control): leave this wanted clip out')
    a = ap.parse_args()
    DROP_FOR_TEST.update(a.drop_clip)
    global MAXB; MAXB = max(2, min(8, a.max_bones))
    out = Path(a.out) if a.out else ROOT / f'build/chars/{a.name}.chr'
    export(a.name, a.model or f'art/chars/{a.name}_m.p3d', a.anims or f'art/chars/{a.name}_a.p3d', out,
           keep=npc_clip_names() if a.npc else None)
    if a.preview:
        for p in preview(out, Path(a.preview)): print('  preview', p)


if __name__ == '__main__':
    main()
