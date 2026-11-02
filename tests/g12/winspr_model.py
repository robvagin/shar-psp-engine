#!/usr/bin/env python3
"""№1099: МОДЕЛЬ кадра на Маке (не эмулятор, не PSP): окна одного фасада уровня до и после, с трёх расстояний. Слева — как сейчас (текстура окна с рамкой, мип по
размеру на экране, билинейно между уровнями), справа — как после правки (дальше WINSPR_DIST ровные середины окон цветом данных, ближе — как слева). Соседних домов, тумана и глубины нет:
только окна и стена за ними (цвет стены условный). Нужна, чтобы показать, КАК выглядит приём, а не мигание — мигание ловит только железо/эмулятор.
python tests/g12/winspr_model.py LEVEL.shr WINSPR.shr OUT.png [d1,d2,d3]"""
import sys, os, struct, collections
import numpy as np
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
R = Path(__file__).resolve().parents[2]; sys.path.insert(0, str(R / 'tools'))
import blendmip_check as B, shr7read as S, winspr_export as W

W_, H_ = 480, 272
def project(P, eye, fwd, fov=0.9):
    up0 = np.array([0, 1., 0]); right = np.cross(fwd, up0); right /= np.linalg.norm(right); up = np.cross(right, fwd)
    d = P - eye; z = d @ fwd; f = (H_ / 2) / np.tan(fov / 2)
    return np.c_[W_ / 2 + f * (d @ right) / z, H_ / 2 - f * (d @ up) / z], z, f

def sample(T, tex, u, v, lod):
    w, h, pal, lv = T[tex]; lod = max(0.0, min(lod, len(lv) - 1.001)); l0 = int(lod); fr = lod - l0
    out = 0
    for lev, wt in ((l0, 1 - fr), (l0 + 1, fr)):
        if wt <= 0: continue
        n = np.asarray(lv[lev]).size // 4; side_w = max(1, w >> lev); side_h = max(1, h >> lev); a = np.asarray(lv[lev]).reshape(side_h, side_w, 4)
        x = (u * side_w - .5); y = (v * side_h - .5); x0 = np.floor(x).astype(int); y0 = np.floor(y).astype(int); fx = (x - x0)[..., None]; fy = (y - y0)[..., None]
        g = lambda xx, yy: a[yy % side_h, xx % side_w].astype(float)
        c = (g(x0, y0) * (1 - fx) + g(x0 + 1, y0) * fx) * (1 - fy) + (g(x0, y0 + 1) * (1 - fx) + g(x0 + 1, y0 + 1) * fx) * fy
        out = out + c * wt
    return out

def render(D, T, win_tris, tris_idx, eye, fwd, mode, dist_n, wallcol=(70, 40, 25)):
    img = np.zeros((H_, W_, 3)); img[:] = wallcol
    if mode == 'old' or True:
        for i in tris_idx:
            P = D['P'][i]; UV = D['UV'][i]; cen = P.mean(0); far = np.linalg.norm(cen - eye) >= dist_n
            if mode == 'new' and far: continue   # дальше порога текстурой не рисуем (правая картинка: плоские ниже)
            sp, z, f = project(P, eye, fwd)
            if (z <= .5).any(): continue
            x0, x1 = int(max(0, np.floor(sp[:, 0].min()))), int(min(W_ - 1, np.ceil(sp[:, 0].max())))
            y0, y1 = int(max(0, np.floor(sp[:, 1].min()))), int(min(H_ - 1, np.ceil(sp[:, 1].max())))
            if x1 < x0 or y1 < y0: continue
            xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
            A = np.array([[sp[1, 0] - sp[0, 0], sp[2, 0] - sp[0, 0]], [sp[1, 1] - sp[0, 1], sp[2, 1] - sp[0, 1]]]); det = np.linalg.det(A)
            if abs(det) < 1e-6: continue
            b = np.linalg.solve(A, np.array([xs.ravel() - sp[0, 0], ys.ravel() - sp[0, 1]])); l1, l2 = b[0].reshape(xs.shape), b[1].reshape(xs.shape); l0 = 1 - l1 - l2
            m = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
            if not m.any(): continue
            w0, w1, w2 = l0 / z[0], l1 / z[1], l2 / z[2]; s = w0 + w1 + w2   # перспективно-верная интерполяция UV
            u = (w0 * UV[0, 0] + w1 * UV[1, 0] + w2 * UV[2, 0]) / s; v = (w0 * UV[0, 1] + w1 * UV[1, 1] + w2 * UV[2, 1]) / s
            tex = int(D['TEX'][i]); tw = T[tex][0]
            area_px = abs(det) / 2; area_uv = abs(np.cross(UV[1] - UV[0], UV[2] - UV[0])) / 2
            lod = 0.5 * np.log2(max(area_uv * tw * T[tex][1] / max(area_px, 1e-6), 1e-6))   # текселей на пиксель
            c = sample(T, tex, u[m] % 1.0 if False else u[m], v[m], lod)
            img[ys[m].astype(int), xs[m].astype(int)] = c[:, :3]
    if mode == 'new':
        for t, col in win_tris:
            cen = t.mean(0)
            if np.linalg.norm(cen - eye) < dist_n: continue
            sp, z, f = project(t, eye, fwd)
            if (z <= .5).any(): continue
            Image_ = Image.fromarray(img.clip(0, 255).astype(np.uint8)); d = ImageDraw.Draw(Image_)
            d.polygon([tuple(p) for p in sp], fill=(col & 255, (col >> 8) & 255, (col >> 16) & 255)); img = np.asarray(Image_).astype(float)
    return img.clip(0, 255).astype(np.uint8)

def main():
    lvl, wsp, out = sys.argv[1:4]; dists = [float(x) for x in (sys.argv[4] if len(sys.argv) > 4 else '35,80,150').split(',')]
    b = Path(lvl).read_bytes(); T = B.textures(b); D = S.load(lvl); G = W.glow_textures(T)
    cnt = collections.Counter(int(t) for t in D['TEX']); G = {t: v for t, v in G.items() if cnt[t] >= W.MIN_TRIS}
    idx = np.where(np.isin(D['TEX'], list(G)))[0]
    cells = collections.Counter(D['CELL'][idx].tolist()); ci = cells.most_common(1)[0][0]
    sel = [i for i in idx if D['CELL'][i] == ci]
    P = D['P'][sel]; n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]); n /= np.linalg.norm(n, axis=1)[:, None] + 1e-12
    vert = np.abs(n[:, 1]) < .3; nh = n[vert].mean(0); nh[1] = 0; nh /= np.linalg.norm(nh)
    tgt = P[vert].mean((0, 1)); sel = [i for i, v in zip(sel, vert) if v]
    # плоские середины этих же окон (из экспортёра)
    flat = []
    for i in sel:
        for tri in W.window_tris(D['P'][i], D['UV'][i], G[int(D['TEX'][i])][0]): flat.append((tri, G[int(D['TEX'][i])][1] & 0xffffff))
    try: fnt = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf', 14)
    except OSError: fnt = None
    im = Image.new('RGB', (2 * W_ + 12, len(dists) * (H_ + 22)), (20, 20, 30)); dr = ImageDraw.Draw(im)
    for k, d in enumerate(dists):
        for sgn in (1, -1):
            eye = tgt + nh * d * sgn + np.array([0, 4., 0]); fwd = tgt - eye; fwd /= np.linalg.norm(fwd)
            if (fwd @ nh) * sgn < 0: pass
            break
        old = render(D, T, flat, sel, eye, fwd, 'old', 50.0); new = render(D, T, flat, sel, eye, fwd, 'new', 50.0)
        y = k * (H_ + 22); im.paste(Image.fromarray(old), (0, y + 20)); im.paste(Image.fromarray(new), (W_ + 12, y + 20))
        dr.text((4, y + 4), f'{d:.0f} м · как сейчас (текстура с рамкой, мип)', fill=(255, 255, 255), font=fnt); dr.text((W_ + 16, y + 4), f'{d:.0f} м · после (дальше 50 м ровные середины окон)', fill=(255, 255, 255), font=fnt)
    im.save(out); print('->', out, im.size, 'окон-треугольников', len(sel), 'плоских', len(flat))

if __name__ == '__main__':
    main()
