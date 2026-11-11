#!/usr/bin/env python3
"""Модели-96: монтаж «до / после» упаковки для Роба: игроки, персонажи миссий, пешеходы; идёт и бежит, слева «до», справа «после»,
имя и размер модели в КБ под каждой. Тот же растеризатор, что у preview (tools/char_export.py).
  montage.py SPEC.json OUT.mp4      SPEC: [{"label","old","new","oldOff","newOff","kb":"file"|"geom"}...]"""
import sys, json, struct, subprocess, tempfile, shutil
from pathlib import Path
from multiprocessing import Pool
import numpy as np
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import char_export as ce
from PIL import Image, ImageDraw, ImageFont

FPS = 30; PW, PH = 432, 480; SS = 1.25
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

def kb(i, which):
    sp = SPEC[i]
    return geom_kb(sp[which], sp.get(which + 'Off', 0)) if sp.get('kb') == 'geom' else Path(sp[which]).stat().st_size / 1024

def clip(m, suffix):
    return next((i for i, c in enumerate(m['clips']) if c['name'].endswith(suffix)), None)

def timeline(i):
    m = model(i, 'new'); w = clip(m, 'loco_walk'); r = clip(m, 'loco_run'); tl = []
    def add(c, cycles):
        d = m['clips'][c]['dur']
        for k in range(int(round(cycles * d * FPS))): tl.append((c, (k / FPS) % d, 'идёт' if c == w else 'бежит'))
    add(w, 2 if r is not None else 3)
    if r is not None: add(r, 2)
    return tl

def pose(m, c, t): return ce.sample_local(m, c, t)

def render_one(a):
    i, which, c, t = a; m = model(i, which); bb = m['bbox']; cy = (bb[1] + bb[4]) / 2; h = max(bb[4] - bb[1], 1.2)
    pal, _ = ce.palette(m, pose(m, c, t)); sk = ce.skin(m, pal)
    k = max(h, 1.6) / 2.0
    eye = np.array([3.3 * k, cy + .5 * k, 3.3 * k])
    im = Image.fromarray(ce.render(m, sk, eye, [0, cy - .05 * k, 0], size=(int(PW * SS), int(PH * SS)), fov=36))
    return im.resize((PW, PH), Image.LANCZOS)

def main():
    global SPEC
    spec, out = sys.argv[1], sys.argv[2]; SPEC = json.load(open(spec))
    tmp = Path(tempfile.mkdtemp(prefix='animpack_montage_')); font = ImageFont.truetype(FONT, 34); small = ImageFont.truetype(FONT, 26)
    W, H = PW * 2 + 30, PH + 170; n = 0
    for i, sp in enumerate(SPEC):
        tl = timeline(i); assert [(c, round(t, 5)) for c, t, _ in tl] == [(c, round(t, 5)) for c, t, _ in timeline_old(i)], sp['label']
        jobs = [(i, w, c, t) for c, t, _ in tl for w in ('old', 'new')]
        with Pool(4, initializer=_init, initargs=(SPEC,)) as pool: imgs = pool.map(render_one, jobs, chunksize=4)
        kbs = {w: kb(i, w) for w in ('old', 'new')}; unit = 'геометрия' if sp.get('kb') == 'geom' else 'модель'
        for f, (c, t, label) in enumerate(tl):
            fr = Image.new('RGB', (W, H), (245, 245, 243)); d = ImageDraw.Draw(fr)
            for k, (which, word) in enumerate((('old', 'до'), ('new', 'после'))):
                x = k * (PW + 30); fr.paste(imgs[2 * f + k], (x, 0))
                d.text((x + PW // 2, PH + 12), word, fill=(20, 20, 20), font=font, anchor='ma')
                d.text((x + PW // 2, PH + 58), f'{unit} {kbs[which]:.0f} КБ', fill=(20, 20, 20), font=small, anchor='ma')
            d.text((W // 2, PH + 100), sp['label'], fill=(20, 20, 20), font=font, anchor='ma')
            d.text((W // 2, PH + 142), f"{label} · {sp.get('group', '')}", fill=(120, 120, 120), font=small, anchor='ma')
            fr.save(tmp / f'f{n:05d}.png'); n += 1
        print(sp['label'], f"{kbs['old']:.0f} -> {kbs['new']:.0f} КБ", flush=True)
    subprocess.check_call(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', str(FPS), '-i', str(tmp / 'f%05d.png'), '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '17', out])
    shutil.rmtree(tmp); print(out, f'{n} кадров, {n / FPS:.1f} с')

def timeline_old(i):
    m = model(i, 'old'); w = clip(m, 'loco_walk'); r = clip(m, 'loco_run'); tl = []
    def add(c, cycles):
        d = m['clips'][c]['dur']
        for k in range(int(round(cycles * d * FPS))): tl.append((c, (k / FPS) % d, ''))
    add(w, 2 if r is not None else 3)
    if r is not None: add(r, 2)
    return tl

if __name__ == '__main__': main()
