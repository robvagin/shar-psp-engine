#!/usr/bin/env python3
"""Пиксельные знаки причастных на заставке GONE Makers (правка Роба 127, 01.10: «очень пиксельные логотипы Диснея, Мэтта
Грейнинга, просто Симпсонов, сайта фанбазы, Sony, PSP — все причастные; шрифтами — плохо; пусть будет как старая игра;
красиво появлялись, подобно GONE Makers, может по-разному; очень сильно оптимизировать»). Своего экрана логотипов у
оригинала нет — правило порта. Знаки — битмапы 1:1 в слоте 144×40, 2 бита на пиксель, 4 серых системной гаммы (правка 101):
индекс 0 — фон, 1..3 — #505050 / #9A9A9A / #E0E0E0, как текст заставки. Без сглаживания сверх палитры.
Входы (по порядку слотов):
  1) с диска Роба: art/frontend/scrooby/frontend.p3d, спрайт gamelogo.png (640×480) — из него две плашки:
     «The Simpsons» (красная) и «HIT & RUN» (зелёная); плашка = строчная выпуклая оболочка пикселей своего цвета
  2) файлы Роба: ~/Downloads/SHAR_логотипы/ и build/assets/boot_marks/ — PNG/JPG (прозрачный или однотонный фон),
     PDF (через sips), SVG (через qlmanage); по имени файла, суффикс _m0.._m5 задаёт появление (иначе по кругу)
Выход build/bootmarks.bin (в git не идёт; рядом с EBOOT по tools/ship_rules.txt). Формат BMK1, little-endian:
  'BMK1' u16 n, u16 slotW, u16 slotH, u16 0; на знак: u16 w, u16 h, u8 bpp=2, u8 режим, u8[14] имя, 4×u32 палитра ABGR,
  биты ceil(w·h/4) байт (строка за строкой, 4 пикселя в байте, старшие биты — левый); в конце u32 crc32 всего до него.
  tools/bootmarks.py [--out F]         собрать
  tools/bootmarks.py --verify [F]      магия, n ≤ 6, каждый знак ≤ 144×40, 2 бита, палитра — 4 серых, режим 0..5, crc
  tools/bootmarks.py --check [F]       F побайтно = сборке из источников (знак без исходного файла — FAIL)
  tools/bootmarks.py --sheet PNG       лист знаков ×3 для глаза"""
import argparse, os, struct, subprocess, sys, tempfile, zlib
from pathlib import Path
import numpy as np
from PIL import Image, ImageFilter
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
SLOT_W, SLOT_H, NSLOT, NMODE = 64, 24, 32, 9   # 397: + кадры покоя (режимы 7/8)   # правка Роба 148: мелкие пиксельные значки, как на экранах BIOS; 254: знаков до 8
PAL = [0xFF000000, 0xFF505050, 0xFF9A9A9A, 0xFFE0E0E0]   # ABGR, серые = системная гамма bootui.h (DIM / KEY / VAL)
MODE_CYCLE = [0, 5, 2, 3, 1, 4]   # волна как GONE, перо, мозаика, растворение, развёртка, палитра NES
ROB_DIRS = [Path.home() / 'Downloads/SHAR_логотипы', ROOT / 'build/assets/boot_marks']
EXT = {'.png', '.jpg', '.jpeg', '.gif', '.bmp', '.webp', '.pdf', '.svg'}
OUT = ROOT / 'build/bootmarks.bin'


def rowhull(m):
    """строчная выпуклая оболочка: в каждой строке всё от первого до последнего пикселя маски (надпись внутри плашки)"""
    o = np.zeros_like(m)
    for y in np.where(m.any(1))[0]:
        xs = np.where(m[y])[0]; o[y, xs[0]:xs[-1] + 1] = True
    return o


def disk_marks():
    """две плашки gamelogo.png с диска: (имя, яркость 0..255, маска знака)"""
    import ui_export as U
    from p3d import read
    p = U.WEB / 'source/game/art/frontend/scrooby/frontend.p3d'
    if not p.exists(): print('  (нет диска: %s)' % p); return []
    im = None
    for c in read(p).children:
        if c.id == 0x19005:
            n, i = U.decode_sprite_image(c)
            if n.startswith('gamelogo'): im = i; break
    if im is None: print('  (в frontend.p3d нет gamelogo)'); return []
    a = np.asarray(im.convert('RGBA')).astype(int); r, g, b = a[..., 0], a[..., 1], a[..., 2]
    lum = (r * 299 + g * 587 + b * 114) // 1000
    out = []   # правка 148: не плашки (они «уродство»), а буквы знака — белым по чёрному
    for name, plate, letters in (('simpsons', (r > 150) & (g < 90) & (b < 90), (r > 190) & (g > 190) & (b > 190)),   # белые буквы на красной табличке
                                 ('hitandrun', (g > 120) & (r < 90) & (b < 140) & (g > r + 50), lum > 200)):
        hull = rowhull(plate); m = hull & letters
        out.append((name, np.where(m, 255, 0), m))
    return out


def movie_marks():
    """знаки из роликов диска (последний кадр — знак стоит): Fox, Gracie Films, Radical, Vivendi Universal Games"""
    import tempfile as _t, movie_export as me
    src = ROOT / 'hit-and-run-web/source/game/movies'; out = []
    for name, clip in (('fox', 'foxlogo'), ('gracie', 'gracie'), ('radical', 'radlogo'), ('vivendi', 'vuglogo')):
        p = src / (clip + '.rmv')
        if not p.exists(): continue
        try:
            with _t.TemporaryDirectory() as d: frs = me.decode_video(me.rmv_parse(p.read_bytes(), clip), d)
        except Exception as e: print('  пропуск %s: %s' % (clip, e)); continue
        fr = frs[-1].astype(int)
        if name == 'fox':   # правка 189: последний кадр foxlogo чёрный — кадр, где золотого монумента больше всего (кадр 110 из 513)
            def gold(f): f = f.astype(int); return int(((f[..., 0] > 140) & (f[..., 1] > 100) & (f[..., 2] < 110) & (f[..., 0] > f[..., 2] + 60)).sum())
            fr = max(frs, key=gold).astype(int)
        r, g, b = fr[..., 0], fr[..., 1], fr[..., 2]; lum = (r * 299 + g * 587 + b * 114) // 1000
        if name == 'fox': m = (r > 140) & (g > 100) & (b < 110) & (r > b + 60); m[int(m.shape[0] * .55):] = False
        elif name == 'gracie':
            box = (b > r + 50) & (b > 90); ys, xs = np.where(box)
            m = np.zeros_like(box)
            if len(ys): y0, y1, x0, x1 = ys.min(), ys.max(), xs.min(), xs.max(); m[y0:y1 + 1, x0:x1 + 1] = box[y0:y1 + 1, x0:x1 + 1]
        elif name == 'radical':
            m = lum > 70; ys = np.where(m.any(1))[0]
            if len(ys): m[int(ys.min() + (ys.max() - ys.min()) * .62):] = False   # строка RAD(i)CAL без мелкой подписи
        else: m = lum < 170
        out.append((name, np.where(m, 255, 0), m))
    return out


def type_marks():
    """знаков нет на диске (Matt Groening, Disney, Sony, PSP) — набор шрифтом Mac в пиксели; файл Роба с тем же именем важнее"""
    from PIL import ImageDraw, ImageFont
    S = '/System/Library/Fonts/Supplemental/'; out = []
    for name, text, font, idx in (('groening', 'Matt Groening', 'Bradley Hand Bold.ttf', 0), ('disney', 'Disney', 'Brush Script.ttf', 0),
                                  ('sony', 'SONY', 'Rockwell.ttc', 2), ('psp', 'PSP', 'Futura.ttc', 2)):
        try: f = ImageFont.truetype(S + font, 160, index=idx)
        except Exception as e: print('  пропуск %s: %s' % (name, e)); continue
        im = Image.new('L', (160 * len(text) + 80, 320), 0); ImageDraw.Draw(im).text((20, 40), text, font=f, fill=255)
        a = np.asarray(im).astype(int); out.append((name, a, a > 127))
    return out


def load_file(f):
    """файл Роба -> RGBA; SVG headlessly with installed CairoSVG, never Quick Look."""
    if f.suffix.lower() == '.svg':
        import io
        try:
            import cairosvg
            png = cairosvg.svg2png(url=str(f), output_width=1200)
        except ImportError:
            # Project venv has no CairoSVG; reuse the existing Homebrew interpreter, never install packages
            python = os.environ.get('SVG_PYTHON', '/opt/homebrew/bin/python3')
            png = subprocess.run([python, '-m', 'cairosvg', '-f', 'png', '--output-width', '1200', str(f)], capture_output=True, check=True).stdout
        return Image.open(io.BytesIO(png)).convert('RGBA')
    if f.suffix.lower() == '.pdf':
        with tempfile.TemporaryDirectory() as d:
            png = Path(d) / 'p.png'; subprocess.run(['sips', '-s', 'format', 'png', str(f), '--out', str(png)], capture_output=True)
            if not png or not png.exists(): raise ValueError('не удалось растрировать')
            return Image.open(png).convert('RGBA')
    return Image.open(f).convert('RGBA')


def rob_marks():
    out = []
    for d in ROB_DIRS:
        if d.is_dir():
            for f in sorted(d.iterdir()):
                if f.suffix.lower() not in EXT: continue
                try: im = load_file(f)
                except Exception as e: print('  пропуск %s: %s' % (f.name, e)); continue
                if f.stem == 'fox':   # правка 189: 20th_Century_Fox_logo.svg (Commons) — только буквы монумента, без прожекторов и цоколя
                    W_, H_ = im.size; im = im.crop((int(W_ * .18), int(H_ * .08), int(W_ * .97), int(H_ * .76)))
                a = np.asarray(im).astype(int); lum = (a[..., 0] * 299 + a[..., 1] * 587 + a[..., 2] * 114) // 1000
                border = np.concatenate([lum[0], lum[-1], lum[:, 0], lum[:, -1]]); bg = int(np.median(border))
                ink = np.abs(lum - bg)   # знак на белом -> светлый на чёрном
                m = (a[..., 3] >= 128) & (ink >= 24) if (a[..., 3] < 128).any() else ink >= 24
                if (a[..., 3] < 128).any(): ink = np.where(a[..., 3] >= 128, np.maximum(ink, lum), 0)
                out.append((f.stem, ink, m))
    return out


def to_mark(name, val, mask, k):
    """кроп по маске, уменьшение усреднением (BOX) в слот 144×40 1:1, 4 уровня серого"""
    ys, xs = np.where(mask)
    if not len(ys): return None
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    m = mask[y0:y1, x0:x1].astype(np.float32); v = m * 255.0   # правка 148: 1 бит — белый по чёрному, без серых плашек
    th = {'simpsons': 24, 'fox': 24, 'disney': 22, 'hitandrun': 12, 'groening': 16, 'sony': 11, 'psp': 13}.get(name.split('_m')[0], SLOT_H)   # одна высота строки знаков
    s = min(SLOT_W / (x1 - x0), min(th, SLOT_H) / (y1 - y0), 1.0)
    w, h = max(1, int((x1 - x0) * s)), max(1, int((y1 - y0) * s))
    vv = np.asarray(Image.fromarray(v).resize((w, h), Image.BOX)); mm = np.asarray(Image.fromarray(m).resize((w, h), Image.BOX))
    lit = mm >= .42
    idx = np.where(lit, 3, 0).astype(np.uint8)   # пиксель знака — светлый, фон — чёрный (как значки на экранах BIOS)
    mode = MODE_CYCLE[k % NMODE]
    if '_m' in name and name.rsplit('_m', 1)[1][:1].isdigit(): mode = int(name.rsplit('_m', 1)[1][0]) % NMODE
    return name, idx, mode


def build():
    # правка Роба 189 (01.10 23:0x, «в который раз прошу»): Sony и PSP убрать; оставить только The Simpsons, 20th Century Fox и
    # Disney — пиксельные, без самоделок. The Simpsons — плашка gamelogo с диска, Fox — кадр ролика foxlogo.rmv с диска (монумент),
    # Disney — настоящий знак (Disney wordmark.svg с Wikimedia Commons в build/assets/boot_marks/disney.svg — файл, не набор шрифтом)
    order = ['simpsons', 'fox', 'disney']
    rob = {s[0]: s for s in rob_marks()}
    base = {s[0]: s for s in disk_marks() + [m for m in movie_marks() if m[0] == 'fox']}
    src = [rob.get(n, base.get(n)) for n in order if rob.get(n, base.get(n)) is not None]   # правка 189: только знаки списка
    miss = [n for n in order if rob.get(n, base.get(n)) is None]
    if miss: print('  нет источника знаков: %s' % ', '.join(miss))
    if len(src) > NSLOT: print('  знаков %d, слотов %d — лишние не входят' % (len(src), NSLOT)); src = src[:NSLOT]
    marks = [mk for k, s in enumerate(src) if (mk := to_mark(s[0], s[1], s[2], k))]
    b = bytearray(b'BMK1' + struct.pack('<4H', len(marks), SLOT_W, SLOT_H, 0))
    for name, idx, mode in marks:
        h, w = idx.shape
        b += struct.pack('<2H2B', w, h, 2, mode) + name.encode('ascii', 'replace')[:14].ljust(14, b'\0') + struct.pack('<4I', *PAL)
        flat = list(idx.flatten()) + [0] * ((-w * h) % 4)
        b += bytes((flat[i] << 6) | (flat[i + 1] << 4) | (flat[i + 2] << 2) | flat[i + 3] for i in range(0, len(flat), 4))
    b += struct.pack('<I', zlib.crc32(b) & 0xFFFFFFFF)
    return bytes(b), marks


# ---- Правка Роба 203 (03.10): «логотипы — настоящие; GONE Makers справа, под ним все остальные». BMK2: те же знаки списка 189
# (The Simpsons — плашка gamelogo с диска, 20th Century Fox — кадр ролика foxlogo с диска, Disney — знак Commons), но в своих
# цветах: 4 бита, 16 цветов (индекс 0 — фон), слот 72×40 — ряд из трёх под знаком GONE ×2 (232 px). Уменьшение — Lanczos,
# палитра — медианное сечение по пикселям знака
SLOT2_W, SLOT2_H = 112, 40     # правка 218 (Роб 03.10 12:3x): «логотипы чёрно-белые, из таких же квадратиков, с той же анимацией, что GONE»:
SCALE2 = 2                      # знак — 1 бит цветом покоя GONE (#C8C8C8), пиксель квадратом 2×2 (как знак GONE ×2), появление — волна / | \ ■
if not os.environ.get('BMK_242_OLD'):   # правка Роба 242 (видео 03.10 0:30: «логотипов больше по количеству, меньше по размеру»):
    SLOT2_W, SLOT2_H, SCALE2 = 72, 24, 1  # шесть знаков по три в ряд под GONE (viewer/bootmarks.h: 232 px -> 3 слота 72), пиксель 1:1
REST2 = 0xFFC8C8C8              # (режим 0); слот 112×40 экрана = 56×20 пикселей знака, по два в ряд под знаком GONE (232 px)
SPONSOR = not any(os.environ.get(k) for k in ('BMK_242_OLD', 'BMK_254_OLD', 'BMK_257_OLD'))   # правка 257: спонсорский блок
if SPONSOR:
    SLOT2_W, SLOT2_H = 112, 24    # слот = наибольший знак (ширина 110, высота 24); раскладка рядов — viewer/bootmarks.h (поток)


def movie_marks_242():
    """правка 242: знаки из роликов диска для мелкого слота 72×24 — буквы GRACIE FILMS в синей плашке (последний кадр gracie),
    красное кольцо (i) Radical (кадр трети radlogo: слово RADICAL в 72 px нечитаемо), VIVENDI UNIVERSAL games (кадр двух третей
    vuglogo: последний кадр — белая вспышка). -> {имя: (RGBA, маска)}"""
    import tempfile as _t, movie_export as me
    src = ROOT / 'hit-and-run-web/source/game/movies'; out = {}
    for name, clip in (('gracie', 'gracie'), ('radical', 'radlogo'), ('vivendi', 'vuglogo')):
        pth = src / (clip + '.rmv')
        if not pth.exists(): continue
        try:
            with _t.TemporaryDirectory() as d: frs = me.decode_video(me.rmv_parse(pth.read_bytes(), clip), d)
        except Exception as e: print('  пропуск %s: %s' % (clip, e)); continue
        n = len(frs)
        fr = (frs[-1] if name == 'gracie' else frs[n // 3] if name == 'radical' else frs[2 * n // 3]).astype(int)
        r, g, b = fr[..., 0], fr[..., 1], fr[..., 2]; lum = (r * 299 + g * 587 + b * 114) // 1000
        if name == 'gracie':
            box = (b > r + 50) & (b > 90); ys, xs = np.where(box); m = np.zeros_like(box)
            if len(ys):
                y0, y1, x0, x1 = ys.min(), ys.max(), xs.min(), xs.max()
                xs2 = np.where(box[y0:y1 + 1].sum(0) > (y1 - y0) * .6)[0]   # сама плашка (без луча проектора слева)
                if len(xs2): x0 = int(xs2.min()) # №380: xs2 already contains absolute columns, not an offset from x0
                m[y0:y1 + 1, x0:x1 + 1] = (lum > 170)[y0:y1 + 1, x0:x1 + 1]
                if not os.environ.get('BMK_396_OLD'):   # правка 396: G — белая буква целиком, даже если её штрих левее найденного края плашки
                    white = np.zeros_like(m); white[y0:y1 + 1, max(0, x0 - 12):x1 + 1] = (lum > 170)[y0:y1 + 1, max(0, x0 - 12):x1 + 1]
                    while True:
                        g2 = m.copy(); g2[1:] |= m[:-1]; g2[:-1] |= m[1:]; g2[:, 1:] |= m[:, :-1]; g2[:, :-1] |= m[:, 1:]; g2 &= white
                        if (g2 == m).all(): break
                        m = g2
        elif name == 'radical': m = (r > 140) & (g < 90) & (b < 90)
        else: m = lum < 200
        a = np.concatenate([fr[..., :3], np.full(fr.shape[:2] + (1,), 255)], 2)
        out[name] = (a, m)
    return out


def rgba_sources():
    """(имя, RGBA-массив, маска знака) в настоящих цветах"""
    import ui_export as U
    from p3d import read
    out = {}
    p = U.WEB / 'source/game/art/frontend/scrooby/frontend.p3d'
    if p.exists():
        for c in read(p).children:
            if c.id == 0x19005:
                n, i = U.decode_sprite_image(c)
                if n.startswith('gamelogo'):
                    a = np.asarray(i.convert('RGBA')).astype(int); r, g, b = a[..., 0], a[..., 1], a[..., 2]
                    plate = rowhull((r > 150) & (g < 90) & (b < 90))   # красная плашка «The Simpsons» с белыми буквами
                    out['simpsons'] = (a, plate & (r > 190) & (g > 190) & (b > 190)); break   # правка 218: монохром — буквы, без плашки
    for name, ink, m in (movie_marks() if not any((d / 'fox.svg').exists() for d in ROB_DIRS) else []):
        if name != 'fox': continue
        import tempfile as _t, movie_export as me
        src = ROOT / 'hit-and-run-web/source/game/movies/foxlogo.rmv'
        with _t.TemporaryDirectory() as d: frs = me.decode_video(me.rmv_parse(src.read_bytes(), 'foxlogo'), d)
        def gold(f): f = f.astype(int); return int(((f[..., 0] > 140) & (f[..., 1] > 100) & (f[..., 2] < 110) & (f[..., 0] > f[..., 2] + 60)).sum())
        fr = max(frs, key=gold).astype(int)
        a = np.concatenate([fr[..., :3], np.full(fr.shape[:2] + (1,), 255)], 2)
        out['fox'] = (a, m)
    if not os.environ.get('BMK_242_OLD'):   # правка 242: Gracie Films, Radical, Vivendi Universal Games — знаки из роликов диска Роба
        out.update(movie_marks_242())
    for d in ROB_DIRS:   # 20th Century Fox — файл Commons (как правка 189): буквы монумента в его цветах, без прожекторов и цоколя
        f = d / 'fox.svg'
        if f.exists():
            im = load_file(f); W_, H_ = im.size; im = im.crop((int(W_ * .18), int(H_ * .08), int(W_ * .97), int(H_ * .76)))
            a = np.asarray(im).astype(int)
            if (a[..., 3] < 128).any(): m = a[..., 3] >= 128
            else:
                lum = (a[..., 0] * 299 + a[..., 1] * 587 + a[..., 2] * 114) // 1000
                bg = int(np.median(np.concatenate([lum[0], lum[-1], lum[:, 0], lum[:, -1]]))); m = np.abs(lum - bg) >= 24
            out['fox'] = (a, m); break
    for d in ROB_DIRS:
        f = d / 'disney.svg'
        if f.exists():
            im = load_file(f); a = np.asarray(im).astype(int)
            m = a[..., 3] >= 128 if (a[..., 3] < 128).any() else np.abs(((a[..., 0] * 299 + a[..., 1] * 587 + a[..., 2] * 114) // 1000) - 255) >= 24
            lum = (a[..., 0] * 299 + a[..., 1] * 587 + a[..., 2] * 114) // 1000
            if lum[m].mean() < 110: a = a.copy(); a[..., :3][m] = 255      # тёмный знак на прозрачном — белым по чёрному экрана
            out['disney'] = (a, m); break
    return out


# ---- Правка Роба 254 (03.10 18:1x): «вся правая часть — логотипы; Sony не надо; старый PlayStation, как с первой Sony,
# пиксельный, в уголке; ещё El Barto — тег Барта от руки из старых серий; всё же лучше всё пиксельными однотонными».
# Оба знака — тем же 1 битом цветом покоя GONE (to_mark2). BMK_254_OLD=1 — прежние шесть знаков (красный контроль гейта)
def disk_elbarto():
    """El Barto с диска Роба: текстура elbarto.tga гэга art/nis/gags/l2_dump.p3d (64×64, красный спрей; та же у l3/l5/l6).
    На стене тег идёт наискось (~24°) — в слоте 24 px наискось буквы рассыпаются, поэтому он выпрямлен поворотом на 24°
    (увеличение ×4 перед поворотом); маска — альфа ≥ 200: бледные концы подтёков отпадают, штрих букв остаётся (сверено глазом)"""
    sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
    from p3d import read
    from textures import decode_image
    p = ROOT / 'hit-and-run-web/source/game/art/nis/gags/l2_dump.p3d'
    if not p.exists(): print('  (нет диска: %s)' % p); return None

    def walk(c):
        yield c
        for k in c.children: yield from walk(k)
    for c in read(p).children:
        if c.id == 0x19000 and b'elbarto' in bytes(c.data[:24]):
            for x in walk(c):
                if x.id == 0x19002:
                    a = np.asarray(decode_image(x.data[4:]).convert('RGBA')).astype(int)
                    m = Image.fromarray(((a[..., 3] >= 200) * 255).astype(np.uint8))
                    m = m.resize((m.width * 4, m.height * 4), Image.BICUBIC).rotate(24, resample=Image.BICUBIC, expand=True)
                    m = np.asarray(m) > 127
                    return np.stack([m * 255] * 3 + [np.full(m.shape, 255)], 2), m
    print('  (в l2_dump.p3d нет elbarto)'); return None


# ---- Правка Роба 264 (04.10: «логотипы не актуальные; El Barto — тег баллончиком из игры (надписи во 2-м/3-м городе); PSP мелко
# или убрать; основные логотипы, которые красиво прорисовываются, расставлены по системе и появляются, а в конце — надпись El Barto,
# будто Барт тегнул»): тег — та же текстура гэга elbarto.tga (l3_dump.p3d, красный спрей 64×64, как на стенах уровней 2/3/5/6)
# цветом и мягкостью спрея: альфа -> 15 ступеней красного по чёрному экрану (4 бита), наискось, как на стене, рисунок один в один.
# Режим 6 — «распыление» (viewer/bootmarks.h), пиксель 2×2, место и время — viewer/bootseq.h (после последнего знака)
TAG_NAME = 'elbarto'
TAG_PX = int(os.environ.get('BMK_TAG_PX', '1'))   # правка 291: пиксель тега на экране (1 — 1×1, мельче и чётче; 2 — прежнее 2×2)
TAG_W, TAG_H = (84, 46) if TAG_PX == 2 else (112, 62)   # пикселей знака (×TAG_PX на экране: было 168×92, стало 112×62) — под рядами знаков
ROB_ELBARTO = ROOT / 'build/assets/boot_marks/elbarto-rob-0410.png'   # файл Роба 04.10 06:30 (чёрным по белому, El Barto с головой Барта)
# Правка Роба 264, уточнение 04.10 06:3x: «El Barto, который я приложил, перевести просто в пиксель, белым по чёрному, чтобы всё
# красиво загружалось; самодельные логотипы убрать». Знак — файл Роба: чернила (тёмное по белому) -> 1 бит цветом покоя GONE,
# уменьшение усреднением BOX, порог 0,42; пиксель 2×2, как знак GONE. Появляется последним, «распылением» (режим 6). Тег с диска
# (красный спрей) снят — ROB_ELBARTO нет — тег не ставится
def elbarto_tag():
    if not ROB_ELBARTO.exists(): print('  (нет %s — El Barto не ставится)' % ROB_ELBARTO); return None
    g = np.asarray(Image.open(ROB_ELBARTO).convert('L')).astype(np.float32) / 255.
    ink = 1. - g; ys, xs = np.where(ink > .5)
    if not len(ys): return None
    ink = ink[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    h0, w0 = ink.shape; s = min(TAG_W / w0, TAG_H / h0); w, h = max(1, int(round(w0 * s))), max(1, int(round(h0 * s)))
    m = np.asarray(Image.fromarray((ink * 255).astype(np.uint8)).resize((w, h), Image.BOX)) >= int((.42 if TAG_PX == 2 else .5) * 255)   # 291: штрих в пиксель — порог 0,5, просветы букв открыты
    idx = m.astype(np.uint8)
    return TAG_NAME, idx, 6, [0xFF000000, REST2] + [0xFF000000] * 14


# №1010 (Роб 26.10: «на мусорных баках в игре написано El Barto красным — на уровне Барта (L6), бак, где он разговаривает в миссии; взять этот исходник для
# заставки GONE Makers вместо найденного на стороне, чтобы было согласовано с игрой»): тег — текстура elbarto.tga гэга-бака art/nis/gags/l6_dump.p3d
# (dumpstergag3, шейдер elbarto_m; 64×64; та же у l3_dump, у l2/l5 другой оттенок) с диска Роба, как в игре: наискось, с подтёками, маска — альфа ≥ 128.
# №1010б (Роб 26.10 вечер: «тем же серым цветом, как все остальные логотипы; он какой-то сжатый и не так отображается»): цвет — серый знаков (REST2, без
# своей краски); пропорции — как на баке: квадратная текстура натянута на грань 2,95 × 1,57 м при UV 0…1 (считается из сетки гэга, convert.geometry),
# то есть по ширине в ≈ 1,87 раза — столбцы повторяются ближайшими пикселями (без размытия), по высоте целым кратным, сколько входит в высоту прежнего
# тега Роба (112×50: ×1) — тег занимает то же место. Пиксели только в build/bootmarks.bin (не в git). Прежний тег 'elbarto' (файл Роба 04.10) остаётся
# записью рядом: движок рисует один из двух (viewer/bootseq.h, ELBARTO1010_OLD / файл elbarto1010old). ELBARTO1010B_OLD=1 — как №1010 (1:1, красный)
TAG6_NAME = 'elbarto6'
TAG_INK = {}   # имя тега -> ABGR краски (4 последних байта поля имени); №1010б: пусто — тег цветом знаков
TAG_OLD_BOX = (112, 50)   # место прежнего тега Роба (elbarto_tag) — по нему масштаб тега L6
def elbarto_bin_aspect(p):
    """№1010б: во сколько раз надпись на баке шире, чем в квадратной текстуре: (ширина грани / диапазон U) / (высота грани / диапазон V)"""
    try:
        sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
        import convert as C
        from p3d import read, walk
        for c in walk(read(p)):
            if c.id != 0x10002: continue
            sh, parts = C.geometry(c)
            if not sh.startswith('elbarto'): continue
            for pt in parts:
                P, uv = pt['position'], pt.get('uv')
                if uv is None: continue
                ext = P.max(0) - P.min(0); ax = np.argsort(ext)[::-1][:2]   # две оси грани: длинная — вдоль U, короткая — вдоль V
                du, dv = float(np.ptp(uv[:, 0])), float(np.ptp(uv[:, 1]))
                if du > 0 and dv > 0: return (float(ext[ax[0]]) / du) / (float(ext[ax[1]]) / dv)
    except Exception as e: print('  (сетка бака не прочитана: %s — пропорции 1,87 по замеру 26.10)' % e)
    return 2.946 / 1.574
def disk_elbarto_l6():
    if os.environ.get('ELBARTO1010_OLD') == '1': return None
    sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
    from p3d import read
    from textures import decode_image
    p = ROOT / 'hit-and-run-web/source/game/art/nis/gags/l6_dump.p3d'
    if not p.exists(): print('  (нет диска: %s — El Barto L6 не ставится)' % p); return None

    def walk(c):
        yield c
        for k in c.children: yield from walk(k)
    for c in read(p).children:
        if c.id == 0x19000 and b'elbarto' in bytes(c.data[:24]):
            for x in walk(c):
                if x.id == 0x19002:
                    a = np.asarray(decode_image(x.data[4:]).convert('RGBA')).astype(int)
                    m = a[..., 3] >= 128
                    if os.environ.get('ELBARTO1010B_OLD') == '1':   # №1010: 1:1 и краской текстуры
                        rgb = a[..., :3][a[..., 3] >= 200].mean(0).round().astype(int)
                        TAG_INK[TAG6_NAME] = 0xFF000000 | int(rgb[2]) << 16 | int(rgb[1]) << 8 | int(rgb[0])
                    else:
                        asp = elbarto_bin_aspect(p); W = int(round(m.shape[1] * asp))
                        m = m[:, np.minimum((np.arange(W) / asp).astype(int), m.shape[1] - 1)]   # ширина как на баке: ближайший столбец
                    ys, xs = np.where(m)
                    if not len(ys): return None
                    m = m[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
                    if os.environ.get('ELBARTO1010B_OLD') != '1':
                        k = max(1, min(TAG_OLD_BOX[1] // m.shape[0], TAG_OLD_BOX[0] // m.shape[1]))   # целым кратным в место прежнего тега
                        if k > 1: m = np.repeat(np.repeat(m, k, 0), k, 1)
                    return TAG6_NAME, m.astype(np.uint8), 6, [0xFF000000, REST2] + [0xFF000000] * 14
    print('  (в l6_dump.p3d нет elbarto)'); return None


def name_field(name):   # 14 байт поля имени записи; у тега №1010 — имя, нули и ABGR краски в последних 4 байтах
    f = name.encode('ascii', 'replace')[:14].ljust(14, b'\0')
    if name in TAG_INK: f = f[:10] + struct.pack('<I', TAG_INK[name])
    return f


PS_FONT = Path('/System/Library/Fonts/Supplemental/Futura.ttc')   # Futura Bold (index 2) — геометрический гротеск, как буквы знака


def ps_mark():
    """Старый знак PlayStation (первая PlayStation): на диске SHAR его нет — собран из букв P и S Futura Bold, как устроен сам
    знак: S лежит на полу (сжата по высоте до 0,42 высоты P, дальний край уходит вправо, сдвиг 0,9), P стоит перед ней на
    её левом крае, вокруг P зазор 6 % её высоты — в один цвет P читается отдельно от S. Чёрно-белый: цветов исходника нет"""
    if not PS_FONT.exists(): print('  (нет шрифта %s — знак PlayStation пропущен)' % PS_FONT); return None
    from PIL import ImageDraw, ImageFont
    f = ImageFont.truetype(str(PS_FONT), 600, index=2)

    def glyph(ch):
        im = Image.new('L', (1200, 1200), 0); ImageDraw.Draw(im).text((150, 0), ch, font=f, fill=255)
        g = np.asarray(im) > 127; ys, xs = np.where(g); return g[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    P, S = glyph('P'), glyph('S'); ph, pw = P.shape; gh, gw = S.shape
    ow, oh, shear = int(pw * 1.5), int(ph * .42), .9                     # S на полу: ширина 1,5 P, высота 0,42 P
    W = int(ow + shear * oh) + 2; Y, X = np.mgrid[0:oh, 0:W]
    u = (X - shear * (oh - Y)) / ow; gy = (Y / oh * gh).astype(int); gx = (u * gw).astype(int)
    ok = (u >= 0) & (u < 1); flat = np.zeros((oh, W), bool); flat[ok] = S[gy[ok], gx[ok]]
    H = ph + oh + 40; c = np.zeros((H, W + 40), bool); sy, sx = H - oh - 10, 20
    c[sy:sy + oh, sx:sx + W] |= flat
    Pm = np.zeros_like(c); Pm[sy + oh - ph:sy + oh, sx:sx + pw] = P   # P стоит на ближнем крае S, её левый край — у левого края S
    gap = int(ph * .06); g = Image.fromarray((Pm * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(2 * gap + 1))
    c &= ~(np.asarray(g) > 127); c |= Pm
    return np.stack([c * 255] * 3 + [np.full(c.shape, 255)], 2), c


# ---- Правка Роба 257 (03.10 20:3x, к 254): «левые логотипы — поставь просто PSP, именно логотип PSP, как на корпусе сзади, и просто
# El Barto; вспомогательные (Radical и др.) не видно — надо красиво, как спонсорские, аккуратно снизу логотипа». Самодельный знак
# PlayStation убран; PSP — файл Commons build/assets/boot_marks/psp.svg (общественное достояние, как fox.svg/disney.svg): только
# буквы P S P тонкой линией, без надписи PlayStation Portable и ™ (в 20 px она не читается); Radical — слово «RAD◎CAL» с
# последнего кадра ролика radlogo диска (раньше — одно кольцо). Размер — как в спонсорском ряду: у каждого знака одна и та же
# площадь чернил (ширина × высота), высота не больше 24, ширина не больше 110 (viewer/bootmarks.h раскладывает ряды по центру
# под знаком GONE). BMK_257_OLD=1 — прежний набор правки 254 (красный контроль)
SPONSOR_AREA, SPONSOR_HMAX, SPONSOR_WMAX = 72 * 20, 24, 110


def bands(mask, axis, gap):
    """полосы с чернилами вдоль оси (0 — строки, 1 — колонки), разделённые пустотой не меньше gap -> [(a, b)]"""
    prof = mask.any(1 - axis) if axis == 0 else mask.any(0)
    idx = np.where(prof)[0]; out = []
    if not len(idx): return out
    a = b = idx[0]
    for i in idx[1:]:
        if i - b > gap: out.append((a, b + 1)); a = i
        b = i
    out.append((a, b + 1)); return out


def psp_mark():
    """знак PSP с корпуса: буквы P S P (первая полоса строк файла), без надписи и ™ (колонки ниже 40 % высоты букв)"""
    for d in ROB_DIRS:
        f = d / 'psp.svg'
        if not f.exists(): continue
        a = np.asarray(load_file(f).convert('RGBA')).astype(int)
        m = a[..., 3] >= 128 if (a[..., 3] < 128).any() else (a[..., :3].sum(2) < 384)
        rows = bands(m, 0, max(4, m.shape[0] // 60))
        if not rows: return None
        y0, y1 = rows[0]; band = m[y0:y1]
        cols = [(c0, c1) for c0, c1 in bands(band, 1, max(4, m.shape[1] // 80))
                if band[:, c0:c1].any(1).sum() >= .4 * (y1 - y0)]       # ™ ниже 40 % высоты букв — прочь
        if not cols: return None
        x0, x1 = cols[0][0], cols[-1][1]; mm = np.zeros_like(m); mm[y0:y1, x0:x1] = band[:, x0:x1]
        for c0, c1 in bands(band, 1, max(4, m.shape[1] // 80)):        # и сам ™ внутри рамки букв
            if (c0, c1) not in cols: mm[y0:y1, c0:c1] = False
        return np.stack([mm * 255] * 3 + [np.full(mm.shape, 255)], 2), mm
    print('  (нет build/assets/boot_marks/psp.svg — знак PSP пропущен)'); return None


def radical_word():
    """слово RAD◎CAL: последний кадр ролика radlogo диска — белые буквы и красное кольцо вместо I, без строки ENTERTAINMENT"""
    import tempfile as _t, movie_export as me
    pth = ROOT / 'hit-and-run-web/source/game/movies/radlogo.rmv'
    if not pth.exists(): return None
    with _t.TemporaryDirectory() as d: frs = me.decode_video(me.rmv_parse(pth.read_bytes(), 'radlogo'), d)
    fr = frs[-1].astype(int); r, g, b = fr[..., 0], fr[..., 1], fr[..., 2]; lum = (r * 299 + g * 587 + b * 114) // 1000
    m = (lum > 150) | ((r > 140) & (g < 90) & (b < 90))
    rows = bands(m, 0, 2)
    if not rows: return None
    y0, y1 = max(rows, key=lambda ab: m[ab[0]:ab[1]].sum())                 # строка слова — самая «чернильная»
    mm = np.zeros_like(m); mm[y0:y1] = m[y0:y1]
    return np.concatenate([fr[..., :3], np.full(fr.shape[:2] + (1,), 255)], 2), mm


# Правка Роба 294 (04.10: «в экран загрузки добавим Claude Code — маленького чувачка, он нам тоже помогал собирать; где-то последним»):
# знак — человечек Claude Code ровно так, как его рисует сам Claude Code в терминале при запуске (официальный, блочные символы
# Unicode: ▐ ▛ █ ▜ ▌ ▝ ▘ — каждое знакоместо 2×2 четверти, четверть вдвое выше ширины, как в терминале), пиксель ×2; справа подпись «Claude Code» шрифтом заставки (msx PSPSDK,
# viewer/bootmsx.h — тот же, что строки BIOS), шаг 6 px. 1 бит цветом покоя GONE, волна GONE, как остальные знаки. BMK_294_OLD=1 — без него
CLAUDE_ART = [' \u2590\u259b\u2588\u2588\u2588\u259c\u258c', '\u259d\u259c\u2588\u2588\u2588\u2588\u2588\u259b\u2598', '  \u2598\u2598 \u259d\u259d']
CLAUDE_Q = {' ': 0, '\u2590': 0b0101, '\u258c': 0b1010, '\u2588': 0b1111, '\u259b': 0b1110, '\u259c': 0b1101, '\u259d': 0b0100, '\u2598': 0b1000}   # UL UR LL LR


def claude_mark_old():
    w = 2 * max(len(r) for r in CLAUDE_ART); art = np.zeros((2 * len(CLAUDE_ART), w), bool)
    for y, row in enumerate(CLAUDE_ART):
        for x, ch in enumerate(row):
            q = CLAUDE_Q[ch]
            art[2 * y, 2 * x], art[2 * y, 2 * x + 1], art[2 * y + 1, 2 * x], art[2 * y + 1, 2 * x + 1] = q >> 3 & 1, q >> 2 & 1, q >> 1 & 1, q & 1
    art = np.kron(art, np.ones((4, 2), bool))   # знакоместо терминала вдвое выше ширины: четверть 1×2, пиксель ×2 -> 2×4 (36×24)
    src = (ROOT / 'viewer/bootmsx.h').read_text()
    msx = bytes(int(v, 16) for v in __import__('re').findall(r'0x([0-9a-fA-F]{2})', src.split('bootMsx[2048] = {', 1)[1])[:2048])
    text = 'Claude Code'; tw = 6 * len(text); txt = np.zeros((8, tw), bool)
    for i, ch in enumerate(text):
        for r in range(8):
            row = msx[ord(ch) * 8 + r]
            for c in range(6): txt[r, 6 * i + c] = bool(row >> (7 - c) & 1)
    H = max(art.shape[0], 8); W = art.shape[1] + 5 + tw
    m = np.zeros((H, W), bool); m[(H - art.shape[0]) // 2:(H - art.shape[0]) // 2 + art.shape[0], :art.shape[1]] = art
    m[(H - 8) // 2:(H - 8) // 2 + 8, art.shape[1] + 5:] = txt
    return 'claude', m.astype(np.uint8), 0, [0xFF000000, REST2] + [0xFF000000] * 14


def ai_mark(name, label, asset):
    """№379: actual application/official brand artwork, rendered in the existing 1-bit boot palette."""
    a = np.asarray(Image.open(ROOT / 'assets/branding' / asset).convert('RGBA')).astype(int)
    if name == 'claude':
        mask = (a[..., 0] > a[..., 1] + 35) & (a[..., 1] > a[..., 2] + 10) & (a[..., 3] >= 128)
    else:
        mask = a[..., 3] >= 128
    ys, xs = np.where(mask)
    if not len(xs): raise ValueError('empty real brand asset: ' + asset)
    cropped = ((mask[ys.min():ys.max()+1, xs.min():xs.max()+1]) * 255).astype(np.uint8)
    art = np.asarray(Image.fromarray(cropped).resize((24, 24), Image.Resampling.BOX)) >= 115
    src = (ROOT / 'viewer/bootmsx.h').read_text()
    msx = bytes(int(v, 16) for v in __import__('re').findall(r'0x([0-9a-fA-F]{2})', src.split('bootMsx[2048] = {', 1)[1])[:2048])
    if not os.environ.get('BMK_395_OLD'): label = ''   # правка 395 (Роб 08.10): «не пиши Claude Code, только символы» — знак без подписи
    m = np.zeros((24, 24 + (5 + 6 * len(label) if label else 0)), np.uint8); m[:, :24] = art
    for i, ch in enumerate(label):
        for y in range(8):
            for x in range(6): m[8+y, 29+6*i+x] = (msx[ord(ch)*8+y] >> (7-x)) & 1
    return name, m, 0, [0xFF000000, REST2] + [0xFF000000] * 14


def claude_icon():
    """правка 396 (Роб 08.10): человечек Claude Code из терминала (правка 294) — без подписи"""
    name, m, mode, cols = claude_mark_old()
    xs = np.where(m.any(0))[0]; art = m[:, :xs[np.argmax(np.diff(np.r_[xs, 10 ** 6]) > 3)] + 1]   # до первого промежутка — сам человечек
    ys, xs = np.where(art); art = art[ys.min():ys.max() + 1, xs.min():xs.max() + 1]   # без полей знакоместа терминала
    if not os.environ.get('BMK_397_OLD'): art = np.r_[np.zeros((1, art.shape[1]), art.dtype), art]   # 397: место под подскок
    return 'claude', art, mode, cols


def codex_symbol():
    """правка 396: знак ChatGPT (цветок OpenAI) без подписи — облако иконки Codex в 24 px выходит криво"""
    n, m, mode, cols = ai_mark('codex', '', 'openai-monoblossom.png'); return 'codex', m, mode, cols


def idle_frames(by_name):
    """правка 397 (Роб 08.10: «пусть дрыгаются»): кадры покоя после появления. codex (цветок OpenAI) — 11 кадров поворота по 5°
    (у цветка шесть лучей: 12 кадров = 60° = бесшовная петля), режим 7 (кадр 6 тиков); claude (человечек) — 5 кадров по 24 тика:
    подскок на 1 px с перебором ножек, снова ровно, …, последний — подскок с закрытыми глазами (моргание)"""
    out = []
    if 'codex' in by_name:
        a = Image.open(ROOT / 'assets/branding/openai-monoblossom.png').convert('RGBA')
        al = np.asarray(a)[..., 3]; ys, xs = np.where(al >= 128); a = a.crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))
        S = max(a.size); sq = Image.new('RGBA', (S, S)); sq.paste(a, ((S - a.size[0]) // 2, (S - a.size[1]) // 2))
        base = by_name['codex'][1]
        for k in range(1, 12):
            r = sq.rotate(-5 * k, resample=Image.BICUBIC); m = (np.asarray(r)[..., 3] >= 128).astype(np.uint8) * 255
            f = (np.asarray(Image.fromarray(m).resize(base.shape[::-1], Image.BOX)) >= 115).astype(np.uint8)
            out.append(('codex', f, 7, by_name['codex'][3]))
    if 'claude' in by_name:
        base = by_name['claude'][1].astype(bool); H, W = base.shape
        def pose(bob, blink):
            m = base.copy()
            if blink: m[4:8, :] |= m[3:4, :]          # глаза (просветы строк 4–7) закрыты верхней строкой лица
            if bob:
                legs = m[16:, :].copy(); m[16:, :] = False; m[16:, 1:] = legs[:, :-1]   # ножки на шаг
                m = np.r_[m[1:], np.zeros((1, W), bool)]   # подскок на 1 px вверх
            return m.astype(np.uint8)
        for bob, blink in ((1, 0), (0, 0), (1, 0), (0, 0), (1, 1)):
            out.append(('claude', pose(bob, blink), 8, by_name['claude'][3]))
    return out


def codex_icon():
    """правка 396: символ с иконки Codex на Маке (облако с >_, /Applications/ChatGPT.app app.icns): облако цветом знака, >_ вырезан"""
    a = np.asarray(Image.open(ROOT / 'assets/branding/codex-app-icon.png').convert('RGBA')).astype(int)
    r, g, b, al = a[..., 0], a[..., 1], a[..., 2], a[..., 3]
    ink = (al >= 128) & (b > r + 40)   # само облако синее; белый >_ и светлое свечение по краю — не знак
    ys, xs = np.where(ink); y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    ink = ink[y0:y1, x0:x1]; h = 24; w = int(round((x1 - x0) * h / (y1 - y0)))
    lit = np.asarray(Image.fromarray((ink * 255).astype(np.uint8)).resize((w, h), Image.BOX)) >= 128
    return 'codex', lit.astype(np.uint8), 0, [0xFF000000, REST2] + [0xFF000000] * 14


def claude_mark():
    return ai_mark('claude', 'Claude Code', 'claude-app-icon.png')


def codex_mark():
    return ai_mark('codex', 'Codex', 'openai-monoblossom.png')


def to_mark2(name, a, m, k):
    """правка 218: 1 бит — знак цветом покоя GONE по маске, уменьшение маски усреднением, порог 0,45; режим 0 (волна GONE)"""
    ys, xs = np.where(m)
    if not len(ys): return None
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    mm = (m[y0:y1, x0:x1] * 255).astype(np.uint8)
    sw, sh = SLOT2_W // SCALE2, SLOT2_H // SCALE2
    s = min(sw / (x1 - x0), sh / (y1 - y0), 1.0)
    if SPONSOR:   # правка 257: одна площадь чернил у всех знаков, высота ≤ 24, ширина ≤ 110
        asp = (x1 - x0) / (y1 - y0); h_ = min(SPONSOR_HMAX, (SPONSOR_AREA / asp) ** .5); w_ = min(SPONSOR_WMAX, h_ * asp); h_ = w_ / asp
        s = min(w_ / (x1 - x0), h_ / (y1 - y0), 1.0)
    w, h = max(1, int(round((x1 - x0) * s))), max(1, int(round((y1 - y0) * s)))
    lit = np.asarray(Image.fromarray(mm).resize((w, h), Image.BOX)) >= 115
    idx = np.where(lit, 1, 0).astype(np.uint8)
    return name, idx, 0, [0xFF000000, REST2] + [0xFF000000] * 14


def to_mark2_color(name, a, m, k):   # правка 203 (цветные) — отменена правкой 218, оставлена для сравнения
    ys, xs = np.where(m)
    if not len(ys): return None
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    rgb = a[y0:y1, x0:x1, :3].astype(np.uint8); al = (m[y0:y1, x0:x1] * 255).astype(np.uint8)
    s = min(SLOT2_W / (x1 - x0), SLOT2_H / (y1 - y0), 1.0)
    w, h = max(1, int(round((x1 - x0) * s))), max(1, int(round((y1 - y0) * s)))
    im = Image.fromarray(rgb).resize((w, h), Image.LANCZOS); am = np.asarray(Image.fromarray(al).resize((w, h), Image.LANCZOS)) >= 110
    px = np.asarray(im)[am]
    if not len(px): return None
    q = Image.fromarray(px.reshape(1, -1, 3)).quantize(colors=15, method=Image.MEDIANCUT)
    pal = (q.getpalette() or [])[:45]; pal += [0] * (45 - len(pal)); qi = np.asarray(q).reshape(-1)   # цветов бывает меньше 15
    idx = np.zeros((h, w), np.uint8); idx[am] = qi + 1
    cols = [0xFF000000] + [0xFF000000 | (pal[3 * i + 2] << 16) | (pal[3 * i + 1] << 8) | pal[3 * i] for i in range(15)]
    return name, idx, MODE_CYCLE[k % NMODE], cols


def radical_ring(idx, a, m):
    """правка 396: кольцо RAD◎CAL при уменьшении рвётся — ровная пиксельная окружность во всю высоту слова и буква i внутри
    (точка, стойка, основание, как в кадре radlogo); место кольца — красная часть кадра в той же раскладке, что to_mark2"""
    ys, xs = np.where(m); x0, x1 = xs.min(), xs.max() + 1
    r, g, b = a[..., 0], a[..., 1], a[..., 2]; red = m & (r > 140) & (g < 90) & (b < 90)
    if not red.any(): return idx
    h, w = idx.shape; cx = int(round((np.where(red)[1].mean() - x0) / (x1 - x0) * w - .5))
    d = h if h % 2 else h - 1; R = d // 2; cy = (h - 1) // 2; out = idx.copy()
    out[:, max(0, cx - R - 1):min(w, cx + R + 2)] = 0
    for y in range(d):
        for x in range(d):
            dx, dy = x - R, y - R
            if R - .5 <= (dx * dx + dy * dy) ** .5 < R + .5: out[cy - R + y, cx - R + x] = 1
    for y, xs_ in ((cy - 3, (0,)), (cy - 1, (0,)), (cy, (0,)), (cy + 1, (0,)), (cy + 2, (-1, 0, 1))):
        for dx in xs_: out[y, cx + dx] = 1
    return out


LOCKED = ROOT / 'build/assets/boot_marks/locked'   # правка 395: <имя>.png — 1 бит (белое = знак), снят с принятого набора


def locked_mark(name, idx):
    f = LOCKED / (name + '.png')
    if not f.exists(): return idx
    a = np.asarray(Image.open(f).convert('L')) > 127
    return a.astype(idx.dtype)


def build2():
    src = rgba_sources()
    order = ['simpsons', 'fox', 'disney'] if os.environ.get('BMK_242_OLD') else ['simpsons', 'fox', 'disney', 'gracie', 'radical', 'vivendi']
    TAG = SPONSOR and not os.environ.get('BMK_264_OLD')
    if TAG:       # правка 264: основные знаки, что читаются пикселем (Vivendi в 20 px не читается, PSP убран), тег El Barto — в конце
        order = ['simpsons', 'fox', 'disney', 'gracie', 'radical']
        r = radical_word()
        if r: src['radical'] = r
    elif SPONSOR:   # правка 257: El Barto и знак PSP; Radical — словом
        order += ['elbarto', 'psp']
        for n, fn in (('elbarto', disk_elbarto), ('psp', psp_mark), ('radical', radical_word)):
            r = fn()
            if r: src[n] = r
    elif not os.environ.get('BMK_242_OLD') and not os.environ.get('BMK_254_OLD'):   # правка 254: тег El Barto и старый PlayStation (в углу)
        order += ['elbarto', 'playstation']
        for n, fn in (('elbarto', disk_elbarto), ('playstation', ps_mark)):
            r = fn()
            if r: src[n] = r
    miss = [n for n in order if n not in src]
    if miss: print('  нет источника знаков: %s' % ', '.join(miss))
    marks = [mk for k, n in enumerate([n for n in order if n in src]) if (mk := to_mark2(n, src[n][0], src[n][1], k))]
    if not os.environ.get('BMK_396_OLD'):
        marks = [(n, radical_ring(idx, *src[n]) if n == 'radical' else idx, mode, cols) for n, idx, mode, cols in marks]
    if not os.environ.get('BMK_395_OLD'):   # правка 395: знак, принятый Робом на PSP, — битмапом как есть (SVG у CairoSVG выходит иначе)
        marks = [(n, locked_mark(n, idx), mode, cols) for n, idx, mode, cols in marks]
    if TAG and not os.environ.get('BMK_294_OLD'):
        if os.environ.get('BMK_379_OLD'): marks.append(claude_mark_old())
        elif os.environ.get('BMK_396_OLD'): marks += [claude_mark(), codex_mark()]
        else:   # правка 396 (Роб 08.10): второй ряд симметрично — ChatGPT слева, Radical по центру, человечек Claude справа
            r = [i for i, mk in enumerate(marks) if mk[0] == 'radical']
            at = r[0] if r else len(marks)
            marks[at:at] = [codex_symbol()]; marks.insert(at + 2, claude_icon())
    if TAG:
        t = elbarto_tag()
        if t: marks.append(t)
        t6 = disk_elbarto_l6()   # №1010: родной тег с бака L6 — рядом с прежним
        if t6: marks.append(t6)
    if TAG and not os.environ.get('BMK_294_OLD') and not os.environ.get('BMK_379_OLD') and not os.environ.get('BMK_396_OLD') \
            and not os.environ.get('BMK_397_OLD'):
        marks += idle_frames(dict((mk[0], mk) for mk in marks))
    if len(marks) > NSLOT: raise ValueError('boot marks exceed renderer capacity')
    compact = not os.environ.get('BOOTMARKS_PACKED_OLD')
    if compact:
        for name, idx, mode, cols in marks:
            if set(int(v) for v in np.unique(idx)) - {0, 1} or cols[:2] != [0xFF000000, REST2]:
                raise ValueError('%s is not compatible with shared 1-bit BMK3 palette' % name)
        b = bytearray(b'BMK3' + struct.pack('<4H', len(marks), SLOT2_W, SLOT2_H, SCALE2))
        b += struct.pack('<2I', 0xFF000000, REST2)
        for name, idx, mode, _cols in marks:
            h, w = idx.shape
            b += struct.pack('<2H2B', w, h, 1, mode) + name_field(name)
            flat = list(idx.flatten()) + [0] * ((-w * h) % 8)
            b += bytes(sum(flat[i + j] << (7 - j) for j in range(8)) for i in range(0, len(flat), 8))
    else:
        b = bytearray(b'BMK2' + struct.pack('<4H', len(marks), SLOT2_W, SLOT2_H, SCALE2))   # OLD: per-record 4-bit palette
        for name, idx, mode, cols in marks:
            h, w = idx.shape
            b += struct.pack('<2H2B', w, h, 4, mode) + name.encode('ascii', 'replace')[:14].ljust(14, b'\0') + struct.pack('<16I', *cols)
            flat = list(idx.flatten()) + [0] * ((-w * h) % 2)
            b += bytes((flat[i] << 4) | flat[i + 1] for i in range(0, len(flat), 2))
    b += struct.pack('<I', zlib.crc32(b) & 0xFFFFFFFF)
    return bytes(b), marks


def parse(b):
    """-> (знаки [(имя, w, h, bpp, режим, палитра, индексы)], ошибки); BMK3 uses a shared palette and 1-bit pixels."""
    err = []
    if len(b) < 16 or b[:3] != b'BMK' or b[3:4] not in (b'1', b'2', b'3'): return [], ['магия не BMK1/BMK2/BMK3']
    v = int(b[3:4]); bpp_want, npal, slot = (2, 4, (SLOT_W, SLOT_H)) if v == 1 else (4, 16, (SLOT2_W, SLOT2_H)) if v == 2 else (1, 2, (SLOT2_W, SLOT2_H))
    if struct.unpack_from('<I', b, len(b) - 4)[0] != zlib.crc32(b[:-4]) & 0xFFFFFFFF: err.append('crc не сходится')
    n, sw, sh, _ = struct.unpack_from('<4H', b, 4); p = 12; out = []
    global_pal = struct.unpack_from('<2I', b, p) if v == 3 and len(b) >= 24 else None
    if v == 3: p += 8
    if n > NSLOT: err.append('знаков %d > %d' % (n, NSLOT))
    if (sw, sh) != slot: err.append('слот %d×%d' % (sw, sh))
    rec = 20 + (0 if v == 3 else 4 * npal)
    for k in range(min(n, NSLOT)):
        if p + rec > len(b) - 4: err.append('знак %d обрезан' % k); break
        w, h, bpp, mode = struct.unpack_from('<2H2B', b, p); name = b[p + 6:p + 20].split(b'\0')[0].decode('ascii', 'replace')   # №1010: после имени у тега — краска
        pal = global_pal if v == 3 else struct.unpack_from('<%dI' % npal, b, p + 20)
        if v != 3: p += rec
        else: p += 20
        nb = (w * h + 7) // 8 if bpp_want == 1 else (w * h + 3) // 4 if bpp_want == 2 else (w * h + 1) // 2
        bits = np.frombuffer(b[p:p + nb], np.uint8); p += nb; idx = None
        if len(bits) == nb:
            idx = (((bits[:, None] >> (7 - np.arange(8))) & 1).reshape(-1) if bpp_want == 1 else
                   (np.stack([(bits >> 6) & 3, (bits >> 4) & 3, (bits >> 2) & 3, bits & 3], 1) if bpp_want == 2 else np.stack([bits >> 4, bits & 15], 1)).flatten())[:w * h].reshape(h, w)
        sc2 = struct.unpack_from('<H', b, 10)[0] if v >= 2 else 1; sc2 = sc2 if sc2 == 2 else 1
        tag = v >= 2 and mode == 6 and name in (TAG_NAME, TAG6_NAME)   # правка 264 (+№1010 тег L6): тег El Barto — свой размер (до 96×64, пиксель 2×2)
        if tag: pass
        elif w < 1 or h < 1 or w * sc2 > slot[0] or h * sc2 > slot[1]: err.append('%s: %d×%d (×%d) вне слота %d×%d' % (name, w, h, sc2, *slot))
        if tag and (w > (128 if TAG_PX == 1 else 96) or h > (72 if TAG_PX == 1 else 64)): err.append('%s: тег %d×%d больше предела' % (name, w, h))
        if bpp != bpp_want: err.append('%s: %d бит на пиксель' % (name, bpp))
        if mode >= NMODE: err.append('%s: режим %d' % (name, mode))
        if v == 1 and (len(set(pal)) > 4 or any(((c >> 16) & 255, (c >> 8) & 255) != (c & 255, c & 255) for c in pal)): err.append('%s: палитра не 4 серых' % name)
        if v == 2 and (pal[0] != 0xFF000000 or any(c >> 24 != 0xFF for c in pal)): err.append('%s: палитра: фон не чёрный или цвет прозрачный' % name)
        if v in (2, 3) and (pal[0] != 0xFF000000 or any(c >> 24 != 0xFF for c in pal)): err.append('%s: палитра: фон не чёрный или цвет прозрачный' % name)
        if v == 3 and pal != (0xFF000000, REST2): err.append('%s: общая палитра BMK3 отличается' % name)
        if v in (2, 3) and not tag and idx is not None and (set(int(c) for c in np.unique(idx)) - {0, 1} or pal[1] != REST2): err.append('%s: не 1 бит цветом GONE (правка 218: чёрно-белые)' % name)
        if v in (2, 3) and not tag and mode not in (0, 6, 7, 8): err.append('%s: режим %d — не волна GONE, тег или кадр покоя' % (name, mode))
        out.append((name, w, h, bpp, mode, pal, idx))
    if p != len(b) - 4: err.append('длина файла %d, разобрано %d' % (len(b), p + 4))
    return out, err


ap = argparse.ArgumentParser(); ap.add_argument('--out', default=str(OUT))
ap.add_argument('--verify', nargs='?', const=str(OUT)); ap.add_argument('--check', nargs='?', const=str(OUT)); ap.add_argument('--sheet')
a = ap.parse_args()
if a.verify:
    f = Path(a.verify)
    if not f.exists(): print('RESULT: FAIL — нет файла %s' % f); sys.exit(1)
    marks, err = parse(f.read_bytes())
    for name, w, h, bpp, mode, pal, idx in marks: print('  %-14s %3d×%-2d режим %d' % (name, w, h, mode))
    if err: print('RESULT: FAIL — ' + '; '.join(err)); sys.exit(1)
    print('RESULT: OK — %s: %d знаков, все в слоте, бит и палитра по версии, crc' % (f.name, len(marks))); sys.exit(0)
if a.check:
    f = Path(a.check); want, marks = build2(); have = f.read_bytes() if f.exists() else b''
    if have != want:
        hm, _ = parse(have); names = {m[0] for m in hm} - {m[0] for m in marks}
        print('RESULT: FAIL — %s не равен сборке из источников%s' % (f.name, ' (знаки без исходного файла: %s)' % ', '.join(sorted(names)) if names else '')); sys.exit(1)
    print('RESULT: OK — %s = сборка из источников (%d знаков)' % (f.name, len(marks))); sys.exit(0)
b, marks = build2()   # правка 203: цветные знаки BMK2 (прежний 1-битный BMK1 — build(), только для сверки старых файлов)
Path(a.out).parent.mkdir(parents=True, exist_ok=True); Path(a.out).write_bytes(b)
for name, idx, mode, cols in marks: print('  %-14s %3d×%-2d режим %d, цветов %d' % (name, idx.shape[1], idx.shape[0], mode, len(set(idx.flatten())) - 1))
print('%s: %d знаков, %d Б' % (a.out, len(marks), len(b)))
if a.sheet:
    sheet = np.zeros((max(idx.shape[0] for _, idx, _, _ in marks) + 16, len(marks) * (SLOT2_W + 8) + 8, 3), np.uint8)
    for k, (name, idx, mode, cols) in enumerate(marks):
        rgb = np.array([[c & 255, (c >> 8) & 255, (c >> 16) & 255] for c in cols], np.uint8)[idx]
        sheet[8:8 + idx.shape[0], 8 + k * (SLOT2_W + 8):8 + k * (SLOT2_W + 8) + idx.shape[1]] = rgb
    Image.fromarray(sheet).resize((sheet.shape[1] * 4, sheet.shape[0] * 4), Image.NEAREST).save(a.sheet); print('лист', a.sheet)
