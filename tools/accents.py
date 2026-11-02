#!/usr/bin/env python3
"""Буквы с диакритикой для шрифтов игры (правка Роба №1095, 02.11.2026): французский, португальский и бразильский.

Откуда что берётся (опись):
  * bart_16bit, simpsons_pix (Patrick H. Lauke, CC BY 3.0, assets/fonts/pixel): все знаки Latin-1 уже есть в файлах шрифта; не было только œ Œ (вне Latin-1) и
    настоящего ¿ (код 0xBF занят знаком кнопки X). Их добавляет pixel_fonts.py на свободные коды (см. FONT_CODE ниже).
  * шрифты Scrooby с диска (boulder_16/24, swz721n_10, swz721mi_12, Tt2001m__14 и производные boulder_px*, swz721_px*): на диске есть á â ä é ê í î ï ó ô ö ú û ü à è ç ñ Á Ä É Ê Í Ñ Ó Ö Ú Ü ß ¡ Œ œ;
    остальное складывается здесь из знаков диска: основа из диска + знак из ближайшего знака диска (тот же шрифт, тот же размер, та же высота над строкой), без новой графики
    (patch_disc). Лигатуры æ Æ и кавычки « » собраны из букв и стрелок того же шрифта; ¿ — ? повёрнутый на 180°.
  * встроенные пиксельные грани pix8_8 (рукописная 6×7) и msx_thin (шрифт PSPSDK 5×7): были только ASCII; недостающее дорисовано в той же сетке и толщине (add_pixel):
    кегль 8 (клетка 10×10, база 8), шаг по ширине без изменений, никакой обводки; знак над строчной в двух верхних строках (клетка 0–1) без сдвига базовой линии; у заглавных
    знак уменьшен до одной строки (клетка 0), а тело буквы теряет одну строку-дубль (высота знака вместе с буквой не больше высоты заглавной).
Выключатель FONT1095_OLD=1 (переменная окружения) — никаких новых знаков, как было.

Кодировка: знак = код Latin-1; исключения (FONT_CODE): Œ -> 0x8C, œ -> 0x9C (как в CP1252), ¿ -> 0x8F (0xBF занят кнопкой X, bible использует его как заместитель). font_bytes() переводит строку в такие коды.
"""
import os

import numpy as np
from PIL import Image

FONT_CODE = {'Œ': 0x8C, 'œ': 0x9C, '¿': 0x8F}
NEEDED = 'éèêëàâçôûùüïîœæãõáíóúÉÈÊËÀÂÇÔÛÙÜÏÎŒÆÃÕÁÍÓÚ«»¿¡'
NEEDED_1111 = 'ßºäöÄÖÜñÑ'   # №1111: немецкий, испанский      # список правки 1095
LATIN_LOWER = 'àáâãäçèéêëìíîïñòóôõöùúûüýÿ'
LATIN_UPPER = 'ÀÁÂÃÄÇÈÉÊËÌÍÎÏÑÒÓÔÕÖÙÚÛÜÝ'
EXTRA = 'æÆœŒ«»¿¡'


def off():
    return os.environ.get('FONT1095_OLD') == '1'


def code_of(ch):
    """код знака в атласе грани"""
    return FONT_CODE.get(ch, ord(ch))


_BTN_X = None


def font_bytes(s):
    """строка -> байты с кодами атласа (Latin-1, кроме Œ œ ¿); знак вне набора -> '?'.
    «¿» в текстах диска — и заместитель кнопки ✕ («PRESS ¿ TO ACCELERATE», «PULSA ¿ PARA ACELERAR»: отдельным словом), и настоящий знак (испанское «¿Seguro?»: слитно с буквой). Кнопка остаётся 0xBF
    (грани пикселей рисуют там ✕), настоящий знак — 0x8F"""
    import re
    out = bytearray()
    for i, ch in enumerate(s):
        c = code_of(ch)
        if ch == '\u00bf' and re.match(r'(^|[ /\\\n])$', s[i - 1:i] if i else '') and re.match(r'^( |/|\\|\n|$)', s[i + 1:i + 2]):
            c = 0xBF
        out.append(c if c < 256 else ord('?'))
    return bytes(out)


# ───────────────────────────── встроенные пиксельные грани (клетка 10×10, база 8) ─────────────────────────────
# знаки над строчной: две строки (клетка 0–1); у заглавной — одна (клетка 0). Рисунки: '#' — пиксель. thin — 1 px (msx), thick — 2 px (рукописная pix8)
MARKS = {'thin': {}, 'thick': {}}
# рисунки знаков: (над строчной — две строки, над заглавной — одна, под буквой — для ced)
MARKS['thin'].update({
    'acute': (['..#', '.#.'], ['..#']),
    'grave': (['#..', '.#.'], ['#..']),
    'circ': (['.#.', '#.#'], ['.#.']),
    'dia': (['#.#', '...'], ['#.#']),
    'tilde': (['.#.#', '#.#.'], ['.#.#']),
    'ced': (['.#.', '#..'], None),
})
MARKS['thick'].update({
    'acute': (['..##', '.##.'], ['.##']),
    'grave': (['##..', '.##.'], ['##.']),
    'circ': (['.##.', '##.#'], ['.##.']),
    'dia': (['##.##', '.....'], ['##.##']),
    'tilde': (['.##.##', '##.##.'], ['.##.##']),
    'ced': (['.#', '##'], None),
})
# (знак, основа): строчные и заглавные Latin-1, которые рисуем из букв ASCII
COMBOS = {
    'à': ('grave', 'a'), 'á': ('acute', 'a'), 'â': ('circ', 'a'), 'ã': ('tilde', 'a'), 'ä': ('dia', 'a'),
    'è': ('grave', 'e'), 'é': ('acute', 'e'), 'ê': ('circ', 'e'), 'ë': ('dia', 'e'),
    'ì': ('grave', 'i'), 'í': ('acute', 'i'), 'î': ('circ', 'i'), 'ï': ('dia', 'i'),
    'ò': ('grave', 'o'), 'ó': ('acute', 'o'), 'ô': ('circ', 'o'), 'õ': ('tilde', 'o'), 'ö': ('dia', 'o'),
    'ù': ('grave', 'u'), 'ú': ('acute', 'u'), 'û': ('circ', 'u'), 'ü': ('dia', 'u'),
    'ñ': ('tilde', 'n'), 'ç': ('ced', 'c'), 'ý': ('acute', 'y'), 'ÿ': ('dia', 'y'),
    'À': ('grave', 'A'), 'Á': ('acute', 'A'), 'Â': ('circ', 'A'), 'Ã': ('tilde', 'A'), 'Ä': ('dia', 'A'),
    'È': ('grave', 'E'), 'É': ('acute', 'E'), 'Ê': ('circ', 'E'), 'Ë': ('dia', 'E'),
    'Ì': ('grave', 'I'), 'Í': ('acute', 'I'), 'Î': ('circ', 'I'), 'Ï': ('dia', 'I'),
    'Ò': ('grave', 'O'), 'Ó': ('acute', 'O'), 'Ô': ('circ', 'O'), 'Õ': ('tilde', 'O'), 'Ö': ('dia', 'O'),
    'Ù': ('grave', 'U'), 'Ú': ('acute', 'U'), 'Û': ('circ', 'U'), 'Ü': ('dia', 'U'),
    'Ñ': ('tilde', 'N'), 'Ç': ('ced', 'C'), 'Ý': ('acute', 'Y'),
}
def _ink(a):
    cols = np.where(a.any(0))[0]
    return (int(cols[0]), int(cols[-1])) if len(cols) else (0, 0)


def _rows(a):
    ys = np.where(a.any(1))[0]
    return (int(ys[0]), int(ys[-1])) if len(ys) else (0, 0)


def _put(dst, mark, x_center2, y0):
    """нарисовать знак (список строк) с центром на удвоенной координате x_center2 (то есть 2*x), верхняя строка в строке y0. Возврат — левый столбец знака (может быть < 0)"""
    mw = max(len(r) for r in mark)
    x0 = int(round((x_center2 - (mw - 1)) / 2.0 - 1e-6))
    for dy, row in enumerate(mark):
        for dx, c in enumerate(row):
            if c == '#' and 0 <= y0 + dy < dst.shape[0] and 0 <= x0 + dx < dst.shape[1]:
                dst[y0 + dy, x0 + dx] = True
    return x0, mw


def _shrunk(a, t, b):
    """убрать одну строку-дубль среди внутренних строк t+1..b-1 (иначе t+2), остальное сдвинуть вниз; нижняя строка b остаётся на месте, строка t освобождается"""
    rows = [a[y].copy() for y in range(t, b + 1)]
    n = len(rows); d = min(2, n - 2)
    for k in (n // 2, n // 2 - 1, n // 2 + 1, 1, n - 2):
        if 1 <= k <= n - 2 and ((rows[k] == rows[k - 1]).all() or (rows[k] == rows[k + 1]).all()):
            d = k
            break
    new = [np.zeros_like(rows[0])] + rows[:d] + rows[d + 1:]
    out = a.copy()
    for i, r in enumerate(new):
        out[t + i] = r
    return out


def _metrics(cellf):
    """верхняя строка заглавной, нижняя строка заглавной, верхняя строка x-высоты строчной (по 'H' и 'x' самой грани)"""
    ct, cb = _rows(cellf(ord('H')))
    xt = _rows(cellf(ord('x')))[0]
    return ct, cb, xt


def _combine(base, kind, lower, style, met):
    """base: bool 10×w клетка основы. -> (bool 10×w' клетка, сдвиг левого края (<=0))"""
    ct, cb, xt = met
    mk = MARKS[style][kind]
    a = base.copy()
    c0, c1 = _ink(a)
    ctr2 = c0 + c1
    if kind == 'ced':
        y0 = _rows(a)[1] + 1
        w = max(a.shape[1], c1 + 3)
        out = np.zeros((10, w), bool); out[:, :a.shape[1]] = a
        _put(out, mk[0], ctr2, y0)
        return out, 0
    distinct = xt > ct                          # у грани есть строчные ниже заглавных (msx, pix8); у Lauke строчные той же высоты, что заглавные
    pad = 3
    wide = np.zeros((10, a.shape[1] + 2 * pad), bool); wide[:, pad:pad + a.shape[1]] = a
    ctr2 += 2 * pad
    if lower and distinct:
        if xt >= 2:
            wide[:xt] = False                   # у «i» и «j» точка заменяется знаком
            _put(wide, mk[0], ctr2, xt - 2)
        else:
            wide = _shrunk(wide, 0, cb); _put(wide, mk[0], ctr2, 0)
    else:
        if ct >= 1:
            _put(wide, mk[1], ctr2, ct - 1)
        else:
            wide = _shrunk(wide, ct, cb); _put(wide, mk[1], ctr2, 0)
    c0, c1 = _ink(wide)
    return wide[:, c0:c1 + 1], c0 - pad - _ink(base)[0] if False else (c0 - pad)


def _ligature(a, b, overlap, bridge=True):
    h = 10
    if a.shape[1] + b.shape[1] - overlap > 10:            # в клетку 10 не влезает: левая часть первой буквы и правая часть второй
        a = a[:, :6]; b = b[:, max(0, b.shape[1] - 5):]; overlap = 1
        a = a[:, :max(1, _ink(a)[1] + 1)]
    wa, wb = a.shape[1], b.shape[1]
    out = np.zeros((h, wa + wb - overlap), bool)
    out[:, :wa] |= a
    out[:, wa - overlap:] |= b
    if bridge:
        for y in range(h):
            for x in range(1, out.shape[1] - 1):
                if not out[y, x] and out[y, x - 1] and out[y, x + 1] and (wa - overlap - 1 <= x <= wa):
                    out[y, x] = True
    return out


def _flip180(a):
    ys = np.where(a.any(1))[0]
    y0, y1 = int(ys[0]), int(ys[-1])
    b = a.copy()
    b[y0:y1 + 1] = a[y0:y1 + 1][::-1, ::-1]
    return b


# ß и º (№1111: немецкий, испанский и португальский порядковые): рисунок от верхней строки заглавной
SHARP_S = {'thin': ['.##..', '#..#.', '#.#..', '#..#.', '#...#', '#...#', '#.##.'],
           'thick': ['.####.', '##..##', '##.##.', '##..##', '##..##', '##..##', '##.###']}
ORD_O = {'thin': ['.#.', '#.#', '.#.'], 'thick': ['.##.', '#..#', '.##.']}


QUOTES = {
    'thin': (['..#.#', '.#.#.', '#.#..', '.#.#.', '..#.#'], 2),
    'thick': (['..##.##', '.##.##.', '##.##..', '.##.##.', '..##.##'], 2),
}


def add_pixel(image, glyphs, style):
    """дорисовать в атлас пиксельной грани (160×160 RGBA, клетка 10×10 на код) все знаки с диакритикой. image — numpy, glyphs — словарь записей, style 'thin' или 'thick'"""
    if off():
        return
    def cell(code):
        g = glyphs[code]; x0, y0 = (code % 16) * 10, (code // 16) * 10
        return image[y0:y0 + 10, x0:x0 + max(int(g['w']), 1), 3] > 0
    def store(code, m, adv=None, left=0):
        m = m[:, :10]
        x0, y0 = (code % 16) * 10, (code // 16) * 10
        image[y0:y0 + 10, x0:x0 + 10] = 0
        image[y0:y0 + 10, x0:x0 + m.shape[1]][m] = 255
        c1 = _ink(m)[1]
        w = c1 + 1 if m.any() else 2
        glyphs[code] = dict(x=x0, y=y0, w=w, h=10, adv=adv if adv is not None else w + 1, left=left)
    met = _metrics(cell)
    for ch, (kind, b) in COMBOS.items():
        code = ord(ch)
        if code in glyphs:
            continue
        base = cell(ord(b))
        m, dl = _combine(base, kind, b.islower(), style, met)
        store(code, m, glyphs[ord(b)]['adv'] if kind != 'ced' else None, glyphs[ord(b)]['left'] + dl)
    ct, cb, xt = met
    q, ov = QUOTES[style]
    qm = np.array([[c == '#' for c in r] for r in q]); qa = np.zeros((10, qm.shape[1]), bool)
    ymid = (max(xt, ct) + cb) // 2                      # середина x-высоты
    qa[ymid - 2:ymid + 3] = qm
    store(0xAB, qa); store(0xBB, qa[:, ::-1])           # правый — зеркало левого
    if 0xA1 not in glyphs: store(0xA1, _flip180(cell(ord('!'))), left=glyphs[ord('!')]['left'])
    store(FONT_CODE['\u00bf'], _flip180(cell(ord('?'))), left=glyphs[ord('?')]['left'])
    for code, art, dy in ((0xDF, SHARP_S[style], 0), (0xBA, ORD_O[style], 0)):   # ß, º
        if code in glyphs: continue
        w = max(len(r) for r in art); m = np.zeros((10, w), bool)
        for k, row in enumerate(art):
            for x, c in enumerate(row): m[ct + dy + k, x] = c == '#'
        store(code, m)
    # лигатуры
    store(0xE6, _ligature(cell(ord('a')), cell(ord('e')), 1), left=glyphs[ord('a')]['left'])
    store(0xC6, _ligature(cell(ord('A')), cell(ord('E')), 2), left=glyphs[ord('A')]['left'])
    store(FONT_CODE['\u0153'], _ligature(cell(ord('o')), cell(ord('e')), 1), left=glyphs[ord('o')]['left'])
    store(FONT_CODE['\u0152'], _ligature(cell(ord('O')), cell(ord('E')), 1), left=glyphs[ord('O')]['left'])


# ───────────────────────────── шрифты диска Scrooby (альфа-атлас с мягкими краями) ─────────────────────────────
def _glyph_alpha(font, a, code):
    g = font['glyphs'][code]
    x0, y0, w, h = int(g['x']), int(g['y']), int(g['w']), int(g['h'])
    return a[y0:y0 + h, x0:x0 + w].copy(), int(g['left']), int(g['adv'])


def _top_row(m, thr=0.3):
    ys = np.where(m.max(1) > thr)[0]
    return int(ys[0]) if len(ys) else 0


def _bottom_row(m, thr=0.3):
    ys = np.where(m.max(1) > thr)[0]
    return int(ys[-1]) if len(ys) else 0


def _bbox_cols(m, thr=0.3):
    xs = np.where(m.max(0) > thr)[0]
    return (int(xs[0]), int(xs[-1]) + 1) if len(xs) else (0, 1)


def patch_disc(font):
    """достроить шрифт диска: знаки, которых на диске нет, из знаков диска. font: {image RGBA, glyphs, height, baseline}"""
    if off() or font.get('_acc1095'):
        return
    G = font['glyphs']
    need = [0x41, 0x45, 0x4f, 0x55, 0x61, 0x65, 0x6f, 0x75, 0x6e, 0x3f]
    if not all(c in G for c in need):
        return
    img = np.asarray(font['image'].convert('RGBA'))
    alpha = img[..., 3].astype(np.float32) / 255
    H = int(font['height'])
    new = {}                                        # код -> (альфа H×w, left, adv)

    def gl(ch):
        return _glyph_alpha(font, alpha, ord(ch) if isinstance(ch, str) else ch)

    def zone(accented, plain, below=False):
        """знак из accented над основой plain: -> (маска строк H×w, центр знака по x в координатах пера, центр основы)"""
        am, al, _ = gl(accented); pm, pl, _ = gl(plain)
        z = am.copy()
        if below:
            z[:_bottom_row(pm) + 1] = 0
        else:
            z[_top_row(pm):] = 0
        x0, x1 = _bbox_cols(z)
        px0, px1 = _bbox_cols(pm)
        return z[:, x0:x1], al + x0, (al + (x0 + x1) / 2.0) - (pl + (px0 + px1) / 2.0)

    def compose(base_ch, acc, flip=False, clear_above=None, name=None):
        bm, bl, badv = gl(base_ch)
        if clear_above is not None:
            bm = bm.copy(); bm[:clear_above] = 0
        zm, zl, off_c = acc
        if flip:
            zm = zm[:, ::-1]; off_c = -off_c
        bx0, bx1 = _bbox_cols(bm)
        base_ctr = bl + (bx0 + bx1) / 2.0
        zctr_pen = base_ctr + off_c
        zleft = int(round(zctr_pen - zm.shape[1] / 2.0))
        lo = min(bl, zleft); hi = max(bl + bm.shape[1], zleft + zm.shape[1])
        out = np.zeros((H, hi - lo), np.float32)
        out[:bm.shape[0], bl - lo:bl - lo + bm.shape[1]] = np.maximum(out[:bm.shape[0], bl - lo:bl - lo + bm.shape[1]], bm)
        zh = zm.shape[0]
        out[:zh, zleft - lo:zleft - lo + zm.shape[1]] = np.maximum(out[:zh, zleft - lo:zleft - lo + zm.shape[1]], zm)
        return out, lo, badv

    # знаки: берём у ближайших знаков диска. Строчные — над x-высотой, заглавные — над высотой заглавной
    acute_l, circ_l, dia_l, tilde_l = zone('é', 'e'), zone('ê', 'e'), zone('ö', 'o'), zone('ñ', 'n')
    acute_u, circ_u, dia_u, tilde_u = zone('É', 'E'), zone('Ê', 'E'), zone('Ä', 'A'), zone('Ñ', 'N')
    ced = zone('ç', 'c', below=True)
    specs = {
        'ã': ('a', tilde_l, False), 'ë': ('e', dia_l, False), 'õ': ('o', tilde_l, False), 'ò': ('o', acute_l, True), 'ù': ('u', acute_l, True),
        'À': ('A', acute_u, True), 'Â': ('A', circ_u, False), 'Ã': ('A', tilde_u, False), 'È': ('E', acute_u, True), 'Ë': ('E', dia_u, False),
        'Ì': ('I', acute_u, True), 'Î': ('I', circ_u, False), 'Ï': ('I', dia_u, False), 'Ò': ('O', acute_u, True), 'Ô': ('O', circ_u, False),
        'Õ': ('O', tilde_u, False), 'Ù': ('U', acute_u, True), 'Û': ('U', circ_u, False), 'Ç': ('C', ced, False),
    }
    for ch, (b, acc, flip) in specs.items():
        if ord(ch) not in G:
            new[ord(ch)] = compose(b, acc, flip)
    if 0xEC not in G and 0xED in G:               # ì: í без знака + гравис
        im, il, iadv = gl('í'); utop = _top_row(gl('u')[0])
        new[0xEC] = compose('í', acute_l, True, clear_above=utop)
    for ch in 'ýÿ':
        pass                                       # ý ÿ в списке правки нет
    # лигатуры: вторая буква входит в первую на долю ширины
    def lig(a_ch, b_ch, frac):
        am, al, aadv = gl(a_ch); bm, bl, badv = gl(b_ch)
        ov = int(round(bm.shape[1] * frac)); w = am.shape[1] + bm.shape[1] - ov
        out = np.zeros((H, w), np.float32)
        out[:am.shape[0], :am.shape[1]] = am
        out[:bm.shape[0], am.shape[1] - ov:] = np.maximum(out[:bm.shape[0], am.shape[1] - ov:], bm)
        return out, min(al, bl), aadv + badv - int(round(ov * 0.8))
    if 0xE6 not in G: new[0xE6] = lig('a', 'e', .30)
    if 0xC6 not in G: new[0xC6] = lig('A', 'E', .45)
    # ¿ — «?» повёрнутый на 180° по его чернилам
    qm, ql, qadv = gl('?'); y0, y1 = _top_row(qm), _bottom_row(qm)
    f = qm.copy(); f[y0:y1 + 1] = qm[y0:y1 + 1][::-1, ::-1]
    new[FONT_CODE['¿']] = (f, ql, qadv)
    # « » — две стрелки «<» «>» того же шрифта, уменьшенные до высоты x-высоты и сдвинутые друг в друга
    for code, src in ((0xAB, '<'), (0xBB, '>')):
        sm, sl, sadv = gl(src)
        x0, x1 = _bbox_cols(sm); y0, y1 = _top_row(sm), _bottom_row(sm) + 1
        crop = sm[y0:y1, x0:x1]
        xh = _bottom_row(gl('x')[0]) - _top_row(gl('x')[0]) + 1
        nh = max(3, int(round(xh * .8))); nw = max(2, int(round(crop.shape[1] * nh / max(1, crop.shape[0]))))
        im = Image.fromarray((crop * 255).astype(np.uint8)).resize((nw, nh), Image.LANCZOS)
        c = np.asarray(im).astype(np.float32) / 255
        step = max(2, int(round(nw * .62)))
        out = np.zeros((H, nw + step + 1), np.float32)
        ytop = _top_row(gl('x')[0]) + (xh - nh) // 2
        out[ytop:ytop + nh, 0:nw] = c
        out[ytop:ytop + nh, step:step + nw] = np.maximum(out[ytop:ytop + nh, step:step + nw], c)
        new[code] = (out, 0, out.shape[1] + 1)
    # Œ œ лежат под кодами Юникода (>255), в атласе пакета нужны под 0x8C / 0x9C
    for u, c8 in ((0x152, 0x8C), (0x153, 0x9C)):
        if u in G and c8 not in G:
            new[c8] = gl(u)
    # дописать в атлас снизу
    iw = img.shape[1]
    items = sorted(new.items())
    rows_needed = 0; x = 0; row_h = H; placed = {}
    ybase = img.shape[0]
    cx, cy = 1, ybase + 1
    for code, (m, left, adv) in items:
        w = m.shape[1]
        if cx + w + 1 > iw:
            cx, cy = 1, cy + H + 1
        placed[code] = (cx, cy, w); cx += w + 1
    total_h = cy + H + 1
    big = np.zeros((total_h, iw, 4), np.uint8)
    big[:img.shape[0]] = img
    for code, (m, left, adv) in items:
        x, y, w = placed[code]
        big[y:y + H, x:x + w, :3] = 255
        big[y:y + H, x:x + w, 3] = np.clip(m[:H] * 255, 0, 255).astype(np.uint8)
        G[code] = dict(x=float(x), y=float(y), w=float(w), h=float(H), adv=float(adv), left=float(left), right=0.0)
    font['image'] = Image.fromarray(big, 'RGBA')
    font['_acc1095'] = True
