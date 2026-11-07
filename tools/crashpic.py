#!/usr/bin/env python3
"""№1208 / 1208а / 1208б (Роб): картинка экрана вылета — «TECHNICAL DIFFICULTIES / PLEASE STAND BY» (картинка Роба, assets/crash/technical_difficulties_source.png)
в той же рамке-телевизоре, что на роликах (общая сборка U.widescreen_tvframe), 480x272, палитра 8 бит, формат crashpic.shx для обработчика вылета (sharexc):
  4 байта «SHXC», u16 w, u16 h, u16 число цветов (<= 256), u16 0, палитра число_цветов x (R, G, B, 255), затем w*h индексов построчно сверху вниз.
Рядом пишется crashpic.txt: прямоугольники строк кода ошибки (x y w h, сетка 8x8) в 480-масштабе — их печатает обработчик поверх картинки.
Рамка — настоящая рамка роликов с диска игрока (TVFrame, общая сборка U.widescreen_tvframe); в репозитории только картинка Роба (assets/crash/technical_difficulties_source.png), crashpic.shx собирает tools/ship_rules.txt при каждой сборке с диском.
  python3 tools/crashpic.py --out FILE | --sheet ПАПКА"""
import os, struct, sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'assets/crash/technical_difficulties_source.png'
FONT = ROOT / 'assets/fonts/pixel/bart-vs-the-space-mutants-16bit.otf'
W, H = 480, 272
WIN = (29, 23, 451, 250)      # окно рамки-телевизора (внутри — картинка), как у рамки роликов
BG = (143, 193, 148)          # ровный зелёный картинки Роба
HOLD = 'HOLD POWER TO TURN OFF'

def bezel_disc():
    sys.path.insert(0, str(ROOT / 'tools'))
    import ui_export as U
    return U.widescreen_tvframe(U.all_images()['tvframe.png'], U.DST_W, U.DST_H, U.S)

def text(img, x, y, s, col=(255, 255, 255), size=8):
    f = ImageFont.truetype(str(FONT), size); d = ImageDraw.Draw(img); d.fontmode = '1'
    d.text((x + 1, y + 1), s, font=f, fill=(0, 0, 0)); d.text((x, y), s, font=f, fill=col)
    return int(d.textlength(s, font=f))

def compose(bezel, sample=False):
    """картинка Роба как есть (целиком, вписана в окно рамки по высоте, по бокам тот же ровный зелёный), рамка поверх; в левом нижнем углу картинки — место под код ошибки
    (печатает обработчик вылета), в правом нижнем — HOLD POWER TO TURN OFF (вшита). sample=True — пример кода для листа."""
    src = Image.open(SRC).convert('RGB')
    img = Image.new('RGB', (W, H), BG)
    wx0, wy0, wx1, wy1 = WIN; ww, wh = wx1 - wx0, wy1 - wy0
    sc = min(ww / src.width, wh / src.height); sw, sh = int(round(src.width * sc)), int(round(src.height * sc))
    img.paste(src.resize((sw, sh), Image.LANCZOS), (wx0 + (ww - sw) // 2, wy0 + (wh - sh) // 2))
    hw = max(text(img, 0, -50, 'HOLD POWER'), text(img, 0, -50, 'TO TURN OFF'))
    text(img, wx1 - 12 - hw, wy1 - 27, 'HOLD POWER'); text(img, wx1 - 12 - hw, wy1 - 16, 'TO TURN OFF')
    code = (wx0 + 12, wy1 - 27, 120, 19)    # две строки по 15 знаков (8x8), левый нижний угол
    if sample:
        text(img, code[0], code[1], 'EXC ADEL 08A1B2C4'[:15]); text(img, code[0], code[1] + 9, 'MOD SHARVIEW RA 1'[:15])
    img = img.convert('RGBA'); img.alpha_composite(bezel); return img.convert('RGB'), code

def write_shx(path, img):
    q = img.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette()[:768]; n = max(q.getdata()) + 1 if False else 256
    data = bytes(q.getdata()); n = max(data) + 1
    with open(path, 'wb') as f:
        f.write(b'SHXC' + struct.pack('<HHHH', img.width, img.height, n, 0))
        for i in range(n): f.write(bytes(pal[i * 3:i * 3 + 3]) + b'\xff')
        f.write(data)
    return q.convert('RGB')

def main():
    """python3 tools/crashpic.py --out FILE.shx      рамка роликов — с диска игрока (tools/ship_rules.txt)
       python3 tools/crashpic.py --sheet ПАПКА       лист одного экрана с примером кода (png 480x272 и x2)"""
    a = sys.argv[1:]
    if '--out' in a:
        o = Path(a[a.index('--out') + 1]); img, code = compose(bezel_disc()); write_shx(o, img)
        o.with_suffix('.txt').write_text('code_rect %d %d %d %d\n' % code); print('crashpic ->', o, 'код', code); return
    if '--sheet' in a:
        d = Path(a[a.index('--sheet') + 1]); d.mkdir(parents=True, exist_ok=True)
        img, code = compose(bezel_disc(), sample=True); img.save(d / 'ИТОГ_480x272.png'); img.resize((W * 2, H * 2), Image.NEAREST).save(d / 'ИТОГ.png'); print('лист ->', d / 'ИТОГ.png'); return
    print(main.__doc__)

if __name__ == '__main__':
    main()
