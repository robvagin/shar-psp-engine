#!/usr/bin/env python3
"""Пиксельные шрифты Patrick H. Lauke (CC BY 3.0) как грани пакетов интерфейса (11.10, слово Роба: «примерить два шрифта»).

«Bart vs. the Space Mutants (16bit)» (FontStruct 1363863) -> грань bart_16bit, «The Simpsons» (FontStruct 879452) -> simpsons_pix.
Файлы и лицензии лежат в assets/fonts/pixel/. Сетка обоих шрифтов: 8 клеток на em, поэтому родной размер 8 px = клетка шрифта 1 px экрана, растр без сглаживания (PIL fontmode '1');
кратные 16, 24, 32 (метка грани *2, *3, *4) — целое повторение пикселей (tools/ui_export.py FontCache). Метрики (ширина знака, отступ) берутся из файла.
Ячейка 10x10 на код, база 8, как у pix8 и msx_thin; атлас 160x160. Грань попадает в пакет, только если её выбрала группа панели (all_fonts лишь знает о ней).
UI_PIXFONTS_OFF=1 — без этих граней. Указание автора: рядом с каждым пакетом, где есть эти грани, лежит <пакет>.fonts; tools/final-build.sh собирает из них fontcredit.txt (страница «FONT CREDITS» после дисклеймера, viewer/bootscreen.h) и кладёт лицензии шрифтов рядом с EBOOT."""
import os
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT_DIR = ROOT / 'assets/fonts/pixel'
FONTS = {
    'bart_16bit': dict(file='bart-vs-the-space-mutants-16bit.otf', lines=['Font: Bart vs. the Space Mutants (16bit)', 'by Patrick H. Lauke, CC BY 3.0']),
    'simpsons_pix': dict(file='the-simpsons.otf', lines=['Font: The Simpsons by Patrick H. Lauke, CC BY 3.0']),
}
NATIVE, LINE, BASE = 8, 10, 8


def _mask(font, ch):
    im = Image.new('L', (24, 24), 0)
    d = ImageDraw.Draw(im)
    d.fontmode = '1'
    ox, oy = 6, 14
    d.text((ox, oy), ch, font=font, fill=255, anchor='ls')
    a = np.asarray(im) > 0
    ys, xs = np.where(a)
    if not len(xs):
        return None
    x0, x1 = xs.min(), xs.max() + 1
    cell = np.zeros((LINE, x1 - x0), np.uint8)
    for y in range(24):
        cy = y - oy + BASE
        if 0 <= cy < LINE:
            cell[cy] |= a[y, x0:x1].astype(np.uint8)
    return cell, int(x0 - ox)


def _same(a, b):
    return a is not None and b is not None and a[0].shape == b[0].shape and bool((a[0] == b[0]).all())


def build(name, buttons):
    """-> запись all_fonts: image RGBA 160x160, glyphs {код: x, y, w, h, adv, left}, height, baseline, pixel=True. buttons — знаки кнопок {код: 8 строк-масок} как у msx_thin"""
    font = ImageFont.truetype(str(FONT_DIR / FONTS[name]['file']), NATIVE)
    import accents   # №1095
    ref = _mask(font, '')
    img = np.zeros((160, 160, 4), np.uint8)
    glyphs = {}
    for code in range(32, 256):
        x0, y0 = (code % 16) * 10, (code // 16) * 10
        if code in buttons:
            rows = buttons[code]
            for y, row in enumerate(rows):
                for x in range(8):
                    if row & (128 >> x):
                        img[y0 + y, x0 + x] = 255
            glyphs[code] = dict(x=x0, y=y0, w=8, h=LINE, adv=9, left=0)
            continue
        ch = {0x8C: '\u0152', 0x9C: '\u0153', 0x8F: '\u00bf'}.get(code, chr(code)) if os.environ.get('FONT1095_OLD') != '1' else chr(code)   # №1095: Œ œ ¿ на свободных кодах (0xBF — кнопка X)
        bm = _mask(font, ch)
        if code != 32 and (bm is None or _same(bm, ref)):
            continue                                     # нет в шрифте: движок возьмёт «?»
        adv = int(round(font.getlength(ch)))
        if bm is None:
            glyphs[code] = dict(x=x0, y=y0, w=0, h=LINE, adv=adv, left=0)
            continue
        rows, left = bm
        w = min(rows.shape[1], 10)
        img[y0:y0 + LINE, x0:x0 + w, :] = (rows[:LINE, :w] * 255)[..., None]
        glyphs[code] = dict(x=x0, y=y0, w=w, h=LINE, adv=adv, left=left)
    if not accents.off():   # №1095: в этих файлах знаков с диакритикой нет — шрифт отдаёт рамку .notdef; рамки убираем только на кодах, которые рисуем сами (остальное как было)
        nd = _mask(font, '\ue000')
        for chx in list(accents.COMBOS) + list('æÆ«»¡ßº'):
            cx = ord(chx)
            if cx in glyphs and cx not in buttons and _same(_mask(font, chx), nd):
                del glyphs[cx]; img[(cx // 16) * 10:(cx // 16) * 10 + LINE, (cx % 16) * 10:(cx % 16) * 10 + 10] = 0
    accents.add_pixel(img, glyphs, 'thin' if name == 'bart_16bit' else 'thick')   # №1095: é è ê … œ æ « » ¿ в той же сетке (в файлах шрифта их нет)
    return dict(image=Image.fromarray(img, 'RGBA'), glyphs=glyphs, height=LINE, baseline=BASE, pixel=True)


def faces(buttons):
    if os.environ.get('UI_PIXFONTS_OFF') == '1' or not all((FONT_DIR / f['file']).exists() for f in FONTS.values()):
        return {}
    return {n: build(n, buttons) for n in FONTS}


# ---- №547/552 (Роб 11.10): пиксельные грани из шрифтов диска. Boulder (заголовок и кнопки карточки) и Swiss 721 (LAPS, st/nd/rd/th) на PS2 нарисованы
# для 640x480; на 480x272 их дробное сжатие даёт каши и битые пиксели. Здесь растр диска пересчитывается в ТОЧНЫЙ размер заглавной (cap, px) один раз:
# площадной фильтр, порог без сглаживания (бинарный знак), целые метрики; 'i' — курсив Swiss 721 Italic диска, тот же пересчёт (параметр shear — целый сдвиг строк знака, 1:4, для граней без своего курсива).
# Грани: boulder_px11/14/18, swz721_px8/10/12 и swz721_px8i/10i/12i. Старые грани диска остаются выбором. BOULDER_PIX_OLD=1 — этих граней нет
DERIVED = {
    'boulder_px11': ('boulder_24', 11, 0, .5), 'boulder_px14': ('boulder_24', 14, 0, .5), 'boulder_px18': ('boulder_24', 18, 0, .5),
    'swz721_px8': ('swz721n_10', 8, 0, .42), 'swz721_px10': ('swz721n_10', 10, 0, .42), 'swz721_px12': ('swz721n_10', 12, 0, .42),
    'swz721_px8i': ('swz721mi_12', 8, 0, .34), 'swz721_px10i': ('swz721mi_12', 10, 0, .34), 'swz721_px12i': ('swz721mi_12', 12, 0, .34),
}

# Парный кернинг пиксельных граней со своим атласом (№552, Роб 11.10: «LA нравится, TH сливается»): пара (первый знак, второй знак без учёта регистра) -> +px к шагу
# первого. Экспортёр кладёт одну пару в поле res заголовка шрифта (бит 24 — есть пара, первый | второй << 8 | дельта << 16), движок прибавляет при раскладке строки
# (viewer/ui.c kernAdj), панель — так же (typo_panel_page.html kernAt). KERNPAIR_OLD=1 — без пары
KERN = {n: (ord('T'), ord('H'), 1) for n in ('swz721_px8i', 'swz721_px10i', 'swz721_px12i')}


def kern_of(face):
    """имя грани пакета ('swz721_px10i@1+1^2') -> (первый, второй, дельта) или None"""
    if os.environ.get('KERNPAIR_OLD') == '1':
        return None
    return KERN.get(face.partition('+')[0].partition('^')[0].partition('*')[0].partition('@')[0])


def kern_res(face):
    k = kern_of(face)
    return 0 if not k else (1 << 24) | k[0] | (k[1] << 8) | ((k[2] & 255) << 16)


def _cap_rows(src):
    a = np.asarray(src['image'].convert('RGBA'))[..., 3].astype(np.float32) / 255
    g = src['glyphs'][ord('H')]
    cell = a[int(g['y']):int(g['y']) + int(g['h']), int(g['x']):int(g['x']) + int(g['w'])]
    ys = np.where(cell.max(1) > .3)[0]
    return a, int(ys[-1] - ys[0] + 1)


def pixelize(src, cap, shear, thr):
    """-> запись all_fonts (image RGBA, glyphs, height, baseline, pixel=True) с заглавной ровно cap px"""
    from PIL import Image as _I
    a, caph = _cap_rows(src)
    k = cap / caph; P = 2
    lh = max(cap + 2, int(round(src['height'] * k))); base = int(round(src['baseline'] * k))
    cells = {}
    for code, g in src['glyphs'].items():
        gw = int(round(g['w']))
        if code < 32 or not gw:
            cells[code] = (None, 0, int(max(1, round(g['adv'] * k))) if code == 32 else 0)
            continue
        x0, y0 = int(round(g['x'])), int(round(g['y']))
        cell = a[y0:y0 + int(round(g['h'])), x0:x0 + gw]
        pad = np.pad(cell, ((0, 0), (P, P)))
        tw = max(1, int(round((gw + 2 * P) * k)))
        bm = np.asarray(_I.fromarray(pad, 'F').resize((tw, lh), _I.BOX)) >= thr
        if shear:                                    # курсив: строка выше базовой линии сдвигается вправо целыми пикселями
            ext = int(np.ceil(shear * base)) + 1
            out = np.zeros((lh, bm.shape[1] + ext), bool)
            for r in range(lh):
                d = max(0, int(round(shear * (base - r))))
                out[r, d:d + bm.shape[1]] = bm[r]
            bm = out
        cols = np.where(bm.any(0))[0]
        if not len(cols):
            cells[code] = (None, 0, int(max(1, round(g['adv'] * k)))); continue
        c0, c1 = cols[0], cols[-1] + 1
        left = int(np.floor(g['left'] * k - P * k + c0 + .5))
        cells[code] = (bm[:, c0:c1], left, int(max(1, round(g['adv'] * k))) + (1 if shear else 0))
    cw = max([c[0].shape[1] for c in cells.values() if c[0] is not None] + [1]) + 1
    codes = sorted(cells); ncol = 16; nrow = (len(codes) + ncol - 1) // ncol
    img = np.zeros((nrow * (lh + 1), ncol * cw, 4), np.uint8); glyphs = {}
    for i, code in enumerate(codes):
        bm, left, adv = cells[code]; x0, y0 = (i % ncol) * cw, (i // ncol) * (lh + 1)
        w = 0
        if bm is not None:
            w = bm.shape[1]; img[y0:y0 + lh, x0:x0 + w][bm] = 255
        glyphs[code] = dict(x=x0, y=y0, w=w, h=lh, adv=adv, left=left)
    return dict(image=_I.fromarray(img, 'RGBA'), glyphs=glyphs, height=lh, baseline=base, pixel=True)


def derived(fonts):
    if os.environ.get('BOULDER_PIX_OLD') == '1':
        return {}
    return {n: pixelize(fonts[s], c, sh, t) for n, (s, c, sh, t) in DERIVED.items() if s in fonts}


def used(faces):
    """имена шрифтов Lauke среди граней пакета ('bart_16bit@1+1', 'simpsons_pix*2' -> bart_16bit, simpsons_pix)"""
    base = {f.partition('+')[0].partition('^')[0].partition('*')[0].partition('@')[0] for f in faces}
    return [n for n in FONTS if n in base]


def sidecar(out, faces):
    """рядом с пакетом <out>.fonts — имена использованных шрифтов Lauke, по строке (нет таких — файла нет); читает tools/final-build.sh -> fontcredit.txt"""
    side = Path(str(out) + '.fonts'); names = used(faces)
    if names:
        side.write_text('\n'.join(names) + '\n')
    elif side.exists():
        side.unlink()


def credit_text(names):
    """fontcredit.txt рядом с EBOOT: строки по ширине экрана (52 знака, шрифт 8 px), указание автора и лицензии CC BY 3.0"""
    out = []
    for n in FONTS:
        if n in names:
            out += FONTS[n]['lines']
    return '\n'.join(out) + ('\n' if out else '')


if __name__ == '__main__':      # python tools/pixel_fonts.py DIR_OR_FILES... -> fontcredit.txt на stdout: объединение *.fonts
    import sys
    names = set()
    for a in sys.argv[1:]:
        for f in ([Path(a)] if Path(a).is_file() else sorted(Path(a).rglob('*.fonts'))):
            names |= set(f.read_text().split())
    sys.stdout.write(credit_text(names))
