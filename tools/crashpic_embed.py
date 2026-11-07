#!/usr/bin/env python3
"""№1208б: картинка экрана вылета (SHXC, палитровая, до 480x272) -> C-заголовок для sharexc.prx.
   crashpic_embed.py <crashpic.shx> <out.h> [--placeholder]
   Массив gEmbPic кладётся в модуль; метка gEmbPicTag = "SHXPIC1:<sha256[:12]>" (или "SHXPIC1:PLACEHOLDER") лежит в .rodata модуля и читается гейтом tools/crashpic_gate.sh.
   Формат .shx: 'SHXC', u16 w, u16 h, u16 ncolors (<=256), u16 0, палитра ncolors*4 (R,G,B,A), w*h индексов построчно сверху вниз (viewer/kexc/crashpic.h)."""
import hashlib, struct, sys
src, out = sys.argv[1], sys.argv[2]; ph = '--placeholder' in sys.argv[3:]
d = open(src, 'rb').read()
if d[:4] != b'SHXC' or len(d) < 12: sys.exit('crashpic: нет метки SHXC в ' + src)
w, h, nc, _ = struct.unpack('<HHHH', d[4:12])
if not (0 < w <= 480 and 0 < h <= 272 and 0 < nc <= 256) or len(d) < 12 + nc * 4 + w * h: sys.exit('crashpic: размеры/палитра/длина не сходятся (%dx%d, %d цветов, %d байт)' % (w, h, nc, len(d)))
import os, re
rect2 = (0, 0, 0, 0)   # power_rect: куда печатать «HOLD POWER TO TURN OFF» (0 — не печатать: надпись уже в картинке)
rect = (4, 180, 472, 84)   # по умолчанию нижняя полоса; из <имя>.txt (строка «code_rect x y w h», рядом с .shx) — как задал автор картинки (g09a)
tx = os.path.splitext(src)[0] + '.txt'
if os.path.exists(tx):
    m = re.search(r'code_rect\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)', open(tx, errors='replace').read())
    if m: rect = tuple(int(v) for v in m.groups())
    m2 = re.search(r'power_rect\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)', open(tx, errors='replace').read())
    if m2: rect2 = tuple(int(v) for v in m2.groups())
tag = 'SHXPIC1:PLACEHOLDER' if ph else 'SHXPIC1:' + hashlib.sha256(d).hexdigest()[:12]
def rle(ix):   # PackBits-подобный поток, формат в viewer/kexc/crashpic.h
    o = bytearray(); i = 0; n = len(ix)
    while i < n:
        j = i + 1
        while j < n and ix[j] == ix[i] and j - i < 129: j += 1
        if j - i >= 2: o += bytes([126 + (j - i), ix[i]]); i = j
        else:
            k = i
            while k < n and k - i < 128 and not (k + 1 < n and ix[k] == ix[k + 1]): k += 1
            if k == i: k += 1
            o += bytes([k - i - 1]) + bytes(ix[i:k]); i = k
    return bytes(o)
if struct.unpack('<H', d[10:12])[0] == 0:   # несжатый .shx -> сжатый в модуле (смотри crashpic.h); сжатый уже — как есть
    raw = d; c = rle(raw[12 + nc * 4: 12 + nc * 4 + w * h]); d = raw[:10] + struct.pack('<H', 1) + raw[12:12 + nc * 4] + struct.pack('<I', len(c)) + c
if not ph: tag = 'SHXPIC1:' + hashlib.sha256(d).hexdigest()[:12]
rows = []
for i in range(0, len(d), 24): rows.append('  ' + ','.join(str(b) for b in d[i:i + 24]) + ',')
open(out, 'w').write('/* сгенерировано tools/crashpic_embed.py из %s: не править */\nstatic const unsigned char gEmbPic[] = {\n%s\n};\nstatic const unsigned gEmbPicN = %d;\nstatic const char gEmbPicTag[] = "%s";\nstatic const unsigned gEmbRect[4] = {%d, %d, %d, %d};   /* код ошибки: x y w h в пикселях экрана */\nstatic const unsigned gEmbRect2[4] = {%d, %d, %d, %d};   /* HOLD POWER: x y w h; нули — не печатать */\n' % (src.split('/')[-1], '\n'.join(rows), len(d), tag, *rect, *rect2))
print('crashpic: %dx%d, %d цветов, в модуле %d байт (сжато), %s, code_rect %s, power_rect %s' % (w, h, nc, len(d), tag, rect, rect2))
