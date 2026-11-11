#!/usr/bin/env python3
"""Модели-96: видео «до / после» упаковки анимаций (Мардж идёт, бежит, прыгает), синхронно, mp4 через ffmpeg.
  video.py OLD.shr NEW.shr OUT.mp4 [--fps 30]
Кадры — тот же растеризатор, что у preview (tools/char_export.py render): модель читается из самого файла, поза по клипу в тот же миг у обеих."""
import sys, subprocess, tempfile, shutil
from pathlib import Path
from multiprocessing import Pool
import numpy as np
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import char_export as ce
from PIL import Image, ImageDraw, ImageFont

FPS = 30; PW, PH = 540, 600; SS = 1.5
FONT = next((f for f in ('/System/Library/Fonts/Supplemental/Arial.ttf', '/Library/Fonts/Arial.ttf', '/System/Library/Fonts/Helvetica.ttc') if Path(f).exists()), None)
MODELS = {}

def plan():
    """[(клип, время клипа с, подпись)] по кадрам: идёт, бежит, прыгает (разбег, воздух, посадка) дважды"""
    seq = [('loco_walk', 3), ('loco_run', 4)]
    out = []
    for name, cycles in seq:
        out.append((name, None, cycles))
    return out

def timeline(m):
    names = [c['name'] for c in m['clips']]
    def clip(s): return next(i for i, n in enumerate(names) if n.endswith(s))
    tl = []
    for s, cycles, label in (('loco_walk', 3, 'идёт'), ('loco_run', 4, 'бежит')):
        c = clip(s); d = m['clips'][c]['dur']
        for k in range(int(round(cycles * d * FPS))): tl.append((c, (k / FPS) % d, label))
    for _ in range(2):
        for s in ('jump_idle_take_off', 'jump_idle_in_air', 'jump_idle_land'):
            c = clip(s); d = m['clips'][c]['dur']
            for k in range(int(round(d * FPS))): tl.append((c, min(k / FPS, d), 'прыгает'))
    return tl

def pose(m, c, t):
    loc = ce.sample_local(m, c, t)
    cl = m['clips'][c]; tr = cl['tracks'][0]
    if tr['tf'] is not None:                                   # корень: только вертикаль (высота прыжка), без ухода по полу
        f = min(max(t * cl['fps'], 0.), cl['frames'] - 1); a, b, u = ce._key(tr['tf'], tr['tv'], f); p = a + (b - a) * u
        loc[0] = (loc[0][0], np.array([m['joints'][0]['t'][0], p[1], m['joints'][0]['t'][2]]))
    return loc

def render_one(args):
    which, c, t = args
    m = MODELS[which]; bb = m['bbox']; cy = (bb[1] + bb[4]) / 2
    pal, _ = ce.palette(m, pose(m, c, t)); sk = ce.skin(m, pal)
    eye = np.array([3.3, cy + .5, 3.3])
    im = Image.fromarray(ce.render(m, sk, eye, [0, cy - .05, 0], size=(int(PW * SS), int(PH * SS)), fov=36))
    return im.resize((PW, PH), Image.LANCZOS)

def init(old, new):
    MODELS['old'] = ce.read_chr(Path(old)); MODELS['new'] = ce.read_chr(Path(new))

def main():
    global FPS
    old, new, out = sys.argv[1:4]
    if '--fps' in sys.argv: FPS = int(sys.argv[sys.argv.index('--fps') + 1])
    init(old, new); tl = timeline(MODELS['new'])
    tl_old = timeline(MODELS['old']); assert [(c, round(t, 6)) for c, t, _ in tl] == [(c, round(t, 6)) for c, t, _ in tl_old], 'клипы разные'
    kb = {k: Path(p).stat().st_size / 1024 for k, p in (('old', old), ('new', new))}
    tmp = Path(tempfile.mkdtemp(prefix='animpack_video_'))
    jobs = [(w, c, t) for c, t, _ in tl for w in ('old', 'new')]
    with Pool(4, initializer=init, initargs=(old, new)) as pool: imgs = pool.map(render_one, jobs, chunksize=4)
    font = ImageFont.truetype(FONT, 34) if FONT else None; small = ImageFont.truetype(FONT, 26) if FONT else None
    W, H = PW * 2 + 30, PH + 150
    for i, (c, t, label) in enumerate(tl):
        fr = Image.new('RGB', (W, H), (245, 245, 243)); d = ImageDraw.Draw(fr)
        for k, (which, word) in enumerate((('old', 'до'), ('new', 'после'))):
            x = k * (PW + 30) + 0
            fr.paste(imgs[2 * i + k], (x, 0))
            d.text((x + PW // 2, PH + 14), word, fill=(20, 20, 20), font=font, anchor='ma')
            d.text((x + PW // 2, PH + 62), f'модель {kb[which]:.0f} КБ', fill=(20, 20, 20), font=small, anchor='ma')
        d.text((W // 2, PH + 112), label, fill=(120, 120, 120), font=small, anchor='ma')
        fr.save(tmp / f'f{i:04d}.png')
    subprocess.check_call(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', str(FPS), '-i', str(tmp / 'f%04d.png'), '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '16', out])
    shutil.rmtree(tmp)
    print(out, f'{len(tl)} кадров, {len(tl) / FPS:.1f} с, до {kb["old"]:.0f} КБ, после {kb["new"]:.0f} КБ')

if __name__ == '__main__':
    main()
