#!/usr/bin/env python3
"""Правка №1095 (02.11.2026): тест покрытия шрифтов знаками с диакритикой + лист образцов для Роба.
  python3 tests/font1095/check.py [--sheet ПУТЬ.png]     код 1 — есть дыры. FONT1095_OLD=1 — как было (тест обязан упасть).
Проверяет в каждой грани: все знаки списка Роба (é è ê ë à â ç ô û ù ü ï î œ æ ã õ á í ó ú, заглавные, « » ¿ ¡) нарисованы (чернила есть), ширина шага задана;
знак с диакритикой не совпадает с основой; чернила не выходят из клетки; у пиксельных граней основа под знаком не сдвинута (нижние строки строчной те же, что у основы);
строка «Économie à São Paulo, ça déçoit» без пустых клеток. Листы — в build/font1095/ (картинки с дисковых шрифтов не коммитить)."""
import os, sys
from pathlib import Path
import numpy as np
from PIL import Image
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import ui_export as U
import accents as A

FACES_PIXEL = ['msx_thin', 'pix8_8', 'bart_16bit', 'simpsons_pix']
FACES_DISC = ['boulder_24', 'boulder_16', 'swz721n_10', 'swz721mi_12', 'Tt2001m__14', 'boulder_px11', 'boulder_px14', 'boulder_px18',
              'swz721_px8', 'swz721_px10', 'swz721_px12', 'swz721_px8i', 'swz721_px10i', 'swz721_px12i']
NEED = A.NEEDED + A.NEEDED_1111 + 'ÀÁÂÃÄÈÉÊËÌÍÎÏÒÓÔÕÖÙÚÛÜÑñäö' 
STRING = 'Économie à São Paulo, ça déçoit'
fails = []


def alpha(f):
    return np.asarray(f['image'].convert('RGBA'))[..., 3]


def glyph(f, a, code):
    g = f['glyphs'].get(code)
    if g is None:
        return None
    x, y, w, h = int(g['x']), int(g['y']), int(g['w']), int(g['h'])
    return a[y:y + h, x:x + w], g


def ck(cond, msg):
    if not cond:
        fails.append(msg)


def main():
    F = U.all_fonts()
    for face in FACES_PIXEL + FACES_DISC:
        f = F.get(face)
        ck(f is not None, f'{face}: грани нет')
        if f is None:
            continue
        a = alpha(f); miss = []
        for ch in NEED:
            code = A.code_of(ch)
            r = glyph(f, a, code)
            if r is None or not (r[0] > 40).any() or r[1].get('adv', 0) <= 0:
                miss.append(ch)
        ck(not miss, f'{face}: нет знаков {"".join(miss)}')
        # строка без пустых клеток
        blanks = [ch for ch in STRING if ch != ' ' and (glyph(f, a, A.code_of(ch)) is None or not (glyph(f, a, A.code_of(ch))[0] > 40).any())]
        ck(not blanks, f'{face}: «{STRING}» пустые клетки {"".join(blanks)}')
        if face in FACES_PIXEL:
            hi = face in ('msx_thin', 'pix8_8')          # у msx/pix8 строчные ниже заглавных (знак в строках 0–1), у Lauke все буквы одной высоты (знак в строке 0)
            for ch in 'éèêëàâôûùüïîãõáíóúçÉÈÊËÀÂÔÛÙÜÏÎÃÕÁÍÓÚÇ':
                r = glyph(f, a, ord(ch)); base = glyph(f, a, ord(A.COMBOS[ch][1]))
                ck(r is not None and base is not None, f'{face}: нет {ch} или основы')
                if r is None or base is None:
                    continue
                m, b = r[0] > 40, base[0] > 40
                ck(m.shape[0] <= 10 and m.shape[1] <= 10, f'{face}: {ch} вне клетки')
                ck(not (m.shape == b.shape and np.array_equal(m, b)), f'{face}: {ch} совпал с основой')
                c0 = int(base[1]['left'] - r[1]['left'])      # на сколько знак раздвинул клетку влево
                c0 = max(c0, 0)
                if ch.islower() and hi and ch not in 'çìíîï':
                    ck(np.array_equal(m[2:7, c0:c0 + b.shape[1]], b[2:7]) if m.shape[1] >= c0 + b.shape[1] else False, f'{face}: {ch} основа изменена ниже знака')       # базовая линия и тело не сдвинуты
                if not hi and ch not in 'çìíîïÇ':
                    top = int(np.where(b.any(1))[0][0])
                    ck(np.array_equal(m[top:, c0:c0 + b.shape[1]], b[top:]) if m.shape[1] >= c0 + b.shape[1] else False, f'{face}: {ch} основа изменена')
                if ch in 'çÇ':
                    ck(m.shape[1] >= c0 + b.shape[1] and (m[:, c0:c0 + b.shape[1]] >= b).all() and m.any(1).sum() > b.any(1).sum(), f'{face}: {ch} без хвоста или основа стёрта')
                ys = np.where(m.any(1))[0]
                if ch != 'ç' and ch != 'Ç':
                    ck(ys[0] >= 0 and ys[-1] <= 7, f'{face}: {ch} строки {ys[0]}–{ys[-1]}, выше кегля 8')
            if hi:
                for ch in 'ÉÀÔÃÜ':
                    r = glyph(f, a, ord(ch))
                    if r is None:
                        ck(False, f'{face}: нет {ch}'); continue
                    m = r[0] > 40
                    ys = np.where(m.any(1))[0]
                    ck(ys[0] == 0 and ys[-1] == 6, f'{face}: {ch} занимает строки {ys[0]}–{ys[-1]}, нужно 0–6 (в пределах заглавной)')
        # знаки кнопок и коды ¿ / Œ œ: 0xBF остаётся кнопкой X, настоящий ¿ на 0x8F
        if face in ('msx_thin', 'pix8_8'):
            ck(0xBF in f['glyphs'] and f['glyphs'][0xBF]['w'] == 8, f'{face}: код 0xBF не кнопка X')
    ck(A.font_bytes('œŒ¿é') == bytes([0x9C, 0x8C, 0x8F, 0xE9]), 'font_bytes: кодировка œ Œ ¿ é')
    print(('FAIL' if fails else 'PASS'), len(fails))
    for m in fails:
        print('  ', m)
    # лист
    sheet = None
    for i, x in enumerate(sys.argv):
        if x == '--sheet' and i + 1 < len(sys.argv):
            sheet = Path(sys.argv[i + 1])
    if sheet:
        make_sheet(F, sheet)
    return 1 if fails else 0


def draw_string(f, a, s, scale=1, color=(255, 255, 255)):
    h = int(f['height']); W = 0
    pos = []
    for ch in s:
        g = f['glyphs'].get(A.code_of(ch))
        adv = int(g['adv']) if g else 4
        pos.append((ch, W, g)); W += adv
    out = np.zeros((h, W + 8, 4), np.uint8)
    for ch, x, g in pos:
        if g is None or g['w'] < 1:
            continue
        gx, gy, gw, gh = int(g['x']), int(g['y']), int(g['w']), int(g['h'])
        m = a[gy:gy + gh, gx:gx + gw]
        xx = x + int(g['left']) + 2
        sub = out[:gh, max(xx, 0):xx + gw]
        mm = m[:, :sub.shape[1]]
        sub[..., :3] = np.maximum(sub[..., :3], (mm[..., None] * np.array(color) / 255).astype(np.uint8))
        sub[..., 3] = np.maximum(sub[..., 3], mm)
    im = Image.fromarray(out, 'RGBA')
    return im.resize((im.width * scale, im.height * scale), Image.NEAREST)


def make_sheet(F, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    faces = FACES_PIXEL + ['boulder_24', 'swz721n_10', 'Tt2001m__14', 'boulder_px14', 'swz721_px10']
    chars = A.NEEDED + 'ÄËÏÖÑñ'
    rows = []
    for face in faces:
        f = F[face]; a = alpha(f)
        big = 5 if f['height'] <= 12 else 2
        title = draw_string(F['msx_thin'], alpha(F['msx_thin']), face + f'  (cell height {int(f["height"])}px; big x{big}; last two lines: x{big} and game size 1:1)', 1)
        letters = [draw_string(f, a, ch + ' ', big) for ch in chars]
        per = 20; lines = [letters[i:i + per] for i in range(0, len(letters), per)]
        line = draw_string(f, a, STRING, big)
        line1 = draw_string(f, a, STRING, 1)
        lh = max(i.height for i in letters)
        wmax = max(max(sum(i.width for i in ln) for ln in lines), line.width) + 20
        h = title.height + 6 + len(lines) * (lh + 6) + line.height + 6 + line1.height + 14
        im = Image.new('RGBA', (wmax, h), (40, 40, 48, 255))
        im.alpha_composite(title, (6, 2)); y = title.height + 8
        for ln in lines:
            x = 6
            for l in ln:
                im.alpha_composite(l, (x, y)); x += l.width
            y += lh + 6
        im.alpha_composite(line, (6, y)); y += line.height + 6
        im.alpha_composite(line1, (6, y))
        rows.append(im)
    W = max(r.width for r in rows); H = sum(r.height for r in rows)
    out = Image.new('RGBA', (W, H), (40, 40, 48, 255)); y = 0
    for r in rows:
        out.alpha_composite(r, (0, y)); y += r.height
    out.convert('RGB').save(path)
    print('лист:', path, out.size)


if __name__ == '__main__':
    sys.exit(main())
