"""№1178 аудит: лист «было / стало» по снимкам двух прогонов смоука одного уровня.
  python3 -I tools/far1177/ab_sheet.py OLD_DIR NEW_DIR SPOTS.json OUT.png
Для каждой камеры: слева кадр целиком (стало), справа вырезка по области отличия ×3: сверху «было», снизу «стало».
Печатает по строке на камеру: сколько пикселей отличается и где (ящик отличия), нет отличия — «=»."""
import sys, json
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw
import numpy as np

old, new, spots, out = sys.argv[1:5]
S = json.loads(Path(spots).read_text()); rows = []; stat = []
for s in S:
    n = s['name']; po, pn = Path(old) / (n + '.png'), Path(new) / (n + '.png')
    if not (po.exists() and pn.exists()): stat.append((n, 'нет снимка')); continue
    a = Image.open(po).convert('RGB'); b = Image.open(pn).convert('RGB')
    if a.size != b.size: stat.append((n, 'размеры разные')); continue
    d = np.asarray(ImageChops.difference(a, b)).max(axis=2); m = d > 24; cnt = int(m.sum())
    if cnt == 0: stat.append((n, '= (отличий нет)')); bb = None
    else:
        ys, xs = np.where(m); bb = (int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1)
        stat.append((n, 'отличий %d px, ящик %s' % (cnt, bb)))
    W, H = a.size; sc = 3
    if bb:
        cx, cy = (bb[0] + bb[2]) // 2, (bb[1] + bb[3]) // 2; w = max(bb[2] - bb[0] + 24, 160); h = max(bb[3] - bb[1] + 24, 90)
        r = max(w / 160, h / 90); w, h = int(160 * r), int(90 * r)
        x0 = min(max(cx - w // 2, 0), W - w); y0 = min(max(cy - h // 2, 0), H - h); box = (x0, y0, x0 + w, y0 + h)
    else:
        box = (W // 2 - 80, H // 2 - 45, W // 2 + 80, H // 2 + 45)
    ca = a.crop(box).resize((480, 270), Image.LANCZOS); cb = b.crop(box).resize((480, 270), Image.LANCZOS)
    full = b.copy(); dr = ImageDraw.Draw(full); dr.rectangle(box, outline=(255, 0, 0), width=2)
    full = full.resize((480, 270), Image.LANCZOS)
    row = Image.new('RGB', (480 * 3 + 8, 270 + 16), (30, 30, 30)); row.paste(full, (0, 16)); row.paste(ca, (484, 16)); row.paste(cb, (968, 16))
    dd = ImageDraw.Draw(row); dd.text((4, 2), '%s  стало (целиком, рамка = вырезка)' % n, fill=(255, 255, 255)); dd.text((488, 2), 'было', fill=(255, 200, 120)); dd.text((972, 2), 'стало', fill=(120, 255, 160))
    rows.append(row)
if rows:
    sheet = Image.new('RGB', (rows[0].width, sum(r.height + 4 for r in rows)), (0, 0, 0)); y = 0
    for r in rows: sheet.paste(r, (0, y)); y += r.height + 4
    sheet.save(out)
for n, t in stat: print(n, t)
