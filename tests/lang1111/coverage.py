#!/usr/bin/env python3
"""№1111: покрытие знаков. Каждая строка текстовой библиотеки диска на английском, французском, немецком и испанском (итальянская таблица на диске пустая) переводится в коды шрифта
(tools/accents.py font_bytes) и каждый знак (кроме пробела и перевода строки) обязан рисоваться в КАЖДОЙ грани пакетов (пиксельные msx_thin, pix8_8, bart_16bit, simpsons_pix и шрифты диска).
Кнопки PS2 («£ ® µ § © ¿ ¥ [ +» в подсказках) рисует пакет значками, их не проверяем. Код 1 — есть дыры; FONT1095_OLD=1 — красный контроль."""
import struct, sys, collections
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import ui_export as U
import accents as A
import numpy as np

def tables():
    root = U.read(U.SCROOBY / 'resource/txtbible/srr2.p3d'); T = {}
    for c in U.walk(root):
        if c.id == 0x1800E:
            _, p = U.string(c.data); lang = c.data[p:p + 1].decode()
            n, mod, size = struct.unpack_from('<III', c.data, p + 1); p += 13
            hs = struct.unpack_from('<%dI' % n, c.data, p); of = struct.unpack_from('<%dI' % n, c.data, p + 4 * n); buf = c.data[p + 8 * n:]
            T[lang] = [buf[o:].decode('utf-16-le', errors='replace').split('\0')[0] for o in of]
    return T

def main():
    F = U.all_fonts(); T = tables(); bad = 0
    faces = ['msx_thin', 'pix8_8', 'bart_16bit', 'simpsons_pix', 'boulder_24', 'boulder_16', 'swz721n_10', 'swz721mi_12', 'Tt2001m__14', 'boulder_px14', 'swz721_px10']
    alpha = {n: np.asarray(F[n]['image'].convert('RGBA'))[..., 3] for n in faces}
    import pixel_fonts as P
    from PIL import ImageFont
    nd = {n: P._mask(ImageFont.truetype(str(P.FONT_DIR / P.FONTS[n]['file']), P.NATIVE), '\ue000')[0] for n in ('bart_16bit', 'simpsons_pix')}   # рамка .notdef шрифтов Lauke: «знак есть» не должно быть ею
    def has(n, code):
        g = F[n]['glyphs'].get(code)
        if g is None or g['adv'] <= 0: return False
        cell = alpha[n][int(g['y']):int(g['y']) + int(g['h']), int(g['x']):int(g['x']) + int(g['w'])]
        if g['w'] and n in nd and cell.shape[1] == nd[n].shape[1] and ((cell > 40) == (nd[n][:cell.shape[0]] > 0)).all() and code > 127: return False   # рамка вместо знака
        return g['w'] == 0 or (cell > 40).any()
    for lang in 'EFGS':
        used = collections.Counter(b for s in T[lang] for b in A.font_bytes(s) if b > 32 and b not in b'\\')
        missing = {}
        for n in faces:
            for code, cnt in used.items():
                if code in (0xA3, 0xA5, 0xA7, 0xA9, 0xAE, 0xB5, 0xBF, 0x40, 0x5B, 0x2B):      # кнопки и стрелки подсказок
                    continue
                if not has(n, code):
                    missing.setdefault(n, []).append((chr(code) if code < 256 else '?', cnt))
        print('язык', lang, 'разных знаков', len(used), 'дыры:', {n: ''.join(c for c, _ in v) for n, v in missing.items()} or 'нет')
        bad += sum(len(v) for v in missing.values())
    print('PASS' if not bad else 'FAIL %d' % bad)
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
