#!/usr/bin/env python3
"""№1352: пипетка по коже Красти на крупном снимке стекла автомата (tests/vend1328/glassshot.sh).
  glassmeasure.py СНИМОК.png [ИСТОЧНИК_64x64.png]     (рядом СНИМОК.json: углы стекла, камера, цвет вершин)
Строит по камере и углам стекла ожидаемую картинку «текстура диска × цвет вершин» и сравнивает со снимком по пикселям кожи (текстура диска ≈ (248,226,169) ± допуск, непрозрачные, не у края).
Печатает: цвет кожи на снимке, ожидаемый с цветом вершин, чистый диска (без тона), ΔE (CIE76) снимок↔чистый и снимок↔ожидаемый, отношение снимок/ожидаемый по каналам (что добавляет движок сверх цвета вершин).
Код выхода 0, число пикселей кожи >= 200 (иначе гейту нечего мерить)."""
import sys, json, math
import numpy as np
from PIL import Image

def lab(rgb):
    c = np.asarray(rgb, float) / 255.0
    c = np.where(c > 0.04045, ((c + 0.055) / 1.055) ** 2.4, c / 12.92)
    M = np.array([[0.4124, 0.3576, 0.1805], [0.2126, 0.7152, 0.0722], [0.0193, 0.1192, 0.9505]])
    xyz = (c @ M.T) / np.array([0.95047, 1.0, 1.08883])
    f = np.where(xyz > 0.008856, np.cbrt(xyz), 7.787 * xyz + 16 / 116)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]), 200 * (f[..., 1] - f[..., 2])], -1)

def dE(a, b): return float(np.linalg.norm(lab(a) - lab(b)))

def measure(png, src='build/vend1328/kg_src64.png'):
    J = json.load(open(png.replace('.png', '.json')))
    img = np.asarray(Image.open(png).convert('RGB')).astype(float); H, W = img.shape[:2]
    tex = np.asarray(Image.open(src).convert('RGBA')).astype(float)
    gv = np.array(J['gv']); eye = np.array(J['eye']); look = np.array(J['look']); fov = math.radians(J['fov'])
    vc = J['color']; c = int(vc, 16); vtx = np.array([(c & 255), (c >> 8) & 255, (c >> 16) & 255], float) / 255.0   # ABGR
    P0, P1, P2 = gv[0, :3], gv[1, :3], gv[2, :3]; e1, e2 = P1 - P0, P2 - P0
    nrm = np.cross(e1, e2); nrm /= np.linalg.norm(nrm)
    fwd = look / np.linalg.norm(look); right = np.cross(fwd, [0, 1, 0]); right /= np.linalg.norm(right); up = np.cross(right, fwd)
    f = (H / 2) / math.tan(fov / 2)
    best = None
    for rs in (1, -1):
        for vflip in (0, 1):
            ys, xs = np.mgrid[0:H, 0:W]
            d = fwd[None, None] + ((xs + .5 - W / 2) / f)[..., None] * (rs * right)[None, None] - ((ys + .5 - H / 2) / f)[..., None] * up[None, None]
            den = d @ nrm; t = ((P0 - eye) @ nrm) / np.where(np.abs(den) < 1e-9, 1e-9, den)
            P = eye[None, None] + t[..., None] * d
            Q = P - P0; M = np.array([e1, e2]).T; sol = np.linalg.lstsq(M, Q.reshape(-1, 3).T, rcond=None)[0]
            a, b = sol[0].reshape(H, W), sol[1].reshape(H, W)
            ok = (a >= 0) & (a <= 1) & (b >= 0) & (b <= 1) & (t > 0)
            tu = np.clip((a * 64).astype(int), 0, 63); tv = np.clip(((b if vflip else 1 - b) * 64).astype(int), 0, 63)
            T = tex[tv, tu]; exp = T[..., :3] * vtx[None, None]
            m = ok & (T[..., 3] > 250)
            if m.sum() < 500: continue
            L = lambda x: x.mean(-1)
            cc = np.corrcoef(L(exp)[m], L(img)[m])[0, 1]
            if best is None or cc > best[0]: best = (cc, rs, vflip, ok, T, exp, tu, tv)
    if best is None: print('стекло не попало в кадр'); return None
    cc, rs, vflip, ok, T, exp, tu, tv = best
    skin = (np.abs(T[..., :3] - np.array([248, 226, 169])).sum(-1) < 40) & (T[..., 3] > 250) & ok
    # не у края: все 4 соседа по текстуре тоже кожа
    sk = skin.copy()
    n = int(sk.sum())
    if n < 200: print('кожи в кадре %d пикселей: мало' % n); return None
    r_ = img[sk].mean(0); e_ = exp[sk].mean(0); p_ = T[..., :3][sk].mean(0)
    out = dict(pixels=n, corr=round(float(cc), 3), render=[round(x, 1) for x in r_], expected_with_vertex=[round(x, 1) for x in e_], disk_texture=[round(x, 1) for x in p_],
               dE_render_vs_disk=round(dE(r_, p_), 2), dE_render_vs_expected=round(dE(r_, e_), 2), ratio_render_over_expected=[round(float(x), 3) for x in r_ / np.maximum(e_, 1)], vertex=[round(float(x), 3) for x in vtx])
    print(json.dumps(out, ensure_ascii=False)); return out

if __name__ == '__main__':
    r = measure(sys.argv[1], *(sys.argv[2:3])); sys.exit(0 if r else 1)
