#!/usr/bin/env python3
"""Экраны вокруг игры из Scrooby оригинала (диск Роба) -> два пакета того же формата UIP1, что ui.shr
(рисует viewer/ui.c, uiScr*; грузятся только на время показа):

  boot.shr  0 license   bootup.p3d License.scr: картинка лицензии PS2 (dynaload/images/license/licensep.p3d)
            1 bootload  bootup.p3d BootupLoad.scr: та же картинка, текст закрыт чёрным (LicenseTextCover), LOADING
            2 splash    frontend.p3d Splash.scr: логотип gamelogo.png + LOADING (белым, пока грузится меню) или
                        «Press START button» (строка PRESS_START_PS2; жёлтым — цвет с эталона PS2 build/ps2ref/boot/04_menu.png)
            3 tvmovie   рамка-телевизор роликов (правка Роба 32): frontend.p3d TVFrame.pag (та же рамка, что в главном меню),
                        один масштаб TV_K, по ширине 16:9 доращена, как в меню (TV_MID — доля добавки на экран, остальное —
                        ровная часть пластика по бокам). Окно рамки — место картинки ролика (tools/movie_export.py tv_picture)
            4 demo      frontend.p3d Demo.scr -> Demo.pag: надпись «- DEMO -» (srr2 DEMO) поверх ролика простоя movies\demo.rmv
                        (правка Роба 33: простой меню -> титульный -> демо; экран Demo в SLES_518.97). COVER.pag экрана —
                        скрытый слой (чёрная заливка), не рисуется
  load.shr  0 loading   backend.p3d Loading.scr: фон loadbgd0 + две полупрозрачные звезды loadbgd1 (Overlay0/1), газета
                        уровня (dynaload/images/loading/loading1..7.p3d, кадр = UI_F_LEVEL), фитиль Wick, огонь Fire (2 кадра),
                        динамит, LOADING
            1 loadingfe backend.p3d LoadingFE.scr: фитиль с бомбой, «LOADING», «LOADING .» … (UI_F_DOTS), 3D-объект Loading =
                        квадрат scrooby/resource/pure3d/loading.p3d с анимацией текстуры 9 кадров (UI_F_ANIM)

    python tools/load_export.py                 # оба пакета -> build/boot.shr, build/load.shr + макеты build/ui/load_*.png
    python tools/load_export.py --pack boot --out X    # один пакет (tools/ship_rules.txt)
    python tools/load_export.py mock            # только макеты (из квантованных страниц, как увидит GE)

Правила 16:9 — как у tools/ui_export.py (R1–R7): один масштаб S = 272/480, якоря по третям, ничего не растягивается.
Своё здесь (по данным и эталону PS2 build/ps2ref/m4.png — кадр настоящего экрана загрузки):
  - Scrooby вписывает картинку спрайта в его рамку (эталон: фон 160x120 и звёзды 100x100 закрывают весь экран);
    если пропорции картинки и рамки расходятся — картинка вписывается целиком без искажения (газета: 283x198 в 283x189)
  - полноэкранный фон Loading (loadbgd0) — «cover»: равномерно на всю ширину 16:9, лишнее сверху/снизу обрезано.
    Правка Роба 336 (05.10: «лучи были анимированы, крутились в разные стороны»): звёзды Overlay1/Overlay0 (loadbgd1)
    — отдельные слои, движок крутит их вокруг центра экрана (viewer/ui.c drawSpin). Числа — из кода оригинала
    (SLES_518.97, экран загрузки, см. STAR_*): каждый кадр картинка звезды в своём размере x8,4, угол = прошло мс /
    20000 x 360 (18 °/с), Overlay0 по часовой, Overlay1 против (кадр PS2 m4.png). LOAD_STARS_OLD=1 — прежнее: звёзды сведены в неподвижный фон
    офлайн (красный контроль гейта tests/ui/load_spin.py)
  - лицензия и логотип — содержимое, не фон: масштаб S, по центру, по бокам чёрный (у картинок свой чёрный фон).
    Правка Роба 255 (03.10 17:00, поверх 243: «слишком крупно, верни как было и потяни на 4 пикселя вниз»): картинка лицензии
    обрезана по содержимому, содержимое — прежний масштаб S, пропорционально увеличенный так, что по высоте прибавилось
    LIC_PLUS_PX px экрана PSP; верх содержимого и середина по ширине — на прежнем месте. Закрышка текста BootupLoad — тем же
    пересчётом. UI_LIC_OLD=1 — как до 243 (вся картинка 640x480 масштабом S); UI_LIC_FILL=1 — правка 243 (вписать в LIC_FILL
    доли экрана 16:9, текст ~1,33 раза крупнее; Роб: «слишком крупно»)
  - фитиль: огонь в начале у левого конца Wick, за долю загрузки PROGRESS доходит до правого конца (к динамиту/бомбе),
    сгоревшая часть Wick не рисуется. Флаг 64 элемента, путь в пикселях PSP в поле ang (viewer/ui.c)
Чего нет в данных (в отчёт, не рисуем): вращение газеты (у оригинала — код, правка 336 нашла: 1,8 с, 15 оборотов с
замедлением, масштаб 0,25 -> 1,95, проявление; у порта свой влёт viewer/main.c newsFly), звук newspaper_spin, вспышка Explosion.pag (скрытый
слой, показывает код), частота смены кадров огня и точек LOADING (ui.c: от часов анимации, 15 к/с из loading.p3d).
Читает только диск (hit-and-run-web/source/game) и hit-and-run-web/tools (MIT) через ui_export.py.
"""
import math
import os
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ui_export as U  # noqa: E402
from p3d import read, walk, string  # noqa: E402  (MIT, hit-and-run-web)
from textures import decode_image  # noqa: E402

GAME = U.WEB / 'source/game/art/frontend'
PACKS = {'boot': ['license', 'bootload', 'splash', 'tvmovie', 'demo'], 'load': ['loading', 'loadingfe']}
PAGES = {
    'license': [('bootup.p3d', 'License.pag')],
    'bootload': [('bootup.p3d', 'License.pag'), ('bootup.p3d', 'BootupLoad.pag'), ('bootup.p3d', 'LoadingText.pag')],
    'splash': [('frontend.p3d', 'Splash.pag')],
    'demo': [('frontend.p3d', 'Demo.pag')],
    'loading': [('backend.p3d', 'Loading.pag')],
    'loadingfe': [('backend.p3d', 'LoadingFE.pag')],
}
TAGS = {'license': {'ALWAYS'}, 'bootload': {'ALWAYS'}, 'splash': {'ALWAYS', 'LOADING'}, 'tvmovie': {'ALWAYS'},
        'loading': {'ALWAYS', 'LOADING'}, 'loadingfe': {'ALWAYS'}, 'demo': {'ALWAYS'}}
PRESS_YELLOW = [255, 255, 0, 255]     # «Press START button» на PS2 жёлтый: пипетка 244/244/0 при белом 248 (эталон)
# Правка Роба 152 (01.10): «PRESS START BUTTON — шрифт на один пункт больше, чтобы лучше ложился на пиксельную сетку — на
# PSP он бьётся; везде, где мелкий шрифт, — крупнее». Как правка 136 (ui_export MENU_FACE): дробный масштаб грани
# (boulder_16 x 272/480: строка 11 px, заглавная 7) пересэмплировал буквы — мыло и неровный шаг. Грань диска 1:1 (суффикс
# '@1': ячейка шрифта диска как есть, адванс и отступ целые; ui.c — целое перо, GU_NEAREST): boulder_16@1 — строка 19 px,
# заглавная 11 — ближайший целый шаг вверх (boulder_24@1 — 17 px заглавная, вдвое крупнее). Центр строки на месте,
# рамка строки расширяется под текст. Надписи: LOADING (BootupLoad, титульный, загрузка уровня, LoadingFE), Press START
# button. UI_BOOT_FACE — другой вкус (снимок), UI_BOOT_SOFT=1 — прежняя дробная грань (красный гейта tests/boot_font)
BOOT_FACE = os.environ.get('UI_BOOT_FACE', 'boulder_16@1')
BOOT_SOFT = bool(os.environ.get('UI_BOOT_SOFT'))
BOOT_TEXT = {('LoadingText.pag', 'Loading'), ('Splash.pag', 'SplashLoading'), ('Splash.pag', 'PressStart'),
             ('Loading.pag', 'Loading'), ('LoadingFE.pag', 'Loading'),
             ('Demo.pag', 'Demo')}   # правка Роба 153: «надпись DEMO дай больше» — та же грань 1:1 (заглавная 7 -> 11 px)


def face_1to1(it, face=None):
    """Текст элемента -> грань диска 1:1 (правка 152): высота строки = строке грани, центр строки тот же; ширина рамки —
    не уже самой длинной строки (по своему выравниванию: слева — x на месте, по центру — центр на месте). Source space."""
    face = face or BOOT_FACE
    fm = U.FontCache('psp').get(face)
    x, y, w, h = it['rect']
    lh = fm['lh'] / U.S
    tw = (max(U.text_width(fm, s) for s in it['strings']) + 4) / U.S
    if tw > w:
        x = x if it.get('halign') == 'left' else x + w / 2 - tw / 2
        w = tw
    it['font'] = face
    it['rect'] = [x, y + h / 2 - lh / 2, w, lh]
    return it


NEWS_LEVELS = 7
# Газета экрана загрузки (правка Роба 35, видео 28.09 2:28: «газета крупнее — почти на весь экран»): картинка газеты
# 283x198 без масштаба (1:1 — чётче некуда, пропорции как есть) по центру ширины, по высоте — посередине между верхним
# полем NEWS_MARGIN и строкой LOADING (фитиль и надпись остаются видны). Было: рамка Scrooby 283x189 из 640x480 * S
# = 153x107 (40% высоты экрана). Разворот газеты при показе (вращение и приближение) — код оригинала, в данных его
# нет: viewer/main.c loadBegin (правило порта по эталону PS2 build/ps2ref/m4.png: газета в полёте повёрнута и крупнее).
# NEWS_NATIVE = False — прежняя раскладка (отрицательный контроль гейта tools/screens_check.py --news-red).
NEWS_NATIVE = True
NEWS_MARGIN = 8
NEWS_DOWN, NEWS_K = 14, 0.93   # правка Роба 90 (30.09): газета чуть ниже и чуть меньше — на подлёте с поворотом не липнет к верху
NEWS_MIN_H = 0.65     # гейт: газета не ниже этой доли высоты экрана (правка Роба 90: газета на 7 % меньше — было 0.70)

# Правка Роба 336 (05.10 00:32): лучи экрана загрузки крутятся. Код оригинала (диск Роба, SLES_518.97, разбор psp-objdump —
# только чтение): экран загрузки (строки «Foreground» 0x464bf0, «Overlay%d» 0x464c00; конструктор 0x1b2644–0x1b26a8 кладёт
# спрайты Overlay0/Overlay1) в обновлении (0x1b28f4–0x1b2a90) каждый кадр: прошло_мс += dt; угол = прошло_мс / 20000.0
# (0x42bc00) * 360; для Overlay0, Overlay1: сброс, ScaleAboutCenter(8.4) (0x1b2a24), RotateAboutCenter(угол, ось 0,0,1),
# угол = -угол. Фон Background — ScaleAboutCenter(4.2) один раз в конструкторе (0x1b2760). Scrooby рисует картинку спрайта
# в её размере (единицы 640x480) по центру рамки: звезда 100x100 x8,4 = круг 840 (накрывает экран, диагональ 800), фон
# 160x120 x4,2 = 672x504. Порт: звезда в одном масштабе с фоном cover (k = 480/640, как у прежнего сведённого фона):
# 100 x 8,4 x 0,75 = 630 px PSP, центр вращения — центр экрана (центр рамки 640x480 под cover). Фон оставлен прежним (cover
# по ширине; у оригинала x4,2 — на 5 % крупнее, не трогал). Направление: экрана загрузки в записи PS2 (yt-bziOipI4gDI) нет —
# сверено по кадру PS2 build/ps2ref/m4.png (газета ещё в полёте — кадр раньше 1,8 с, угол звёзд меньше 32°): подгонка фон
# x4,2 + звёзды x8,4 по центру даёт Overlay0 -15° (по часовой), Overlay1 +15° (против), корреляция 0,983 (зеркально 0,964,
# без поворота 0,770; масштаб 8,4 лучше 7,4 и 9,4). Положительный угол Scrooby — по часовой: Overlay0 крутится по часовой,
# Overlay1 — против, оба 18 °/с; газета у оригинала тоже по часовой (как newsFly порта)
STAR_SCALE = 8.4                                # 0x1b2a24: ScaleAboutCenter каждому Overlay каждый кадр
STAR_PERIOD_MS = 20000.0                        # 0x42bc00: полный оборот за столько мс
STAR_DEG_S = 360.0 * 1000.0 / STAR_PERIOD_MS    # 18 °/с
STARS_OLD = bool(os.environ.get('LOAD_STARS_OLD'))   # 1 — прежнее: звёзды сведены в неподвижный фон (красный контроль)


# Рамка-телевизор роликов (правка Роба 32, видео 28.09 1:05–1:45: «ролики 4:3 с чёрными полями выглядят недоработанно —
# рамку-телевизор, как в меню, и/или капельку растянуть, совместить»). Рамка из данных (TVFrame.pag), тот же приём, что
# в меню (ui_export.widescreen_tvframe: края рамки в своих пропорциях, добавка по ширине — растяжкой прямой части), но
# окно рамки остаётся почти 4:3 (картинка ролика не растягивается сильнее TV_STRETCH_MAX): ширину 16:9 добирает ровная
# часть пластика по бокам (source x 10..40, без фаски и контура). Сверху и снизу рамка обрезана краем экрана (картинка
# ролика крупнее, чем окно рамки меню).
TV_K = 0.60            # масштаб рамки 640x480 (в меню 272/480 = 0.567 — окно 404x206, ролику 4:3 там тесно)
TV_AS_MENU = True     # правка 89: рамка роликов = рамка меню один в один (False — прежняя tv_bezel)
TV_MID = 0.10          # доля добавки ширины, отданная окну (остальное — бокам)
TV_UNDER = 2           # картинка ролика заходит под рамку на столько px с каждой стороны (без щели у контура)
TV_STRETCH_MAX = 1.10  # «капельку»: картинка 4:3 шире своей пропорции не больше чем на 10%


def tv_bezel():
    """Рамка роликов 480x272 RGBA и окно (x0, y0, x1, y1): прозрачная середина по центральным строке и столбцу."""
    tv = U.all_images()['tvframe.png']
    a = np.asarray(tv)[..., 3]
    c0, c1 = _run(a[:, tv.width // 2] < 250, tv.height // 2)
    cy = (c0 + c1) / 2 * TV_K                                        # середина окна по высоте (в масштабе)
    k = TV_K; sw, sh = int(round(tv.width * k)), int(round(tv.height * k))
    base = U.resample(tv, sw, sh)
    y0 = int(round(cy)) - U.DST_H // 2
    full = Image.new('RGBA', (sw, U.DST_H)); full.paste(base, (0, -y0))
    extra = U.DST_W - sw; em = int(round(extra * TV_MID)); es = (extra - em) // 2; esr = extra - em - es
    e0, e1, L = int(round(10 * k)), int(round(40 * k)), int(round(120 * k))
    out = Image.new('RGBA', (U.DST_W, U.DST_H)); x = 0
    for a0, a1, w in ((0, e0, e0), (e0, e1, e1 - e0 + es), (e1, L, L - e1)):           # левый край: контур, пластик, фаска
        out.paste(U.resample(full.crop((a0, 0, a1, U.DST_H)), w, U.DST_H), (x, 0)); x += w
    mw = U.DST_W - x - (L + esr)
    out.paste(U.resample(full.crop((L, 0, sw - L, U.DST_H)), mw, U.DST_H), (x, 0)); x += mw
    for a0, a1, w in ((sw - L, sw - e1, L - e1), (sw - e1, sw - e0, e1 - e0 + esr), (sw - e0, sw, e0)):
        out.paste(U.resample(full.crop((a0, 0, a1, U.DST_H)), w, U.DST_H), (x, 0)); x += w
    al = np.asarray(out)[..., 3]
    x0, x1 = _run(al[U.DST_H // 2] < 250, U.DST_W // 2); y0, y1 = _run(al[:, U.DST_W // 2] < 250, U.DST_H // 2)
    return out, (x0, y0, x1, y1)


def _run(m, i):
    """Границы [a, b) непрерывного отрезка True в строке m, содержащего индекс i (окно рамки — от середины до контура)."""
    a = b = i
    while a > 0 and m[a - 1]: a -= 1
    while b < len(m) and m[b]: b += 1
    return int(a), int(b)


def tv_picture(opening=None):
    """Место картинки ролика в кадре 480x272 (x, y, w, h) и растяжка по ширине: окно рамки + TV_UNDER под рамку."""
    x0, y0, x1, y1 = opening or tv_bezel()[1]
    x, y, w, h = x0 - TV_UNDER, y0 - TV_UNDER, x1 - x0 + 2 * TV_UNDER, y1 - y0 + 2 * TV_UNDER
    return (x, y, w, h), w / (h * 4 / 3)


def sprite_images(path):
    """Спрайты Scrooby (0x19005) из p3d -> {имя.png: RGBA}"""
    out = {}
    for c in read(path).children:
        if c.id == 0x19005:
            n, im = U.decode_sprite_image(c); out[n] = im
    return out


def fe_anim():
    """loading.p3d: квадрат 4:3 (loading_screenShape) под камерой loading_screen_cameraShape, текстура сменяется по
    ключам канала TEX анимации -> (кадры RGBA по порядку ключей, доля ширины рамки, смещение центра в долях рамки, к/с)"""
    root = read(GAME / 'scrooby/resource/pure3d/loading.p3d')
    tex, cam, quad, keys, fps = {}, None, None, None, None
    for c in walk(root):
        if c.id == 0x19000:
            n, _ = string(c.data)
            raw = next(s for s in walk(c) if s.id == 0x19002)
            tex[n] = decode_image(raw.data[4:])
        elif c.id == 0x2200:
            _, p = string(c.data)
            ver, fov, aspect, near, far = struct.unpack_from('<I4f', c.data, p); p += 20
            pos, look, up = [struct.unpack_from('<3f', c.data, p + 12 * i) for i in range(3)]
            cam = dict(fov=fov, pos=np.array(pos), look=np.array(look), up=np.array(up))
        elif c.id == 0x10002:
            import convert  # noqa  (MIT, hit-and-run-web)
            _, ps = convert.geometry(c)
            quad = np.concatenate([p['position'] for p in ps])
        elif c.id == 0x121000:
            _, p = string(c.data, 4)
            typ = c.data[p:p + 4]; nfr, rate = struct.unpack_from('<2f', c.data, p + 4)
            if typ == b'TEX\0':
                fps = rate
        elif c.id == 0x121107:
            n, = struct.unpack_from('<I', c.data, 8)
            frames = struct.unpack_from('<%dH' % n, c.data, 12)
            p = 12 + 2 * n; names = []
            for _ in range(n):
                s, p = string(c.data, p); names.append(s)
            keys = list(zip(frames, names))
    # проекция: fov Pure3D — горизонтальный при 4:3 (так же в tools/frontend3d_export.py, сверено с эталоном гостиной)
    f = cam['look'] / np.linalg.norm(cam['look']); up = cam['up'] / np.linalg.norm(cam['up'])
    right = np.cross(up, f); right /= np.linalg.norm(right); up = np.cross(f, right)
    th = math.tan(cam['fov'] / 2); tv = th * 3 / 4
    sx, sy = [], []
    for v in quad:
        d = v - cam['pos']; z = d @ f
        sx.append((d @ right) / z / th); sy.append((d @ up) / z / tv)       # -1..1 по рамке
    x0, x1, y0, y1 = min(sx), max(sx), min(sy), max(sy)
    # кадры: ключи канала TEX по 2 такта при 30 к/с -> равные доли; частота кадров анимации = rate / шаг ключей
    step = keys[1][0] - keys[0][0] if len(keys) > 1 else 1
    return ([tex[n] for _, n in keys], dict(w=(x1 - x0) / 2, h=(y1 - y0) / 2, cx=(x0 + x1) / 4, cy=-(y0 + y1) / 4,
                                            fps=fps / step, nkeys=len(keys)))


def contain(box, iw, ih, jx='centre', jy='centre'):
    """Картинка целиком в рамке без искажения (source space, y вниз)"""
    x, y, w, h = box
    k = min(w / iw, h / ih); cw, ch = iw * k, ih * k
    ox = x if jx == 'left' else (x + w - cw if jx == 'right' else x + (w - cw) / 2)
    oy = y if jy == 'top' else (y + h - ch if jy == 'bottom' else y + (h - ch) / 2)
    return [ox, oy, cw, ch]


class Loader:
    def lic_rect(self, rect):   # правка 243: рамка содержимого лицензии (единицы Scrooby) и пересчёт рамок картинки 640x480
        rx, ry, rw, rh = rect; x0, y0, x1, y1 = self.lic_box; W, H = self.img['licensep.png'].size
        cw, ch = x1 - x0, y1 - y0
        if LIC_FILL_243:   # правка 243: вписать в LIC_FILL экрана, по центру
            k = min(LIC_FILL * U.DST_W / cw, LIC_FILL * U.DST_H / ch) / U.S   # единиц Scrooby на px картинки
            nw, nh = cw * k, ch * k; nx, ny = U.SRC_W / 2 - nw / 2, U.SRC_H / 2 - nh / 2
        else:              # правка 255: прежний масштаб + LIC_PLUS_PX по высоте экрана, верх и середина по ширине на месте
            ix0, iy0, ix1, iy1 = self.lic_ink                                 # чернила (без поля LIC_MARGIN), px картинки
            h0 = math.ceil((iy1 - iy0) * rh / H * U.S - 1e-6)                 # видимых строк текста на экране PSP до 243
            k = (h0 + LIC_PLUS_PX) / ((iy1 - iy0) * U.S)                      # (173,4 px -> 174 строки) + 4 = 178
            nw, nh = cw * k, ch * k
            nx = rx + (ix0 + ix1) / 2 * rw / W - ((ix0 + ix1) / 2 - x0) * k   # середина чернил по ширине — на месте
            ny = ry + iy0 * rh / H - (iy0 - y0) * k                           # верх чернил — на месте

        def remap(r):
            ax, ay = (r[0] - rx) * W / rw, (r[1] - ry) * H / rh
            return [nx + (ax - x0) * k, ny + (ay - y0) * k, r[2] * W / rw * k, r[3] * H / rh * k]
        return [nx, ny, nw, nh], remap

    def __init__(self):
        for sc, pages in PAGES.items():
            U.SCREEN_PAGES[sc] = pages
        self.img = {}
        self.img.update(sprite_images(GAME / 'scrooby/bootup.p3d'))
        self.img.update(sprite_images(GAME / 'scrooby/backend.p3d'))
        lp_ = GAME / 'dynaload/images/license/licensep.p3d'   # №880: на NTSC файла нет — картинка лицензии license.png в scrooby/bootup.p3d (тот же экран 640x480)
        self.img['licensep.png'] = sprite_images(lp_)['licensep.png'] if lp_.exists() else next(v for k, v in self.img.items() if k.lower() == 'license.png')
        lic = self.img['licensep.png']; m = np.asarray(lic.convert('RGB')).astype(int).sum(2) > 60   # правка 243: содержимое
        ys, xs = np.where(m); x0, y0 = max(0, xs.min() - LIC_MARGIN), max(0, ys.min() - LIC_MARGIN)
        x1, y1 = min(lic.width, xs.max() + 1 + LIC_MARGIN), min(lic.height, ys.max() + 1 + LIC_MARGIN)
        self.lic_box = (x0, y0, x1, y1); self.img['licensep_c.png'] = lic.crop(self.lic_box)
        self.lic_ink = (int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1)   # правка 255: чернила без поля
        for lv in range(1, NEWS_LEVELS + 1):
            lp_ = GAME / ('dynaload/images/loading/loading%d.p3d' % lv)   # №880: на NTSC без папки loading/
            if not lp_.exists(): lp_ = GAME / ('dynaload/images/loading%d.p3d' % lv)
            self.img['loading%d.png' % lv] = sprite_images(lp_)['loading%d.png' % lv]
        frames, self.anim = fe_anim()
        # квадрат 4:3 с квадратной текстурой: в оригинале её растягивает сам квадрат -> картинка кадра сразу 4:3
        aw, ah = frames[0].size
        for i, im in enumerate(frames):
            self.img['loadanim%d.png' % i] = im.resize((round(ah * 4 / 3), ah), Image.LANCZOS)
        self.fire_pad()

    def fire_pad(self):
        # кадр fire.png (30×33) в рамке спрайта 46×50 по центру, в натуральном размере (рамка Fire.jx/jy = centre), см. правку 499 в items()
        from PIL import Image as _I
        fa, fb = self.img['fire.png'].convert('RGBA'), self.img['fire2.png']
        pad = _I.new('RGBA', fb.size, (0, 0, 0, 0)); pad.paste(fa, ((fb.size[0] - fa.size[0]) // 2, (fb.size[1] - fa.size[1]) // 2))
        self.img['fire_pad.png'] = pad

    def assets(self):
        a = U.Assets()
        for k, v in self.img.items():
            a.images.setdefault(k, v)
        return a

    def items(self, sc):
        """Элементы экрана в source space (640x480, y вниз) + то, что добавляет код оригинала по данным"""
        items, _ = U.build_items(sc)
        out, extra = [], []
        S = U.S
        for it in items:
            nm, pg = it['name'], it['page']
            if pg == 'Loading.pag' and nm in ('Background', 'Overlay0', 'Overlay1'):
                continue                                     # фон -> одна картинка cover (post_adapt)
            if pg == 'Loading.pag' and it['type'] == U.T_IMG and nm == 'Loading':      # газета уровня
                im = self.img['loading1.png']
                it.update(frames=['loading%d.png' % lv for lv in range(1, NEWS_LEVELS + 1)], field='LEVEL',
                          rect=contain(it['rect'], *im.size, it['jx'], it['jy']))
            if pg == 'Loading.pag' and it['type'] == U.T_TEXT and nm == 'Loading':   # правка Роба 244: «должно мигать» — своя метка,
                it['tag'] = 'LOADING'                         # движок гасит её через раз (viewer/main.c scrFrame, LOAD_BLINK_S)
            if nm == 'Wick':
                it['fuse'] = it['rect'][2] * S               # путь огня = длина фитиля
            if nm == 'Fire':                                 # 2 кадра огня; частоты в данных нет -> часы анимации loading.p3d
                it.update(field='FIRE', rate=self.anim['fps'])
                # Правка Роба 499 (=385, третий повтор 10.10): «в оригинале огонёк мигает, дёргается — будто две PNG разного масштаба чередуются». На диске кадры огня РАЗНОГО
                # размера: fire.png 30×33 и fire2.png 46×50 (рамка спрайта 46×50 = второй кадр). Сжатие обоих в одну рамку (раньше) стирало разницу масштаба — огонь горел ровно.
                # Теперь меньший кадр кладётся в рамку в своём натуральном размере по выравниванию спрайта (jx/jy), как рисует Scrooby: кадры чередуются 30×33 ↔ 46×50 в единицах 640×480.
                # LOAD_FIRE_SCALE_OLD=1 — прежнее (оба кадра в рамку)
                if not os.environ.get('LOAD_FIRE_SCALE_OLD') and len(it.get('frames', [])) == 2: it['frames'] = ['fire_pad.png', it['frames'][1]]
                # Темп по записи PS2 (наблюдатель, кадры через 4 vsync): цикл 0,40 с = малая 0,24 + большая 0,16 (порядок на записи: малая ×3, большая ×2 на шаге 0,08 с).
                # Раньше кадры шли поровну по 1/15 с (цикл 0,13 с), в 3 раза быстрее оригинала: на железе сливалось. Теперь список из 5 кадров при 12,5 к/с.
                # LOAD_FIRE_TEMPO_OLD=1 — прежнее (2 кадра по часам анимации)
                if not os.environ.get('LOAD_FIRE_TEMPO_OLD') and it['frames'][0] == 'fire_pad.png':
                    it['frames'] = ['fire_pad.png'] * 3 + [it['frames'][1]] * 2; it['rate'] = 12.5
            if pg in ('Loading.pag', 'LoadingFE.pag') and it['type'] == U.T_IMG and nm == 'Fire':
                wick = next(t for t in items if t['page'] == pg and t['name'] == 'Wick')
                it['fuse'] = wick['rect'][2] * S
            if pg == 'LoadingFE.pag' and it['type'] == U.T_TEXT and nm == 'Loading':     # LOADING_0..3; шага в данных нет:
                it.update(field='DOTS', fmt='select', rate=self.anim['fps'] / self.anim['nkeys'])   # точка за цикл анимации
                # сверка g09b-menu (M1 №14, 02.11.2026): у оригинала строка «Loading» меняет число точек каждые 500 мс по кругу (в данных шага нет, он в коде игры).
                # LOAD_DOTS_OLD=1 — прежний темп (по часам анимации loading.p3d)
                if not os.environ.get('LOAD_DOTS_OLD'): it['rate'] = 2.0
            if pg == 'Splash.pag' and it['type'] == U.T_TEXT and nm == 'PressStart':
                ps2 = U._BIBLE('PRESS_START_PS2')
                out.append(dict(it, name='SplashLoading', strings=[it['strings'][0]], tag='LOADING', sel=0))
                out.append(dict(it, name='PressStart', strings=[ps2], tag='PRESS_START', color=PRESS_YELLOW, sel=0))
                continue
            out.append(it)
        if not BOOT_SOFT:                                   # правка 152: надписи — грань диска 1:1, на пункт крупнее
            for t in out:
                if t['type'] == U.T_TEXT and (t['page'], t['name']) in BOOT_TEXT:
                    face_1to1(t)
        if sc in ('license', 'bootload'):                   # License.pag: спрайт-пустышка dot -> картинка лицензии PS2
            prj = U.project('bootup.p3d')
            e = next(e for p, e in U.iter_leaves(prj.pages['License.pag']) if e['name'] == 'License')
            rect = [e['x'], U.ydown(e['y'], e['h']), e['w'], e['h']]; frames = ['licensep.png']
            if not LIC_OLD:   # правки 243/255: содержимое картинки отдельно от поля; закрышка текста — тем же пересчётом
                rect, remap = self.lic_rect(rect); frames = ['licensep_c.png']
                for t in out:
                    if t['name'] == 'LicenseTextCover': t['rect'] = remap(t['rect'])
            extra.append(dict(name='License', page='License.pag', path='Foreground', tag='ALWAYS', unit=('License.pag', 'Foreground'),
                              forced=None, field='NONE', fmt='none', menu=-1, type=U.T_IMG, rect=rect, color=e['color'],
                              frames=frames, jx=e['jx'], jy=e['jy']))
        if sc == 'loadingfe':                               # 3D-объект Loading (res=dummy) = квадрат loading.p3d в рамке
            prj = U.project('backend.p3d')
            e = next(e for p, e in U.iter_leaves(prj.pages['LoadingFE.pag']) if e['kind'] == 'p3d' and e['name'] == 'Loading')
            bx, by, bw, bh = e['x'], U.ydown(e['y'], e['h']), e['w'], e['h']
            a = self.anim; w, h = bw * a['w'], bh * a['h']
            cx, cy = bx + bw / 2 + a['cx'] * bw, by + bh / 2 + a['cy'] * bh
            extra.append(dict(name='LoadAnim', page='LoadingFE.pag', path='Background', tag='ALWAYS',
                              unit=('LoadingFE.pag', 'Background'), forced=None, field='ANIM', fmt='none', menu=-1,
                              type=U.T_IMG, rect=[cx - w / 2, cy - h / 2, w, h], color=e['color'], rate=a['fps'],
                              frames=['loadanim%d.png' % i for i in range(a['nkeys'])], jx='centre', jy='centre'))
        # порядок рисования: как в странице (фон и лицензия первыми — они под остальным)
        first = [t for t in out if t['type'] == U.T_RECT and t.get('fill')]
        rest = [t for t in out if not (t['type'] == U.T_RECT and t.get('fill'))]
        if sc == 'bootload':                                 # BootupLoad.pag (закрытие текста) идёт после License.pag
            if not LIC_COVER:   # правка Роба 271 (04.10): текст лицензии остаётся под LOADING, «чтобы почитать» — без закрышки
                out = [t for t in out if t['name'] != 'LicenseTextCover']   # (правило порта; у оригинала текст закрыт чёрным)
            return extra + out
        return first + extra + rest

    def cover_bg(self, assets):
        """Loading.pag: BGColour (чёрный) + Background loadbgd0 + Overlay1 (альфа 64) + Overlay0 (альфа 115), каждый
        растянут в свою рамку 640x480, как в оригинале; на 16:9 — равномерно 0.75 на всю ширину 480, обрезка по высоте"""
        prj = U.project('backend.p3d')
        els = {e['name']: e for p, e in U.iter_leaves(prj.pages['Loading.pag'])}
        k = U.DST_W / U.SRC_W; W, H = U.DST_W, int(round(U.SRC_H * k))
        canvas = Image.new('RGBA', (W, H), (0, 0, 0, 255))
        # порядок слоёв страницы; правка 336: звёзды — отдельные крутящиеся слои (stars), в фон сводится только Background
        for nm in ('Background', 'Overlay1', 'Overlay0') if STARS_OLD else ('Background',):
            e = els[nm]
            src = self.img[prj.res_images[e['images'][0]]]
            if nm == 'Background':                  # сборка плиток оставляет пустыми последний столбец и строку непрозрачной
                src = src.crop(src.getchannel('A').getbbox())   # картинки; на эталоне PS2 (m4.png) тёмной полосы у края нет
            x, y, w, h = e['x'], U.ydown(e['y'], e['h']), e['w'], e['h']
            im = U.resample(src, int(round(w * k)), int(round(h * k)))
            if e['color'] != [255, 255, 255, 255]:
                a = np.asarray(im).astype(np.float32) * np.array(e['color'], np.float32) / 255
                im = Image.fromarray(np.clip(a + .5, 0, 255).astype(np.uint8), 'RGBA')
            canvas.alpha_composite(im, (int(round(x * k)), int(round(y * k))))
        y0 = (H - U.DST_H) // 2
        return assets.put('loadbg@%dx%d' % (W, U.DST_H), canvas.crop((0, y0, W, y0 + U.DST_H)))

    def stars(self, assets):
        """Правка 336: Overlay1 (альфа 64), затем Overlay0 (альфа 115) — по порядку страницы; одна картинка loadbgd1 в своём
        размере (своя страница текстур, линейный фильтр), место — квадрат STAR_SCALE x картинка x k вокруг центра экрана,
        spin — °/с против часовой стрелки на экране (Overlay0 -18 — по часовой, Overlay1 +18), pivot — центр, px PSP (ui.c drawSpin)"""
        prj = U.project('backend.p3d')
        els = {e['name']: e for p, e in U.iter_leaves(prj.pages['Loading.pag'])}
        k = U.DST_W / U.SRC_W                                            # масштаб фона cover (cover_bg)
        out = []
        for nm, sign in (('Overlay1', 1.0), ('Overlay0', -1.0)):     # Overlay0 — по часовой (кадр m4.png), Overlay1 — против
            e = els[nm]
            src = self.img[prj.res_images[e['images'][0]]]
            key = assets.put('loadstar@%dx%d' % src.size, src)
            cx = (e['x'] + e['w'] / 2) * k                               # центр рамки 640x480 -> центр экрана под cover
            cy = (U.ydown(e['y'], e['h']) + e['h'] / 2) * k - (int(round(U.SRC_H * k)) - U.DST_H) // 2
            d = int(round(src.width * STAR_SCALE * k))                   # 100 x 8,4 x 0,75 = 630
            out.append(dict(name=nm, page='Loading.pag', path='Foreground', tag='ALWAYS', type=U.T_IMG, field='NONE',
                            fmt='none', menu=-1, color=list(e['color']), anchor='fill', bitmaps=[key],
                            dst=[int(round(cx - d / 2)), int(round(cy - d / 2)), d, d], pivot=(cx, cy), spin=sign * STAR_DEG_S))
        return out

    def build(self, sc, assets, fonts):
        if sc == 'tvmovie':                                   # рамка роликов: только непрозрачные куски (окно не хранится)
            # правка Роба 89 (30.09): рамка ИДЕНТИЧНА меню — та же сборка, что у TVFrame главного меню (ui_export
            # widescreen_tvframe, масштаб S), а не своя растяжка tv_bezel (TV_K/TV_MID)
            bm = U.widescreen_tvframe(U.all_images()['tvframe.png'], U.DST_W, U.DST_H, U.S) if TV_AS_MENU else tv_bezel()[0]; dl = []
            for (x0, y0, x1, y1), nm in U.split_tvframe(bm):
                dl.append(dict(name='TVMovie_' + nm, page='TVFrame.pag', path='TVFrame', tag='ALWAYS', type=U.T_IMG, field='NONE',
                               fmt='none', menu=-1, color=[255, 255, 255, 255], dst=[x0, y0, x1 - x0, y1 - y0],
                               bitmaps=[assets.put('tvmovie_%s@%dx%d' % (nm, x1 - x0, y1 - y0), bm.crop((x0, y0, x1, y1)))], anchor='fill'))
            return dl
        items = self.items(sc)
        dl, units = U.adapt(items, {}, 'psp', assets, fonts)
        if sc in ('loading', 'loadingfe') and not os.environ.get('LOAD_FUSE_43'):
            # правка Роба 191 (01.10 22:2x, развитие 177): «фитиль во всю ширину — кота с динамитом к правому краю, фитиль до левого,
            # загрузка для широкого экрана, а не со старого». Огонь и начало фитиля уходят к левому краю 16:9, динамит (бомба) — к
            # правому, на те же поля, что были у краёв кадра 4:3 (огонь 52, динамит 47 из 640); фитиль и путь огня длиннее на 2·Δ.
            # LOAD_FUSE_43=1 — прежняя раскладка 4:3 (отрицательный контроль)
            d = int(round((U.DST_W - 640 * U.S) / 2))   # 59 px PSP: поле кадра 4:3 с каждой стороны
            wick = next(t for t in dl if t['name'] == 'Wick')
            fire = next(t for t in dl if t['name'] == 'Fire')
            end = next(t for t in dl if t['name'] in ('Dynamite', 'Bomb'))
            fire['dst'] = [fire['dst'][0] - d] + list(fire['dst'][1:])
            end['dst'] = [end['dst'][0] + d] + list(end['dst'][1:])
            wick['dst'] = [wick['dst'][0] - d, wick['dst'][1], wick['dst'][2] + 2 * d, wick['dst'][3]]
            wick['fuse'] = fire['fuse'] = wick['dst'][2]
        for t in dl:                                          # путь фитиля — в пикселях PSP уже (S), он и есть ang
            if t.get('fuse'):
                t['fuse'] = round(t['fuse'], 2)
        if sc == 'loading' and NEWS_NATIVE:                   # газета 1:1 почти во весь экран (правка Роба 35)
            txt = next(t for t in dl if t['type'] == U.T_TEXT and t['name'] == 'Loading')
            news = next(t for t in dl if t['type'] == U.T_IMG and t.get('field') == 'LEVEL')
            iw, ih = self.img['loading1.png'].size
            top, bot = NEWS_MARGIN, txt['dst'][1] - NEWS_MARGIN
            k = min(1.0, (bot - top) / ih, (U.DST_W - 2 * NEWS_MARGIN) / iw) * NEWS_K
            w, h = int(round(iw * k)), int(round(ih * k))
            news['dst'] = [(U.DST_W - w) // 2, top + (bot - top - h) // 2 + NEWS_DOWN, w, h]
            news['bitmaps'] = [assets.put('loading%d@%dx%d' % (lv, w, h), U.resample(self.img['loading%d.png' % lv], w, h) if k < 1
                                          else self.img['loading%d.png' % lv]) for lv in range(1, NEWS_LEVELS + 1)]
        if sc == 'loading':                                   # фон cover — сразу после чёрной заливки BGColour
            bg = dict(name='LoadBackground', page='Loading.pag', path='Background', tag='ALWAYS', type=U.T_IMG, field='NONE',
                      fmt='none', menu=-1, color=[255, 255, 255, 255], dst=[0, 0, U.DST_W, U.DST_H], bitmaps=[self.cover_bg(assets)],
                      anchor='fill')
            k = next(i for i, t in enumerate(dl) if t['type'] == U.T_RECT and t.get('fill')) + 1
            dl.insert(k, bg)
            if not STARS_OLD:                                 # правка 336: крутящиеся звёзды — сразу над фоном, под фитилём и газетой
                dl[k + 1:k + 1] = self.stars(assets)
        return dl


LIC_OLD = bool(os.environ.get('UI_LIC_OLD'))
LIC_COVER = bool(os.environ.get('UI_LIC_COVER'))   # правка 271: 1 — прежнее (BootupLoad закрывает текст лицензии)
LIC_FILL_243 = bool(os.environ.get('UI_LIC_FILL'))   # правка 243 (вписать в экран) — отрицательный контроль гейта 255
LIC_FILL = 0.88     # доля ширины и высоты экрана PSP под содержимое лицензии (правка 243, только с UI_LIC_FILL=1)
LIC_PLUS_PX = 4     # правка Роба 255: насколько выше прежнего содержимое лицензии на экране PSP, px
LIC_MARGIN = 6      # поле вокруг содержимого, px картинки


# №409 (Роб: «фон-градиент под логотипом ступеньками?»): страница логотипа титульного (группа logo) — палитра из точных цветов кадра
# 5650 и рассеяние Флойда–Стейнберга (tools/ui_export.py quantize_page_565); движок рисует титульный без дизеринга GE
# (viewer/main.c scrFrame, TITLE_DITHER_OLD). Замер ступеней и сетки — tools/title_band.py. UI_LOGO_FS_OLD=1 — прежнее (k-means)
LOGO_FS_OLD = bool(os.environ.get('UI_LOGO_FS_OLD'))


def group_of(key):
    b = key.split('@')[0]
    if b.startswith('loadbg'):
        return 'bg'
    if b.startswith('loadstar'):                     # правка 336: звезда — своя страница (линейный фильтр без соседей по атласу)
        return 'star'
    if b.startswith('licensep'):
        return 'lic'
    if b.startswith('gamelogo'):
        return 'logo'
    if b.startswith('loading') and b[7:8].isdigit():
        return 'news'
    if b.startswith('loadanim'):
        return 'anim'
    if b.startswith('tvmovie'):
        return 'tv'
    return 'ui'


def fuse_shift(t, prog):
    """Сдвиг огня/начала фитиля: как viewer/ui.c (floorf(prog/1000 * ang + .5))"""
    return int(math.floor(prog / 1000.0 * t['fuse'] + 0.5))


def clock_index(t, rate):
    """Кадр по часам экрана: как viewer/ui.c — float * float, отбросить дробь"""
    return int(np.float32(t) * np.float32(rate))


def spin_angle(t, T):
    """Правка 336: угол слоя (°, против часовой) на часах экрана T — как viewer/ui.c drawSpin (float)"""
    return float(np.float32(t['spin']) * np.float32(T))


def spin_layer(t, bm, ang):
    """Правка 336: слой звезды 480x272 как его рисует viewer/ui.c drawSpin: квадрат dst повёрнут на ang° против часовой
    вокруг pivot, картинка bm (квантованная, как в GE) растянута на квадрат билинейно. Без цвета элемента (его кладёт comp)"""
    x, y, w, h = t['dst']; cx, cy = t['pivot']
    th = math.radians(ang); c, s = math.cos(th), math.sin(th)
    kx, ky = bm.width / w, bm.height / h
    # точка экрана P -> квадрат до поворота Q = C + R(-ang)(P - C) (ось y вниз) -> пиксель картинки (Q - угол) * k
    a, b = c * kx, -s * kx; d, e = s * ky, c * ky
    cc = (cx - x - c * cx + s * cy) * kx
    ff = (cy - y - s * cx - c * cy) * ky
    return bm.transform((U.DST_W, U.DST_H), Image.AFFINE, (a, b, cc, d, e, ff), resample=Image.BILINEAR)


def mock(res, sc, state, bg=(0, 0, 0, 255)):
    """Макет экрана из квантованных страниц (то, что увидит GE); state: show, PROGRESS, LEVEL, T (часы экрана, с)"""
    dl = []
    qb = res['qbitmaps']
    for t in res['screens'][sc]:
        if t.get('spin') is not None:                       # правка 336: звезда, повёрнутая на часах экрана
            key = 'spin:%s@%.4f' % (t['name'], spin_angle(t, state.get('T', 0.0)))
            if key not in qb:
                qb = dict(qb) if qb is res['qbitmaps'] else qb
                qb[key] = spin_layer(t, res['qbitmaps'][t['bitmaps'][0]], spin_angle(t, state.get('T', 0.0)))
            dl.append(dict(t, dst=[0, 0, U.DST_W, U.DST_H], bitmaps=[key]))
            continue
        if t.get('rate'):                                   # кадр / строка по часам — в поле, которое читает render()
            n = len(t['bitmaps']) if t['type'] == U.T_IMG else len(t['strings'])
            state = dict(state); state[t['field']] = clock_index(state.get('T', 0.0), t['rate']) % n
        if t.get('fuse'):
            t = dict(t); d = list(t['dst']); s = fuse_shift(t, state.get('PROGRESS', 0))
            if t['type'] == U.T_RECT:
                d[0] += s; d[2] -= s
                if d[2] <= 0:
                    continue
            else:
                d[0] += s
            t['dst'] = d
        dl.append(t)
    st = dict(state); st.setdefault('show', TAGS[sc])
    return U.render(dl, U.DST_W, U.DST_H, st, qb, Image.new('RGBA', (U.DST_W, U.DST_H), bg), None, -1)


def export(name, out=None, mocks=True):
    ld = Loader()
    spec = dict(screens=PACKS[name], build=ld.build, tags=TAGS, group=group_of, radar=False, fs565=() if LOGO_FS_OLD else ('logo',),
                groups=('bg', 'star', 'lic', 'logo', 'news', 'anim', 'tv', 'ui'), layout='layout_%s.json' % name, assets=ld.assets())
    res = U.pack(out or (U.ROOT / 'build' / ('%s.shr' % name)), spec)
    print('%s.shr: %d B (%.1f KB), экраны %s; текстуры %s' % (name, res['size'], res['size'] / 1024, ', '.join(PACKS[name]),
          ', '.join('%s %dx%d/%d' % (t['name'], t['w'], t['h'], t['rows']) for t in res['tex'])))
    if mocks:
        U.OUTDIR.mkdir(parents=True, exist_ok=True)
        for sc in PACKS[name]:
            states = {'splash': [dict(show={'ALWAYS', 'LOADING'}), dict(show={'ALWAYS', 'PRESS_START'})],
                      'loading': [dict(PROGRESS=0), dict(PROGRESS=500, T=0.07), dict(PROGRESS=1000)],
                      'loadingfe': [dict(PROGRESS=0), dict(PROGRESS=500, T=1.35)]}.get(sc, [dict()])
            for i, stt in enumerate(states):
                p = U.OUTDIR / ('load_%s_%d.png' % (sc, i)); mock(res, sc, stt).save(p)
                print('  макет', p.relative_to(U.ROOT))
    return res


if __name__ == '__main__':
    argv = sys.argv[1:]
    out = pk = None
    if '--out' in argv:
        i = argv.index('--out'); out = argv[i + 1]; del argv[i:i + 2]
    if '--pack' in argv:
        i = argv.index('--pack'); pk = argv[i + 1]; del argv[i:i + 2]
    names = [pk] if pk else list(PACKS)
    for n in names:
        export(n, out if pk else None, mocks=not out or 'mock' in argv)
