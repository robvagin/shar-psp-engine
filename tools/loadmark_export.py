#!/usr/bin/env python3
"""1360: знак загрузки (UMD-диск с бегущими Симпсонами, жёлтый круг #f1d148, тушь #000100) -> loadmark.bin рядом с EBOOT.
  python3 tools/loadmark_export.py ВЫХОД.bin [РАЗМЕР=30: рисунок РАЗМЕР x РАЗМЕР px (вариант B Роба), текстура — степень двойки не меньше]     SVG берётся из $LOADMARK_SVG (по умолчанию ~/Work/shar-psp-teardown/site/frames-in/logo/logo.svg)
Формат LMK2 (с 1366в; LMK1 — прежний: 'LMK1', u16 w, u16 h): 'LMK2', u16 tw, th, aw, ah, затем tw*th пикселей RGBA 8888 (R, G, B, A по байту, строки сверху вниз). Знак не перерисовывается и не перекрашивается: только масштаб; полные цвета без палитры,
чтобы жёлтый не терялся, край — сглаженная альфа. Сам знак в репозиторий не кладётся (не публикуется): файл собирается в набор при выпуске, движок без него рисует прежнюю надпись LOADING."""
import io, os, re, struct, sys
import cairosvg
from PIL import Image

out = sys.argv[1]; art = int(sys.argv[2]) if len(sys.argv) > 2 else 30   # 1366в: рисунок art x art px (по умолчанию 30), текстура — ближайшая степень двойки (32), формат LMK2
tex = 1
while tex < art: tex *= 2
assert 8 <= art <= 64 and tex <= 64, 'размер рисунка 8..64'
svg = os.environ.get('LOADMARK_SVG') or os.path.expanduser('~/Work/shar-psp-teardown/site/frames-in/logo/logo.svg')
svgtxt = open(svg, encoding='utf8').read()
if os.environ.get('LOADMARK1366_OLD') != '1':   # 1366: жёлтый контур диска, внутри пустота, в ней жёлтая семья (было: сплошной жёлтый круг и чёрные фигуры)
    Y = '#f1d148'
    svgtxt = re.sub(r'<circle class="cls-1"[^>]*/>', '', svgtxt)                               # залитый круг убран: внутри кольца пусто (альфа 0)
    svgtxt = re.sub(r'\.cls-2\s*\{\s*fill:\s*#[0-9a-fA-F]+;', '.cls-2 { fill: ' + Y + ';', svgtxt)   # фигуры жёлтые
    svgtxt = re.sub(r'<path d=', '<path fill="' + Y + '" d=', svgtxt, count=1)                    # рамка (path без класса, первый path) жёлтая
png = cairosvg.svg2png(bytestring=svgtxt.encode('utf8'), output_width=art * 4, output_height=art * 4)   # в четыре раза крупнее и честный пересчёт вниз LANCZOS: ровный чёткий край
im = Image.open(io.BytesIO(png)).convert('RGBA').resize((art, art), Image.LANCZOS)
canvas = Image.new('RGBA', (tex, tex), (0, 0, 0, 0)); canvas.paste(im, (0, 0))
px = canvas.tobytes()
with open(out, 'wb') as f: f.write(b'LMK2' + struct.pack('<HHHH', tex, tex, art, art) + px)
print('loadmark: %s текстура %dx%d, рисунок %dx%d, %d bytes' % (out, tex, tex, art, art, 12 + len(px)))
