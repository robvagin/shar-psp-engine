#!/usr/bin/env python3
"""№1022: полосы кадров «TEST71 (flag glintfix1022 0) / стало» — ключ (wo/wn) и будка (po/pn), кроп вокруг центра.  python3 sheet.py RUN_DIR OUT.png"""
import sys, glob, os
from PIL import Image, ImageDraw
d, out = sys.argv[1], sys.argv[2]
rows = [('wo', 'ключ TEST71'), ('wn', 'ключ стало'), ('po', 'будка TEST71'), ('pn', 'будка стало')]
cells = []
for pref, lab in rows:
    fs = sorted(glob.glob(os.path.join(d, pref + '[0-9][0-9].png')))[:8]
    ims = []
    for f in fs:
        im = Image.open(f).convert('RGB'); w, h = im.size
        ims.append(im.crop((int(w * .25), int(h * .08), int(w * .75), int(h * .62))).resize((240, 156)))
    cells.append((lab, ims))
W = 240 * 8; H = (156 + 16) * len(cells)
s = Image.new('RGB', (W, H), 'white'); dr = ImageDraw.Draw(s)
for r, (lab, ims) in enumerate(cells):
    dr.text((4, r * 172 + 2), lab, fill='black')
    for c, im in enumerate(ims): s.paste(im, (c * 240, r * 172 + 16))
s.save(out); print(out)
