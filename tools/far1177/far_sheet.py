"""Разбор «плохой дали»: лист сравнения видов вдаль (порт: варианты a–d; оригинал PS2 — если есть).
  python3 -I tools/far1177/far_sheet.py PORT_DIR OUT.png [PS2_DIR]
PORT_DIR: far_<точка>_<вариант>.png (tests/fardiag/far_views.txt): a — как в игре, b — без таблицы дали (flag dal 0), c — со слоями за дальностью (horizon + silh), d — без таблицы и дальность 200 %.
PS2_DIR: <точка>.png (кадры оригинала по заявке emu-ps2). Печатает по точке долю «пустого» (небо/подложка) и яркость горизонта."""
import sys
from pathlib import Path
from PIL import Image, ImageDraw
import numpy as np
port = Path(sys.argv[1]); out = sys.argv[2]; ps2 = Path(sys.argv[3]) if len(sys.argv) > 3 else None
pts = sorted({p.stem.split('_')[1] for p in port.glob('far_*_a.png')}); W, H = 400, 225; rows = []
for pt in pts:
    cols = []
    if ps2 and (ps2 / (pt + '.png')).exists(): cols.append(('PS2 ' + pt, Image.open(ps2 / (pt + '.png')).convert('RGB')))
    for v, nm in (('a', 'порт: как в игре'), ('b', 'без таблицы дали'), ('c', 'слои за дальностью'), ('d', 'без таблицы, дальность 200 %')):
        f = port / ('far_%s_%s.png' % (pt, v))
        if f.exists(): cols.append((nm, Image.open(f).convert('RGB')))
    row = Image.new('RGB', (W * len(cols), H + 14), (25, 25, 25)); d = ImageDraw.Draw(row)
    for i, (nm, im) in enumerate(cols): row.paste(im.resize((W, H), Image.LANCZOS), (i * W, 14)); d.text((i * W + 4, 1), '%s · %s' % (pt, nm), fill=(255, 255, 255))
    rows.append(row)
    a = np.asarray(cols[-4 if len(cols) >= 4 else 0][1].convert('RGB')).astype(int)
sheet = Image.new('RGB', (max(r.width for r in rows), sum(r.height + 3 for r in rows)), (0, 0, 0)); y = 0
for r in rows: sheet.paste(r, (0, y)); y += r.height + 3
sheet.save(out); print('точек', len(pts), '->', out)
