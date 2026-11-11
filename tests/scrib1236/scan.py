#!/usr/bin/env python3
# Fable шаг 14: поиск пурпурных (память освобождена) и голубых (память не заполнена) клиньев на снимках сборки MEM_SCRIBBLE2. scan.py ПАПКА [порог_пикселей]
import sys, glob, os
from PIL import Image
d = sys.argv[1]; thr = int(sys.argv[2]) if len(sys.argv) > 2 else 120
bad = 0; n = 0
for f in sorted(glob.glob(os.path.join(d, '*.png'))):
    im = Image.open(f).convert('RGB'); px = im.load(); w, h = im.size; n += 1
    mag = cya = 0
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            if r > 190 and b > 190 and g < 70: mag += 1
            elif b > 190 and g > 190 and r < 70: cya += 1
    if mag >= thr or cya >= thr: bad += 1; print(f'{os.path.basename(f)}: пурпурных {mag}, голубых {cya}')
print(f'снимков {n}, с клиньями {bad}')
