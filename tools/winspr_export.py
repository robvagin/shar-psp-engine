#!/usr/bin/env python3
"""№1099 (Роб 02.11.2026): светящиеся окна вечернего и ночного города — вдали ровными цветными прямоугольниками, одной пачкой (viewer/winspr.h).
Окно у этих уровней — наклейка-текстура 32x32 (тёмная рамка 2–3 текселя и яркая середина: L4 и L5 жёлтая, L7 и L1/L4 малая оранжевая). Издалека рамка
меньше пикселя, мип смешивает окна в пятно и рамка мигает при движении. Здесь из level.shr (куски города, треугольники этих текстур) считаются
ЯРКИЕ СЕРЕДИНЫ каждого окна (обрезка треугольника по внутреннему прямоугольнику каждого повтора текстуры) — готовые треугольники мира с цветом
середины. Файл рядом с уровнем, `winspr-l<N>.shr`, формат WSP1 (little endian):
  заголовок 48 Б: 'WSP1', u32 версия 1, u32 уровень, u32 ntex, u32 npieces, u32 ncell (клеток уровня: отпечаток level.shr), u32 nCellRec, u32 offCellRec,
                  u32 nTri, u32 offTri, u32 nTex, u32 offTex, u32 0
  запись клетки 12 Б: u32 номер клетки уровня, u32 первый треугольник, u32 число
  треугольник 40 Б: f32 x,y,z × 3 (оси движка), u32 цвет ABGR
  записи текстур-окон 4 Б: i32 номер текстуры (движок не рисует куски этих текстур дальше дальней плоскости: viewer/winspr.h WINSPR_SKIP)
Окна находятся по ТЕКСТУРЕ, а не по номеру: 32x32, тёмная рамка (средняя яркость края < 0,15), яркая тёплая середина (> 0,55), не меньше 30 треугольников.
Выключатель WINSPR1099_OLD=1 в окружении — файл не пишется.
    python tools/winspr_export.py --level 4 --level-shr build/levels/l4/level.shr --out build/winspr-l4.shr"""
import argparse, collections, os, struct, sys
from pathlib import Path
import numpy as np
ROOT = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(ROOT / 'tools'))
import blendmip_check as B, shr7read as S

MIN_TRIS = 30


def glow_textures(T):
    """-> {номер: (внутренний прямоугольник (u0, u1, v0, v1) в долях, цвет ABGR)}"""
    out = {}
    for i, (w, h, pal, lv) in enumerate(T):
        if (w, h) != (32, 32): continue
        a = np.asarray(lv[0]).reshape(h, w, 4).astype(float)
        lum = (0.3 * a[..., 0] + 0.59 * a[..., 1] + 0.11 * a[..., 2]) / 255
        c = lum[h // 4:3 * h // 4, w // 4:3 * w // 4]; e = np.concatenate([lum[0], lum[-1], lum[:, 0], lum[:, -1]])
        warm = (a[..., 0] - a[..., 2])[h // 4:3 * h // 4, w // 4:3 * w // 4].mean() / 255
        if not (c.mean() > 0.55 and e.mean() < 0.15 and warm > 0.25 and a[..., 3].mean() > 200): continue
        bright = lum > 0.5; rows = np.where(bright.any(1))[0]; cols = np.where(bright.any(0))[0]
        rgb = a[bright][:, :3].mean(0).round().astype(int)
        out[i] = ((cols.min() / w, (cols.max() + 1) / w, rows.min() / h, (rows.max() + 1) / h), 0xff000000 | (int(rgb[2]) << 16) | (int(rgb[1]) << 8) | int(rgb[0]))
    return out


def clip(poly, axis, val, keep_ge):
    """Sutherland–Hodgman: poly — список (u, v, w0, w1, w2) где w — барицентрические веса вершин исходного треугольника"""
    out = []
    for k in range(len(poly)):
        p, q = poly[k], poly[(k + 1) % len(poly)]
        ip, iq = (p[axis] >= val) == keep_ge, (q[axis] >= val) == keep_ge
        if ip: out.append(p)
        if ip != iq:
            t = (val - p[axis]) / (q[axis] - p[axis]); out.append(tuple(p[j] + (q[j] - p[j]) * t for j in range(5)))
    return out


def window_tris(P, UV, rect):
    """треугольник (3x3 мира, 3x2 UV) -> список треугольников середин окон (по каждому повтору текстуры)"""
    u0, u1, v0, v1 = rect; res = []
    umin, umax, vmin, vmax = UV[:, 0].min(), UV[:, 0].max(), UV[:, 1].min(), UV[:, 1].max()
    for iu in range(int(np.floor(umin + 1e-6)), int(np.ceil(umax - 1e-6))):
        for iv in range(int(np.floor(vmin + 1e-6)), int(np.ceil(vmax - 1e-6))):
            poly = [(UV[k, 0], UV[k, 1], 1.0 if k == 0 else 0.0, 1.0 if k == 1 else 0.0, 1.0 if k == 2 else 0.0) for k in range(3)]
            for axis, val, ge in ((0, iu + u0, True), (0, iu + u1, False), (1, iv + v0, True), (1, iv + v1, False)):
                poly = clip(poly, axis, val, ge)
                if len(poly) < 3: break
            if len(poly) < 3: continue
            pts = [np.array(p[2:5]) @ P for p in poly]
            for k in range(1, len(pts) - 1):
                t = np.array([pts[0], pts[k], pts[k + 1]])
                if np.linalg.norm(np.cross(t[1] - t[0], t[2] - t[0])) > 1e-6: res.append(t)
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--level', type=int, required=True); ap.add_argument('--level-shr', required=True); ap.add_argument('--out', required=True)
    a = ap.parse_args()
    if os.environ.get('WINSPR1099_OLD') == '1': print('WINSPR1099_OLD=1: файл не пишется'); return 1
    b = Path(a.level_shr).read_bytes(); T = B.textures(b); h = struct.unpack_from('<26I', b, 0); ntex, npc, ncell = h[1], h[2], h[20]
    G = glow_textures(T); D = S.load(a.level_shr)
    cnt = collections.Counter(int(t) for t in D['TEX'])
    G = {t: v for t, v in G.items() if cnt.get(t, 0) >= MIN_TRIS}
    bycell = collections.defaultdict(list); n_in = 0
    for t, (rect, col) in G.items():
        for i in np.where(D['TEX'] == t)[0]:
            n_in += 1
            for tri in window_tris(D['P'][i], D['UV'][i], rect): bycell[int(D['CELL'][i])].append((tri, col))
    if not G: print(f'уровень {a.level}: окон-светильников нет, файл не пишется'); return 2
    cells = sorted(bycell); body = bytearray(); recs = []
    for ci in cells:
        recs.append((ci, len(body) // 40, len(bycell[ci])))
        for tri, col in bycell[ci]: body += struct.pack('<9fI', *tri.flatten(), col)
    ntri = len(body) // 40; offC = 48; offT = offC + 12 * len(recs); offX = offT + len(body)
    head = struct.pack('<4s11I', b'WSP1', 1, a.level, ntex, npc, ncell, len(recs), offC, ntri, offT, len(G), offX) + b'\0' * 0
    head = head.ljust(48, b'\0')
    out = head + b''.join(struct.pack('<3I', *r) for r in recs) + bytes(body) + b''.join(struct.pack('<i', t) for t in sorted(G))
    Path(a.out).parent.mkdir(parents=True, exist_ok=True); Path(a.out).write_bytes(out)
    print(f'уровень {a.level}: текстуры-окна {sorted(G)}; исходных треугольников {n_in}, середин окон (треугольников) {ntri} в {len(recs)} клетках; цвета {[hex(v[1]) for v in G.values()]}; -> {a.out} {len(out)} Б')
    return 0


if __name__ == '__main__':
    sys.exit(main())
