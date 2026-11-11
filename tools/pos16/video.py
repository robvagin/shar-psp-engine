#!/usr/bin/env python3
"""Модели-96: видео «до / после» для позиций вершин f32 -> s16. Слева «до», справа «после»; три ряда: общий вид, лицо, рука.
Тот же растеризатор, что у preview (tools/char_export.py), модели читает read_chr (так же, как читатель игры: целое × масштаб модели).
  video.py SPEC.json OUT.mp4 [--frames N]     SPEC: [{"label","group","old","new","oldOff","newOff"}...]  (oldOff/newOff — смещение заголовка CHR1 в блобе пешеходов)
Под каждой моделью: геометрия (вершины) в КБ до и после и худший сдвиг точки в позе, мм (из tests/pos16)."""
import sys, json, struct, subprocess, tempfile, shutil, math
from pathlib import Path
from multiprocessing import Pool
import numpy as np
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import char_export as ce
from PIL import Image, ImageDraw, ImageFont

FPS = 30; PW = PH = 360; SS = 1.0
FONT = next((f for f in ('/System/Library/Fonts/Supplemental/Arial.ttf', '/Library/Fonts/Arial.ttf', '/System/Library/Fonts/Helvetica.ttc') if Path(f).exists()), None)
SPEC = []; CACHE = {}

def _init(spec):
    global SPEC
    SPEC = spec

def model(i, which):
    k = (i, which)
    if k not in CACHE:
        sp = SPEC[i]; CACHE[k] = ce.read_chr(Path(sp[which]), sp.get(which + 'Off', 0))
    return CACHE[k]

def geom_kb(path, off):
    b = Path(path).read_bytes(); magic, ver, size, nj, nbt, ntx, ncl, nbl, offJ, offB = struct.unpack_from('<4s9I', b, off); tot = 0
    for i in range(nbt):
        o = offB + 48 * i; nv = struct.unpack_from('<I', b, o + 8)[0]; stride, fl = struct.unpack_from('<II', b, o + 28); oi, stored, _ = struct.unpack_from('<III', b, o + 36)
        tot += (stored * stride + nv * 2) if fl & 4 else nv * stride
    return tot / 1024

def clip(m, suffix):
    return next((i for i, c in enumerate(m['clips']) if c['name'].endswith(suffix)), None)

def jidx(m, *names):
    low = [j['name'].lower() for j in m['joints']]
    for n in names:
        if n in low: return low.index(n)
    return None

def view_params(kind, m, world, k):
    """глаз и цель для ряда kind по мировым положениям суставов в позе"""
    bb = m['bbox']; cy = (bb[1] + bb[4]) / 2; h = max(bb[4] - bb[1], 1.2)
    if kind == 'body':
        kk = max(h, 1.6) / 2.0
        return np.array([3.3 * kk, cy + .5 * kk, 3.3 * kk]), np.array([0, cy - .05 * kk, 0]), 36
    if kind == 'face':
        hd = jidx(m, 'head'); p = world[hd][:3, 3] + np.array([0, .10 * h / 1.8, 0]); d = np.array([1, .12, 1]); d /= np.linalg.norm(d)
        return p + d * 1.2 * h / 1.8, p - np.array([0, .04 * h / 1.8, 0]), 30
    side = jidx(m, 'wrist_r', 'hand_r', 'r_hand'); p = world[side][:3, 3]; d = np.array([1, .35, 1]); d /= np.linalg.norm(d)
    return p + d * .6 * h / 1.8, p, 30

LIFT = {}
def lift(i, which, c):
    """Посадка по правилу игры (viewer/skin.c skinFootLift): самая низкая вершина по циклу клипа ложится на землю (y = 0). Без неё прохожие стоят по своему корню:
    у olady3 и joger1 подошвы на 19–22 см ниже решётки «асфальта» — в игре этого нет, там поправка есть (Роб увидел это в видео 09.11)"""
    k = (i, which, c)
    if k not in LIFT:
        m = model(i, which); d = m['clips'][c]['dur']; lo = 1e9
        for f in range(36):
            pal, _ = ce.palette(m, ce.sample_local(m, c, d * f / 36.0)); lo = min(lo, min(float(P[:, 1].min()) for P, *_ in ce.skin(m, pal)))
        LIFT[k] = -lo
    return LIFT[k]

def render_one(a):
    i, which, c, t, kind = a; m = model(i, which)
    pal, world = ce.palette(m, ce.sample_local(m, c, t)); L = lift(i, which, c)
    for B in pal: B[1, 3] += L
    for W_ in world: W_[1, 3] += L
    sk = ce.skin(m, pal)
    eye, tgt, fov = view_params(kind, m, world, 1)
    if kind == 'body': eye = eye + np.array([0, L, 0]); tgt = tgt + np.array([0, L, 0])   # рамка общего вида считается по bbox позы привязки — переносим вместе с моделью
    im = Image.fromarray(ce.render(m, sk, eye, tgt, size=(int(PW * SS), int(PH * SS)), fov=fov))
    return im.resize((PW, PH), Image.LANCZOS) if SS != 1.0 else im

def main():
    global SPEC
    args = [a for a in sys.argv[1:] if not a.startswith('--')]; nfr = int(sys.argv[sys.argv.index('--frames') + 1]) if '--frames' in sys.argv else 60
    if '--frames' in sys.argv: args.remove(str(nfr))
    spec, out = args[0], args[1]; SPEC = json.load(open(spec))
    tmp = Path(tempfile.mkdtemp(prefix='pos16_video_')); font = ImageFont.truetype(FONT, 30); small = ImageFont.truetype(FONT, 22)
    gap = 16; rows = ('body', 'face', 'hand'); W = PW * 2 + gap * 3; H = PH * 3 + gap * 4 + 130; n = 0
    for i, sp in enumerate(SPEC):
        m = model(i, 'new'); mo = model(i, 'old'); c = clip(m, 'loco_walk'); assert c is not None and clip(mo, 'loco_walk') == c, sp['label']
        d = m['clips'][c]['dur']; tl = [(c, (k / FPS) % d) for k in range(nfr)]
        jobs = [(i, w, c, t, r) for c, t in tl for r in rows for w in ('old', 'new')]
        with Pool(4, initializer=_init, initargs=(SPEC,)) as pool: imgs = pool.map(render_one, jobs, chunksize=3)
        kbo, kbn = geom_kb(sp['old'], sp.get('oldOff', 0)), geom_kb(sp['new'], sp.get('newOff', 0))
        for f in range(len(tl)):
            fr = Image.new('RGB', (W, H), (245, 245, 243)); dr = ImageDraw.Draw(fr)
            for r in range(3):
                for k in range(2):
                    fr.paste(imgs[(f * 3 + r) * 2 + k], (gap + k * (PW + gap), gap + r * (PH + gap)))
            y = gap * 4 + PH * 3 - gap
            for k, word in enumerate(('до', 'после')):
                dr.text((gap + k * (PW + gap) + PW // 2, y + 6), f'{word} · геометрия {(kbo, kbn)[k]:.1f} КБ', fill=(20, 20, 20), font=small, anchor='ma')
            dr.text((W // 2, y + 44), sp['label'], fill=(20, 20, 20), font=font, anchor='ma')
            dr.text((W // 2, y + 84), f"{sp.get('group', '')} · идёт · общий вид, лицо, рука", fill=(120, 120, 120), font=small, anchor='ma')
            fr.save(tmp / f'f{n:05d}.png'); n += 1
        print(sp['label'], f'{kbo:.1f} -> {kbn:.1f} КБ', flush=True)
    subprocess.check_call(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', str(FPS), '-i', str(tmp / 'f%05d.png'), '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '17', out])
    shutil.rmtree(tmp); print(out, f'{n} кадров, {n / FPS:.1f} с')

if __name__ == '__main__': main()
