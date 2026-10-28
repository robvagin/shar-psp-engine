#!/usr/bin/env python3
"""Меню оригинала (Scrooby, диск Роба) -> два пакета того же формата UIP1, что ui.shr (рисует viewer/ui.c, uiScr*;
грузятся только на время показа меню):

  menu.shr   экраны главного меню внутри рамки-телевизора (над 3D-гостиной frontend3d):
             0 options  frontend.p3d Options.scr: TVFrame + Options.pag + Back + Accept
             1 sound    frontend.p3d Sound.scr: TVFrame + Sound.pag + Back
  pextra.shr экраны паузы: 0 viewcards (PauseViewCards.scr), 1 missionselect (MissionSelect.scr); раскладки tools/ui_export.py
  pmenu.shr  экраны паузы (над остановленным городом):
             0 pauseoptions  ingame.p3d PauseOptions.scr: PauseBgd + XSmallBoard + PauseFgd + PauseOptions + Back + Accept
             1 pausesound    ingame.p3d PauseSound.scr: PauseBgd + BigBoard + PauseFgd + Sound + Back
             2 pausesettings ingame.p3d PauseSettings.scr: PauseBgd + BigBoard + PauseFgd + PauseSettings + Back

Состав экранов — списки страниц .scr из данных; раскладка 16:9 — правила tools/ui_export.py (R1–R7).
Строки меню (общее правило для всех экранов): дети группы Foreground/Menu страницы меню — текст прямо в Menu (пункт) или
группа (строка с подписью, значением _Value, стрелками _LArrow/_RArrow, ползунком _Slider поверх _SliderBgd), сверху вниз.
Номер строки = menu; значение строки — поле ROW<номер> (UI_F_ROW0 + k): _Value — номер варианта (строки Scrooby), ползунок —
заполнение 0..1000 (T_BAR). Подпись строки — пункт меню (жёлтая, когда выбрана), стрелки — только у выбранной строки.
Правила порта (в данных нет, решает код оригинала / платформа):
  - строки для других платформ и для того, чего у PSP нет, не выводятся: Display, DisplayMode (ПК/GameCube), Controller
    (раскладка геймпада PS2), SurroundSound (у PSP стерео), Vibration (у PSP нет вибрации; в Settings паузы на её месте — строка Colour, правка 144); значок «звук выключен» (_Off)
    не рисуется (его показывает код при нуле)
  - Cheats.pag (скрытое меню кодов) не выводится
    python tools/menu_export.py                   # оба пакета -> build/menu.shr, build/pmenu.shr + макеты build/ui/menu_*.png
    python tools/menu_export.py --pack menu --out X
Читает только диск (hit-and-run-web/source/game) и hit-and-run-web/tools (MIT) через ui_export.py.
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ui_export as U  # noqa: E402

# OPTIONS главного меню: 2 viewcredits (ViewCredits.scr), 3 viewmovies (ViewMovies.scr) — 29.09 (viewer/main.c optionsRun).
# MENU_OLD=1 — подброска гейта tests/ui/options_extra.sh: пакет как до 29.09 (только options, sound)
# №777 (Роб 18.10: «Settings примитивный — размножить на подкатегории, панель тонкой настройки изображения»): SETTINGS паузы — раздел (экран 2,
# доска пунктов как OPTIONS: CAMERA / GAME / PICTURE / CARTOON), строки — на подстраницах 3..6 (доска и строки прежней SETTINGS, шрифт Bart).
# Движок (viewer/main_save.h) узнаёт раздел по числу экранов пакета (7). SETTINGS_PAGES_OLD=1 — прежняя одна страница на 9 строк
SETTINGS_PAGES = os.environ.get('SETTINGS_PAGES_OLD') != '1'
SUBPAGES = ['pausecamera', 'pausegame', 'pausepicture', 'pausecartoon'] if SETTINGS_PAGES else []
PACKS = {'menu': ['options', 'sound'] + ([] if os.environ.get('MENU_OLD') else ['viewcredits', 'viewmovies']),
         'pmenu': ['pauseoptions', 'pausesound', 'pausesettings'] + SUBPAGES,
         'pextra': ['viewcards', 'missionselect']}   # экраны паузы 29.09: VIEW CARDS (PauseViewCards.scr), MISSION SELECT
                                                     # (MissionSelect.scr) — раскладки tools/ui_export.py viewcards_layout / missionselect_layout
BG = ('ingamel%d.p3d' % U.LEVEL, 'PauseBgd.pag')
PAGES = {
    'options': [('frontend.p3d', 'TVFrame.pag'), ('frontend.p3d', 'Options.pag'), ('frontend.p3d', 'Back.pag'),
                ('frontend.p3d', 'Accept.pag')],
    'sound': [('frontend.p3d', 'TVFrame.pag'), ('frontend.p3d', 'Sound.pag'), ('frontend.p3d', 'Back.pag')],
    # ViewCredits.scr: чёрный фон CreditsBgd, титры ViewCredits.pag (блоки CREDITS_00..31), рамка TVFrame, Back — в порядке экрана
    'viewcredits': [('frontend.p3d', 'CreditsBgd.pag'), ('frontend.p3d', 'ViewCredits.pag'), ('frontend.p3d', 'TVFrame.pag'),
                    ('frontend.p3d', 'Back.pag')],
    # ViewMovies.scr: рамка, доска BigBoard, 9 строк роликов (IntroMovie, Movie1..8), Back / Accept
    'viewmovies': [('frontend.p3d', 'TVFrame.pag'), ('frontend.p3d', 'BigBoard.pag'), ('frontend.p3d', 'ViewMovies.pag'),
                   ('frontend.p3d', 'Back.pag'), ('frontend.p3d', 'Accept.pag')],
    'pauseoptions': [BG, ('ingame.p3d', 'XSmallBoard.pag'), ('ingame.p3d', 'PauseFgd.pag'), ('ingame.p3d', 'PauseOptions.pag'),
                     ('ingame.p3d', 'Back.pag'), ('ingame.p3d', 'Accept.pag')],
    'pausesound': [BG, ('ingame.p3d', 'BigBoard.pag'), ('ingame.p3d', 'PauseFgd.pag'), ('ingame.p3d', 'Sound.pag'),
                   ('ingame.p3d', 'Back.pag')],
    'pausesettings': [BG, ('ingame.p3d', 'BigBoard.pag'), ('ingame.p3d', 'PauseFgd.pag'), ('ingame.p3d', 'PauseSettings.pag'),
                      ('ingame.p3d', 'Back.pag')],
}
for _sc in SUBPAGES:
    PAGES[_sc] = list(PAGES['pausesettings'])
if SETTINGS_PAGES:
    PAGES['pausesettings'] = list(PAGES['pauseoptions'])
PAGES['viewcards'] = U.SCREEN_PAGES['viewcards']; PAGES['missionselect'] = U.SCREEN_PAGES['missionselect']
MENU_PAGES = {'Options.pag', 'Sound.pag', 'PauseOptions.pag', 'PauseSettings.pag', 'ViewMovies.pag'}
HIDE_ROWS = {'Display', 'DisplayMode', 'Controller', 'SurroundSound', 'Vibration'}
MENU_TOP = os.environ.get('MENU_TOP') == '1'   # красный: OPTIONS главного меню прижаты к верху, как до 03.10
MAX_ROWS = 12
TAGS = {sc: {'ALWAYS'} for sc in PAGES}
TAGS['viewcards'] = {'ALWAYS', 'SKIP'}
TAGS['viewcredits'] = {'ALWAYS', 'SKIP', 'STREETRACE'}   # в коде — по проходам (uiScrDrawCredits)   # сетка карточек видна сразу (UI_T_VC_GRID); крупная карточка — STREETRACE (UI_T_VC_ZOOM)


def row_key(it):
    """строка меню элемента: имя группы под Foreground/Menu или имя текста прямо в Menu; None — не строка"""
    if it['page'] not in MENU_PAGES:
        return None
    p = it['path'].split('/')
    if 'Menu' not in p:
        return None
    rest = p[p.index('Menu') + 1:]
    return rest[0] if rest else it['name']


def rows(items):
    """{ключ строки: номер} сверху вниз по подписи строки (у группы без подписи — по верху)"""
    tops = {}
    for it in items:
        k = row_key(it)
        if k is None or k in HIDE_ROWS:
            continue
        y = it['rect'][1] + (0 if it['type'] == U.T_TEXT and it['name'] == k else 1000)
        tops[k] = min(tops.get(k, 1e9), y)
    order = sorted(tops, key=lambda k: tops[k])
    assert len(order) <= MAX_ROWS, order
    return {k: i for i, k in enumerate(order)}


def compact(its, rk, center=False):
    """скрытые строки не оставляют дыр: видимые строки встают на места строк сверху вниз, начиная с первой видимой.
    center (OPTIONS главного меню, отзыв Роба 26.09 и аудит 151: «3 пункта жмутся к верху ТВ-рамки, низ пустой»): столбец
    видимых строк — посередине места всех строк оригинала (у PSP скрыты Display, Controller, SurroundSound; правило порта)"""
    tops = {}
    for it in its:
        k = row_key(it)
        if k is not None:
            tops[k] = min(tops.get(k, 1e9), it['rect'][1])
    slots = sorted(tops.values())
    vis = sorted((k for k in tops if k in rk), key=lambda k: tops[k])
    if not vis:
        return
    first = slots.index(tops[vis[0]])
    down = (slots[-1] - slots[first + len(vis) - 1]) / 2 if center and not MENU_TOP else 0
    shift = {k: slots[first + i] + down - tops[k] for i, k in enumerate(vis)}
    for it in its:
        k = row_key(it)
        if k in shift and shift[k]:
            it['rect'] = [it['rect'][0], it['rect'][1] + shift[k]] + list(it['rect'][2:])


def stretched(assets, name, w, h):
    """картинка ползунка на всю рамку Scrooby (дорожка = рамка; значок ходит по ней): правило порта до сверки с PS2"""
    key = '%s_track%dx%d' % (name, round(w), round(h))
    if key not in assets.images:
        assets.images[key] = assets.images[name].resize((max(1, round(w)), max(1, round(h))), U.Image.LANCZOS)
    return key


# Правка Роба 144 (01.10, «насыщенность — в настройках отображения, чтобы можно было донастроить»): строка Colour в Settings
# паузы. ПРАВИЛО ПОРТА: в данных настройки насыщенности нет (библия srr2.p3d: DISPLAY / Display Mode / Colour Depth — ПК),
# поэтому строка встаёт на место скрытой Vibration (у PSP вибрации нет): та же раскладка, стрелки и шрифт из PauseSettings.pag,
# подпись и значения — порта: насыщенность 1.00 .. 1.60 шагом 0.04 (viewer/tone.h TONE_SAT_*; значение строки = номер).
# MENU_NO_COLOUR=1 — подброска гейта tests/tone/run.sh: пакет без строки (как до 01.10)
COLOUR_ROW = os.environ.get('MENU_NO_COLOUR') != '1'
COLOUR_VALUES = ['%.2f' % (1.0 + 0.04 * i) for i in range(16)]


def colour_row(its):
    """Vibration -> Colour (группа строки целиком: подпись, _Value, стрелки)"""
    for it in its:
        if it['page'] != 'PauseSettings.pag' or row_key(it) != 'Vibration':
            continue
        it['path'] = it['path'].replace('/Vibration', '/Colour')
        it['name'] = it['name'].replace('Vibration', 'Colour')
        if it['type'] == U.T_TEXT and it['name'] == 'Colour':
            it['strings'] = ['Colour' if os.environ.get('SATURATION_LABEL_OLD') == '1' else 'Saturation']   # №753 (Роб 17.10): строка насыщенности подписана прямо
        elif it['type'] == U.T_TEXT and it['name'] == 'Colour_Value':
            it['strings'] = list(COLOUR_VALUES)


# №428 (Роб 16.10, «мультик»): строка Cartoon в Settings паузы ПОСЛЕДНЯЯ, после Tutorial (Дирижёр 16.10: раскладка оригинального меню не сдвигается,
# сценарии со стрелками считают прежние строки): копия группы Tutorial на шаг ниже (подпись, _Value Off / On, стрелки; шаг = Tutorial - Vibration),
# подпись порта; значение viewer/main_save.h cfgSlot "CARTOON", по умолчанию Off. Серая Jump Cameras остаётся как в оригинале.
# MENU_NO_TOON=1 — пакет без строки (как до правки; откат на стороне движка — SHAR_TOON_RELEASE_OLD=1)
TOON_ROW = os.environ.get('MENU_NO_TOON') != '1'


def toon_row(its):
    """Tutorial -> копия Cartoon на следующем месте (группа строки целиком)"""
    import copy
    src = [it for it in its if it['page'] == 'PauseSettings.pag' and row_key(it) == 'Tutorial']
    vib = [it for it in its if it['page'] == 'PauseSettings.pag' and row_key(it) in ('Vibration', 'Colour') and it['type'] == U.T_TEXT]
    tut = [it for it in src if it['type'] == U.T_TEXT and it['name'] == 'Tutorial']
    if not src or not vib or not tut:
        return
    pitch = tut[0]['rect'][1] - vib[0]['rect'][1]
    for it in src:
        c = copy.deepcopy(it)
        c['path'] = c['path'].replace('/Tutorial', '/Cartoon')
        c['name'] = c['name'].replace('Tutorial', 'Cartoon')
        c['rect'] = [c['rect'][0], c['rect'][1] + pitch] + list(c['rect'][2:])
        if c['type'] == U.T_TEXT and c['name'] == 'Cartoon':
            c['strings'] = ['Cartoon']
        its.append(c)


# Роб 17.10 (после TEST32): строка Tree Shadows в Settings паузы — тени деревьев на земле L1/L2 Off / On («чтобы можно было оптимизировать игру»);
# копия группы Tutorial на два шага ниже (после Cartoon), значение viewer/main_save.h cfgSlot "TREE", по умолчанию Off. MENU_NO_TREE=1 — без строки
TREE_ROW = os.environ.get('MENU_NO_TREE') != '1'


def tree_row(its):
    """Tutorial -> копия Tree Shadows через строку (после Cartoon)"""
    import copy
    src = [it for it in its if it['page'] == 'PauseSettings.pag' and row_key(it) == 'Tutorial']
    vib = [it for it in its if it['page'] == 'PauseSettings.pag' and row_key(it) in ('Vibration', 'Colour') and it['type'] == U.T_TEXT]
    tut = [it for it in src if it['type'] == U.T_TEXT and it['name'] == 'Tutorial']
    if not src or not vib or not tut:
        return
    pitch = tut[0]['rect'][1] - vib[0]['rect'][1]
    for it in src:
        c = copy.deepcopy(it)
        c['path'] = c['path'].replace('/Tutorial', '/TreeShadows')
        c['name'] = c['name'].replace('Tutorial', 'TreeShadows')
        c['rect'] = [c['rect'][0], c['rect'][1] + 2 * pitch] + list(c['rect'][2:])
        if c['type'] == U.T_TEXT and c['name'] == 'TreeShadows':
            c['strings'] = ['Tree Shadows']
        its.append(c)


# №777: новые строки подстраниц — копии группы строки Colour (подпись, _Value, стрелки), подпись и значения порта; значения — номер варианта
# (viewer/main_save.h cfgSlot: CONTRAST, LINE, EYE, TEXTURE). Контраст 0.50..2.00 шагом 0.10 (1.00 — как было), толщина линии 0..10, глаза Off / Soft / Full,
# доля родной текстуры в мультике 0..100 % шагом 5
# №996 (Роб 26.10: «настройка высоты и дальности камеры в меню, и в машине, и пешком»): на странице CAMERA две строки — Height и Distance, по 5 шагов, середина (Normal) — как было.
# CAMCFG996_OLD=1 — страница CAMERA как в TEST70 (3 строки)
CAMCFG996_OLD = os.environ.get('CAMCFG996_OLD') == '1'
# №1003 (Роб 26.10: «тени из-под машин убрать — мерцают; в меню тогл: тени деревьев и тени машин»): строка Car Shadows на странице PICTURE после Tree Shadows. CARSHADOW1003_OLD=1 — как TEST71
CARSHADOW1003_OLD = os.environ.get('CARSHADOW1003_OLD') == '1'
# №1064 (Роб 28.10: «текстуру бордюра поставить родную и иметь возможность это настроить в настройках»): строка Curbs на странице PICTURE после Draw Distance (Original / Tinted). CURB1064_OLD=1 — страница PICTURE как в TEST76
CURB1064_OLD = os.environ.get('CURB1064_OLD') == '1'
NEW_ROWS = {
    'Contrast': ('Contrast', ['%.2f' % (0.5 + 0.1 * i) for i in range(16)]),
    'LineWidth': ('Line Width', [str(i) for i in range(11)]),
    'EyeLines': ('Eye Lines', ['Off', 'Soft', 'Full']),
    'InkStyle': ('Ink', ['Indigo', 'Black', 'Tone']),   # №951: viewer/main_save.h cfgSlot "INK" — цвет контура (skin.h skinInkAbgr)
    'TexDetail': ('Texture Detail', ['%d%%' % (5 * i) for i in range(21)]),
    'Textures': ('Textures', ['Off', 'On']),   # №912: viewer/main_save.h cfgSlot "TEXTURES" — Off: мир без текстур (заливка), On: родные текстуры
    'DrawDist': ('Draw Distance', ['150 M', '200 M', '250 M']),   # №795: viewer/main_save.h cfgSlot "DRAW"
    **({} if CURB1064_OLD else {'CurbStyle': ('Curbs', ['Original', 'Tinted'])}),   # №1064: viewer/main_save.h cfgSlot "CURB" — бордюр: Original — родной, Tinted — подкраска №987 (TEST76)
    'OldTV': ('Old TV', ['Off'] + ['%d%%' % (10 * i) for i in range(1, 11)]),   # №800/№830: сила «старого телевизора» (viewer/main_save.h cfgSlot "OLD", viewer/oldtv.h)
    **({} if CAMCFG996_OLD else {
        'CamHeight': ('Height', ['Low', 'Lower', 'Normal', 'Higher', 'High']),      # №996: viewer/main_save.h cfgSlot "HEIGHT" — высота камеры (машина и пешком), середина — как было
        'CamDist': ('Distance', ['Near', 'Nearer', 'Normal', 'Farther', 'Far']),    # №996: cfgSlot "DISTANCE" — дальность камеры
    }),
    **({} if CARSHADOW1003_OLD else {'CarShadows': ('Car Shadows', ['Off', 'On'])}),   # №1003: viewer/main_save.h cfgSlot "CAR SHADOW" — тени машин, по умолчанию Off
}
# строки подстраниц сверху вниз (ключи групп PauseSettings.pag и копий порта)
# №783 (Роб 18.10 после TEST34): включение мультика — на странице CARTOON первой строкой («в Cartoon строго настройки мультика, в том числе
# вкл/выкл»), PICTURE — Saturation, Contrast, Tree Shadows. CARTOON_PAGE_OLD=1 — прежнее (Cartoon в PICTURE)
CARTOON_PAGE_OLD = os.environ.get('CARTOON_PAGE_OLD') == '1'
# №783 (Роб 18.10, следом): «в OPTIONS сразу SOUND, CAMERA, GAME, PICTURE; внутри PICTURE — CARTOON, внутри CARTOON под вкл/выкл — его настройки»:
# пункты OPTIONS паузы — SOUND / CAMERA / GAME / PICTURE (раздел SETTINGS не нужен; экран 2 в пакете остаётся, движок туда не ходит),
# на PICTURE последней строкой — переход «Cartoon» (одна подпись, без значения: ✕ открывает страницу CARTOON). MENU_FLAT_OLD=1 — раздел SETTINGS, как TEST34
MENU_FLAT_OLD = os.environ.get('MENU_FLAT_OLD') == '1'
# №890 (Роб 23.10: «Picture поставить первым пунктом в Options») и №891 («внутри Picture пункт Cartoon первым; видно, что это подменю, в которое провалишься — стрелка ›
# справа, чтобы не путали со строкой-настройкой»): Picture — первый пункт OPTIONS паузы (движок ищет пункты по подписи, порядок ему не важен), Cartoon — первая строка
# PICTURE, справа у неё постоянная «>» (грань Bart, как значения строк; у настроек справа значения и ◄ ► только у выбранной). OPT_PICTURE_FIRST_OLD=1 /
# PICTURE_CARTOON_FIRST_OLD=1 — прежние порядки; LINK_CHEV_OLD=1 — без «>»
OPT_PICTURE_FIRST = os.environ.get('OPT_PICTURE_FIRST_OLD') != '1'
CARTOON_FIRST = os.environ.get('PICTURE_CARTOON_FIRST_OLD') != '1'
LINK_CHEV = os.environ.get('LINK_CHEV_OLD') != '1'
OPT_ROWS = ([('Picture', 'PICTURE')] if OPT_PICTURE_FIRST else []) + [('Sound', 'SOUND'), ('Camera', 'CAMERA'), ('Game', 'GAME')] \
           + ([] if OPT_PICTURE_FIRST else [('Picture', 'PICTURE')])
_LINK_ROW = [] if CARTOON_PAGE_OLD or MENU_FLAT_OLD else ['CartoonLink']
PAGE_ROWS = {
    'pausecamera': ['Camera', 'JumpCamera', 'InvertCamControl'] + ([] if CAMCFG996_OLD else ['CamHeight', 'CamDist']),
    'pausegame': ['IntersectNavSystem', 'Radar', 'Tutorial'],
    'pausepicture': (_LINK_ROW if CARTOON_FIRST else []) + (['Textures'] if os.environ.get('TEXTURES_ROW_OLD') != '1' else []) + ['Colour', 'Contrast', 'TreeShadows'] + ([] if CARSHADOW1003_OLD else ['CarShadows'])
                    + ([] if os.environ.get('DRAWDIST_ROW_OLD') == '1' else ['DrawDist']) + ([] if CURB1064_OLD else ['CurbStyle'])
                    + (['Cartoon'] if CARTOON_PAGE_OLD else ([] if CARTOON_FIRST else _LINK_ROW)),
    'pausecartoon': ([] if CARTOON_PAGE_OLD else ['Cartoon']) + ['LineWidth', 'EyeLines'] + ([] if os.environ.get('INK951_ROW_OLD') == '1' else ['InkStyle']) + ['TexDetail']
                    + ([] if os.environ.get('OLDTV_ROW_OLD') == '1' else ['OldTV']),   # №800: строка Old TV
}
# пункты раздела SETTINGS (экран 2): копии пункта Sound доски OPTIONS паузы, шаг пунктов из данных (Sound -> Settings)
HUB_ROWS = [('Camera', 'CAMERA'), ('Game', 'GAME'), ('Picture', 'PICTURE'), ('Cartoon', 'CARTOON')]


def copy_row(its, src, key, label, values):
    """группа строки src страницы PauseSettings.pag -> копия key с подписью label и вариантами values"""
    import copy
    for it in [it for it in its if it['page'] == 'PauseSettings.pag' and row_key(it) == src]:
        c = copy.deepcopy(it)
        c['path'] = c['path'].replace('/' + src, '/' + key)
        c['name'] = c['name'].replace(src, key)
        if c['type'] == U.T_TEXT and c['name'] == key:
            c['strings'] = [label]
        elif c['type'] == U.T_TEXT and c['name'] == key + '_Value':
            c['strings'] = list(values)
        its.append(c)


def page_rows(its, order):
    """подстраница: только строки order и в этом порядке — на места строк страницы сверху (шаг данных); прочие элементы как есть"""
    tops = {}
    for it in its:
        k = row_key(it)
        if k is not None and it['type'] == U.T_TEXT and it['name'] == k:
            tops[k] = it['rect'][1]
    slots = sorted(set(tops.values()))
    top0, pitch = slots[0], slots[1] - slots[0]
    out = []
    for it in its:
        k = row_key(it)
        if k is None:
            out.append(it)
        elif k in order:
            dy = top0 + order.index(k) * pitch - tops[k]
            it['rect'] = [it['rect'][0], it['rect'][1] + dy] + list(it['rect'][2:])
            out.append(it)
    return out


def hub_rows(its, rows_=None):
    """раздел SETTINGS: пункты CAMERA / GAME / PICTURE / CARTOON вместо пунктов OPTIONS (копии пункта Sound, шаг данных); rows_ — свои пункты (OPTIONS №783)"""
    import copy
    opt = [it for it in its if it['page'] == 'PauseOptions.pag' and row_key(it) is not None]
    lab = {it['name']: it['rect'][1] for it in opt if it['type'] == U.T_TEXT and it['name'] == row_key(it)}
    tpl = [it for it in opt if row_key(it) == 'Sound']
    pitch = lab['Settings'] - lab['Sound']
    out = [it for it in its if not (it['page'] == 'PauseOptions.pag' and row_key(it) is not None)]
    for i, (key, text) in enumerate(rows_ or HUB_ROWS):
        for it in tpl:
            c = copy.deepcopy(it)
            c['path'] = c['path'].replace('/Sound', '/' + key)
            c['name'] = c['name'].replace('Sound', key)
            c['rect'] = [c['rect'][0], c['rect'][1] + (i - 1) * pitch] + list(c['rect'][2:])   # первый пункт — на месте Sound минус шаг (как Controller)
            if c['type'] == U.T_TEXT and c['name'] == key:
                c['strings'] = [text]
            out.append(c)
    return out


def link_chevron(its):
    """№891: «>» справа у строки-перехода Cartoon — по центру места правой стрелки строки Colour (там у настроек стоит ►), та же строка и высота, что у подписи"""
    import copy
    lab = next(it for it in its if it['type'] == U.T_TEXT and it['name'] == 'CartoonLink')
    ra = next(it for it in its if it['page'] == 'PauseSettings.pag' and it['name'] == 'Colour_RArrow')
    c = copy.deepcopy(lab)
    c['name'] = 'CartoonLink_Chev'; c['strings'] = ['>']
    cx = ra['rect'][0] + ra['rect'][2] / 2
    cy = lab['rect'][1] + lab['rect'][3] / 2; hh = 24 / U.S   # грань boulder_16 1:1 (строка 19 px): жирнее и крупнее Bart подписей, «>» читается; по центру строки подписи
    c['rect'] = [cx - 20, cy - hh / 2, 40, hh]
    c.update(halign='centre', valign='centre', wrap=False, font=U.ITEM_FACE)
    return c


def items(sc, assets):
    its, _ = U.build_items(sc)
    settings_rows = sc == 'pausesettings' and not SETTINGS_PAGES or sc in SUBPAGES   # страница(ы) со строками значений
    if settings_rows and COLOUR_ROW:
        colour_row(its)
    if settings_rows and TOON_ROW:
        toon_row(its)
    if settings_rows and TREE_ROW:
        tree_row(its)
    if sc in SUBPAGES:
        for key, (label, values) in NEW_ROWS.items():
            copy_row(its, 'Colour', key, label, values)
        if 'CartoonLink' in PAGE_ROWS[sc]:   # №783: переход на страницу CARTOON — только подпись строки (без значения, стрелок и ползунка)
            copy_row(its, 'Colour', 'CartoonLink', 'Cartoon', [''])
            its = [it for it in its if row_key(it) != 'CartoonLink' or (it['type'] == U.T_TEXT and it['name'] == 'CartoonLink')]
            if LINK_CHEV:
                its.append(link_chevron(its))
        its = page_rows(its, PAGE_ROWS[sc])
    if sc == 'pausesettings' and SETTINGS_PAGES:
        its = hub_rows(its)
    if sc == 'pauseoptions' and SETTINGS_PAGES and not MENU_FLAT_OLD:   # №783: SOUND / CAMERA / GAME / PICTURE сразу в OPTIONS
        its = hub_rows(its, OPT_ROWS)
    rk = rows(its)
    compact(its, rk, center=sc == 'options')
    track = {}
    for it in its:                                                        # дорожка ползунка строки = рамка _Slider
        k = row_key(it)
        if k in rk and it['type'] == U.T_IMG and it['name'].endswith('_Slider'):
            track[k] = it['rect']
    out = []
    for it in its:
        k = row_key(it)
        if k is not None and k in HIDE_ROWS:
            continue
        if it['page'] == 'Cheats.pag':
            continue
        if k is not None:
            r = rk[k]; nm = it['name']
            if it['type'] == U.T_TEXT and nm == k:                        # подпись строки = пункт меню
                it.update(menu=r, sel=0)
            elif it['type'] == U.T_TEXT and nm.endswith('_Value'):        # варианты: строка по значению ROW<r>
                it.update(field='ROW%d' % r, fmt='select', sel=0)
            elif it['type'] == U.T_IMG and ('Arrow' in nm):               # ◄ ► только у выбранной строки
                it.update(menu=r, selonly=True)
            elif it['type'] == U.T_IMG and nm.endswith('_Slider'):        # ползунок: заполнение по ROW<r> (0..1000)
                x, y, w, h = it['rect']
                it = dict(it, type=U.T_BAR, image=stretched(assets, it['frames'][0], w, h), field='ROW%d' % r)
                it.pop('frames', None)
            elif it['type'] == U.T_IMG and nm.endswith('_SliderBgd'):
                x, y, w, h = it['rect']
                it = dict(it, frames=[stretched(assets, it['frames'][0], w, h)])
            elif it['type'] == U.T_IMG and nm.endswith('_Icon') and k in track:   # значок = бегунок: по дорожке
                it = dict(it, slide=r + 1, track=track[k][2] * U.S)
            elif it['type'] == U.T_IMG and nm.endswith('_Off'):           # «звук выключен» показывает код при нуле
                continue
        out.append(it)
    return out


# Слово Роба 11.10: пункт Credits меню настроек — после титров оригинала блоки порта, шрифтом Bart vs. the Space Mutants (16bit): кто сделал порт, код, шрифты (CC BY 3.0 требует указания автора),
# открытые проекты, правообладатели. Заголовок крупнее (×2), строки родным размером; строки блока разделены «\\» (viewer/ui.c uiScrDrawCredits). CREDITS_PORT_OLD=1 — только титры оригинала.
# Список — по лицензиям репозитория: donut (GPL-3.0, README), hit-and-run-web tools (MIT, CLAUDE.md), PSPSDK (BSD 3-clause, viewer/bootmsx.h), atracdenc (LGPL, tools/snd0.py)
PORT_CREDITS = [
    ('PSP PORT', ['Rob Vagin', 'port and art direction']),
    ('CODE', ['Claude (Anthropic)', 'Codex (OpenAI)']),
    ('FONTS', ['Patrick H. Lauke', 'Bart vs. the Space Mutants (16bit)', 'The Simpsons', 'CC BY 3.0, FontStruct']),
    ('THANKS', ['The Simpsons: Hit & Run Wiki', 'fan community', 'simpsons-hit-and-run', '.fandom.com']),   # Роб 17.10: фан-вики — источник фактов об оригинале (будка, ремонт, остов)
    ('OPEN SOURCE', ['donut (GPL-3.0)', 'hit-and-run-web tools (MIT)', 'PSPSDK (BSD)', 'atracdenc (LGPL)']),
    ('HIT & RUN', ['The Simpsons: Hit & Run', '(c) Radical Entertainment,', 'Vivendi Universal Games,', '20th Century Fox', 'Unofficial fan port, not affiliated', 'with the rights holders']),
]


CREDITS_MAX_CHARS = 29   # (рамка блока 249 px − поля по 8) / 8 px знака шрифта Bart: ни одна строка не выходит за рамку


def wrap_credit(line, cap=CREDITS_MAX_CHARS):
    """строка по словам не шире cap знаков; на две строки — по самой ровной точке (без висячего слова)"""
    if len(line) <= cap:
        return [line]
    words = line.split(' ')
    best = None
    for k in range(1, len(words)):
        a, b = ' '.join(words[:k]), ' '.join(words[k:])
        if len(a) <= cap and len(b) <= cap and (best is None or abs(len(a) - len(b)) < abs(len(best[0]) - len(best[1]))): best = (a, b)
    if best:
        return list(best)
    out, cur = [], ''
    for w in words:
        t = (cur + ' ' + w) if cur else w
        if len(t) <= cap or not cur: cur = t
        else: out.append(cur); cur = w
    out.append(cur)
    return out


def port_credits(last):
    import copy
    out = []
    for k, (head, lines) in enumerate(PORT_CREDITS):
        for part, (face, text) in enumerate((('bart_16bit*2', ('\\' if part_gap(k) else '') + head), ('bart_16bit@1', '\\'.join(x for ln in lines for x in wrap_credit(ln))))):
            it = copy.deepcopy(last); it['name'] = 'CreditsPort%d_%d' % (k, part)
            it.update(font=face, strings=[text], text=text, subset=True, tag='SKIP')
            out.append(it)
    return out


def part_gap(k):
    return True       # пустая строка перед заголовком блока (воздух между блоками)


def credits(its):
    """VIEW CREDITS: метки для viewer/ui.c uiScrDrawCredits — фон ALWAYS, блоки титров SKIP по номеру (Credits0..31: в данных
    они идут с конца), рамка и Back — STREETRACE (рисуются поверх титров)"""
    import re
    # №880: NTSC ViewCredits.pag — 31 блок (Credits0..30) и лишние Top/Bottom: берутся только Credits<номер>, 31 или 32 (движок читает число из данных)
    cr = sorted((it for it in its if it['page'] == 'ViewCredits.pag' and re.fullmatch(r'Credits\d+', it['name'])), key=lambda it: int(it['name'][len('Credits'):]))
    for it in cr:
        it['tag'] = 'SKIP'
    assert [it['name'] for it in cr] == ['Credits%d' % i for i in range(len(cr))] and len(cr) in (31, 32), [it['name'] for it in cr]
    if os.environ.get('CREDITS_PORT_OLD') != '1' and cr:
        cr = cr + port_credits(cr[-1])
    out, done = [], False
    for it in its:
        if it['page'] == 'ViewCredits.pag':
            if not done: out += cr; done = True
            continue
        if it['page'] in ('TVFrame.pag', 'Back.pag'):
            it['tag'] = 'STREETRACE'
        out.append(it)
    return out


# Правки Роба 150/151 (01.10): OPTIONS паузы — вёрстка как у паузы (tools/ui_export.py board_fit): заголовок над доской (строка
# пункта паузы, как «PAUSE MENU»), доска по содержимому с ровными полями, рамка той же толщины (x PAUSE_K), подсказки кнопок —
# в правом нижнем углу одной строкой. OPTIONS — та же колонка, что пауза (XSmallBoard, шрифт и шаг пунктов, ширина семейства);
# подписи ползунков SOUND — грань пунктов паузы (PAUSE_FONT), не мельче остального семейства;
# SOUND и SETTINGS (BigBoard) — строки из данных (подпись, значение, стрелки, ползунок) одним блоком по центру, шаг ровный:
# у SOUND — из данных (37 px), у SETTINGS — PAUSE_SETTINGS_PITCH (в данных 40 источника = 22,7 px: 22/23 вперемешку)
PAUSE_TITLES = {'pauseoptions': 'OPTIONS', 'pausesound': 'SOUND', 'pausesettings': 'SETTINGS',
                'pausecamera': 'CAMERA', 'pausegame': 'GAME', 'pausepicture': 'PICTURE', 'pausecartoon': 'CARTOON'}   # №777: заголовки подстраниц
HINT_IN_PAD = 8          # px PSP: подписи кнопок внутри доски VIEW MOVIES — от края заливки (правка 163)
PAUSE_SETTINGS_PITCH = 24
# Аудит 16.10 (Sonnet-Аудит-Типографики, v9): с 8-й строкой Cartoon доска по содержимому при шаге 24 выше экрана (40 + 8 x 24 + поля > 272): рамка уезжала
# (верх 88 вместо 44, низ за кадром). Шаг при 8 строках 22 (PAUSE_PITCH): низ рамки в экране, верх на месте TEST26. SETTINGS8_FRAME_OLD=1 — прежний шаг 24
PAUSE_SETTINGS_PITCH8 = 22
PAUSE_SETTINGS_PITCH9 = 20   # Роб 17.10: 9-я строка Tree Shadows — шаг 20, доска в экране (40 + 9 x 20 + поля)


def pause_fit(sc, its, assets):
    rk = rows(its)
    grp = {}
    for it in its:
        k = row_key(it)
        if k in rk:
            grp.setdefault(rk[k], []).append(it)
    rws = [grp[i] for i in sorted(grp)]
    if sc == 'pauseoptions' or (sc == 'pausesettings' and SETTINGS_PAGES):   # №777: раздел SETTINGS — доска пунктов, как OPTIONS
        for r in rws:
            for it in r:
                if it['type'] == U.T_TEXT:
                    it.update(font=U.PAUSE_FONT, halign='centre', valign='top', wrap=False)
        # №807 (Роб 18.10: «отступы сверху и снизу одинаковые, строки свободнее»): пункты OPTIONS (SOUND / CAMERA / GAME / PICTURE) — шаг 30 px вместо 26
        # (просвет между заглавными 20 px), поля сверху и снизу равные (board_fit). OPTIONS_PITCH_OLD=1 — шаг паузы 26
        pitch_ = U.PAUSE_FIT_PITCH + (4 if sc == 'pauseoptions' and SETTINGS_PAGES and not MENU_FLAT_OLD and os.environ.get('OPTIONS_PITCH_OLD') != '1' else 0)
        fb = U.board_fit(its, rws, 'XSmallBoard.pag', U.PAUSE_K, pitch=pitch_, width=U.pause_board_width(), centre_each=True,
                          images=assets.images)
    else:
        if sc == 'pausesound':   # подписи ползунков (boulder_16 данных: заглавная ~6 px на PSP, мельче всего семейства) — грань пунктов паузы
            for r in rws:
                for it in r:
                    if it['type'] == U.T_TEXT and it.get('menu', -1) >= 0:
                        it.update(font=U.PAUSE_FONT)
        # Роб 17.10 (после TEST32): «в настройках графики везде шрифты актуальные — наш Bart vs the Space Mutants»: строки SETTINGS паузы (подписи и значения:
        # Camera, Radar, Saturation, Tutorial, Cartoon, Tree Shadows, Off/On, Auto, 1.36) — грань Bart, как подсказки кнопок; в панели Роба группы у этих строк нет
        # (раньше — шрифт данных диска). SETTINGS_BART_OLD=1 — как было
        vpage = sc == 'pausesettings' or sc in SUBPAGES   # №777: подстраницы — вёрстка и шрифт прежней SETTINGS
        if vpage and os.environ.get('SETTINGS_BART_OLD') != '1':
            for r in rws:
                for it in r:
                    if it['type'] == U.T_TEXT and not it['name'].endswith('_Chev'):   # №891: «>» перехода — своя грань (link_chevron)
                        it.update(font='bart_16bit@1')
        # правка 163: SOUND — колонка, как OPTIONS: та же постоянная ширина доски, что у семейства паузы (рамка при переходе
        # OPTIONS -> SOUND не прыгает); SETTINGS (две колонки) — по содержимому, поля со всех сторон PAUSE_FIT_PAD_Y: правый край
        # доски левее подсказки «△ Back» в углу (подсказка крупнее, правка 163)
        sp = PAUSE_SETTINGS_PITCH8 if len(rws) >= 8 and os.environ.get('SETTINGS8_FRAME_OLD') != '1' else PAUSE_SETTINGS_PITCH
        if len(rws) >= 9: sp = PAUSE_SETTINGS_PITCH9
        fb = U.board_fit(its, rws, 'BigBoard.pag', U.PAUSE_K, pitch=sp if vpage else None,
                          width=U.pause_board_width() if sc == 'pausesound' and not U.UI_DENSE else None,
                          pad_x=U.PAUSE_FIT_PAD_Y if vpage and not U.UI_DENSE else U.PAUSE_FIT_PAD_X,
                          images=assets.images)
    base = next(it for it in its if it['type'] == U.T_TEXT and it.get('menu', -1) == 0)
    title = U.board_title(base, (None if sc in SUBPAGES else U._BIBLE(PAUSE_TITLES[sc])) or PAUSE_TITLES[sc], fb)   # №777: заголовки подстраниц — подписи порта заглавными (в библии «Camera» строчными)
    title.update(unit=('BoardTitle', sc), forced='C')
    U.hints_to_corner(its)
    return its + [title]


# Правка 163: пункты OPTIONS главного меню (SOUND, VIEW MOVIES, VIEW CREDITS) и подписи ползунков SOUND — грань пунктов паузы
# U.ITEM_FACE (boulder_16 1:1 + трекинг), как у паузы: было boulder_24 / boulder_16 x 0,57 — пересэмпл с утолщением, буквы
# слипались. Места строк — из данных; подпись ползунка SOUND — над дорожкой: низ букв не ниже FE_LABEL_GAP px над её верхом
# (крупная подпись с прежнего верха налезала бы на дорожку). Список VIEW MOVIES (9 строк шагом 17 px внутри доски) — прежней
# гранью данных: крупной грани там тесно (просвет 6 px, нижняя строка у подсказок), его вёрстка — отдельный шаг
FE_ITEMS = ('options', 'sound')
FE_LABEL_GAP = 4


def fe_items(sc, its):
    if U.UI_DENSE or sc not in FE_ITEMS:
        return
    for it in its:
        if it['type'] != U.T_TEXT or it.get('menu', -1) < 0:
            continue
        it['font'] = U.ITEM_FACE
        bgd = next((t for t in its if t['name'] == it['name'] + '_SliderBgd'), None)
        if sc == 'sound' and bgd is not None:
            ink_bottom = max(U.glyph_ink(U.ITEM_FACE, s)[3] for s in it['strings'] if s)
            top = (bgd['rect'][1] * U.S - FE_LABEL_GAP - ink_bottom) / U.S
            if top < it['rect'][1]:
                it['rect'] = [it['rect'][0], top] + list(it['rect'][2:])


def build(sc, assets, fonts):
    its = U.build_items(sc)[0] if sc in ('viewcards', 'missionselect') else items(sc, assets)   # готовая раскладка ui_export
    fe_items(sc, its)
    if sc in PAUSE_TITLES and not U.PAUSE_OLD:
        its = pause_fit(sc, its, assets)
    if sc == 'viewcredits':
        its = credits(its)
    if sc == 'viewmovies':   # подсказки Back / Accept в оригинале лежат внутри доски BigBoard (4:3: доска 50..587, подсказки 435..570)
        for it in its:       # — едут с ней по центру (C), а не к правому краю экрана (R вылезал на рамку доски)
            if it['page'] in ('Back.pag', 'Accept.pag'):
                it['forced'] = 'C'
        if not U.UI_DENSE:   # правка 163: подписи крупнее (HINT_FACE) — значки и подписи левее: самая длинная подпись кончается
            fill = next(it['rect'] for it in its if it['page'] == 'BigBoard.pag' and it['name'] == 'Frame_Bgd')   # в заливке доски
            hint = [it for it in its if it['page'] in ('Back.pag', 'Accept.pag')]                                   # с полем HINT_IN_PAD
            r = max(it['rect'][0] + it['rect'][2] for it in hint if it['type'] == U.T_TEXT)
            dx = min(0.0, fill[0] + fill[2] - HINT_IN_PAD / U.S - r)
            for it in hint:
                it['rect'] = [it['rect'][0] + dx] + list(it['rect'][1:])
    dl, _ = U.adapt(its, {}, 'psp', assets, fonts)
    if sc == 'viewmovies' and os.environ.get('MOVIES_PITCH_OLD') != '1':
        # аудит UI 01.10 (замечание «VIEW MOVIES: шаг ~17, список плотный»), 11.10: шаг строк 20 (было 17), блок по середине доски
        rows = sorted((t for t in dl if t.get('page') == 'ViewMovies.pag' and t['type'] == U.T_TEXT), key=lambda t: t['dst'][1])
        bd = next((t for t in dl if t.get('name') == 'Frame_Bgd' and t.get('page') == 'BigBoard.pag'), None)
        if rows and bd is not None:
            step = 20; span = (len(rows) - 1) * step + rows[0]['dst'][3]; mid = bd['dst'][1] + bd['dst'][3] // 2 - 1
            y0 = mid - span // 2
            for i, t in enumerate(rows): t['dst'] = [t['dst'][0], y0 + i * step] + list(t['dst'][2:])
    if sc == 'missionselect':
        dl = U.missionselect_psp(dl, fonts)   # правка 348: шаг строк, полоса выбора во всю доску, LEVEL N по центру планки
    return dl


def group_of(key):
    b = key.split('@')[0]
    if b.startswith('tvframe'):
        return 'tv'
    return 'ui'


def export(name, out=None, mocks=True):
    for sc in PACKS[name]:
        U.SCREEN_PAGES[sc] = PAGES[sc]
    spec = dict(screens=PACKS[name], build=build, tags=TAGS, group=group_of, radar=False, groups=('tv', 'ui'),
                layout='layout_planted.json' if os.environ.get('MENU_OLD') else 'layout_%s.json' % name)
    res = U.pack(out or (U.ROOT / 'build' / ('%s.shr' % name)), spec)
    print('%s.shr: %d B (%.1f KB), экраны %s; текстуры %s' % (name, res['size'], res['size'] / 1024, ', '.join(PACKS[name]),
          ', '.join('%s %dx%d/%d' % (t['name'], t['w'], t['h'], t['rows']) for t in res['tex'])))
    if mocks:
        from PIL import Image
        U.OUTDIR.mkdir(parents=True, exist_ok=True)
        for si, sc in enumerate(PACKS[name]):
            dl = res['screens'][sc]
            st = {'show': TAGS[sc]}
            for r in range(MAX_ROWS):
                st['ROW%d' % r] = 700 if any(t['type'] == U.T_BAR and t['field'] == 'ROW%d' % r for t in dl) else 1
            bg = (40, 44, 90, 255) if name == 'menu' else (0, 0, 0, 255)
            img = U.render(dl, U.DST_W, U.DST_H, st, res['qbitmaps'], Image.new('RGBA', (U.DST_W, U.DST_H), bg), None, 0)
            p = U.OUTDIR / ('menu_%s.png' % sc); img.save(p)
            print('  макет', p.relative_to(U.ROOT), 'строк', max([t['menu'] for t in dl if t['menu'] >= 0] + [-1]) + 1)
    return res


if __name__ == '__main__':
    argv = sys.argv[1:]
    out = pk = None
    if '--out' in argv:
        i = argv.index('--out'); out = argv[i + 1]; del argv[i:i + 2]
    if '--pack' in argv:
        i = argv.index('--pack'); pk = argv[i + 1]; del argv[i:i + 2]
    for n in ([pk] if pk else list(PACKS)):
        export(n, out if pk else None, mocks=not out)
