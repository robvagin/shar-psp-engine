#!/usr/bin/env python3
"""Лист кадров пузыря: sheet.py ПАПКА_bs ВЫХОД.png [с_кадра] [сколько] — подряд идущие кадры, крупно нижняя левая часть (бабл), подпись номера кадра."""
import sys, glob, os
from PIL import Image, ImageDraw
d, out = sys.argv[1:3]; f0 = int(sys.argv[3]) if len(sys.argv) > 3 else 0; n = int(sys.argv[4]) if len(sys.argv) > 4 else 24
fs = [p for p in sorted(glob.glob(d + '/bs_*.png')) if int(os.path.basename(p)[3:8]) >= f0][:n]
box = (0, 176, 300, 272); Z = 2; W, H = (box[2]-box[0])*Z, (box[3]-box[1])*Z
cols = 4; rows = (len(fs) + cols - 1) // cols
s = Image.new('RGB', (cols*W, rows*(H+16)), (30, 30, 30)); dr = ImageDraw.Draw(s)
for i, p in enumerate(fs):
    im = Image.open(p).convert('RGB').crop(box).resize((W, H), Image.NEAREST)
    x, y = (i % cols)*W, (i//cols)*(H+16); s.paste(im, (x, y+16)); dr.text((x+4, y+2), 'кадр ' + os.path.basename(p)[3:8], fill=(255, 255, 255))
s.save(out); print(out, len(fs))
