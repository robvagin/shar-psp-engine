#!/usr/bin/env python3
"""Scrooby (SHAR frontend) -> native-widescreen PSP UI pack (stream C, plan stage 7).

Reads the original Scrooby projects from the owner's own disc
(hit-and-run-web/source/game/art/frontend/scrooby/*.p3d), catalogues the in-game HUD and the
pause / main menus, re-lays them out for the 480x272 PSP screen with ONE uniform scale and
edge anchors (never stretched), and writes the binary pack that viewer/ui.c draws with the GE.

    python tools/ui_export.py catalog   # <scratch>/catalog.md + catalog.json
    python tools/ui_export.py mock      # <scratch>/mock_<screen>.png  (4:3 stretched | anchored)
    python tools/ui_export.py pack      # build/ui/ui.pak (+ build/ui/layout.json, human readable)
    python tools/ui_export.py --out X   # pack -> X instead of build/<PAK_NAME> (tools/ship_rules.txt, ship.sh stage)
    python tools/ui_export.py all       # everything
    python tools/ui_export.py dump ingame.p3d Hud.pag ...   # raw element listing

Reads only: the disc (hit-and-run-web/source/game) and hit-and-run-web/tools/{p3d,textures,
convert}.py (MIT).  Nothing here comes from the leaked SHAR source.

ADAPTATION RULES (the contract; see also the report):
  R1  uniform scale S = 272/480 for every element (height based) -> aspect ratio always kept.
  R2  horizontal anchor per ANCHOR UNIT (a Scrooby group that must move together):
        L: x' = S*x            C: x' = S*x + (480 - 640*S)/2        R: x' = S*x + (480 - 640*S)
      default = the third of the 640-wide layout that holds the unit's bbox centre;
      explicit overrides in UNIT_ANCHOR (e.g. objective box + its icon = L, lap counter = L).
  R3  vertical: 480*S == 272 exactly, so top/middle/bottom anchors coincide; y' = S*y.
  R4  safe area: every unit is pushed inside [SAFE_X, 480-SAFE_X] x [SAFE_Y, 272-SAFE_Y]
      (the original TV-safe margins, scaled, already satisfy it for all HUD elements).
  R5  backgrounds: flat full-width polygons (overlays, covers, letterbox) extend to the full
      480 width (cover); the full-screen TV-frame bitmap is widened by a horizontal 3-slice
      (corners keep aspect, only the straight bezel between them is extended).
  R6  every bitmap is pre-resampled offline to its exact on-screen pixel size, so the GE draws
      it 1:1 with nearest filtering (crisp, no runtime minification); only rotated sprites
      (hit&run sectors) and the radar map use bilinear.
  R7  fonts are re-rasterised from the original bitmap fonts at the target size
      (scale = max(S, FONT_MIN_PX / line height)) -> small fonts stay legible, glyphs keep
      their aspect; stored as 4-bit alpha, coloured by vertex colour; shadows scaled by S
      (min 1 px).
"""
import json
import math
import os
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / 'hit-and-run-web'
sys.path.insert(0, str(WEB / 'tools'))
from p3d import read, walk, string  # noqa: E402  (MIT, hit-and-run-web)
from textures import decode_image  # noqa: E402

SCROOBY = WEB / 'source/game/art/frontend/scrooby'
SCRATCH = Path(os.environ.get('UI_SCRATCH', '/private/tmp/claude-501/-Users-USER-Downloads-shar-psp/'
                              '3f2ce002-62fd-4bf8-9553-98f95067c719/scratchpad/streams/ui'))
OUTDIR = ROOT / 'build/ui'           # layout.json (human readable); the pack itself -> build/<PAK_NAME>
PREFIX_GAP = 2                       # px between the coin icon and the coin count (ui.c: same)

SRC_W, SRC_H = 640, 480
DST_W, DST_H = 480, 272
S = DST_H / SRC_H                     # 0.5667
SAFE_X, SAFE_Y = 8, 6
FONT_MIN_PX = 9.0                     # minimum line height for re-rasterised fonts
MISSION_CAP_MIN_PX = 8                 # Роб 28.09: тексты миссий на экране PSP — заглавная H не меньше 8 px (читается),
MISSION_EFF_MAX = 1.33                 # ...но шрифт — свой (грань Scrooby та же) и не крупнее ~1.3 от простого масштаба 272/480
FAIL_CAP_MIN_PX = 9                  # №381: причина/подсказка/YES/NO читаемы на PSP
FAIL_EFF_MAX = 1.75                   # правило Роба: экран провала крупнее прежнего
LEVEL = int(os.environ.get('LEVEL', '1'))          # pause background + radar map (l<N>hudmap)
LANG = os.environ.get('LANG_BIBLE', 'E')             # srr2 text bible table: E F G I S
PAK_NAME = 'ui.shr' if LEVEL == 1 else 'ui_l%d.shr' % LEVEL      # -> build/<PAK_NAME> (next to level.shr)
MENU_FONT_MUL = 1.35                 # main menu: Rob 26.09 +20%, 27.09 ещё ~+12%: крупнее, ниже, по центру, рамка та же
# Правка Роба 136 (01.10) «шрифт главного меню — пиксель-перфект под разрешение PSP». Дробный масштаб грани (boulder_24 x
# 272/480 x 1.35 = 0.765) пересэмплировал каждую букву: штрихи разной толщины, неровные интервалы («Accep t»). Пиксель-перфект
# бывает только при целом масштабе: грань диска 1:1 (суффикс '@1' — FontCache берёт ячейку диска как есть, адванс и отступ
# целые с диска; на PSP — целое перо, спрайт 1:1, GU_NEAREST). Решено терминалом за Роба: пункт меню — boulder_16@1
# (заглавная 11 px, строка 19 px; ближе всего к нынешним 13 px: boulder_24@1 — 17 px, +31 %, boulder_16 x2 — 22 px).
# Вкус — константа (и UI_MENU_FACE=boulder_24@1 для снимка); UI_MENU_SOFT=1 — прежний дробный x1.35 (отрицательный
# контроль гейта tests/menu_font/run.sh). Подсказка «Accept» главного меню — Tt2001m__14@1 (заглавная 9 px вместо мыльных 6)
MENU_SOFT = bool(os.environ.get('UI_MENU_SOFT'))
MENU_FACE = os.environ.get('UI_MENU_FACE', 'boulder_16@1')   # правка 201 (Роб 03.10, снова 190): «разделы меню меньше, меньше меж букв» — 11 px, 1:1 (было boulder_24@1, 17 px)
MENU_HINT_FACE = os.environ.get('UI_MENU_HINT_FACE', 'swz721n_10@1')   # правка 233 (03.10: «Accept / Back всё ещё крупные — мельче»): Swiss 721 узкий диска 1:1, заглавная 7 px (была 9), «Accept» 38 px вместо 56
MENU_HINT_FACE_OLD = 'Tt2001m__14@1'   # правка 201: «Accept меньше» — тонкий Tt2001m 1:1, как у оригинала (было boulder_16@1)
COIN_ICON_K = 0.8                         # значок монеты у счёта: 0.8 высоты цифр, не выше родного размера картинки диска
MAIN_MENU_Y0, MAIN_MENU_PITCH = 132, 46   # source space (640x480, y down): first item top, item pitch (прежний список)
# pause (Rob 27.09: "too small, pinned to the top, items merge"): board ~1.9x the area, centred on 480x272,
# PSP pixels (x, y, w, h); frame corners/edges x PAUSE_K; title + menu in PAUSE_FONT (boulder_24 = the HUD's own
# face, so no extra glyph atlas: ~16 px lines instead of ~11) at PAUSE_PITCH px per line
PAUSE_BOARD = (120, 40, 240, 226)   # Роб 28.09: заголовок PAUSE MENU над рамкой -> рамка ниже
PAUSE_K, PAUSE_FONT, PAUSE_PITCH = 1.5, 'boulder_24', 22
PAUSE_TITLE_Y, PAUSE_ITEM0_DY = 21, 37               # centre of the title (PSP px from screen top, над рамкой) / first item below the board top
# Роб 28.09 (третий раз): «PAUSE MENU слишком мелкий — подбить под высоту, крупнее, не как пункт». Заглавная H заголовка —
# половина полосы над рамкой (от SAFE_Y до верха рамки: 34 px -> 17 px; пункты меню boulder_24 — 10 px, прежний
# заголовок boulder_16 — 7 px), грань та же boulder (Pause.pag: font0_16 = boulder_16; берётся boulder_24 — исходник 28 px,
# при ~1.8x он почти 1:1, без размытия увеличения). Заглавные ровно посередине полосы. Правило порта (размер), сверить с Робом
PAUSE_TITLE_BASE, PAUSE_TITLE_CAP_OF_BAND = 'boulder_24', 0.5
# ---- Правка Роба 163 (01.10 ~14:50, после правок 136 и 150): «Зачем сделал меню таким узким? Рамка не должна постоянно
# адаптироваться — просто приятная по ширине, она была ок. Дай её шире, а PAUSE MENU — мельче, усреднить по ширине. Межбуквенный
# был шире — CONTINUE, MISSION SELECT очень плотно, на PSP толсто и сливается в кашу. ACCEPT — крупнее, а там толстое и тонкое.
# Доработать шрифты, всё усреднить». Правило порта (вкус — константы; решено терминалом за Роба, снимки вариантов —
# ~/Downloads/SHAR_меню_пауза/):
#   * трекинг — целые пиксели PSP к адвансу грани диска 1:1, суффикс '+N' ('boulder_16@1+2'): буквы — те же ячейки диска
#     (пиксель-перфект), шаг знака +N, отступ +N//2 (строка по центру не съезжает), пробел тоже +N; атлас общий с гранью без '+';
#   * ITEM_FACE — пункты паузы, паузы миссии, OPTIONS паузы и главного меню, подписи SOUND (было boulder_24 x 0,57: пересэмпл
#     с утолщением, просвет между буквами −1…1 px — «каша»); boulder_16 1:1 — заглавная 11 px, +2 px — просвет 2…4 px;
#   * MENU_FACE — строка главного меню (крупная грань 1:1 правки 136, заглавная 17 px) с тем же трекингом; другой вариант —
#     MENU_FACE_ALT (грань пунктов паузы), снимок рядом;
#   * HINT_FACE — подписи кнопок (Accept, Back, Continue, Quit) всех экранов меню и паузы одной гранью: было Tt2001m x 0,69
#     (пересэмпл: «Accep t», штрихи разной толщины) у паузы и boulder_16 1:1 у главного меню — вперемешку;
#   * рамка XSmallBoard паузы, паузы миссии и OPTIONS паузы — постоянной ширины PAUSE_BOARD_W (как до правки 150), высота по строкам;
#   * заголовок над доской (PAUSE MENU, OPTIONS, SOUND, SETTINGS, LEVEL PROGRESS, VIEW CARDS, MISSION SELECT) — TITLE_FACE:
#     мельче прежнего (заглавная 17 px — почти как монеты), но крупнее пунктов: «не как пункт» (Роб 28.09).
# UI_DENSE=1 — раскладка до правки 163 (рамка по содержимому 192, грани без трекинга, подсказки вперемешку, заголовок 17 px):
# отрицательный контроль гейтов tests/pause_layout и tests/menu_font
# ---- Правки Роба 190 и 192 (01.10 22:2x, после 163): «межбукв слишком большой, обвод уродский, ACCEPT слишком крупный, NEW GAME
# капельку меньше — крайности во всём». Сверка с оригиналом (близнец tw_pause_menu и v15_boot2, pcsx2-twins 01.10): пункты паузы
# без разрядки, мягкая тень; «Accept» — тонкий Tt2001m, не boulder; NEW GAME — boulder с натуральным шагом. Середина между до-163
# и 163: пункты +1 px (было +2), подсказки — Tt2001m 1:1 (заглавная 9, было boulder 11), строка меню без разрядки (1:1, как
# требует гейт 136), тени меню и паузы вдвое прозрачнее (SHADOW_SOFT; HUD не трогаем)
UI_DENSE = bool(os.environ.get('UI_DENSE'))
ITEM_FACE = os.environ.get('UI_ITEM_FACE', 'boulder_16@1')   # правка 201: без разрядки (было +1)
MENU_FACE_ALT = 'boulder_16@1+1'            # вариант строки главного меню: грань пунктов паузы (снимок «B» правки 163)
HINT_FACE = os.environ.get('UI_HINT_FACE', 'swz721n_10@1')   # правка 233: подписи кнопок паузы и меню — swz721n 1:1 (было Tt2001m__14@1)   # правка 201: без разрядки (было +1)
SHADOW_SOFT = float(os.environ.get('UI_SHADOW_SOFT', '0.5'))   # правка 192: доля альфы тени текста вне HUD на PSP
HUD_PAGES = ('Hud.pag', 'Coins.pag')        # тени HUD прежние: текст поверх города
# ---- Правка Роба 193 (01.10 22:3x, «штраф сразу, полиции нет»): при полной шкале порт показывал квитанцию штрафа «50» вместе с
# надписью — как будто штраф уже взят. Оригинал (близнец tw_hnr_text, pcsx2-twins, кадры 148–268 после шкалы 100 на кадре 150):
# только крупная красная «Hit & Run!» — x 201–477 из 682, буквы y 170–213 из 512 (заглавная H 42, центр 0,374 высоты), цвет (218, 3, 0), 4 вспышки
# (кадры 166–172, 181–187, 199–205, 214–220 при 50 к/с); квитанции нет (по данным Hud.pag она — для «BUSTED!»). Строки поля
# HNR_TEXT: 0 — «Hit & Run!» (крупная), 1 — «BUSTED!» с квитанцией, 2 — пусто (пауза мигания, main.c hnrPursuit)
HNR_BIG_FACE = 'boulder_24*2.3'             # заглавная ~22 px PSP (17 x 0,567 x 2,3): у оригинала 42/512 высоты экрана (22,3 px на 272)
HNR_RED = [218, 3, 0, 255]
HNR_CY = 0.374 * 480                        # центр букв по высоте экрана оригинала (источник 640x480)
TITLE_FACE = os.environ.get('UI_TITLE_FACE', 'boulder_24*1.35+1')
PAUSE_BOARD_W = 240                         # рамка паузы до правки 150 (PAUSE_BOARD): 240 px, видимая — 239
MENU_SHADOW_PX = int(os.environ.get('UI_MENU_SHADOW', '1'))   # тень строки главного меню: 1 px (было 2 — сплошная чёрная заливала
                                                               # просвет между буквами, «толсто»); атлас — только буквы пунктов
HINT_PAGES = ('Accept.pag', 'Back.pag', 'Accept3.pag', 'Continue.pag')
if UI_DENSE:
    ITEM_FACE, HINT_FACE, TITLE_FACE = 'boulder_24', None, None
elif not os.environ.get('UI_MENU_FACE'):
    MENU_FACE = 'boulder_16@1'              # правка 201 (было boulder_24@1, правка 190: без разрядки +2)
PAUSE_FONT = 'boulder_24' if os.environ.get('PAUSE_OLD') else ITEM_FACE   # PAUSE_OLD — раскладка до правки 150 целиком

# ------------------------------------------------------------------------------------------
# enums shared with viewer/ui.h  (keep in sync!)
TAGS = ['ALWAYS', 'RADAR', 'SPEED', 'HNR_METER', 'COINS', 'OBJECTIVE', 'TIMER', 'PARTIME', 'LAP',
        'POSITION', 'DAMAGE', 'PROXIMITY', 'HNR_TICKET', 'ACTION', 'CARDS', 'CARD_TEXT',
        'ITEMS_COMPLETE', 'WASPS', 'COUNTDOWN', 'STAGE_COMPLETE', 'MISSION_COMPLETE',
        'MISSION_FAILED', 'ITEM_DROPPED', 'TUTORIAL', 'COLLECTIBLES', 'PRESS_START', 'LEVEL_NAME',
        'HNR_FULL', 'LOADING', 'GAMBLE', 'STREETRACE', 'SKIP']
# PROGRESS..DOTS — экраны загрузки (tools/load_export.py): доля загрузки 0..1000, кадр анимации, кадр огня, точки
FIELDS = ['NONE', 'SPEED', 'SPEED_UNIT', 'COINS', 'TIMER', 'PARTIME', 'LAP', 'LAPS', 'POSITION',
          'DAMAGE', 'PROXIMITY', 'HNR', 'HNR_LIGHT', 'OBJECTIVE', 'OBJICON', 'CARDS', 'CARDS_TOTAL',
          'COUNTDOWN', 'HNR_TEXT', 'CARD_TITLE', 'COLLECTED', 'COLLECT_TOTAL', 'MENU_ITEM', 'LEVEL',
          'PROGRESS', 'ANIM', 'FIRE', 'DOTS', 'MSN_TITLE', 'MSN_INFO', 'MSN_FAIL', 'MSN_HINT'] + \
         ['ROW%d' % _i for _i in range(12)] + \
         ['STAT_MODE', 'STAT_PCT', 'STAT_GAGS', 'STAT_GAGS_T', 'STAT_WASPS', 'STAT_WASPS_T', 'STAT_CARS', 'STAT_CARS_T',
          'STAT_SKINS', 'STAT_SKINS_T', 'STAT_CARDS', 'STAT_CARDS_T', 'STAT_SR', 'STAT_SR_T', 'STAT_BM', 'STAT_BM_T',
          'STAT_SM', 'STAT_SM_T'] + \
         ['VC%d' % _i for _i in range(7)] + \
         ['MS%d' % _i for _i in range(7)] + \
         ['MSGBOX', 'MSN_DONE', 'ACTION_TEXT'] + \
         ['MS_LEVEL', 'MS_LARROW', 'MS_RARROW'] + \
         ['RACERS'] + ['PLACE'] + ['WAGER_FEE', 'WAGER_PAR', 'WAGER_PAY']   # №598: код клетки на паузе; аудит 09.10: число участников гонки, «1/2» в поле Position (viewer/ui_anim.h uiPosText); правка 348: MISSION SELECT — уровень на экране 0..6, шарики стрелок (0 серый, 1 красный)
# MSN_DONE — вспышка конца миссии (правка 122); VIEW CARDS: карточка k собрана (1/0); MISSION SELECT: строка k — 0 dash, 1 cross, 2 check; MSGBOX — текст окна сообщения (правка 122)
FMT = {'none': 0, 'int': 1, 'time': 2, 'frac': 3, 'ordinal': 4, 'select': 5, 'pct': 6}
SCREENS = ['hud', 'pause', 'main', 'missionload', 'missionover', 'letterbox',   # letterbox — правка 40 (разговор, камера старта)
           'pausemsn',   # пауза во время миссии (PauseMission.scr, 29.09): CONTINUE, RESTART MISSION, CANCEL MISSION, OPTIONS, QUIT GAME
           'levelstats',   # LEVEL PROGRESS паузы (LevelStats.scr) и конец уровня (LevelEnd.scr) — одна страница LevelStats.pag
           'tutorial',     # подсказки обучения Барта (Tutorial.scr): SmallBoard, Tutorial.pag, Continue.pag
           'message']      # правка 122: окно сообщения (Message.scr: MessageBox.pag, Message.pag) + Continue / Accept — «Congratulations! You've unlocked …»
TUT_PLANT = os.environ.get('UI_NO_TUTORIAL') or os.environ.get('UI_TUT_GLYPH_K')   # подброски гейта tests/ui/tutorial.sh
if os.environ.get('UI_NO_TUTORIAL'):   # подброска: пакет без экрана подсказок (прежний ui.shr) — гейт обязан упасть
    SCREENS = [x for x in SCREENS if x != 'tutorial']
# 'viewcards' (VIEW CARDS паузы, PauseViewCards.scr) — не в ui.shr: отдельный пакет vcards.shr на время показа (tools/menu_export.py),
# рисунки 7 карточек уровня крупно — ~300 КБ, в куче игры их держать незачем
ICONS = ['PlayerCars', 'BonusMissions', 'WagerRaces', 'StreetRaces', 'PhoneBooths', 'PurchaseCentres',
         'CheckeredFlags', 'WaypointFlags', 'AIHarassCars', 'AICars', 'Missions', 'Collectibles']
T_RECT, T_POLY, T_IMG, T_TEXT, T_NUM, T_RADARMAP, T_RADARICONS, T_SLOT, T_BAR, T_SECTOR = range(10)
HALIGN = {'left': 0, 'centre': 1, 'right': 2, 'top': 0, 'bottom': 0}
VALIGN = {'top': 0, 'centre': 1, 'bottom': 2, 'left': 0, 'right': 0}

# which pages make each PSP screen (Scrooby screen order), from the .scr lists
SCREEN_PAGES = {
    'hud': [('ingame.p3d', 'Hud.pag'), ('ingame.p3d', 'Coins.pag')],
    'pause': [('ingamel%d.p3d' % LEVEL, 'PauseBgd.pag'), ('ingame.p3d', 'XSmallBoard.pag'),
              ('ingame.p3d', 'Pause.pag'), ('ingame.p3d', 'PauseMissionObjective.pag'),
              ('ingame.p3d', 'PauseFgd.pag'), ('ingame.p3d', 'Coins.pag'),
              ('ingame.p3d', 'PauseSunday.pag'), ('ingame.p3d', 'Accept.pag')],   # free-drive pause (+ RESET CAR)
    # пауза миссии: те же страницы PauseMission.scr, пункты PauseMission.pag (RESET CAR у оригинала здесь нет)
    # LEVEL PROGRESS (LevelStats.scr: PauseBgd, PauseFgd, XSmallBoard, LevelStats, Back) и конец уровня (LevelEnd.scr: XSmallBoard,
    # LevelStats, Accept): метки экрана — SKIP (UI_T_LS_PAUSE: фон паузы и Back) / STREETRACE (UI_T_LS_END: Accept и подсказка
    # UseMissionSelect); заголовок LevelProgress — строка 0 «LEVEL PROGRESS» или 1 «Level Complete» по полю STAT_MODE
    'levelstats': [('ingamel%d.p3d' % LEVEL, 'PauseBgd.pag'), ('ingame.p3d', 'PauseFgd.pag'), ('ingame.p3d', 'XSmallBoard.pag'),
                   ('ingame.p3d', 'LevelStats.pag'), ('ingame.p3d', 'Back.pag'), ('ingame.p3d', 'Accept.pag')],
    # VIEW CARDS (PauseViewCards.scr): рамка BigBoard, 7 мест карточек + рамка выбора (PauseViewCards.pag), карточка крупно
    # (LowResCard.pag), Back / Accept. Рисунки карточек — frontend.p3d card00..card55 (191x245; в ingame.p3d — пустышки _t)
    'viewcards': [('ingamel%d.p3d' % LEVEL, 'PauseBgd.pag'), ('ingame.p3d', 'BigBoard.pag'), ('ingame.p3d', 'PauseViewCards.pag'),
                  ('ingame.p3d', 'LowResCard.pag'), ('ingame.p3d', 'Back.pag'), ('ingame.p3d', 'Accept.pag')],
    # MISSION SELECT (MissionSelect.scr): рамка BigBoard, MissionSelect.pag (номера, названия миссий уровня, значки состояния,
    # полоса выбора, LEVEL N), Back / Accept
    'missionselect': [('ingamel%d.p3d' % LEVEL, 'PauseBgd.pag'), ('ingame.p3d', 'BigBoard.pag'), ('ingame.p3d', 'MissionSelect.pag'),
                      ('ingame.p3d', 'Back.pag'), ('ingame.p3d', 'Accept.pag')],
    # Tutorial.scr: рамка SmallBoard, Tutorial.pag (Барт — walkie, «Disable Tutorials» с □, текст TutorialMessage — спрайтовый шрифт
    # с кнопками PS2), Continue.pag (метка STREETRACE = UI_T_TUT_CONTINUE: видна, когда реплика Барта кончилась)
    'tutorial': [('ingame.p3d', 'SmallBoard.pag'), ('ingame.p3d', 'Tutorial.pag'), ('ingame.p3d', 'Continue.pag')],
    # Message.scr (правка 122, запись PS2 26:50): рамка MessageBox.pag со значком ErrorIcon, текст Message.pag; ✕ Accept — как в
    # кадре записи (Accept.pag); жёлтое «Continue» внутри рамки у оригинала — без своей страницы, не переносится
    'message': [('ingame.p3d', 'MessageBox.pag'), ('ingame.p3d', 'Message.pag'), ('ingame.p3d', 'Accept.pag')],
    'pausemsn': [('ingamel%d.p3d' % LEVEL, 'PauseBgd.pag'), ('ingame.p3d', 'XSmallBoard.pag'),
                 ('ingame.p3d', 'Pause.pag'), ('ingame.p3d', 'PauseMissionObjective.pag'),
                 ('ingame.p3d', 'PauseFgd.pag'), ('ingame.p3d', 'Coins.pag'),
                 ('ingame.p3d', 'PauseMission.pag'), ('ingame.p3d', 'Accept.pag')],
    'main': [('frontend.p3d', '3dFE.pag'), ('frontend.p3d', 'TVFrame.pag'), ('frontend.p3d', 'Level.pag'),
             ('frontend.p3d', 'MainMenu.pag'), ('frontend.p3d', 'Accept3.pag')],
    # экран брифинга миссии (MissionLoad.scr): планшет с картинкой задания SetPresentationBitmap (MissionStartBitmap —
    # картинка из данных миссии, viewer/ui.c uiSetBriefingImage), Continue / Abort; сведения о ставке — метка GAMBLE
    'missionload': [('ingame.p3d', 'LetterBox.pag'), ('ingame.p3d', 'MissionLoad.pag')],
    # провал миссии (MissionOver.scr): FAILED, причина и подсказка (строки библии из движка), «Retry Mission?» YES / NO;
    # SKIP (пропуск миссии после нескольких провалов) — метка SKIP, скрыта: пропуска в порте нет
    'missionover': [('ingame.p3d', 'Overlay.pag'), ('ingame.p3d', 'MissionOver.pag'), ('ingame.p3d', 'Accept3.pag')],
    # экран разговора (LetterBox.scr = LetterBox.pag + LetterBoxButtons.pag + 3dIris.pag), правка 40: подписи внизу — «✕ SKIP»
    # (метка SKIP) или «✕ Start Mission / △ Cancel» (метка STREETRACE — у этого экрана своя маска меток); полосы LetterBox.pag —
    # полигоны на пол-экрана, их выезд задаёт код оригинала: полосы рисует viewer/ui.c uiDrawLetterbox
    'letterbox': [('ingame.p3d', 'LetterBoxButtons.pag')],
}
# explicit anchor overrides: (page, unit path prefix) -> 'L' | 'C' | 'R'
UNIT_ANCHOR = {
    ('Hud.pag', 'Foreground/Message'): 'L',            # objective box moves with its icon
    ('Hud.pag', 'Foreground/MissionObjective'): 'L',
    ('Hud.pag', 'Foreground/MissionOverlays/Lap'): 'L',  # "1/3 LAPS" belongs to the timer corner
    ('Hud.pag', 'Foreground/MissionOverlays/Position'): 'L',   # №552: «4th/4» вместе с кругами в одном блоке у левого края
    ('PauseMissionObjective.pag', 'Foreground/MissionObjective'): 'L',
    ('Tutorial.pag', 'Foreground/DisableTutorial'): 'L',   # «□ Disable Tutorials» — левый нижний угол, как «✕ Continue» — правый (R)
}
# units that must share ONE anchor offset (same key -> merged bbox)
UNIT_MERGE = {
    # №552 (Роб 11.10): блок гонки «1/3 LAPS» + «4th/4» — один узел, у левого края 16:9 с полем угловых элементов (compute_units). RACEHUD_LEFT_OLD=1 — как было
    **({} if os.environ.get('RACEHUD_LEFT_OLD') == '1' else {('Hud.pag', 'Foreground/MissionOverlays/Lap'): 'racehud', ('Hud.pag', 'Foreground/MissionOverlays/Position'): 'racehud'}),
    ('Hud.pag', 'Foreground/Message'): 'objective',
    ('Hud.pag', 'Foreground/MissionObjective'): 'objective',
    # правка 122: окно сообщения — значок ErrorIcon на углу рамки, как в записи PS2 26:50 (одна группа с рамкой и текстом)
    ('MessageBox.pag', 'Foreground'): 'msgbox', ('MessageBox.pag', 'Foreground/Frame'): 'msgbox', ('Message.pag', 'Foreground'): 'msgbox',
}

# ------------------------------------------------------------------------------------------
# Scrooby chunk ids
C_PROJECT, C_SCREEN, C_PAGE, C_LAYER, C_GROUP = 0x18000, 0x18001, 0x18002, 0x18003, 0x18004
C_SPRITE, C_TEXT, C_P3DOBJ, C_POLY, C_STRREF, C_STRLIT = 0x18006, 0x18007, 0x18008, 0x18009, 0x1800B, 0x1800C
C_RES_IMAGE, C_RES_P3D, C_RES_FONT, C_RES_BIBLE = 0x18100, 0x18101, 0x18104, 0x18105
C_IMAGE, C_FONT = 0x19005, 0x22000
JUST = {0: 'left', 1: 'right', 2: 'top', 3: 'bottom', 4: 'centre'}


def _layout_fields(d, p):
    ver, x, y, w, h, jx, jy, b, g, r, a, transl, rot = struct.unpack_from('<IiiIIIIBBBBIf', d, p)
    return dict(ver=ver, x=x, y=y, w=w, h=h, jx=JUST.get(jx, jx), jy=JUST.get(jy, jy),
                color=[r, g, b, a], transl=transl, rot=rot), p + 40


def parse_element(c, bible):
    d = c.data
    name, p = string(d)
    if c.id == C_LAYER:
        ver, vis, edit, alpha = struct.unpack_from('<IIII', d, p)
        return dict(kind='layer', name=name, visible=bool(vis), alpha=alpha,
                    children=[parse_element(s, bible) for s in c.children])
    if c.id == C_GROUP:
        ver, alpha = struct.unpack_from('<II', d, p)
        return dict(kind='group', name=name, alpha=alpha, children=[parse_element(s, bible) for s in c.children])
    if c.id == C_SPRITE:
        e, p = _layout_fields(d, p)
        n, = struct.unpack_from('<I', d, p); p += 4
        imgs = []
        for _ in range(n):
            s, p = string(d, p); imgs.append(s)
        e.update(kind='sprite', name=name, images=imgs)
        return e
    if c.id == C_TEXT:
        e, p = _layout_fields(d, p)
        font, p = string(d, p)
        shadow = d[p]; sb, sg, sr, sa = d[p + 1:p + 5]
        sx, sy, hard = struct.unpack_from('<iiI', d, p + 5)
        strings = []
        for s in c.children:
            if s.id == C_STRREF:
                bname, q = string(s.data); key, q = string(s.data, q)
                strings.append(dict(key=key, text=bible(key) if bible else None))
            elif s.id == C_STRLIT:
                strings.append(dict(key=None, text=string(s.data)[0]))
        e.update(kind='text', name=name, font=font,
                 shadow=dict(on=bool(shadow), color=[sr, sg, sb, sa], dx=sx, dy=sy), strings=strings)
        return e
    if c.id == C_POLY:
        ver, transl, n = struct.unpack_from('<III', d, p); p += 12
        pts = [list(struct.unpack_from('<3f', d, p + 12 * i))[:2] for i in range(n)]; p += 12 * n
        cols = [[d[p + 4 * i + 2], d[p + 4 * i + 1], d[p + 4 * i], d[p + 4 * i + 3]] for i in range(n)]
        xs = [q[0] for q in pts]; ys = [q[1] for q in pts]
        return dict(kind='poly', name=name, points=pts, colors=cols,
                    x=min(xs), y=min(ys), w=max(xs) - min(xs), h=max(ys) - min(ys))
    if c.id == C_P3DOBJ:
        e, p = _layout_fields(d, p)
        res, p = string(d, p)
        e.update(kind='p3d', name=name, resource=res)
        return e
    return dict(kind='unknown_%x' % c.id, name=name)


def load_bible(lang='E'):
    """srr2 text bible, one language table ('E' English).  Same hash as hit-and-run-web campaign.py."""
    root = read(SCROOBY / 'resource/txtbible/srr2.p3d')
    tab = None
    for c in walk(root):
        if c.id == 0x1800E:
            _, p = string(c.data)
            if c.data[p:p + 1] == lang.encode():
                tab = c
    _, p = string(tab.data)
    n, mod, size = struct.unpack_from('<III', tab.data, p + 1); p += 13
    hashes = struct.unpack_from('<%dI' % n, tab.data, p)
    offs = struct.unpack_from('<%dI' % n, tab.data, p + 4 * n)
    buf = tab.data[p + 8 * n:]
    values = {h: buf[o:].decode('utf-16-le', errors='replace').split('\0')[0] for h, o in zip(hashes, offs)}

    def get(key):
        h = 0
        for ch in key.encode():
            h = (ch + (h << 6)) % mod
        return values.get(h)
    return get


def decode_sprite_image(chunk):
    name, p = string(chunk.data)
    nw, nh = struct.unpack_from('<II', chunk.data, p)
    _, p = string(chunk.data, p + 8)
    w, h, count, border = struct.unpack_from('<4I', chunk.data, p)
    out = Image.new('RGBA', (w, h)); x = y = 0; rowh = 0
    for im in chunk.children:
        if im.id != 0x19001:
            continue
        raw = next(c for c in im.children if c.id == 0x19002).data[4:]
        tile = decode_image(raw); tw, th = tile.size
        if border:
            tile = tile.crop((1, 1, tw - 1, th - 1)); tw -= 2; th -= 2
        if x >= w:
            x = 0; y += rowh; rowh = 0
        out.paste(tile, (x, y)); x += tw; rowh = max(rowh, th)
    return name.lower(), out


def decode_font(c):
    name, p = string(c.data, 4)
    _, p = string(c.data, p)
    size, width, height, baseline = struct.unpack_from('<4f', c.data, p)
    tex = next(t for t in c.children if t.id == 0x19000)
    img = decode_image(tex.children[0].children[0].data[4:])
    gc = next(t for t in c.children if t.id == 0x22001)
    count, = struct.unpack_from('<I', gc.data)
    glyphs = {}
    for i in range(count):
        t, u0, v0, u1, v1, left, right, w, adv, code = struct.unpack_from('<I8fI', gc.data, 4 + i * 40)
        glyphs[code] = dict(x=u0 * img.width, y=(1 - v1) * img.height, w=(u1 - u0) * img.width,
                            h=(v1 - v0) * img.height, adv=adv, left=left, right=right)
    return name, dict(name=name, size=size, width=width, height=height, baseline=baseline, image=img, glyphs=glyphs)


class Project:
    def __init__(self, fname, bible):
        root = read(SCROOBY / fname)
        self.file = fname
        self.images, self.fonts, self.pages, self.screens = {}, {}, {}, {}
        self.res_images, self.res_fonts = {}, {}
        for c in root.children:
            if c.id == C_IMAGE:
                n, im = decode_sprite_image(c); self.images[n] = im
            elif c.id == C_FONT:
                n, f = decode_font(c); self.fonts[n] = f
        proj = next(c for c in root.children if c.id == C_PROJECT)
        pn, p = string(proj.data)
        ver, self.w, self.h = struct.unpack_from('<III', proj.data, p)
        for c in proj.children:
            if c.id == C_PAGE:
                pname, p = string(c.data)
                layers = []
                for s in c.children:
                    if s.id == C_RES_IMAGE:
                        rn, q = string(s.data); fn, q = string(s.data, q + 4)
                        self.res_images[rn] = fn.split('\\')[-1].lower()
                    elif s.id == C_RES_FONT:
                        rn, q = string(s.data); fn, q = string(s.data, q + 4); face, q = string(s.data, q)
                        self.res_fonts[rn] = face
                    elif s.id == C_LAYER:
                        layers.append(parse_element(s, bible))
                self.pages[pname] = layers
            elif c.id == C_SCREEN:
                sn, p = string(c.data); v, n = struct.unpack_from('<II', c.data, p); p += 8; pl = []
                for _ in range(n):
                    s, p = string(c.data, p); pl.append(s)
                self.screens[sn.split('\\')[-1]] = pl

    def image(self, resname):
        fn = self.res_images.get(resname)
        return self.images.get(fn) if fn else None


def iter_leaves(node, path=()):
    if isinstance(node, list):
        for n in node:
            yield from iter_leaves(n, path)
        return
    if node['kind'] in ('layer', 'group'):
        if node['kind'] == 'layer' and not node['visible']:
            return
        for ch in node['children']:
            yield from iter_leaves(ch, path + (node['name'],))
    else:
        yield path, node


_PROJECTS = {}
_BIBLE = None


def project(fname):
    global _BIBLE
    if _BIBLE is None:
        _BIBLE = load_bible(LANG)
    if fname not in _PROJECTS:
        _PROJECTS[fname] = Project(fname, _BIBLE)
    return _PROJECTS[fname]


EXTRA_IMAGES = {}   # синтетические картинки пакета (имя -> PIL RGBA), заполняет brief_plate до сборки


def all_images():
    """Image lookup across the projects (ingame first)."""
    out = {}
    for f in ('frontend.p3d', 'ingame.p3d'):
        out.update(project(f).images)
    out.update(EXTRA_IMAGES)   # №613: плашка под картинкой задания (brief_plate)
    if 'hrticket.png' in out:   # правка 193: пустой кадр квитанции (поле HNR_TEXT 0 и 2 — квитанции нет)
        out['hrticket_off.png'] = Image.new('RGBA', out['hrticket.png'].size, (0, 0, 0, 0))
    return out


def all_fonts():
    out = {}
    for f in ('frontend.p3d', 'ingame.p3d'):
        out.update(project(f).fonts)
    # Requested sizes of the engine's pixel face use the same UIP1 font atlas path.
    # With no typography override this extra source is unused; existing packs stay exact.
    import re
    msx = bytes(int(v, 16) for v in re.findall(r'0x([0-9a-fA-F]{2})', (ROOT / 'viewer/bootmsx.h').read_text().split('{', 1)[1]))
    fix = {m[0]: [int(x, 16) for x in m[1].split(',')] for m in re.findall(r"\{'(.)', \{([^}]*)\}\}", (ROOT / 'viewer/ui_layout.h').read_text())}
    image = np.zeros((160, 160, 4), dtype=np.uint8)
    glyphs = {}
    cross = [0, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0]
    circle = [0, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x3c, 0]
    triangle = [0, 0x18, 0x18, 0x24, 0x24, 0x42, 0x7e, 0]
    square = [0, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x7e, 0]
    buttons = {0xa3: cross, 0xbf: cross, 0xae: circle, 0xb5: circle, ord('@'): triangle, 0xa7: square, 0xa9: square}
    for code in list(range(32, 127)) + [0xa3, 0xbf, 0xae, 0xb5, 0xa7, 0xa9]:
        rows = buttons.get(code) or fix.get(chr(code), [((r | ((r >> 1) & ~(r << 1))) & 255) for r in msx[code * 8:code * 8 + 8]])
        mask = 0
        for row in rows: mask |= row
        cols = [x for x in range(8) if mask & (128 >> x)]
        left, width = (min(cols), max(cols) - min(cols) + 1) if cols else (0, 2)
        if code in buttons: left, width = 0, 8
        x0, y0 = (code % 16) * 10, (code // 16) * 10
        for y, row in enumerate(rows):
            for x in range(width):
                if row & (128 >> (left + x)): image[y0 + y, x0 + x] = [255, 255, 255, 255]
        glyphs[code] = dict(x=x0, y=y0, w=width, h=10, adv=width + 1, left=0)
    out['pix8_8'] = dict(image=Image.fromarray(image, 'RGBA'), glyphs=glyphs, height=10, baseline=8)
    # Правка Роба 500 (10.10: «шрифт дисклеймера в начале игры — мелкий, читабельный, не жирный; добавь опцией в аппку»): экран NOTICE при запуске (viewer/bootscreen.h
    # bootNotice, bootui.h buiTextX) рисуется встроенным шрифтом PSPSDK `msx` 8×8, 1 бит, без утолщения, шаг знака — по чернилам + 1 px, пробел 3 px (buiAdv). Здесь тот же
    # шрифт гранью «msx_thin» (строки msx как есть, без жирных рукописных букв pix8): назначается любой группе панели, пакеты UIP1 рисуют его как обычную грань диска.
    # Шрифт — PSPSDK libpspdebug (BSD 3-clause, копирайт и условия в шапке viewer/bootmsx.h; в EBOOT он уже слинкован ради pspDebugScreen) — отдельной лицензии не требует.
    # Знаки кнопок (X ○ △ □) — те же, что у pix8. UI_MSX_THIN_OFF=1 — без этой грани
    if os.environ.get('UI_MSX_THIN_OFF') != '1':
        img2 = np.zeros((160, 160, 4), dtype=np.uint8); gl2 = {}
        for code in list(range(32, 127)) + [0xa3, 0xbf, 0xae, 0xb5, 0xa7, 0xa9]:
            rows = buttons.get(code) or list(msx[code * 8:code * 8 + 8])
            mask = 0
            for row in rows: mask |= row
            cols = [x for x in range(8) if mask & (128 >> x)]
            left, width = (min(cols), max(cols) - min(cols) + 1) if cols else (0, 2)
            if code in buttons: left, width = 0, 8
            x0, y0 = (code % 16) * 10, (code // 16) * 10
            for y, row in enumerate(rows):
                for x in range(width):
                    if row & (128 >> (left + x)): img2[y0 + y, x0 + x] = [255, 255, 255, 255]
            gl2[code] = dict(x=x0, y=y0, w=width, h=10, adv=width + 1, left=0)
        out['msx_thin'] = dict(image=Image.fromarray(img2, 'RGBA'), glyphs=gl2, height=10, baseline=8)
    # Два пиксельных шрифта Lauke (CC BY 3.0, 11.10): грани bart_16bit и simpsons_pix, в пакет попадают только при выборе группой (UI_PIXFONTS_OFF=1 — без них)
    import pixel_fonts
    out.update(pixel_fonts.faces(buttons))
    out.update(pixel_fonts.derived(out))   # №547/552: Boulder и Swiss 721 пиксельные грани (boulder_px11/14/18, swz721_px8/10/12[i])
    return out


# ------------------------------------------------------------------------------------------
# 1) flatten Scrooby pages into draw items in SOURCE space (640x480, y DOWN, top-left origin)

def ydown(y, h):
    return SRC_H - (y + h)


def tag_for(page, path, name):
    p = '/'.join(path)
    if page == 'Coins.pag':
        return 'COINS'
    if page == 'PauseFgd.pag':
        return 'PRESS_START'
    if page == 'Level.pag':
        return 'LEVEL_NAME'
    if page == 'PauseMissionObjective.pag':
        return 'OBJECTIVE'
    if page == 'MissionLoad.pag' and 'GamblingInfo' in p:
        return 'GAMBLE'
    if page == 'MissionOver.pag' and name == 'Response2':
        return 'SKIP'
    if page == 'MissionLoad.pag' and 'LoadCompleted' in p:   # правка Роба 52: группа «загрузка закончена» (код её показывает)
        return 'SKIP' if name == 'Abort' else 'PRESS_START'   # Continue — после LOADING; Abort в эталоне PS2 не показан
    if page == 'MissionLoad.pag' and name.startswith('PattyAndSelma'):
        return 'STREETRACE'                               # итог уличных гонок (Пэтти и Сельма) — только у sr
    if page == 'LetterBoxButtons.pag':
        return 'SKIP' if 'SkipLabel' in p else 'STREETRACE'   # viewer/ui.h UI_T_LB_SKIP / UI_T_LB_START
    if page != 'Hud.pag':
        return 'ALWAYS'
    if 'HudMap0/Speed0' in p:
        return 'SPEED'
    if 'HitNRunMeter' in p:
        # металлическая рамка шкалы и есть круг радара (Роб 27.09: «карта была в круге»): видна всегда вместе с радаром;
        # неподвижная синяя дорожка HnRMeterBgd остаётся HNR_METER (движок её не рисует по просьбе Роба)
        if name == 'HnRMeterFrame':
            return 'RADAR'
        return 'HNR_FULL' if name == 'HnRMeter' else 'HNR_METER'
    if 'HudMap0' in p:
        return 'RADAR'
    table = [('Foreground/Message', 'OBJECTIVE'), ('Foreground/MissionObjective', 'OBJECTIVE'),
             ('MissionOverlays/Timer', 'TIMER'), ('MissionOverlays/ParTime', 'PARTIME'),
             ('MissionOverlays/Lap', 'LAP'), ('MissionOverlays/Position', 'POSITION'),
             ('MissionOverlays/DamageMeter', 'DAMAGE'), ('MissionOverlays/ProximityMeter', 'PROXIMITY'),
             ('MissionOverlays/CountDown', 'COUNTDOWN'), ('MissionOverlays/MissionProgress', 'STAGE_COMPLETE'),
             ('MissionOverlays/ItemDropped', 'ITEM_DROPPED'), ('MissionOverlays/Collectibles', 'COLLECTIBLES'),
             ('Foreground/HitNRun', 'HNR_TICKET'), ('Foreground/ActionButton', 'ACTION'),
             ('Foreground/WaspsDestroyed', 'WASPS'), ('ItemCollected/ItemsCount', 'CARDS'),
             ('ItemCollected/CardText', 'CARD_TEXT'), ('ItemCollected/ItemsComplete', 'ITEMS_COMPLETE'),
             ('ItemCollected', 'CARD_TEXT'), ('TutorialMode', 'TUTORIAL')]
    if 'MissionOverlays/MissionComplete' in p:
        return 'MISSION_FAILED' if name == 'MissionFailed' else 'MISSION_COMPLETE'
    for key, tag in table:
        if key in p:
            return tag
    return 'ALWAYS'


def unit_for(page, path, name):
    """Anchor unit = layer/group (2 levels), MissionOverlays and ItemCollected split one deeper."""
    parts = list(path)
    depth = 3 if len(parts) >= 2 and parts[1] in ('MissionOverlays', 'ItemCollected') else 2
    key = '/'.join(parts[:depth]) if parts else name
    if page == 'Coins.pag' or (page == 'TVFrame.pag'):
        key = key + '/' + name            # coins / each TV-glass highlight anchor on their own
    merged = UNIT_MERGE.get((page, key))
    return (page, merged or key), UNIT_ANCHOR.get((page, key))


SPRITE_FONT_KEYS = None


# Кнопки PS2 в спрайтовом шрифте подсказок (Tutorial.pag TutorialMessage: картинки but_* в конце набора): знак-заместитель в текстах
# библии TUTORIAL_NNN_(PS2) -> картинка. По смыслу строк: «USE + OR LEFT ANALOG STICK TO MOVE» (+ = крестовина), «USE £ TO JUMP» (X),
# «HOLD ® TO RUN» (○), «PRESS ¿ TO ACCELERATE» (X), «µ TO BRAKE» (○), «§ TO HANDBRAKE» / «§ BUTTON TO KICK THE WASP» (□),
# «© TO KICK BOX» (□), «@ TO GET INTO CAR» (△); сверено с записью PS2 1:05–1:20 (крестовина, X, ○ на тех же местах)
BUTTON_CHARS = {'but_dpad': '+', 'but_jump': '\xa3', 'but_run': '\xae', 'but_gas': '\xbf', 'but_brak': '\xb5', 'but_hand': '\xa7',
                'but_kick': '\xa9', 'but_actn': '@'}
# Правка Роба 216 (03.10: «MISSION COMPLETE — крупно, на всю ширину экрана, с такими же краями, как у карточки и прочих»): надпись
# спрайтовым шрифтом Hud.pag шла на 260 px из 480 — множитель картинок букв 1,69: «MISSION COMPLETE!» ~440 px, поля ~20 px; шаг двух
# строк («BONUS MISSION» / «COMPLETE!») тем же множителем. UI_MSN_DONE_K=1 — прежнее
# Правка 500 (аудит панели 10.10: «MISSION COMPLETE! упирается в края»): 1,69 давало 446 px из 480 — поля 17 px, М и «!» у самых краёв. 1,60 — ≈420 px, поля по 30 px
# (безопасная зона), надпись остаётся крупной (правка 216); больше или меньше — ползунком панели «MISSION COMPLETE!» (hud_mdone). UI_MSN_DONE_SAFE_OLD=1 — прежние 1,69
MSN_DONE_K = float(os.environ.get('UI_MSN_DONE_K', '1.69' if os.environ.get('UI_MSN_DONE_SAFE_OLD') == '1' else '1.6'))
# Правка Роба 267 (04.10: «подсказки Барта выходят за пределы рамки; текст громадный, еле вмещается; доработать типографику по
# размерам, подаче, ритму; использовать пиксельность PSP»): картинки букв ровно вчетверо мельче своих (1 px PSP = 4×4 px картинки,
# чистое уменьшение без мыла): k = 0,25 / S. Самая широкая строка («USE LEFT ANALOG STICK OR + TO STEER») 293 -> 235 px в рамке
# 300 px, буква ~15 px в ячейке (было 19). Прежнее 0,55 — UI_TUT_GLYPH_K=0.55 (красный контроль tests/ui/tutorial_fit.py)
TUT_GLYPH_K = float(os.environ.get('UI_TUT_GLYPH_K', '0.55'))   # 04.10 07:2x Роб: «в подсказках Барта размер был нормальный» — правка 267 по ним откачена (была 0,25/S)   # подброска гейта: 1.0 — строки шире рамки. До 267: спрайтовый шрифт
                     # подсказок в оригинале вдвое мельче своих картинок (запись PS2: буква 15 из 30 px) — 0.55 для PSP
TUT_COUNT = 22       # TUTORIAL_000 .. TUTORIAL_021 (последняя — «INVALID TUTORIAL ENTRY»)


def tutorial_texts():
    B = _BIBLE or load_bible(LANG)
    return [B('TUTORIAL_%03d_(PS2)' % i) or B('TUTORIAL_%03d' % i) or '' for i in range(TUT_COUNT)]


def sprite_font_map(images):
    """Scrooby sprite-font: image list index -> character (dot = unused slot)."""
    chars = {}
    for im in images:
        if im in BUTTON_CHARS: chars[BUTTON_CHARS[im]] = im
    special = {'space': ' ', 'colon': ':', 'comma': ',', 'hyphen': '-', 'exclamation': '!', 'money': '$',
               'period': '.', 'exclamation-inverted': '\xa1', 'question': '?', 'slash': '/',
               'A_Diaeresis': '\xc4', 'A_Grave': '\xc0', 'E_Acute': '\xc9', 'E_Circumflex': '\xca',
               'I_Acute': '\xcd', 'N_Tilde': '\xd1', 'O_Acute': '\xd3', 'O_Diaereris': '\xd6',
               'S_Sharp': '\xdf', 'U_Acute': '\xda', 'U_Diaeresis': '\xdc'}
    for im in images:
        if im in special:
            chars[special[im]] = im
        elif len(im) == 1 and im.isdigit():
            chars[im] = im
        elif im.startswith('letter') and len(im) == 7:
            chars[im[6]] = im
    return chars


def pause_title_face(board_top):
    """грань и место заголовка паузы (PSP px): заглавная H = PAUSE_TITLE_CAP_OF_BAND полосы SAFE_Y..board_top, по центру
    полосы. -> (грань, верх строки, высота строки)"""
    band = board_top - SAFE_Y
    if TITLE_FACE:                                             # правка 163: грань заголовка — константа (мельче прежней, крупнее пунктов)
        face = TITLE_FACE
    else:
        want = int(round(band * PAUSE_TITLE_CAP_OF_BAND))
        k = 1.0
        while cap_px(face_mul(PAUSE_TITLE_BASE, k)) < want and k < 3.0:
            k = round(k + 0.05, 2)
        face = face_mul(PAUSE_TITLE_BASE, k)
    g = FontCache('psp').get(face)
    rows = np.where(g['glyphs'][ord('H')]['bm'].max(1) >= .5)[0]
    cap_mid = (rows[0] + rows[-1] + 1) / 2                     # середина заглавной от верха строки
    return face, SAFE_Y + band / 2 - cap_mid, g['lh']


# ---- Правки Роба 150/151 (01.10): «RESET CAR убрал, а меню осталось таким же сжатым — больше воздуха между строк, рамку меньше,
# всё усредни; мы сайты строим». Правило порта — вёрстка доски по содержимому (как блок на сайте, правило близости):
#   * шаг строк меню PAUSE_FIT_PITCH (просвет между заглавными 16 px, был 12); строки — по центру экрана, каждая ровно по своим буквам;
#   * поля от наружного края рамки до букв: сверху = снизу PAUSE_FIT_PAD_Y (внутри рамки ~1,2 просвета строк: поля шире просветов,
#     группа читается целой), слева = справа PAUSE_FIT_PAD_X от самой широкой строки (у колонки по центру — шире вертикальных);
#   * ширина доски XSmallBoard одна на всё семейство паузы (пауза, пауза миссии, OPTIONS паузы) — при переходе рамка не прыгает;
#     высота — по строкам, которые видит игрок; верх доски прежний (BOARD_TOP: заголовок над ней — прежний, pause_title_face);
#   * служебные строки порта (RESET CAR, у оригинала его нет — правка 137) в высоту не входят: строки пакета стоят шагом по центру
#     полей, viewer/ui.c hideDy (скрытая: ниже — вверх на шаг, все — вниз на полшага) ставит строки игрока ровно в поля, а служебный
#     режим видит все строки внутри той же рамки;
#   * подсказки кнопок (✕ Accept, △ Back) — в правом нижнем углу экрана одной строкой, как у паузы.
# Всё в целых пикселях PSP (сдвиги кратны пикселю, шаг чётный — полшага целые). PAUSE_OLD=1 — прежняя раскладка (отрицательный
# контроль гейта tests/pause_layout)
PAUSE_OLD = bool(os.environ.get('PAUSE_OLD'))
# Правка Роба 232 (03.10: «рамка паузы статичная, не под объём содержимого, как у оригинала — в начальных экранах и в паузе миссии
# во всю высоту с нормальным отступом от надписи PAUSE MENU»): рамка паузы и паузы миссии — одной высоты, на 7 строк (столько у
# паузы свободной езды без служебной RESET CAR), строки паузы миссии (5) — по центру. Экраны внутри (OPTIONS и т.п.) — по содержимому.
# UI_PAUSE_MIN_ROWS=0 — прежнее (по содержимому)
PAUSE_MIN_ROWS = int(os.environ.get('UI_PAUSE_MIN_ROWS', '7'))   # 7 — видимые строки паузы свободной езды без служебной RESET CAR (по ним её рамка)
PAUSE_FIT_PITCH, PAUSE_FIT_PAD_Y, PAUSE_FIT_PAD_X = 26, 28, 40
BOARD_TOP = 40
PAUSE_SERVICE_ROWS = {'ResetCar'}
PAUSE_FAMILY = [('PauseSunday.pag', ()), ('PauseMission.pag', ()), ('PauseOptions.pag', ('Controller',))]   # Controller: menu_export HIDE_ROWS


def psp_src(x, y, w, h):
    """прямоугольник PSP px (якорь C) -> пространство источника 640x480"""
    oxc = (DST_W - SRC_W * S) / 2
    return [(x - oxc) / S, y / S, w / S, h / S]


def glyph_ink(face, s):
    """буквы строки s гранью face на PSP: (x0, x1, верх, низ) от начала пера и верха строки (альфа >= .5), без тени"""
    f = FontCache('psp').get(face); pen = 0; xs, ys = [], []
    for ch in s:
        g = f['glyphs'].get(ord(ch)) or f['glyphs'][ord('?')]
        if g['bm'] is not None:
            m = g['bm'] >= .5
            cols, rows = np.where(m.any(0))[0], np.where(m.any(1))[0]
            if len(cols):
                xs += [pen + g['left'] + cols[0], pen + g['left'] + cols[-1] + 1]; ys += [rows[0], rows[-1] + 1]
        pen += g['adv']
    return (min(xs), max(xs), min(ys), max(ys)) if xs else (0, 0, 0, 0)


def pause_family_width():
    """общая ширина доски семейства паузы: самая широкая видимая строка всех трёх экранов + поля"""
    p = project('ingame.p3d'); w = 0
    for page, hide in PAUSE_FAMILY:
        for _, e in iter_leaves(p.pages[page]):
            if e['kind'] == 'text' and e['name'] not in hide and e['name'] not in PAUSE_SERVICE_ROWS:
                for s in e.get('strings') or []:
                    x0, x1, _, _ = glyph_ink(PAUSE_FONT, s['text']); w = max(w, x1 - x0)
    w += 2 * PAUSE_FIT_PAD_X
    return w + (w & 1)


def pause_board_width():
    """ширина доски семейства паузы (пауза, пауза миссии, OPTIONS паузы): с правки 163 — постоянная PAUSE_BOARD_W, как до правки
    150 (Роб: «не надо, чтобы рамка постоянно адаптировалась — приятная по ширине»); UI_DENSE — по содержимому (правка 150)"""
    return pause_family_width() if UI_DENSE else PAUSE_BOARD_W


def place_board(items, page, rect, K, vis=None):
    """рамка доски (XSmallBoard / BigBoard: углы и края x K) на прямоугольник PSP px. Заливка Frame_Bgd — отступ оригинала x K,
    меньший из двух по каждой оси с обеих сторон: у XSmallBoard диска он кривой (слева 10, справа 5 — при x K полоса между заливкой
    и рамкой слева вдвое шире), излишек заливки уходит под рамку (её края рисуются после). vis — видимая рамка (x0, y0, x1, y1):
    заливка от неё целыми пикселями (у картинок рамки крайние столбцы прозрачные)"""
    fr = psp_src(*rect)
    f0 = next(it['rect'] for it in items if it['page'] == page and it['name'] == 'Frame_Top')
    for it in items:
        if it['page'] != page:
            continue
        if it['name'] == 'Frame_Bgd':
            if '_ins' not in it:
                x, y, w, h = it['rect']
                it['_ins'] = (min(x - f0[0], f0[0] + f0[2] - x - w), min(y - f0[1], f0[1] + f0[3] - y - h))
            ix, iy = it['_ins'][0] * K, it['_ins'][1] * K
            if vis is None:
                it['rect'] = [fr[0] + ix, fr[1] + iy, fr[2] - 2 * ix, fr[3] - 2 * iy]
            else:
                ix, iy = int(round(ix * S)), int(round(iy * S))
                it['rect'] = psp_src(vis[0] + ix, vis[1] + iy, vis[2] - vis[0] - 2 * ix, vis[3] - vis[1] - 2 * iy)
        else:
            it['rect'] = list(fr); it['k'] = K


def hints_to_corner(items, pages=('Accept.pag', 'Back.pag'), gap=12):
    """подсказки кнопок (значок + подпись) — в правый нижний угол одной строкой: последняя из pages у самого края, остальные левее
    через gap px PSP (по буквам подписи, не по рамке Scrooby)"""
    groups = [[it for it in items if it['page'] == pg] for pg in pages]
    groups = [g for g in groups if g]
    right = SRC_W - SAFE_X / S
    for g in reversed(groups):
        rs = []
        for it in g:
            x, y, w, h = it['rect']
            if it['type'] == T_TEXT:                         # подпись: ширина по буквам (адвансы грани на PSP, правка 163)
                tw = face_width(it['font'], it['strings']) / S
                x0 = x if it['halign'] == 'left' else (x + w - tw if it['halign'] == 'right' else x + (w - tw) / 2)
                rs.append([x0, y, tw, h]); it['halign'] = 'left'
            else:
                rs.append([x, y, w, h])
        x0 = min(r[0] for r in rs); x1 = max(r[0] + r[2] for r in rs); y1 = max(r[1] + r[3] for r in rs)
        ty = SRC_H - SAFE_Y / S
        for it, (x, y, w, h) in zip(g, rs):
            it['rect'] = [right - (x1 - x), ty - (y1 - y), w, h]
        right -= (x1 - x0) + gap / S


def ink_box(t, bitmaps):
    """«чернила» элемента раскладки PSP (после adapt): (x0, y0, x1, y1) по альфе >= 128, без тени текста; None — пусто.
    Строка с вариантами — все варианты; стрелки строки — как у выбранной; бегунок ползунка — весь ход по дорожке"""
    t = dict(t); t.pop('sh', None)
    if t['tag'] != 'ALWAYS':
        t['tag'] = 'ALWAYS'
    states = [{}]
    if t['type'] == T_TEXT and t['field'] != 'NONE' and t.get('strings'):
        states = [{t['field']: i} for i in range(len(t['strings']))]
    elif t['type'] == T_BAR:
        states = [{t['field']: 1000}]
    box = None
    for st in states:
        im = render([t], DST_W, DST_H, st, bitmaps, Image.new('RGBA', (DST_W, DST_H), (0, 0, 0, 0)), None, t.get('menu', -1))
        a = np.asarray(im)[..., 3] >= 128
        ys, xs = np.where(a.any(1))[0], np.where(a.any(0))[0]
        if len(xs):
            b = [int(xs[0]), int(ys[0]), int(xs[-1]) + 1, int(ys[-1]) + 1]
            box = b if box is None else [min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3])]
    if box is not None and t.get('slide') and t.get('track'):           # бегунок: ход по дорожке, как viewer/ui.c (значение 0 и 1000)
        lo, hi = int(math.floor(-.5 * t['track'] + .5)), int(math.floor(.5 * t['track'] + .5))
        box = [box[0] + lo, box[1], box[2] + hi, box[3]]
    return box


def _probe_assets(images):
    pa = Assets()
    if images is not None:
        pa.images = images                                  # картинки, собранные раскладкой (дорожки ползунков menu_export)
    return pa


def _row_inks(rows, images=None):
    """каждая строка на PSP по пробной раскладке только этих строк: (буквы/картинки строки ink_box, верх строки подписи —
    ориентир шага: у подписей разные буквы, у строки со значением «(N/A)» скобки выше заглавных)"""
    probe = [dict(it) for r in rows for it in r]
    pa = _probe_assets(images); dl, _ = adapt(probe, {}, 'psp', pa, FontCache('psp'))
    k = 0; boxes, refs = [], []
    for r in rows:
        ts = dl[k:k + len(r)]; k += len(r)
        bb = [b for b in (ink_box(t, pa.bitmaps) for t in ts) if b]
        boxes.append([min(b[0] for b in bb), min(b[1] for b in bb), max(b[2] for b in bb), max(b[3] for b in bb)])
        lab = next((t for t in ts if t['type'] == T_TEXT and t.get('menu', -1) >= 0), None)
        refs.append(lab['dst'][1] if lab else boxes[-1][1])
    return boxes, refs


def _board_ink(items, page, images=None):
    """видимая рамка доски page на PSP (края картинок по альфе: у рамок диска крайние столбцы прозрачные)"""
    pr = [dict(it) for it in items if it['page'] == page and it['name'] != 'Frame_Bgd']
    pa = _probe_assets(images); dl, _ = adapt(pr, {}, 'psp', pa, FontCache('psp'))
    im = np.asarray(render(dl, DST_W, DST_H, {}, pa.bitmaps, Image.new('RGBA', (DST_W, DST_H), (0, 0, 0, 0))))[..., 3] >= 128
    ys, xs = np.where(im.any(1))[0], np.where(im.any(0))[0]
    return [int(xs[0]), int(ys[0]), int(xs[-1]) + 1, int(ys[-1]) + 1]


def board_fit(items, rows, page, K, *, hidden=(), pitch=None, width=None, centre_each=False,
              pad_y=PAUSE_FIT_PAD_Y, pad_x=PAUSE_FIT_PAD_X, top=BOARD_TOP, images=None, min_rows=0):
    """доска по содержимому. rows — строки сверху вниз (каждая — список элементов источника); hidden — номера строк, которых игрок не
    видит (в высоту не входят). pitch — шаг строк по буквам (None — промежутки строк из данных); centre_each — каждая строка по
    центру доски по своим буквам (колонка по центру), иначе весь блок одним сдвигом (две колонки: подпись и значение).
    Поля и центр меряются по видимой рамке и буквам (пробная раскладка, до двух поправок на округление). Ставит строки и рамку
    page, возвращает видимую рамку (x0, y0, x1, y1) PSP px."""
    boxes, refs = _row_inks(rows, images)
    vis = [i for i in range(len(rows)) if i not in hidden]
    nh = len(rows) - len(vis)
    assert not nh or (pitch and pitch % 2 == 0)
    rel = {i: (vis.index(i) * pitch if pitch else refs[i] - refs[0]) for i in vis}          # шаг строк игрока (по подписям)
    b0 = min(rel[i] + boxes[i][1] - refs[i] for i in vis); b1 = max(rel[i] + boxes[i][3] - refs[i] for i in vis)
    r0 = top + pad_y - b0
    ty = {i: (r0 + i * pitch - nh * pitch // 2 if pitch else r0 + rel[i]) for i in range(len(rows))}   # hideDy: вниз на полшага
    y1 = r0 + b1
    if min_rows and pitch and len(vis) < min_rows:   # правка 232: доска постоянной высоты (min_rows строк), строки по центру по высоте
        extra = (min_rows - len(vis)) * pitch
        ty = {i: v + extra // 2 for i, v in ty.items()}; y1 += extra
    if centre_each:
        cw = max(boxes[i][2] - boxes[i][0] for i in vis)
    else:
        x0, x1 = min(boxes[i][0] for i in vis), max(boxes[i][2] for i in vis); cw = x1 - x0
    bw = width or cw + 2 * pad_x; bw += bw & 1                      # чётная: центр доски — граница пикселей, как у чётной строки
    want = [DST_W // 2 - bw // 2, top, DST_W // 2 + bw // 2, y1 + pad_y]
    rect = list(want)
    for _ in range(3):                                              # видимая рамка = want (прозрачные края картинок рамки)
        place_board(items, page, (rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1]), K)
        fb = _board_ink(items, page, images)
        d = [w - f for w, f in zip(want, fb)]
        if not any(d): break
        rect = [r + e for r, e in zip(rect, d)]
    place_board(items, page, (rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1]), K, vis=fb)
    cx2 = fb[0] + fb[2]                                             # удвоенный центр видимой рамки
    for _ in range(3):                                              # строки: на свои места по буквам (поправка округления)
        moved = False
        for i, r in enumerate(rows):
            b = boxes[i]
            if centre_each:
                tx = (cx2 - (b[2] - b[0])) // 2
            else:
                tx = b[0] + (cx2 - (x0 + x1)) // 2
            dx, dy = tx - b[0], ty[i] - refs[i]
            if dx or dy:
                moved = True
                for it in r:
                    it['rect'] = [it['rect'][0] + dx / S, it['rect'][1] + dy / S] + list(it['rect'][2:])
        if not moved: break
        boxes, refs = _row_inks(rows, images)
        if not centre_each:
            x0, x1 = min(boxes[i][0] for i in vis), max(boxes[i][2] for i in vis)
    return fb


def board_title(base, text, fb):
    """заголовок над доской fb (видимая рамка x0, y0, x1, y1; как «PAUSE MENU»: pause_title_face — та же грань и место), белый"""
    tf, ttop, lh = pause_title_face(fb[1])
    return dict(base, type=T_TEXT, name='BoardTitle', path='BoardTitle', rect=psp_src(fb[0] - 60, ttop, fb[2] - fb[0] + 120, lh),
                font=tf, color=[255, 255, 255, 255], shadow=None, strings=[text], halign='centre', valign='top', tag='ALWAYS',
                field='NONE', fmt='none', sel=0, wrap=False, menu=-1)


LS_PITCH = 20   # LEVEL PROGRESS: шаг строк (px PSP); рамка паузы — по высоте 9 строк
LS_ROW_LH = 13   # LEVEL PROGRESS: высота строки текста строк (px PSP) — шрифт строк Tt2001m__14 крупнее до неё (читаемо, как пункты паузы)


def levelstats_layout(items):
    """LEVEL PROGRESS / LevelEnd (LevelStats.pag) в той же рамке, что пауза (PAUSE_BOARD, углы x PAUSE_K): заголовок над рамкой,
    как «PAUSE MENU»; 9 строк (название слева, значение справа) — порядок и тексты LevelStats.pag, шаг по высоте рамки;
    Back / Accept — в правом нижнем углу, подсказка UseMissionSelect — в левом нижнем (правило порта: у Scrooby она левее рамки)."""
    oxc = (DST_W - SRC_W * S) / 2
    src = lambda x, y, w, h: [(x - oxc) / S, y / S, w / S, h / S]
    bx, by, bw, bh = PAUSE_BOARD
    f0 = next(it['rect'] for it in items if it['name'] == 'Frame_Top')
    labels = sorted((it for it in items if it['page'] == 'LevelStats.pag' and it['type'] == T_TEXT and 'Stats' in it.get('path', '')
                     and it['field'] == 'NONE'), key=lambda t: t['rect'][1])
    vals = {round(it['rect'][1]): it for it in items if it['page'] == 'LevelStats.pag' and it['field'].startswith('STAT_') and it['field'] != 'STAT_MODE'}
    k = 1.0
    while FontCache('psp').get(face_mul('Tt2001m__14', k))['lh'] < LS_ROW_LH and k < 3: k = round(k + 0.05, 2)
    face = face_mul('Tt2001m__14', k)
    n = len(labels); pitch = LS_PITCH; bh = n * pitch + 28; y0 = by + 14   # рамка по строкам: под ней — подсказка UseMissionSelect
    fr = src(bx, by, bw, bh)
    corner = [it for it in items if it['page'] in ('Back.pag', 'Accept.pag')]
    out = []
    for it in items:
        if it['page'] == 'XSmallBoard.pag':
            if it['name'] == 'Frame_Bgd':
                x, y, w, h = it['rect']; K = PAUSE_K
                l, t = (x - f0[0]) * K, (y - f0[1]) * K
                r, b = (f0[0] + f0[2] - x - w) * K, (f0[1] + f0[3] - y - h) * K
                it['rect'] = [fr[0] + l, fr[1] + t, fr[2] - l - r, fr[3] - t - b]
            else:
                it['rect'] = list(fr); it['k'] = PAUSE_K
        elif it['name'] == 'LevelProgress':
            tf, top, lh = pause_title_face(by)
            it.update(rect=src(bx - 60, top, bw + 120, lh), font=tf, halign='centre', valign='top', title=1)
        elif it in labels:
            i = labels.index(it); cy = y0 + (i + .5) * pitch
            v = vals.get(round(it['rect'][1]))
            it.update(rect=src(bx + 14, cy - pitch / 2, bw - 28, pitch), font=face, halign='left', valign='centre')
            if v is not None:
                v.update(rect=src(bx + 14, cy - pitch / 2, bw - 28, pitch), font=face, halign='right', valign='centre')
        elif it['name'] == 'UseMissionSelect':
            f = FontCache('psp').get(face)
            it.update(rect=[SAFE_X / S, (by + bh + 3) / S, (DST_W * 0.8) / S, f['lh'] / S], font=face, halign='left', valign='top')
        elif it['page'] == 'PauseFgd.pag' and it['type'] == T_TEXT:
            continue                                        # «PRESS START» паузы здесь не нужна
        out.append(it)
    for grp in ('Back.pag', 'Accept.pag'):                  # значок + подпись — в правый нижний угол, как Accept паузы
        acc = [it for it in corner if it['page'] == grp]
        if not acc: continue
        rs = [list(it['rect']) for it in acc]
        x1 = max(r[0] + r[2] for r in rs); y1 = max(r[1] + r[3] for r in rs)
        tx, ty = SRC_W - SAFE_X / S, SRC_H - SAFE_Y / S
        for it, (x, y, w, h) in zip(acc, rs):
            it['rect'] = [tx - (x1 - x), ty - (y1 - y), w, h]
            if it['type'] == T_TEXT: it['halign'] = 'left'
    return out


VC_K = 1.3   # VIEW CARDS: раскладка PauseViewCards.pag крупнее простого масштаба (как рамка паузы, Роб 27.09 «мелко»), от центра экрана


def viewcards_layout(items):
    """VIEW CARDS (PauseViewCards.scr) в PSP px. Места карточек и шаг — из PauseViewCards.pag (центры мест Card0..6, шаг 120), размер
    карточки — рамка выбора frame (84x108 родного размера, в неё вписан рисунок карточки уровня: frontend.p3d card<NN>, NN =
    (уровень − 1) x 8 + k), несобранная — cardyell. Всё x VC_K от центра экрана. Выбор (◄ ► ▲ ▼) — рамка выбора и крупная карточка
    выбранного места (LowResCard.pag: родной размер 191x245) с названием, эпизодом и описанием из библии (CARD_TITLE_NN,
    CARD_EPISODE_NN, CARD_DESC_NN; правило порта: текст справа от крупной карточки). Метки экрана: SKIP — сетка (UI_T_VC_GRID),
    STREETRACE — карточка крупно (UI_T_VC_ZOOM). Заголовок — строка пункта паузы VIEW_CARDS над рамкой, как «PAUSE MENU»."""
    oxc = (DST_W - SRC_W * S) / 2
    src = lambda x, y, w, h: [(x - oxc) / S, y / S, w / S, h / S]
    psp = lambda x, y: ((x * S + oxc - DST_W / 2) * VC_K + DST_W / 2, (y * S - DST_H / 2) * VC_K + DST_H / 2)   # источник -> PSP x VC_K
    A = all_images(); B = _BIBLE or load_bible(LANG)
    base = next(it for it in items if it['page'] == 'PauseViewCards.pag')
    slots = {int(it['name'][4:]): it for it in items if it['page'] == 'PauseViewCards.pag' and it['name'][:4] == 'Card' and it['name'][4:].isdigit()}
    frame = next(it for it in items if it['name'] == 'CardFrame')
    fw, fh = (A['frame.png'].size if 'frame.png' in A else (84, 108))
    cw, ch = fw * S * VC_K, fh * S * VC_K
    big = next((it for it in items if it['page'] == 'LowResCard.pag'), None)
    out = []
    bx, by, bw, bh = 34, 34, 412, 202                      # рамка BigBoard (PSP): вокруг сетки
    fr = src(bx, by, bw, bh); f0 = next(it['rect'] for it in items if it['page'] == 'BigBoard.pag' and it['name'] == 'Frame_Top')
    for it in items:
        if it['page'] == 'BigBoard.pag':
            if it['name'] == 'Frame_Bgd':
                x, y, w, h = it['rect']; K = PAUSE_K
                l, t = (x - f0[0]) * K * .5, (y - f0[1]) * K * .5
                r, b = (f0[0] + f0[2] - x - w) * K * .5, (f0[1] + f0[3] - y - h) * K * .5
                it['rect'] = [fr[0] + l, fr[1] + t, fr[2] - l - r, fr[3] - t - b]
            else:
                it['rect'] = list(fr); it['k'] = PAUSE_K * .5
            out.append(it)
        elif it['page'] in ('PauseBgd.pag', 'Back.pag', 'Accept.pag'):
            out.append(it)
    for i, sl in sorted(slots.items()):
        x, y, w, h = sl['rect']; cx, cy = psp(x + w / 2, y + h / 2)
        k = (LEVEL - 1) * 8 + i   # правка 364: на диске по 8 на уровень (7 карточек + «LEVEL N BONUS CARD» пустышка 8×8: card07, card15…); было × 7 — со 2-го уровня чужие карточки
        img = 'card%02d.png' % k
        r = src(cx - cw * .46, cy - ch * .46, cw * .92, ch * .92)
        out.append(dict(sl, name='Card%d' % i, rect=r, frames=['cardyell.png', img], field='VC%d' % i, tag='SKIP', jx='centre', jy='centre'))
        out.append(dict(frame, name='CardFrame%d' % i, rect=src(cx - cw / 2, cy - ch / 2, cw, ch), menu=i, selonly=True, tag='SKIP',
                        jx='centre', jy='centre'))
        if big is not None:   # крупно: слева в рамке, пропорции LowResCard.pag (191x245), на всю высоту рамки
            gx, gy, gw, gh = big['rect']; zh = bh - 28; zw = zh * gw / gh
            zx, zy = bx + 14, by + 14
            out.append(dict(big, name='BigCard%d' % i, rect=src(zx, zy, zw, zh), frames=[img], menu=i, selonly=True, tag='STREETRACE',
                            jx='centre', jy='centre', unit=base['unit'], forced=base.get('forced')))   # якорь — как у сетки (C)
            tx = zx + zw + 14; tw = bx + bw - 12 - tx
            txt = dict(base, type=T_TEXT, color=[255, 255, 255, 255], shadow=None, sel=0, menu=i, selonly=True, tag='STREETRACE',
                       field='NONE', fmt='none', valign='top', halign='left')
            out.append(dict(txt, name='CardTitle%d' % i, rect=src(tx, zy + 4, tw, 34), font='boulder_16*1.3', color=[255, 220, 40, 255],
                            strings=[B('CARD_TITLE_%02d' % k) or ''], wrap=True, subset=True))
            out.append(dict(txt, name='CardEpisode%d' % i, rect=src(tx, zy + 44, tw, 30), font='Tt2001m__14*1.2',
                            strings=[B('CARD_EPISODE_%02d' % k) or ''], wrap=True, subset=True))
            qw = VC_QR_W if ':' in (B('CARD_EPISODE_%02d' % k) or '') else 0   # идея 85: справа внизу QR эпизода (viewer/main.c vcQrDraw)
            out.append(dict(txt, name='CardDesc%d' % i, rect=src(tx, zy + 80, tw - qw, zh - 80), font='Tt2001m__14*1.2',
                            strings=[B('CARD_DESC_%02d' % k) or ''], wrap=True, subset=True))
    tf, top, lh = pause_title_face(by)
    title = dict(base, type=T_TEXT, name='ViewCardsTitle', rect=src(bx, top, bw, lh), font=tf, color=[255, 255, 255, 255], shadow=None,
                 strings=[B('VIEW_CARDS') or 'VIEW CARDS'], halign='centre', valign='top', title=1, tag='ALWAYS', field='NONE',
                 fmt='none', sel=0, wrap=False, menu=-1)
    out.append(title)
    if not UI_DENSE:                                        # правка 163: подписи крупнее — «✕ Accept  △ Back» одной строкой в угол
        for it in out:                                      # через ровный зазор (hints_to_corner), а не Accept на 110 левее
            if it['page'] == 'Accept.pag': it['tag'] = 'SKIP'
        hints_to_corner(out)
        return out
    for grp in ('Back.pag', 'Accept.pag'):                  # значок + подпись — в углы: Back справа, Accept (смотреть) слева от него
        acc = [it for it in out if it['page'] == grp]
        if not acc: continue
        if grp == 'Accept.pag':
            for it in acc: it['tag'] = 'SKIP'
        rs = [list(it['rect']) for it in acc]
        x1 = max(r[0] + r[2] for r in rs); y1 = max(r[1] + r[3] for r in rs)
        tx, ty = SRC_W - SAFE_X / S - (0 if grp == 'Back.pag' else 110), SRC_H - SAFE_Y / S
        for it, (x, y, w, h) in zip(acc, rs):
            it['rect'] = [tx - (x1 - x), ty - (y1 - y), w, h]
            if it['type'] == T_TEXT: it['halign'] = 'left'
    return out


MS_K = 1.0       # MISSION SELECT: места MissionSelect.pag в простом масштабе (страница на всю ширину 4:3 — крупнее не входит в рамку)
MS_FONT_K = 1.2  # ...шрифты строк крупнее (читаемо на PSP, как пункты паузы) — только прежняя раскладка (UI_MS_OLD)
# Правка Роба 348 (05.10, голосом): «названий миссий нет, LEVEL 1 и Status — шрифты кривые, строки очень плотные, переключение уровней».
# [диск] MissionSelect.pag: 7 строк (Mission0..6, у каждой 7 строк текста — MISSION_TITLE_L1..L7_M1..M7), грань строк font1_14 =
# Tt2001m__14, LEVEL N — font0_16 = boulder_16 (чёрная на планке LevelBar), Status — font1_14 жёлтая; шаг строк 25 при строке грани 13
# (×1,9); полоса выбора HighlightBar — чёрная 50 % во всю ширину доски (61..577 при заливке 60..580), высота = шаг; за дырами стрелок
# планки — шарики LArrowBgd/RArrowBgd (greybgd / redbgd). Грани 1:1 (правка 136: пиксель-перфект только при целом масштабе) —
# дробные *1.2 пересэмплировали буквы («кривые»). Шаг: при строке 13 px шаг оригинала ×1,9 = 25 px не входит в доску (7 строк),
# MS_PITCH = 18 px — правило порта (ручка панели типографики ms_rows: pitch). UI_MS_OLD=1 — раскладка до 348 (красный контроль)
MS_OLD = bool(os.environ.get('UI_MS_OLD'))
MS_ROW_FACE = os.environ.get('UI_MS_ROW_FACE', 'Tt2001m__14@1')     # номер, название, Status: грань строк оригинала 1:1 (13 px)
MS_LEVEL_FACE = os.environ.get('UI_MS_LEVEL_FACE', 'boulder_16@1')  # LEVEL N: грань оригинала 1:1, как пункты паузы (правка 201)
# Правка Роба 363 (05.10, голосом): «LEVEL 3 и Status выше внутри рамки, ближе к верху, свободнее; Status немножечко от края; LEVEL на
# планке — вверх; межстрочный красивее; номера неровные (2, 3 вылазят) — по левому краю равны, отступ от края чуть больше». Поле MS_EDGE
# от жёлтой линии рамки до букв — одно со всех сторон: слева номера (были 6–8 px, разные), справа Status (было 2 px), сверху планка
# LEVEL N (было 25 px). Номера — по левому краю букв (у «1» поле слева шире, у «2», «3» уже: по перу они вылезали на 1–2 px).
# [диск] levelbar.png 131×20: лицо планки — строки 0..15, ниже тёмный кант и тень (16..18) — LEVEL N и Status по центру лица (у оригинала
# Level и Status на одной высоте, 127 и 127,3 из 480). Шаг: свободно 195 − 2 × MS_EDGE − 22 (планка) = 153 px на 7 строк и зазор под
# планкой; 25 (оригинал) не входит, 21 оставляет под планкой 6 px — MS_PITCH 20, зазор 13 (правило порта). UI_MS_348=1 — раскладка 348
MS_348 = bool(os.environ.get('UI_MS_348'))
MS_PITCH = int(os.environ.get('UI_MS_PITCH', '18' if MS_348 else '20'))   # шаг строк, px PSP (правило порта, см. выше)
MS_EDGE = int(os.environ.get('UI_MS_EDGE', '10'))   # правка 363: от жёлтой линии рамки до букв (номера, Status) и до планки LEVEL N
MS_NUM_GAP = 9      # правка 363: от правого края букв самого широкого номера до начала названия (как было: «6.» 57 -> 66)
MS_BAR_FACE = 16 / 20.0   # [диск] levelbar.png: лицо планки — 16 строк из 20


def missionselect_layout(items):
    """MISSION SELECT (MissionSelect.scr): всё из MissionSelect.pag на своих местах (x MS_K от центра), шрифты x MS_FONT_K.
    Строка k: номер «k+1.», название миссии уровня (строка LEVEL−1 текста Mission<k>: у всех 7 уровней это 7 последних сюжетных
    миссий), значок состояния MissionStatus<k> (dash / cross / check по полю MS<k>), полоса выбора HighlightBar — копия на каждую
    строку, видна у выбранной; название — пункт меню (выбранная — жёлтая, недоступная — серая). LEVEL N — строка уровня, стрелки
    уровней не показываются (выбор уровня — в главном меню). Заголовок — строка пункта паузы MISSION_SELECT над рамкой."""
    oxc = (DST_W - SRC_W * S) / 2
    src = lambda x, y, w, h: [(x - oxc) / S, y / S, w / S, h / S]
    B = _BIBLE or load_bible(LANG)
    k0 = lambda x, y, w, h: [SRC_W / 2 + (x - SRC_W / 2) * MS_K, SRC_H / 2 + (y - SRC_H / 2) * MS_K, w * MS_K, h * MS_K]
    rows = {}
    for it in items:
        if it['page'] != 'MissionSelect.pag': continue
        for pre in ('MissionNum', 'MissionStatus', 'Mission'):
            if it['name'].startswith(pre) and it['name'][len(pre):].isdigit():
                rows.setdefault(int(it['name'][len(pre):]), {})[pre] = it; break
    bar = next((it for it in items if it['name'] == 'HighlightBar'), None)
    out = []
    bx, by, bw, bh = 34, 34, 412, 202
    fr = src(bx, by, bw, bh); f0 = next(it['rect'] for it in items if it['page'] == 'BigBoard.pag' and it['name'] == 'Frame_Top')
    base = next(it for it in items if it['page'] == 'MissionSelect.pag')
    for it in items:
        if it['page'] == 'BigBoard.pag':
            if it['name'] == 'Frame_Bgd':
                x, y, w, h = it['rect']; K = PAUSE_K * .5
                l, t = (x - f0[0]) * K, (y - f0[1]) * K
                r, b = (f0[0] + f0[2] - x - w) * K, (f0[1] + f0[3] - y - h) * K
                it['rect'] = [fr[0] + l, fr[1] + t, fr[2] - l - r, fr[3] - t - b]
            else:
                it['rect'] = list(fr); it['k'] = PAUSE_K * .5
            out.append(it)
        elif it['page'] in ('PauseBgd.pag', 'Back.pag', 'Accept.pag'):
            out.append(it)
        elif it['page'] == 'MissionSelect.pag':
            if (MS_OLD and it['name'] in ('LArrowBgd', 'RArrowBgd')) or it['name'] == 'HighlightBar' or it['name'] == 'Status': continue
            if any(it is r.get(pre) for r in rows.values() for pre in r): continue
            it['rect'] = k0(*it['rect'])
            if it['name'] in ('LArrowBgd', 'RArrowBgd'):   # правка 348: шарики за стрелками планки — кадр по полю (0 серый, 1 красный)
                it['field'] = 'MS_LARROW' if it['name'] == 'LArrowBgd' else 'MS_RARROW'
            if it['type'] == T_TEXT:
                if MS_OLD:
                    it['font'] = face_mul(it['font'], MS_FONT_K)
                    if it['name'] == 'Level': it['strings'] = [it['strings'][min(LEVEL - 1, len(it['strings']) - 1)]]
                elif it['name'] == 'Level':   # правка 348: строки всех 7 уровней, видна строка поля MS_LEVEL (уровень на экране)
                    it.update(font=MS_LEVEL_FACE, field='MS_LEVEL', fmt='select')
            out.append(it)
    for k, r in sorted(rows.items()):
        if bar is not None:
            y = r['Mission']['rect'][1]; x, _, w, h = bar['rect']
            out.append(dict(bar, name='HighlightBar%d' % k, rect=k0(x, y + r['Mission']['rect'][3] / 2 - h / 2, w, h), menu=k, selonly=True))
        for pre in ('MissionNum', 'MissionStatus', 'Mission'):
            it = r.get(pre)
            if it is None: continue
            it['rect'] = k0(*it['rect'])
            if it['type'] == T_TEXT: it['font'] = face_mul(it['font'], MS_FONT_K) if MS_OLD else MS_ROW_FACE
            if pre == 'Mission':
                if MS_OLD: it.update(strings=[it['strings'][min(LEVEL - 1, len(it['strings']) - 1)]], menu=k, sel=0)
                else: it.update(menu=k, sel=0, field='MS_LEVEL', fmt='select')   # правка 348: названия всех 7 уровней (◄ ►)
            if pre == 'MissionStatus':
                it['field'] = 'MS%d' % k
            out.append(it)
    st = next((it for it in items if it['name'] == 'Status'), None)
    if st is not None:
        st['rect'] = k0(*st['rect']); st['font'] = face_mul(st['font'], MS_FONT_K) if MS_OLD else MS_ROW_FACE; out.append(st)
    tf, top, lh = pause_title_face(by)
    out.append(dict(base, type=T_TEXT, name='MissionSelectTitle', rect=src(bx, top, bw, lh), font=tf, color=[255, 255, 255, 255],
                    shadow=None, strings=[B('MISSION_SELECT') or 'MISSION SELECT'], halign='centre', valign='top', title=1,
                    tag='ALWAYS', field='NONE', fmt='none', sel=0, wrap=False, menu=-1))
    if not UI_DENSE:                                        # правка 163: «✕ Accept  △ Back» одной строкой в угол, ровный зазор
        hints_to_corner(out)
        return out
    for grp in ('Back.pag', 'Accept.pag'):
        acc = [it for it in out if it['page'] == grp]
        if not acc: continue
        rs = [list(it['rect']) for it in acc]
        x1 = max(r[0] + r[2] for r in rs); y1 = max(r[1] + r[3] for r in rs)
        tx, ty = SRC_W - SAFE_X / S - (0 if grp == 'Back.pag' else 110), SRC_H - SAFE_Y / S
        for it, (x, y, w, h) in zip(acc, rs):
            it['rect'] = [tx - (x1 - x), ty - (y1 - y), w, h]
            if it['type'] == T_TEXT: it['halign'] = 'left'
    return out


def missionselect_psp(dl, fonts):
    """Правка 348: MISSION SELECT в пикселях PSP (после adapt: номера, названия и планка — к левой трети, значки — к правой).
    Строки шагом MS_PITCH от места первой строки оригинала (под планкой LEVEL N тот же зазор); строка грани и значок — по центру
    своей строки; полоса выбора — во всю заливку доски (у оригинала 61..577 при заливке 60..580), высотой в шаг строки (25 при 25);
    название — от номера на отступ оригинала 1:1 (грань 1:1), рамка — до столбца значков (самое длинное «There's Something About
    Monty» 253 px); LEVEL N — по центру планки.
    UI_MS_OLD — список как до 348 (у полосы был центр 4:3: 93..385 при номерах с 45 и значках у 408)."""
    if MS_OLD:
        return dl
    if not MS_348:
        return missionselect_363(dl, fonts)
    by = {t['name']: t for t in dl}
    bgd = by.get('Frame_Bgd')
    rows = sorted(int(t['name'][7:]) for t in dl if t['type'] == T_TEXT and t['name'][:7] == 'Mission' and t['name'][7:].isdigit())
    if not rows or bgd is None:
        return dl
    m0 = by['Mission%d' % rows[0]]
    c0 = m0['dst'][1] + m0['dst'][3] / 2.0
    bx, _, bw, _ = bgd['dst']
    x0, x1 = bx + max(1, int(round(1 * S))), bx + bw - max(1, int(round(3 * S)))
    for k in rows:
        c = c0 + (k - rows[0]) * MS_PITCH
        q = by.get('MissionStatus%d' % k)
        for nm in ('MissionNum%d' % k, 'Mission%d' % k):
            t = by.get(nm)
            if t is None:
                continue
            lh = fonts.get(t['font'])['lh']; x, _, w, _ = t['dst']
            n = by.get('MissionNum%d' % k)
            if nm[7:8] != 'N' and n is not None:   # номер и название набраны гранью 1:1 — и отступ между ними 1:1 (у оригинала 100 − 80)
                x = n['dst'][0] + int(round(t['rect'][0] - n['rect'][0]))
            if nm[7:8] != 'N' and q is not None:
                w = max(w, q['dst'][0] - 4 - x)
            t['dst'] = [x, int(math.floor(c - lh / 2.0 + .5)), w, lh]
        if q is not None:
            q['dst'] = [q['dst'][0], int(math.floor(c - q['dst'][3] / 2.0 + .5))] + list(q['dst'][2:])
        b = by.get('HighlightBar%d' % k)
        if b is not None:
            b['dst'] = [x0, int(math.floor(c - MS_PITCH / 2.0 + .5)), x1 - x0, MS_PITCH]
    lv, bar = by.get('Level'), by.get('LevelBar')
    if lv is not None and bar is not None:
        lh = fonts.get(lv['font'])['lh']
        lv['dst'] = [lv['dst'][0], int(math.floor(bar['dst'][1] + bar['dst'][3] / 2.0 - lh / 2.0 + .5)), lv['dst'][2], lh]
    return dl


def missionselect_363(dl, fonts):
    """Правка 363: поле MS_EDGE от жёлтой линии рамки — слева до букв номеров (все номера по одному левому краю букв), справа до букв
    Status (столбец значков — по центру Status), сверху до планки LEVEL N; LEVEL N и Status — по центру лица планки (MS_BAR_FACE);
    7 строк шагом MS_PITCH снизу на поле MS_EDGE, буквы строки (высота цифр) — по центру полосы выбора; полоса — во всю заливку.
    Центры — в координатах границ пикселей (верх буквы t0, низ t1 — glyph_ink, низ не включён)."""
    by = {t['name']: t for t in dl}
    bgd = by.get('Frame_Bgd')
    rows = sorted(int(t['name'][7:]) for t in dl if t['type'] == T_TEXT and t['name'][:7] == 'Mission' and t['name'][7:].isdigit())
    if not rows or bgd is None:
        return dl
    fx, fy, fw, fh = bgd['dst']
    # жёлтая линия рамки (кадр эмулятора): слева кончается на fx, сверху — на fy − 1, справа начинается на fx + fw − 1 (перед ней
    # чёрный кант 2 px), снизу — на fy + fh: внутреннее поле — fx + 1 .. fx + fw − 2, fy .. fy + fh − 1
    left, top, right, bottom = fx + 1, fy, fx + fw - 2, fy + fh - 1
    xi = left + MS_EDGE                                         # левый край букв номеров
    ink = lambda t, s: glyph_ink(t['font'], s)
    # планка LEVEL N (+ шарики стрелок + строка): левый край — по номерам, верх — на поле MS_EDGE
    bar = by.get('LevelBar'); face_c = None
    if bar is not None:
        dx, dy = xi - bar['dst'][0], top + MS_EDGE - bar['dst'][1]
        for nm in ('LevelBar', 'LArrowBgd', 'RArrowBgd', 'Level'):
            t = by.get(nm)
            if t is not None:
                t['dst'] = [t['dst'][0] + dx, t['dst'][1] + dy] + list(t['dst'][2:])
        face_c = bar['dst'][1] + bar['dst'][3] * MS_BAR_FACE / 2.0
        lv = by.get('Level')
        if lv is not None:
            a = [ink(lv, sv) for sv in lv['strings'] if sv]; t0 = min(q[2] for q in a); t1 = max(q[3] for q in a)
            lv['dst'] = [lv['dst'][0], int(math.floor(face_c - (t0 + t1) / 2.0)),   # чуть выше центра, как у оригинала
                      lv['dst'][2], fonts.get(lv['font'])['lh']]
    # строки: 7 полос шагом MS_PITCH, низ последней — на поле MS_EDGE от нижней линии
    n = len(rows); y_end = bottom - MS_EDGE + 1; y0 = y_end - n * MS_PITCH
    nums = [by.get('MissionNum%d' % k) for k in rows]
    wnum = max((ink(t, t['strings'][0])[1] - ink(t, t['strings'][0])[0]) for t in nums if t is not None)
    xname = xi + wnum + MS_NUM_GAP
    st = by.get('Status'); col_c = None
    if st is not None:                                          # Status: правый край букв на поле MS_EDGE, по центру лица планки
        sx0, sx1, st0, st1 = ink(st, st['strings'][0]); W = int(round(face_width(st['font'], st['strings'][:1])))
        pen = right - MS_EDGE - (sx1 - 1)
        lh = fonts.get(st['font'])['lh']
        yc = face_c if face_c is not None else st['dst'][1] + st['dst'][3] / 2.0
        st['dst'] = [pen, int(math.floor(yc - (st0 + st1) / 2.0)), W, lh]
        st['valign'] = 'top'; col_c = pen + (sx0 + sx1) / 2.0
    for i, k in enumerate(rows):
        c = y0 + i * MS_PITCH + MS_PITCH / 2.0
        q = by.get('MissionStatus%d' % k)
        if q is not None:
            x = int(math.floor(col_c - q['dst'][2] / 2.0 + .5)) if col_c is not None else q['dst'][0]
            q['dst'] = [x, int(math.floor(c - q['dst'][3] / 2.0 + .5))] + list(q['dst'][2:])
        t = by.get('Mission%d' % k); yrow = None
        if t is not None:   # номер и название — одна строка грани (общая линия букв): высота цифр и заглавных — по центру полосы
            _, _, c0, c1 = ink(t, '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'); lh = fonts.get(t['font'])['lh']
            yrow = int(math.floor(c - (c0 + c1) / 2.0 + .5))
            w = (q['dst'][0] - 4 - xname) if q is not None else t['dst'][2]
            t['dst'] = [xname, yrow, w, lh]
        t = by.get('MissionNum%d' % k)
        if t is not None:
            x0 = ink(t, t['strings'][0])[0]; lh = fonts.get(t['font'])['lh']
            t['dst'] = [xi - x0, yrow if yrow is not None else t['dst'][1], max(t['dst'][2], wnum + x0), lh]
        b = by.get('HighlightBar%d' % k)
        if b is not None:
            b['dst'] = [fx + max(1, int(round(1 * S))), int(round(c - MS_PITCH / 2.0)), fw - max(1, int(round(1 * S))) - max(1, int(round(3 * S))),
                        MS_PITCH]
    return dl


MENU_ARROWS_ITEM = os.environ.get('MENU_ARROWS_ITEM_OLD') != '1'
MENU_ARROW_GAP = 28   # ед. исходника (≈ 16 px PSP): от края слова до стрелки при наибольшем пульсе 1,0 + тень


def pause_layout(items):
    """Pause screen laid out in PSP pixels (converted back to source space for the uniform S): the board and
    the menu bigger and vertically centred (PAUSE_*), the title above the board (Rob 28.09), X Accept in the bottom-right
    corner, PRESS START (hidden by default) in the bottom-left one.  The mission-objective box would sit under
    the bigger board -> not on the pause screen (the HUD still shows the objective)."""
    oxc = (DST_W - SRC_W * S) / 2                        # anchor C: dst x = src x * S + oxc

    def src(x, y, w, h):
        return [(x - oxc) / S, y / S, w / S, h / S]

    def text_extent(it):                                 # visible text in source space (one line)
        # ширина — адвансы грани на PSP (face_width: грань 1:1 и трекинг правки 163 — честно; прежде — данные x glyph_scale)
        tw = face_width(it['font'], it['strings']) / S if any(it['strings']) else it['rect'][2]
        x, y, w, h = it['rect']
        x0 = x if it['halign'] == 'left' else (x + w - tw if it['halign'] == 'right' else x + (w - tw) / 2)
        return [x0, y, tw, h]

    bx, by, bw, bh = PAUSE_BOARD
    fr = src(bx, by, bw, bh)
    f0 = next(it['rect'] for it in items if it['name'] == 'Frame_Top')      # the original frame box
    menu = sorted((it for it in items if it['page'] in ('PauseSunday.pag', 'PauseMission.pag') and it['menu'] >= 0), key=lambda t: t['rect'][1])
    midx = {id(m): i for i, m in enumerate(menu)}
    acc = [it for it in items if it['page'] == 'Accept.pag']
    out = []
    for it in items:
        if it['page'] == 'PauseMissionObjective.pag':
            continue
        if it['page'] == 'XSmallBoard.pag' and not PAUSE_OLD:
            pass                                           # правки 150/151: рамку ставит board_fit (по содержимому)
        elif it['page'] == 'XSmallBoard.pag':
            if it['name'] == 'Frame_Bgd':                  # dark fill: the original insets, x PAUSE_K
                x, y, w, h = it['rect']; K = PAUSE_K
                l, t = (x - f0[0]) * K, (y - f0[1]) * K
                r, b = (f0[0] + f0[2] - x - w) * K, (f0[1] + f0[3] - y - h) * K
                it['rect'] = [fr[0] + l, fr[1] + t, fr[2] - l - r, fr[3] - t - b]
            else:
                it['rect'] = list(fr); it['k'] = PAUSE_K
        elif it['page'] == 'Pause.pag' and it['type'] == T_TEXT:            # "PAUSE MENU": оглавление, не пункт (Роб 28.09,
            face, top, lh = pause_title_face(by)                             # трижды) — над рамкой, крупно, цвет Pause.pag
            it.update(rect=src(bx - 60, top, bw + 120, lh), font=face, halign='centre', valign='top', title=1)
        elif id(it) in midx:   # меньше 8 строк (пауза миссии, 5) — строки по центру рамки, шаг тот же
            cy = by + PAUSE_ITEM0_DY + (midx[id(it)] + (8 - len(menu)) / 2) * PAUSE_PITCH
            it.update(rect=src(bx + 12, cy - PAUSE_PITCH / 2, bw - 24, PAUSE_PITCH),
                      font=PAUSE_FONT, halign='centre', valign='centre')
        elif it['page'] == 'Coins.pag':                  # счёт монет — на своём месте HUD (Роб 28.09: в углу у самой кромки резался);
            pass                                         # рамка паузы (x 120..360) его не задевает
        elif it['page'] == 'PauseFgd.pag' and it['type'] == T_TEXT:
            x, y, w, h = text_extent(it)
            it.update(rect=[SAFE_X / S, SRC_H - SAFE_Y / S - h, w, h], halign='left')
        out.append(it)
    if not PAUSE_OLD:                                    # правки 150/151: доска по содержимому (board_fit), строки шагом PAUSE_FIT_PITCH
        rows = [[it] for it in menu]
        for it in menu:
            it.update(font=PAUSE_FONT, halign='centre', valign='top', wrap=False)   # блок строк по центру (якорь C), буквы — board_fit
        board_fit(out, rows, 'XSmallBoard.pag', PAUSE_K, hidden={i for i, it in enumerate(menu) if it['name'] in PAUSE_SERVICE_ROWS},
                  pitch=PAUSE_FIT_PITCH, width=pause_board_width(), centre_each=True, min_rows=PAUSE_MIN_ROWS)
    if acc:                                              # X icon + "Accept": the whole unit into the corner
        rs = [text_extent(it) if it['type'] == T_TEXT else list(it['rect']) for it in acc]
        x1 = max(r[0] + r[2] for r in rs); y1 = max(r[1] + r[3] for r in rs)
        tx, ty = SRC_W - SAFE_X / S, SRC_H - SAFE_Y / S   # corner (anchor R: dst = src * S + DST_W - SRC_W * S)
        for it, (x, y, w, h) in zip(acc, rs):
            it['rect'] = [tx - (x1 - x), ty - (y1 - y), w, h]
            if it['type'] == T_TEXT:
                it['halign'] = 'left'
    return out


def build_items(screen):
    """Draw items for one PSP screen, source space.  Each item: dict with 'type' etc."""
    items = []
    icon_defs = {}
    for fname, page in SCREEN_PAGES[screen]:
        prj = project(fname)
        speed_digits = []
        for path, e in iter_leaves(prj.pages[page]):
            name = e['name']
            tag = tag_for(page, path, name)
            if screen == 'main' and page in ('MainMenu.pag', 'Accept3.pag'):
                tag = 'SKIP'   # viewer/ui.h UI_T_MAIN_ROW: строка меню и подсказка скрыты, пока камера летит (рамка TVFrame.pag — нет)
            if screen == 'tutorial' and page == 'Continue.pag':
                tag = 'STREETRACE'   # viewer/ui.h UI_T_TUT_CONTINUE
            if screen == 'message': tag = 'ALWAYS'   # окно сообщения: всё видно сразу (Continue — X)
            if screen == 'tutorial' and name == 'TutorialMessage':
                tag = 'SKIP'         # строки подсказки рисует viewer/ui.c uiDrawTutorial (по одной, по центру места)
            if screen == 'levelstats':   # viewer/ui.h UI_T_LS_PAUSE / UI_T_LS_END
                tag = 'SKIP' if page in ('PauseBgd.pag', 'PauseFgd.pag', 'Back.pag') else \
                      'STREETRACE' if page == 'Accept.pag' or name == 'UseMissionSelect' else 'ALWAYS'
            unit, forced = unit_for(page, path, name)
            base = dict(name=name, page=page, path='/'.join(path), tag=tag, unit=unit, forced=forced,
                        field='NONE', fmt='none', menu=-1)
            kind = e['kind']
            if kind == 'p3d':
                if name == 'Map0':
                    items.append(dict(base, type=T_RADARMAP, rect=[e['x'], ydown(e['y'], e['h']), e['w'], e['h']],
                                      color=[255, 255, 255, 255]))
                elif page == '3dFE.pag' and name == 'CamAndSet':
                    # 3D living-room set: not portable here -> full-screen placeholder backdrop
                    items.append(dict(base, type=T_RECT, rect=[0, 0, SRC_W, SRC_H], color=[34, 40, 92, 255],
                                      colors=[[70, 60, 120, 255], [70, 60, 120, 255], [20, 24, 60, 255], [20, 24, 60, 255]],
                                      fill=True, note='placeholder for the 3D FE scene (camset)'))
                continue    # hudhole, iris, gags...: 3D objects, handled by code or out of scope
            if kind == 'poly':
                col = e['colors']
                if all(c[3] == 0 for c in col):
                    continue                              # invisible clip boxes (RadarConeBox0)
                pts = [[x, SRC_H - y] for x, y in e['points']]
                rect = [e['x'], ydown(e['y'], e['h']), e['w'], e['h']]
                full = e['x'] <= 0 and e['x'] + e['w'] >= SRC_W
                is_rect = len(pts) == 4 and len({round(x) for x, _ in pts}) == 2 and len({round(y) for _, y in pts}) == 2
                if name == 'RadarCone0':
                    col = [[c[0], c[1], c[2], 96] for c in col]
                    col[0] = col[0][:3] + [150]           # translucent: apex stronger than the far edge
                if is_rect:
                    it = dict(base, type=T_RECT, rect=rect, color=col[0], fill=full)
                    if len({tuple(c) for c in col}) > 1:
                        it['colors'] = col
                else:
                    it = dict(base, type=T_POLY, rect=rect, points=pts, colors=col)
                if name == 'RadarCone0':
                    it['cone'] = True                     # правка Роба 50: код поворачивает клин на цель (res[0] бит 2)
                items.append(it)
                continue
            if kind == 'sprite':
                imgs = e['images']
                rect = [e['x'], ydown(e['y'], e['h']), e['w'], e['h']]
                col = e['color']
                grp = path[-1] if path else ''
                if grp in ICONS or name.startswith('IconPlayerCar'):
                    g = 'PlayerCars' if name.startswith('IconPlayerCar') else grp
                    if g not in icon_defs:
                        icon_defs[g] = dict(image=prj.res_images[imgs[0]], color=col, w=e['w'], h=e['h'],
                                            rect=rect, jx=e['jx'], jy=e['jy'])
                        if not any(i['type'] == T_RADARICONS for i in items):
                            items.append(dict(base, type=T_RADARICONS, rect=rect, color=col))
                    continue
                if len(imgs) > 20 and 'dot' in imgs:          # sprite-font number field
                    it = dict(base, type=T_NUM, rect=rect, color=col, chars=sprite_font_map(imgs),
                              halign=e['jx'], valign=e['jy'], text='')
                    binding = {'NumCoins': ('COINS', 'int', '1234'), 'Timer': ('TIMER', 'time', '1:23'),
                               'ParTime': ('PARTIME', 'time', '2:00'), 'Lap': ('LAP', 'frac', '1/3'),
                               'Position': ('POSITION', 'int', '2'), 'ItemsCount': ('CARDS', 'frac', '3/7'),
                               'CountDown': ('COUNTDOWN', 'int', '3'), 'Collectibles': ('COLLECTED', 'frac', '4/10'),
                               'MissionComplete': ('MSN_DONE', 'none', 'MISSION COMPLETE!'), 'MissionFailed': ('NONE', 'none', 'FAILED'),
                               'TutorialMessage': ('NONE', 'none', '')}
                    # PS2 race unknown-position glyph is dynamically selected; it
                    # exists in the resource dictionary, outside Position's list.
                    if name == 'Position' and os.environ.get('C3_RACE_OFFROAD_OLD') != '1':
                        it['chars']['?'] = 'question'
                    f, fm, sample = binding.get(name, ('NONE', 'none', ''))
                    it.update(field=f, fmt=fm, text=sample)
                    if name == 'MissionComplete' and MSN_DONE_K != 1.0:   # правка 216: крупно, на всю ширину кадра (поля ~20 px)
                        x_, y_, w_, h_ = it['rect']; it.update(glyph_k=MSN_DONE_K, rect=[x_, y_ + h_ / 2 - h_ * MSN_DONE_K / 2, w_, h_ * MSN_DONE_K])
                    if page == 'Tutorial.pag' and name == 'TutorialMessage':   # тексты подсказок: TUTORIAL_NNN библии (PS2-вариант)
                        it.update(texts=tutorial_texts(), glyph_k=TUT_GLYPH_K)
                    items.append(it)
                    continue
                if name == 'MissionStartBitmap':           # картинка задания из данных миссии (SetPresentationBitmap)
                    items.append(dict(base, type=T_IMG, rect=rect, color=col, frames=[], jx=e['jx'], jy=e['jy'], ext=1))
                    continue
                if name == 'CardCollected':
                    continue     # card00_t..card55_t on the disc are white _stubs; real art is loaded elsewhere
                if name == 'ObjectiveIcon':
                    icons = sorted(v for k, v in prj.res_images.items() if k.startswith('q') and k[1:].isalpha())
                    OBJ_ICONS.update(icons)
                    items.append(dict(base, type=T_SLOT, rect=rect, color=col, images=icons, field='OBJICON'))
                    continue
                if name.startswith('HnRSector'):
                    k = int(name[len('HnRSector'):])
                    items.append(dict(base, type=T_SECTOR, rect=rect, color=col, sector=k,
                                      image=prj.res_images[imgs[0]], jx=e['jx'], jy=e['jy'], field='HNR'))
                    continue
                if name in ('DamageBar', 'ProximityBar'):
                    items.append(dict(base, type=T_BAR, rect=rect, color=col, image=prj.res_images[imgs[0]],
                                      jx=e['jx'], jy=e['jy'], field='DAMAGE' if name == 'DamageBar' else 'PROXIMITY'))
                    continue
                if not imgs or imgs == ['dot']:
                    continue
                frames = [prj.res_images[i] for i in imgs if i in prj.res_images]
                it = dict(base, type=T_IMG, rect=rect, color=col, frames=frames, jx=e['jx'], jy=e['jy'])
                if name == 'HitNRun':
                    it['field'] = 'HNR_LIGHT'
                if name.startswith('MainMenu_'):
                    it['fillframe'] = 0
                if page == 'TVFrame.pag' and name == 'TVFrame':
                    it['tvframe'] = True
                items.append(it)
                continue
            if kind == 'text':
                if name == 'MainMenu_PC':
                    continue                                  # PC-only variant of the main menu line
                if name.startswith('Speed0_'):
                    speed_digits.append((e, base))
                    continue
                strings = [s['text'] or '' for s in e['strings']]
                face = prj.res_fonts.get(e['font'], e['font'])
                it = dict(base, type=T_TEXT, rect=[e['x'], ydown(e['y'], e['h']), e['w'], e['h']], color=e['color'],
                          font=face, halign=e['jx'], valign=e['jy'], strings=strings, sel=0,
                          shadow=e['shadow'] if e['shadow']['on'] else None,
                          wrap=name in ('Message', 'MissionObjective', 'ItemsUnlocked', 'CardTitle', 'MissionInfo',
                                        'MissionFailureReason', 'MissionFailureHint', 'PattyAndSelmaInfo'))
                binding = {'SpeedUnit0': 'SPEED_UNIT', 'Message': 'OBJECTIVE', 'MissionObjective': 'OBJECTIVE',
                           'HitNRun': 'HNR_TEXT', 'CardTitle': 'CARD_TITLE', 'PositionOrdinal': 'POSITION',
                           'MainMenu': 'MENU_ITEM', 'Level': 'LEVEL', 'MissionTitle': 'MSN_TITLE', 'MissionInfo': 'MSN_INFO',
                           'MissionFailureReason': 'MSN_FAIL', 'MissionFailureHint': 'MSN_HINT'}
                if name in binding:
                    it['field'] = binding[name]; it['fmt'] = 'ordinal' if name == 'PositionOrdinal' else 'select'
                if screen == 'message' and name == 'Message':   # правка 122: текст окна сообщения — из движка (uiMessageBox)
                    it['field'] = 'MSGBOX'; it['fmt'] = 'select'
                if page == 'LevelStats.pag':   # LEVEL PROGRESS: значения строк из движка (viewer/ui.c uiLevelStats)
                    stat = {'LevelComplete_Value': ('STAT_PCT', 'pct'), 'Gags_Unlocked': ('STAT_GAGS', 'frac'),
                            'Wasps_Unlocked': ('STAT_WASPS', 'frac'), 'Vehicles_Unlocked': ('STAT_CARS', 'frac'),
                            'Clothing_Unlocked': ('STAT_SKINS', 'frac'), 'Cards_Unlocked': ('STAT_CARDS', 'frac'),
                            'StreetRaces_Unlocked': ('STAT_SR', 'frac'), 'BonusMissions_Unlocked': ('STAT_BM', 'frac'),
                            'StoryMissions_Unlocked': ('STAT_SM', 'frac'), 'LevelProgress': ('STAT_MODE', 'select')}
                    if name in stat:
                        it['field'], it['fmt'] = stat[name]
                        if it['fmt'] == 'frac': it['strings'] = ['10/10']      # ширина для атласа и макета
                        if it['fmt'] == 'pct': it['strings'] = ['100%']
                if page in ('PauseMission.pag', 'PauseSunday.pag', 'MissionOver.pag') and 'Menu' in path:
                    it['menu'] = 0                              # numbered below, top to bottom
                if name == 'PressStartResumePlay':
                    it['sel'] = 0
                items.append(it)
                continue
        if speed_digits:
            # three fixed-width digit slots -> one right-aligned number field (same font/box row)
            xs = [d[0]['x'] for d in speed_digits]; e0, b0 = speed_digits[0]
            x0, x1 = min(xs), max(d[0]['x'] + d[0]['w'] for d in speed_digits)
            face = prj.res_fonts.get(e0['font'], e0['font'])
            items.append(dict(b0, name='Speed', type=T_TEXT, rect=[x0, ydown(e0['y'], e0['h']), x1 - x0 + 4, e0['h']],
                              color=e0['color'], font=face, halign='right', valign=e0['jy'], strings=['0'], sel=0,
                              shadow=e0['shadow'], wrap=False, field='SPEED', fmt='int', text='88'))
    hint_faces(items, screen)                            # правка 163: подписи кнопок — одна грань (до раскладок экранов: те ставят их в угол)
    if screen == 'letterbox' and not UI_DENSE:   # правка 158: подписи разговора — гранью PRESS START (boulder_16 1:1, правка 152)
        lb_faces(items)
    if screen == 'pause':
        # RESET CAR (text bible key RESET_CAR, same font/pitch as the menu) right under CONTINUE;
        # the other items move one row down (the XSmallBoard frame still holds all 8)
        mi = [it for it in items if it['page'] == 'PauseSunday.pag' and it['menu'] >= 0]
        cont = next(it for it in mi if it['name'] == 'Continue')
        pitch = 25
        for it in mi:
            if it is not cont:
                it['rect'] = [it['rect'][0], it['rect'][1] + pitch] + list(it['rect'][2:])
        rc = dict(cont, name='ResetCar', strings=[_BIBLE('RESET_CAR') or 'RESET CAR'],
                  rect=[cont['rect'][0], cont['rect'][1] + pitch] + list(cont['rect'][2:]))
        items.insert(items.index(cont) + 1, rc)
        items = pause_layout(items)
    if screen == 'pausemsn':   # пункты PauseMission.pag сверху вниз в той же рамке и тем же шагом, что у паузы свободной езды
        items = pause_layout(items)
    if screen == 'levelstats':
        items = levelstats_layout(items)
    if screen == 'viewcards':
        items = viewcards_layout(items)
    if screen == 'missionselect':
        items = missionselect_layout(items)
    if screen == 'main':
        # MENU_ARROWS_ITEM (Роб 17.10): «на RESUME GAME строку обрезают стрелки — подвинуть или под широкие слова менять положение»: у каждого пункта
        # стрелки на MENU_ARROW_GAP (ед. исходника) от краёв его слова, а не от самого широкого; RESUME GAME — шире прежнего на 12. MENU_ARROWS_ITEM_OLD=1 — прежнее
        # Роб 28.09 вечером: «меню списком, а в оригинале 3D — выбираешь один из объектов». По данным так и есть: MainMenu.pag —
        # карусель (одна строка ◄ ►, все пункты в одной точке), номер строки = номер подсветки Glow предмета гостиной
        # (tools/frontend3d_export.py). Показываем одну строку выбранного пункта со стрелками; ◄ ► (и ↑ ↓) листают по кругу,
        # в гостиной светится предмет пункта. Шрифт крупнее оригинала (Роб 26–27.09, MENU_FONT_MUL), строка — на своём месте
        # из данных (вверху стекла телевизора). Номер пункта = номер строки (menu=i)
        mm = next(it for it in items if it['name'] == 'MainMenu')
        arrows = [it for it in items if it['name'] in ('MainMenu_LArrow', 'MainMenu_RArrow')]
        k = items.index(mm)
        items.remove(mm)
        for a in arrows:
            items.remove(a)
        x, y, w, h = mm['rect']
        if MENU_SOFT:   # прежнее (до правки 136): грань строки MainMenu.pag x MENU_FONT_MUL, дробный масштаб
            face = mm['font'] + '*%g' % MENU_FONT_MUL
            src = all_fonts()[mm['font']]
            maxw = max(sum(src['glyphs'].get(ord(ch), src['glyphs'][ord('?')])['adv'] for ch in sv)
                       for sv in mm['strings']) * MENU_FONT_MUL
            hh = h * MENU_FONT_MUL
        else:           # правка 136: грань 1:1 — ширина и высота строки в пикселях PSP, в исходные единицы делением на S
            face = MENU_FACE
            fm = FontCache('psp').get(face)
            maxw = max(sum(fm['glyphs'].get(ord(ch), fm['glyphs'][ord('?')])['adv'] for ch in sv) for sv in mm['strings']) / S
            hh = fm['lh'] / S
        cx = 320.0
        ty = y + h / 2 - hh / 2   # на месте строки MainMenu.pag (вверху стекла ТВ; Роб 28.09 «было сверху»), центр строки тот же
        new = []
        for i, sv in enumerate(mm['strings']):
            new.append(dict(mm, name='MainMenu%d' % i, rect=[cx - maxw / 2 - 10, ty, maxw + 20, hh],
                            font=face, strings=[sv], field='NONE', fmt='none',
                            menu=i, sel=0, selonly=True, **({} if UI_DENSE or MENU_SOFT else
                                                            dict(shadow_px=MENU_SHADOW_PX, subset=True))))   # правка 163
            for a in arrows:
                ax, ay, aw, ah = a['rect']
                side = -1 if 'LArrow' in a['name'] else 1
                acx = cx + side * (maxw / 2 + 16 + aw / 2)
                if MENU_ARROWS_ITEM and not MENU_SOFT:   # Роб 17.10: стрелки — по ширине СВОЕГО слова, запас под пульс и тень (RESUME GAME резался стрелками)
                    iw = sum(fm['glyphs'].get(ord(ch), fm['glyphs'][ord('?')])['adv'] for ch in sv) / S
                    acx = cx + side * (iw / 2 + MENU_ARROW_GAP + aw / 2)
                new.append(dict(a, name='%s%d' % (a['name'], i), rect=[acx - aw / 2, ty + hh / 2 - ah / 2, aw, ah],
                                menu=i, selonly=True))
        items[k:k] = new
        if not MENU_SOFT and UI_DENSE:   # правка 136: подсказка «Accept» (Accept3.pag) — тоже грань 1:1, центр строки на месте (с правки 163 — hint_faces)
            for it in items:
                if it['type'] == T_TEXT and it['name'] == 'Accept' and it['page'] == 'Accept3.pag':
                    lh1 = FontCache('psp').get(MENU_HINT_FACE)['lh'] / S; x1, y1, w1, h1 = it['rect']
                    it['font'] = MENU_HINT_FACE; it['rect'] = [x1, y1 + h1 / 2 - lh1 / 2, w1, lh1]
    # menu numbering: pause menu items top to bottom
    menu = [it for it in items if it['menu'] >= 0 and it['page'] in ('PauseMission.pag', 'PauseSunday.pag', 'MissionOver.pag')]
    for i, it in enumerate(sorted(menu, key=lambda t: t['rect'][1])):
        it['menu'] = i
    items = legible_mission_text(screen, items)
    if screen == 'missionload':
        briefing_layout(items)
        briefing_loading(items)
        items = briefing_wide(items)
        briefing_arms_to_edges(items)
        brief_plate(items)
    items = tile_chains_seamless(items)   # правка 319: звенья одной картинки встык — одной полосой (шов руки брифинга)
    if screen == 'missionload':
        briefing_wager(items)
        briefing_anim_marks(items)
    if screen == 'hud':
        widescreen_objective(items)
        avatar_fit(items)
        objective_text_centre(items)
        hnr_like_original(items)
        action_label(items)
    if screen in ('pause', 'pausemsn') and os.environ.get('UI_PLACE_CODE') != '0':   # №598: элемент PlaceCode с полем PLACE (движок №598 есть в каждой сборке с TEST30). №1044: по умолчанию ВКЛЮЧЕНО — без переменной ship.sh собирал ui.shr без кода клетки, и код рисовался запасным шрифтом msx (Роб: «код в вонючем шрифте, у нас Bart vs the Space Mutants»); UI_PLACE_CODE=0 — прежний вывод uiText
        pause_place_code(items)
    if screen == 'message' and os.environ.get('UI_MSGBOX_FIT_OLD') != '1':
        # Правка 500 (аудит панели 10.10, сцена 26): текст окна сообщения жался к верху рамки (рамка текста — одна строка 16 ед. от y 144), а «✕ Accept» стоял на
        # нижней кромке рамки. Теперь текст — по центру высоты рамки (заливка 106..370, valign centre), значок и подпись Accept целиком внутри рамки (на 40 ед.
        # выше прежнего). UI_MSGBOX_FIT_OLD=1 — прежнее
        for it in items:
            if it['page'] == 'Message.pag' and it['name'] == 'Message':
                it['rect'] = [it['rect'][0], 106, it['rect'][2], 264]; it['valign'] = 'centre'   # ровно по высоте заливки рамки Frame_Bgd (106..370)
    return items, icon_defs


# Правка Роба 208 (03.10: «металлические руки, которые привозят и увозят карточку миссии, обрезаются по-тупому — дорастить, чтобы
# бесшовно доходили до краёв экрана»): у Scrooby 4:3 руки — по 4 звена трубы (clipArmLeft1..4 / clipArmRight1..4, шаг 31–32 ед.),
# левая кончается на x −82, а край кадра 16:9 PSP в единицах Scrooby — x −103,5 (правый 743,5). Добавляем звенья той же картинки
# тем же шагом, пока труба не уйдёт за край кадра (труба по длине однородная — стык не виден). UI_ARMS_OLD=1 — прежнее
# Правка Роба 512 (11.10, голосом): металлические руки должны привозить карточку миссии (в порте стояли на месте). Данных анимации в MissionLoad.pag нет (движение задаёт
# код оригинала, PS2-Наблюдатель разобрал запись по кадрам), поэтому пакет только помечает элементы: левая рука (clipLeft, clipArmLeft*), правая рука (clipRight, clipArmRight*),
# фон за карточкой (полосы LetterBox.pag); остальное — карточка. Движок (viewer/ui.c uiBriefPose) двигает группы. UI_BRIEF_ANIM_OLD=1 — пакет без меток, всё на месте
def briefing_anim_marks(items):
    if os.environ.get('UI_BRIEF_ANIM_OLD'):
        return
    for it in items:
        nm = it['name'].lower()
        if it.get('page') == 'LetterBox.pag':
            it['back'] = True
        elif it['type'] == T_IMG and nm.startswith(('cliparm', 'clipleft', 'clipright')):
            it['armL' if 'left' in nm else 'armR'] = True

# №550 (Роб 11.10): карточка миссии под широкий экран PSP. Синяя основа шире (по 16:9 с полями), полосатая рамка растянута «девятью частями»: углы целые, прямые участки — те же звенья
# с шагом 55 ед. (целое число звеньев; tile_chains_seamless потом склеит их одной полосой и уменьшит один раз — шов не сбивается), вертикальные стороны и руки едут с углами; рука — от края
# экрана до рамки (звенья, ушедшие за край, выбрасываются; болт-зажим остаётся на своём месте относительно рамки). BRIEF_WIDE_OLD=1 — прежняя раскладка (экспорт; в движке правки нет)
BRIEF_WIDE_DL, BRIEF_WIDE_DR = (28, 27) if os.environ.get('BRIEF_WIDE_612_OLD') == '1' else (43, 42)   # №612 (Роб 15.10: «капельку шире»): 85 ед. вместо 55, ≈ +5,5 % к нынешней ширине; звенья рамки растягиваются по шагу (BRIEF_WIDE_612_OLD=1 — прежние 55)
#   # ед. Scrooby (640x480) слева и справа: вместе 55 = одно звено рамки (≈ +10 % к прежней ширине; Роб 12.10: «немножечко шире», руки остаются видны)
def briefing_wide(items):
    if os.environ.get('BRIEF_WIDE_OLD'):
        return items
    DL, DR = BRIEF_WIDE_DL, BRIEF_WIDE_DR; D = DL + DR
    edge_l = SRC_W / 2 - (DST_W / 2) / S; edge_r = SRC_W / 2 + (DST_W / 2) / S
    left = {'corner2', 'corner3', 'left1', 'left2', 'left3', 'left4', 'clipLeft', 'Continue'}
    right = {'corner1', 'corner4', 'right1', 'right2', 'right3', 'right4', 'clipRight1', 'flag', 'Abort'}
    out, up_done, down_done = [], False, False
    up = next((it for it in items if it['page'] == 'MissionLoad.pag' and it['name'] == 'up2'), None)
    down = next((it for it in items if it['page'] == 'MissionLoad.pag' and it['name'] == 'down1'), None)
    n_up = 9 + D // 55; n_down = 9 + D // 55
    pitch = (55 * 9 + D) / float(n_up); kw = pitch / 55.0   # №612: целое число звеньев на любую ширину — шаг и ширина звена чуть больше 55 (при D=55 ровно 55, как было)
    for it in items:
        if it['page'] != 'MissionLoad.pag':
            out.append(it); continue
        nm = it['name']; r = list(it['rect'])
        if nm.startswith('up') and nm[2:].isdigit() and up:
            if not up_done:
                up_done = True
                for i in range(n_up):
                    out.append(dict(up, name='up%d' % (i + 1), rect=[92 - DL + pitch * i, up['rect'][1], up['rect'][2] * kw, up['rect'][3]]))
            continue
        if nm.startswith('down') and nm[4:].isdigit() and down:
            if not down_done:
                down_done = True
                for i in range(n_down):
                    out.append(dict(down, name='down%d' % i, rect=[73 - DL + pitch * i, down['rect'][1], down['rect'][2] * kw, down['rect'][3]]))
            continue
        if nm == 'background' and it['type'] == T_RECT:
            it = dict(it, rect=[r[0] - DL, r[1], r[2] + D, r[3]])
        elif nm.startswith('clipArmLeft'):
            it = dict(it, rect=[r[0] - DL] + r[1:])
            if it['rect'][0] + it['rect'][2] < edge_l - 4: continue          # звено целиком за краем экрана
        elif nm.startswith('clipArmRight'):
            it = dict(it, rect=[r[0] + DR] + r[1:])
            if it['rect'][0] > edge_r + 4: continue
        elif nm in left:
            it = dict(it, rect=[r[0] - DL] + r[1:])
        elif nm in right:
            it = dict(it, rect=[r[0] + DR] + r[1:])
        elif nm in ('MissionTitle', 'PattyAndSelmaTitle', 'MissionInfo', 'PattyAndSelmaInfo') and it['type'] == T_TEXT:
            it = dict(it, rect=[r[0] - DL, r[1], r[2] + D, r[3]])
        out.append(it)
    return out


BRIEF_PLATE_R = 26.0   # радиус скругления плашки, ед. Scrooby (≈ 15 px PSP, как у чёрной плашки картинки задания)
BRIEF_PLATE_M = 30.0   # поле плашки от края синей основы слева и справа, ед. (равные)


def brief_plate(items):
    """№613 (Роб 15.10): карточка стала шире, а картинка задания (mis*_00, своя у каждой миссии) не растягивается. Под неё подложена своя чёрная плашка:
    тот же цвет и радиус скругления, по вертикали совпадает с плашкой картинки, шире неё до равных полей от синей основы — картинка выглядит её частью.
    Плашка — одна синтетическая картинка пакета (brief_plate.png), рисуется до картинки задания. BRIEF_PLATE_OLD=1 — без плашки"""
    if os.environ.get('BRIEF_PLATE_OLD') == '1':
        return
    k = next((i for i, it in enumerate(items) if it['name'] == 'MissionStartBitmap'), None)
    bg = next((it for it in items if it['name'] == 'background' and it['type'] == T_RECT), None)
    if k is None or bg is None:
        return
    pic = items[k]; px, py, pw, ph = pic['rect']; bx, by, bw, bh = bg['rect']
    x = bx + BRIEF_PLATE_M; w = bw - 2 * BRIEF_PLATE_M
    if w <= pw + 4:
        return
    ss = 4; W, H = int(round(w)), int(round(ph))
    big = Image.new('L', (W * ss, H * ss), 0); ImageDraw.Draw(big).rounded_rectangle([0, 0, W * ss - 1, H * ss - 1], radius=BRIEF_PLATE_R * ss, fill=255)
    a = big.resize((W, H), Image.LANCZOS)
    img = Image.new('RGBA', (W, H), (0, 0, 0, 0)); img.putalpha(a)                        # чёрный, края по покрытию
    EXTRA_IMAGES['brief_plate.png'] = img
    plate = dict(pic, name='PicturePlate', rect=[x, py, w, ph], frames=['brief_plate.png'], color=[255, 255, 255, 255])
    plate.pop('ext', None)
    items.insert(k, plate)


WAGER_CARD = os.environ.get('WAGER_CARD_OLD') != '1'


def briefing_wager(items):
    """Роб 17.10 «добивай» (карточки по нашему формату: широкие, руки, чёрная плашка — memory project_shar_brief_card_format): гонка на ставку.
    Строки MissionLoad.pag диска (метка GAMBLE) — справа на чёрной плашке, три настоящих: Entry Fee (монеты), Time to Beat (М:СС), Payout (монеты);
    значения — поля WAGER_* (движок берёт из скрипта миссии, viewer/msn_view.c titleCb). Best Time и Difficulty сняты: в порте нет лучшего времени
    и коэффициентов машин (выдумывать нельзя). Картинка задания движком левее (viewer/ui.c WAGER_PIC_DX). WAGER_CARD_OLD=1 — как было (строки не выводятся)"""
    if not WAGER_CARD:
        return
    plate = next((it for it in items if it['name'] == 'PicturePlate'), None) or next((it for it in items if it['name'] == 'MissionStartBitmap'), None)
    if plate is None:
        return
    for n in ('BestTime', 'BestTime_Value', 'VehicleOdds', 'VehicleOdds_Value'):
        for it in [it for it in items if it['name'] == n and it.get('tag') == 'GAMBLE']: items.remove(it)
    x, y, w, h = plate['rect']
    rows = (('EntryFee', 'WAGER_FEE', 'int', True), ('TimeToBeat', 'WAGER_PAR', 'time', False), ('Payout', 'WAGER_PAY', 'int', True))
    for k, (nm, fld, fmt, coin) in enumerate(rows):
        lab = next((it for it in items if it['name'] == nm and it['type'] == T_TEXT), None)
        val = next((it for it in items if it['name'] == nm + '_Value' and it['type'] == T_TEXT), None)
        ico = next((it for it in items if it['name'] == nm + '_Coins'), None)
        if lab is None or val is None: continue
        rh = lab['rect'][3]; cy = y + h * (0.30 + 0.20 * k)
        lab['rect'] = [x + w * 0.45, cy - rh / 2, w * 0.37, rh]
        val.update(field=fld, fmt=fmt, strings=['0123456789:'], halign='left')   # строка — набор знаков грани (цифры и двоеточие)
        val['rect'] = [x + w * 0.845, cy - rh / 2, w * 0.08, rh]
        if ico is not None:
            if coin:
                iw, ih = ico['rect'][2], ico['rect'][3]; ico['rect'] = [x + w * 0.93, cy - ih / 2, iw, ih]
            else:
                items.remove(ico)


def briefing_arms_to_edges(items):
    if os.environ.get('UI_ARMS_OLD'):
        return
    edge_l = SRC_W / 2 - (DST_W / 2) / S; edge_r = SRC_W / 2 + (DST_W / 2) / S
    for side, sgn in (('Left', -1), ('Right', 1)):
        arm = sorted([it for it in items if it['type'] == T_IMG and it['name'].startswith('clipArm' + side)], key=lambda t: t['rect'][0])
        if len(arm) < 3:
            continue
        xs = [it['rect'][0] for it in arm]
        step = abs((xs[-1] - xs[-2]) if sgn > 0 else (xs[1] - xs[0])) or arm[0]['rect'][2] - 2
        out = arm[0] if sgn < 0 else arm[-1]
        x, n = out['rect'][0], 0
        while (x > edge_l - 1 if sgn < 0 else x + out['rect'][2] < edge_r + 1) and n < 8:
            x += sgn * step; n += 1
            items.append(dict(out, name='%sx%d' % (out['name'], n), rect=[x, out['rect'][1], out['rect'][2], out['rect'][3]]))


# Правка Роба 319 (04.10: «у правой металлической руки, держащей карточку, явный шов-разрыв»). Причина (замер): звено трубы
# cliparm.png 33x66 ед. — 32 столбца трубы + 1 прозрачный; у Scrooby звенья идут встык с шагом 31–32 ед. (у оригинала труба
# цельная). Каждое звено уменьшалось до 19 px отдельно: прозрачный столбец уходил в последний столбец звена полупрозрачным
# (альфа ~0,4 -> тёмная полоса 44 при трубе ~110), а целые места звеньев дают шаги 17/18/19 px — на стыке Right3|Right4
# (453+19 = 472) перекрытия нет, тёмный столбец x 471 виден. ПРАВИЛО (все экраны): цепочка из 3+ одинаковых картинок одного
# ряда (тот же кадр, тот же y/h, тег, единица, стык/нахлёст <= 1 ед.) собирается в ИСХОДНОМ масштабе в одну полосу и
# уменьшается один раз — как рисует 4:3 оригинал. UI_TILE_SEAM_OLD=1 — прежнее (каждое звено отдельно)
_STRIPS = {}
def tile_chains_seamless(items):
    if os.environ.get('UI_TILE_SEAM_OLD'):
        return items
    groups = {}
    for k, it in enumerate(items):
        if it['type'] != T_IMG or len(it.get('frames') or []) != 1 or it.get('tvframe') or it.get('hwiden') or it.get('k', 1.0) != 1.0 \
                or it['name'].startswith('Frame_') or it.get('field', 'NONE') not in ('NONE', None):
            continue
        r = it['rect']
        groups.setdefault((it['frames'][0], round(r[1], 3), round(r[3], 3), it.get('tag'), it.get('unit'), it.get('page'),
                           it.get('jx'), it.get('jy')), []).append(k)
    imgs = all_images(); drop, put = set(), {}
    for key, ks in groups.items():
        ks = sorted(ks, key=lambda k: items[k]['rect'][0])
        chain = [ks[0]]
        for k in ks[1:] + [None]:
            if k is not None:
                a, b = items[chain[-1]]['rect'], items[k]['rect']
                if a[0] < b[0] <= a[0] + a[2] + 1:
                    chain.append(k); continue
            if len(chain) >= 3 and all(float(items[c]['rect'][0]).is_integer() for c in chain):
                f = key[0]; im = imgs[f].convert('RGBA'); x0 = int(min(items[c]['rect'][0] for c in chain))
                x1 = int(max(items[c]['rect'][0] + im.width for c in chain)); r0 = items[chain[0]]['rect']
                strip = Image.new('RGBA', (x1 - x0, im.height), (0, 0, 0, 0))
                for c in sorted(chain):                 # порядок рисования пакета
                    strip.alpha_composite(im, (int(items[c]['rect'][0]) - x0, 0))
                nm = '%s_strip%d_%d' % (f.rsplit('.', 1)[0], x0, x1)
                project('ingame.p3d').images[nm] = strip; _STRIPS[nm] = strip   # all_images() и Assets (adapt) видят полосу
                first = min(chain)
                put[first] = dict(items[first], name=items[first]['name'] + '_strip', frames=[nm],
                                  rect=[x0, r0[1], x1 - x0, im.height])
                drop.update(chain)
            chain = [k] if k is not None else []
    if not put:
        return items
    return [put[k] if k in put else it for k, it in enumerate(items) if k in put or k not in drop]


# Правки Роба 158/206 (03.10: «по-прежнему нет подписей рядом с треугольниками — что делает действие, поговорить с Мардж и т.п.»):
# у HUD оригинала рядом со значком действия (Hud.pag ActionButton) подписи нет — правило порта: строка справа от значка, тем же
# шрифтом, что подписи кнопок экрана разговора (PRESS START, LB_FACE 1:1), середина строки — на середине значка; текст ставит
# движок (поле ACTION_TEXT, viewer/ui.c uiActionText): слова из подсказок диска (TUTORIAL: GET INTO CAR, GO INSIDE, USE TELEPHONE
# BOOTH), разговор — имя собеседника. В атлас — все буквы (строка с алфавитом). UI_ACTION_LABEL_OFF=1 — без подписи
# Правка Роба 272 (04.10: «подпись у треугольника — Talk To Marge — немножечко крупновато и по вёрстке гуляет, доработать»):
# та же грань Boulder на шаг мельче — boulder_24 диска в масштабе 0,55 (заглавная 10 px, было 11 px у boulder_16 1:1; «Talk To
# Marge» 107 -> 90 px); середина заглавных — на середине значка (по строкам буквы T грани, не по высоте строки); проявление вместе
# с пружиной значка — viewer/ui.c (ACT_FADE_S). UI_ACT_FACE_OLD=1 — прежняя грань LB_FACE и середина строки
ACT_FACE_OLD = bool(os.environ.get('UI_ACT_FACE_OLD'))
ACT_272_OLD = bool(os.environ.get('UI_ACT_272_OLD'))   # правка 272 (07:2x): 1 — подпись справа от значка (как до 07:2x)
ACT_UNDER_GAP = 1.0   # px PSP между кольцом значка и подписью под ним
def act_face():
    return LB_FACE if ACT_FACE_OLD else 'boulder_24*%.4f' % (0.55 / glyph_scale(all_fonts()['boulder_24'], 'psp'))
def cap_mid(face):   # середина заглавной T грани от верха строки, px PSP
    bm = FontCache('psp').get(face)['glyphs'][ord('T')]['bm']; rows = np.where(bm.max(1) > .3)[0]
    return (rows[0] + rows[-1] + 1) / 2.0
ACTION_GAP = 4.0   # px PSP между значком и подписью
def action_label(items):
    if os.environ.get('UI_ACTION_LABEL_OFF'):
        return
    btn = next((it for it in items if it['type'] == T_IMG and it['name'] == 'ActionButton'), None)
    if not btn:
        return
    abc = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz0123456789&\'.-!?'
    tpl = next((it for it in items if it['type'] == T_TEXT and it['page'] == 'Hud.pag' and it.get('shadow')), None) or \
          next(it for it in items if it['type'] == T_TEXT)
    if not ACT_272_OLD:   # правка Роба 272 (04.10 07:2x: «треугольник должен быть по центру экрана, под ним мелким Talk To Marge и все
        # остальные подписи»): значок — по центру экрана (якорь C, середина x 320 Scrooby = 240 px PSP), подпись под ним по центру,
        # мелкой пиксельной гранью pix8 (как текст пузырей, 8 px, белым с тенью), впритык под кольцом значка. UI_ACT_272_OLD=1 — справа
        x, y, w, h = btn['rect']; btn['rect'] = [SRC_W / 2 - w / 2, y, w, h]; btn['forced'] = 'C'
        lh = PIX8_LH / S; lw = 300.0
        t = dict(tpl, name='ActionText', page=btn['page'], path=btn.get('path', ''), tag=btn['tag'], unit=btn.get('unit'),
                 forced='C', field='ACTION_TEXT', fmt='select', halign='centre', valign='top', pix8=True,
                 strings=[abc.upper()], sel=0, wrap=False, color=[255, 255, 255, 255], menu=-1,
                 shadow=dict(on=True, color=[0, 0, 0, 200], dx=1, dy=-1),
                 rect=[SRC_W / 2 - lw / 2, y + h + ACT_UNDER_GAP / S, lw, lh])
        for k in ('subset', 'hint', 'rect0'): t.pop(k, None)
        items.append(t); return
    face = act_face(); x, y, w, h = btn['rect']; lh = FontCache('psp').get(face)['lh'] / S
    top = y + h / 2 - lh / 2 if ACT_FACE_OLD else y + h / 2 - cap_mid(face) / S
    t = dict(tpl, name='ActionText', page=btn['page'], path=btn.get('path', ''), tag=btn['tag'], unit=btn.get('unit'),
             forced=btn.get('forced'), field='ACTION_TEXT', fmt='select', font=face, halign='left', valign='centre',
             strings=[abc], sel=0, wrap=False, color=[255, 255, 255, 255], menu=-1,
             shadow=dict(on=True, color=[0, 0, 0, 200], dx=1, dy=-1),
             rect=[x + w + ACTION_GAP / S, top, 260, lh])
    for k in ('subset', 'hint', 'rect0'): t.pop(k, None)
    items.append(t)


PLACE_FACE = 'bart_16bit@1'   # №598 (Роб 15.10): код клетки на паузе шрифтом Bart vs the Space Mutants (широкий, «#»), не отладочным msx
PLACE_X, PLACE_Y = 14, 250     # px PSP: левый край 11 (как правое поле подсказок), верх строки; строка 10 px -> низ 12 от края экрана


def pause_place_code(items):
    """№598: код клетки («L1 K9-7», слева внизу паузы) — элемент PlaceCode пакета, поле PLACE: строку ставит движок (uiPlaceCode), шрифт, место и поля — из ui.shr и панели
    типографики (группа pause_cell). Включается UI_PLACE_CODE=1 (набор для EBOOT с №598); без него элемента нет и движок рисует прежним uiText (msx); откат в EBOOT — флаг pausecellold"""
    tpl = next((it for it in items if it['type'] == T_TEXT), None)
    if tpl is None:
        return
    abc = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789-'
    lh = FontCache('psp').get(PLACE_FACE)['lh']
    oxc = (DST_W - SRC_W * S) / 2
    rect = [(PLACE_X - oxc) / S, PLACE_Y / S, 200 / S, lh / S]
    t = dict(tpl, name='PlaceCode', page='PlaceCode.pag', path='', tag='ALWAYS', unit=('PlaceCode.pag', 'PlaceCode'), forced='L', field='PLACE', fmt='select', font=PLACE_FACE,
             halign='left', valign='top', strings=[abc], sel=0, wrap=False, color=[255, 255, 255, 176], menu=-1,
             shadow=dict(on=True, color=[0, 0, 0, 192], dx=1, dy=-1), rect=rect)
    for k in ('subset', 'hint', 'rect0', 'pix8'): t.pop(k, None)
    items.append(t)


def hnr_like_original(items):
    """правка 193: «Hit & Run!» — крупная красная (HNR_BIG_FACE, HNR_RED), «BUSTED!» — прежняя строка Hud.pag; квитанция — кадр по полю
    HNR_TEXT: 0 и 2 пусто, 1 — hrticket (только арест). Подробности и замер — у HNR_BIG_FACE"""
    for k, it in enumerate(items):
        if it['type'] == T_TEXT and it.get('field') == 'HNR_TEXT' and len(it.get('strings') or []) >= 2:
            s0, s1 = it['strings'][0], it['strings'][1]
            big = dict(it, name='HitNRunBig', font=HNR_BIG_FACE, color=list(HNR_RED), strings=[s0, '', ''], subset=True,
                       rect=[0, HNR_CY - 40, 640, 80], halign='centre', valign='centre', shadow_px=2, shadow_keep=True)
            if not big.get('shadow'): big['shadow'] = dict(on=True, color=[0, 0, 0, 255], dx=2, dy=-2)
            it['strings'] = ['', s1, '']
            items.insert(k + 1, big)
            break
    for it in items:
        if it['type'] == T_IMG and it['name'] == 'HitNRunTicket' and it.get('frames') == ['hrticket.png']:
            it['field'] = 'HNR_TEXT'; it['frames'] = ['hrticket_off.png', 'hrticket.png', 'hrticket_off.png']


# ------------------------------------------------------------------------------------------
# Тексты миссий читаемы на экране PSP (Роб 28.09): заглавная не меньше MISSION_CAP_MIN_PX, буквы не растянуты
# (множитель шрифта одинаков по x и y), текст в своей рамке. Рамка задания HUD (единица 'objective': рамка, значок,
# текст) растёт целиком тем же множителем от своего левого нижнего угла — перенос строк остаётся тем же, что у оригинала.
VC_QR_W = 80   # px PSP: место QR карточки (37 модулей × 2 px + зазор 6) — описание уже на эту ширину
MISSION_TEXT = {('Hud.pag', 'Message'), ('Hud.pag', 'MissionObjective'), ('Hud.pag', 'Laps'), ('Hud.pag', 'PositionOrdinal'),
                ('Hud.pag', 'StageComplete')}
MISSION_PAGES = {'MissionLoad.pag', 'MissionOver.pag'}


def is_mission_text(it, screen):
    return it['type'] == T_TEXT and not it.get('hint') and ((it['page'], it['name']) in MISSION_TEXT or it['page'] in MISSION_PAGES or
                                                             (screen == 'missionover' and it['page'] == 'Accept3.pag'))


HINT_SKIP = {'purchase', 'phone', 'wardrobe'}   # лавка, будка, гардероб: подписи кнопок — пиксельная грань pix8 (правка 141)


def hint_faces(items, screen):
    """правка 163: подписи кнопок (Accept, Back, Continue, Quit — страницы HINT_PAGES) одной гранью HINT_FACE на всех экранах.
    Рамка подписи — по буквам: ширина — самая длинная строка грани (адвансы на PSP), высота — строка грани; край по выравниванию
    данных (слева — значок кнопки) и середина по высоте — на месте, поэтому значок и подпись стоят как в данных, а раскладки,
    которые ставят их в угол по рамкам (LEVEL PROGRESS, VIEW CARDS, MISSION SELECT), видят настоящую ширину подписи"""
    if UI_DENSE or not HINT_FACE or screen in HINT_SKIP:
        return
    lh = FontCache('psp').get(HINT_FACE)['lh'] / S
    for it in items:
        if it['type'] != T_TEXT or it['page'] not in HINT_PAGES:
            continue
        x, y, w, h = it['rect']; tw = face_width(HINT_FACE, it['strings']) / S
        nx = x if it['halign'] == 'left' else (x + w - tw if it['halign'] == 'right' else x + (w - tw) / 2)
        it.update(font=HINT_FACE, hint=True, wrap=False, rect=[nx, y + h / 2 - lh / 2, tw, lh])


# Правка Роба 158 (01.10): «подписи у треугольника (Talk to Marge, сесть в машину…) — тем же шрифтом, что PRESS START BUTTON».
# Экран разговора LetterBoxButtons.pag: «✕ Start Mission / △ Cancel», «✕ SKIP». У порта — boulder_16 x 272/480 (заглавная 7 px,
# мыло пересэмпла); у PS2 (запись Роба bziOipI4gDI 1:31, 4:3) — тот же жирный шрифт, что «Press START button», заглавная
# 28/720 высоты кадра = 10,6 px на 272 — ровно грань PRESS START boulder_16@1 (заглавная 11). Значки кнопок и места — как
# были (совпадают с PS2 до 2 px); центр строки по высоте опущен на LB_TEXT_DY (у PS2 середина строки на 13 px ниже верха полосы)
LB_FACE = os.environ.get('UI_LB_FACE', 'boulder_16@1')
LB_TEXT_DY = float(os.environ.get('UI_LB_TEXT_DY', '0'))   # px PSP; правка 214 (03.10: «подписи не по центру кнопок по вертикали»): 0 — середина строки на середине значка (было 2,0 — ниже)


def lb_faces(items):
    lh = FontCache('psp').get(LB_FACE)['lh'] / S
    for it in items:
        if it['type'] != T_TEXT or it['page'] != 'LetterBoxButtons.pag':
            continue
        x, y, w, h = it['rect']; tw = (face_width(LB_FACE, it['strings']) + 2) / S
        it.update(font=LB_FACE, wrap=False, rect=[x, y + h / 2 - lh / 2 + LB_TEXT_DY / S, max(w, tw), lh])


_CAPS = {}


def cap_px(face):
    """высота заглавной H (строк с альфой >= 0.5) шрифта face, перерисованного для PSP"""
    if face not in _CAPS:
        g = FontCache('psp').get(face)['glyphs'].get(ord('H'))
        rows = np.where(g['bm'].max(1) >= .5)[0] if g and g['bm'] is not None else []
        _CAPS[face] = int(rows[-1] - rows[0] + 1) if len(rows) else 0
    return _CAPS[face]


def face_base(face):
    """'boulder_24*1.35' / 'boulder_16@1' / 'boulder_16@1+2' -> 'boulder_16' (имя грани диска)"""
    return face.partition('+')[0].partition('*')[0].partition('@')[0]


def face_mul(face, k):
    if '@' in face:
        return face                                        # грань 1:1 (правка 136) не масштабируется
    core, plus, trk = face.partition('+')                  # трекинг (правка 163) — целые пиксели, не масштабируется
    base, _, mul = core.partition('*')
    m = (float(mul) if mul else 1.0) * k
    return (base if abs(m - 1.0) < 1e-6 else '%s*%g' % (base, round(m, 3))) + plus + trk


_PSPF = []


def face_width(face, strings):
    """ширина самой длинной строки гранью face на PSP, px: сумма адвансов, как viewer/ui.c lineWidth (грань 1:1 и трекинг — как есть)"""
    if not _PSPF:
        _PSPF.append(FontCache('psp'))
    g = _PSPF[0].get(face)['glyphs']
    return max([sum((g.get(ord(c)) or g[ord('?')])['adv'] for c in s) for s in strings if s] + [0])


def eff_scale(face):
    """во сколько раз шрифт крупнее простого масштаба S (272/480) — с учётом FONT_MIN_PX и множителя грани"""
    return FontCache('psp').get(face)['scale'] / S


def mul_for_cap(face, minimum=MISSION_CAP_MIN_PX, limit=MISSION_EFF_MAX):
    """мягкий шаг: наименьший множитель (шаг 0.01), при котором заглавная >= MISSION_CAP_MIN_PX; не выше MISSION_EFF_MAX"""
    k = 1.0
    while cap_px(face_mul(face, k)) < minimum and eff_scale(face_mul(face, round(k + 0.01, 2))) <= limit:
        k = round(k + 0.01, 2)
    return k


def legible_mission_text(screen, items):
    if screen not in ('hud', 'missionload', 'missionover'):
        return items
    # рамка задания HUD целиком: множитель по шрифту текста задания
    obj = [it for it in items if it.get('unit') and it['unit'][1] == 'objective']
    msg = next((it for it in obj if it['type'] == T_TEXT and it['name'] == 'Message'), None)
    k = mul_for_cap(msg['font']) if msg else 1.0
    if msg and k > 1.0:   # рамка растёт так, чтобы в ней помещалось столько же строк, сколько в оригинале (строки целые)
        fcx = FontCache('psp'); lh0, lh1 = fcx.get(msg['font'])['lh'], fcx.get(face_mul(msg['font'], k))['lh']
        fr = next((it for it in obj if it['type'] == T_IMG and it['rect'][0] <= msg['rect'][0] < it['rect'][0] + it['rect'][2]
                   and it['rect'][1] <= msg['rect'][1] < it['rect'][1] + it['rect'][3]), None)
        if fr:
            inset = msg['rect'][1] - fr['rect'][1]; area = (fr['rect'][1] + fr['rect'][3] - inset - msg['rect'][1]) * S
            lines = max(1, int(area // lh0)); k = max(lh1 / lh0, (lines * lh1 + 1) / area)
        else:
            k = lh1 / lh0
    if obj and k > 1.0:
        x0 = min(it['rect'][0] for it in obj); y1 = max(it['rect'][1] + it['rect'][3] for it in obj)
        for it in obj:
            x, y, w, h = it['rect']; it['rect0'] = [x, y, w, h]   # как в Scrooby (widescreen_objective: доля экрана)
            it['rect'] = [x0 + (x - x0) * k, y1 + (y - y1) * k, w * k, h * k]
            if it['type'] == T_TEXT: it['font'] = face_mul(it['font'], mul_for_cap(it['font']))
    # остальные тексты миссий: только шрифт; однострочная надпись шире рамки — рамка шире (по выравниванию)
    fc = FontCache('psp')
    for it in items:
        if not is_mission_text(it, screen) or (it.get('unit') and it['unit'][1] == 'objective'):
            continue
        minimum, limit = (FAIL_CAP_MIN_PX, FAIL_EFF_MAX) if screen == 'missionover' else (MISSION_CAP_MIN_PX, MISSION_EFF_MAX)
        kk = mul_for_cap(it['font'], minimum, limit)
        if kk > 1.0: it['font'] = face_mul(it['font'], kk)
        f = fc.get(it['font'])
        x, y, w, h = it['rect']
        need = max([text_width(f, sv) for sv in it.get('strings') or [] if sv and it['field'] == 'NONE'] + [0]) / S
        if need > w and not it.get('wrap'):   # переносимый текст (описания) — переносится, рамка та же
            dw = need - w + 2
            x = x - dw / 2 if it.get('halign') == 'centre' else (x - dw if it.get('halign') == 'right' else x)
            w = need + 2
        it['rect'] = [x, y, w, max(h, f['lh'] / S)]
    return items


# Рамка задания HUD под 16:9 (Роб 28.09: «текст слишком справа — начало текста подвинуть влево под наше соотношение сторон,
# не как в оригинальном 4:3»; аватар и текст стабильного размера). Правило порта (эталона PS2 для 16:9 нет):
#  * аватар — значок задания msnicons (все 146 значков миссий на диске 101x95, круг в одном месте) — размер не меняется;
#  * текст начинается сразу за самым широким значком заданий всех уровней (SetHUDIcon: дом Симпсонов доходит до 90/101
#    ширины) + OBJ_TEXT_GAP px — ни один значок не наезжает на текст, начало текста одно для всех этапов;
#  * рамка (фон) шире по горизонтали 3-срезом (концы-закругления не тянутся): её правый край занимает ту же долю ширины
#    экрана, что в оригинале 4:3 (420/640); строки переносятся по новой ширине. UI_OBJ_43=1 — прежняя раскладка (гейт).
OBJ_TEXT_GAP = 3
_ICON_EXT = None
# Правка Роба 48 (29.09, игра на PSP): «пузырь слишком широкий, будто растянут; текст иногда вылезает влево» — пузырь снова как
# в Hud.pag: ширина и пропорции MessageBox оригинала (R1 — один масштаб на всё, R2 — якорь L), без 3-среза вширь. Аватар —
# высотой пузыря (правка 22), текст — сразу за ним и до правого края пузыря минус поле (viewer/ui.c fitW: поле справа = отступу
# текста от верха пузыря), строки переносятся по этой ширине. UI_OBJ_WIDE=1 — прежний пузырь шире под 16:9 (отрицательный
# контроль гейта tests/missions/objective_gate.py)
OBJ_WIDE = not os.environ.get('UI_OBJ_NARROW')   # правка Роба 66 (29.09): «место для текста шире, текст не вываливается»
OBJ_WIDEN_K = 0.5   # правка 66 против правки 48 («не растянутый»): середина пузыря длиннее 3-срезом на половину запаса 16:9


def objective_icon_extent():
    """доля ширины значка, до которой доходит непрозрачное (альфа >= 0.5) у самого широкого значка заданий всех уровней"""
    global _ICON_EXT
    if _ICON_EXT is None:
        import re as _re
        used = set()
        for f in (WEB / 'source/game/scripts/missions').glob('level0*/*.mfk'):
            used |= {m.lower() for m in _re.findall(r'SetHUDIcon\s*\(\s*"([^"]*)"', f.read_text(errors='replace'))}
        best = 0.0
        for p in (WEB / 'source/game/art/frontend/dynaload/images/msnicons').glob('*/*.p3d'):
            if p.stem.lower() not in used:
                continue
            for c in read(p).children:
                if c.id == 0x19005:
                    _, im = decode_sprite_image(c)
                    bb = im.getchannel('A').point(lambda a: 255 if a >= 128 else 0).getbbox()
                    if bb: best = max(best, bb[2] / im.width)
        _ICON_EXT = best or 1.0
    return _ICON_EXT


def widescreen_objective(items):
    if os.environ.get('UI_OBJ_43'):
        return
    obj = [it for it in items if it.get('unit') and it['unit'][1] == 'objective']
    icon = next((it for it in obj if it['name'] == 'ObjectiveIcon'), None)
    box = next((it for it in obj if it['name'] == 'MessageBox'), None)
    msg = next((it for it in obj if it['type'] == T_TEXT and it['name'] == 'Message'), None)
    if not (icon and box and msg):
        return
    msg['w_ps2'] = msg['rect'][2]   # правка 215: ширина строки оригинала (после множителя читаемости) — переносы как на PS2
    ix, iy, iw, ih = icon['rect']
    x0 = ix + iw * objective_icon_extent() + OBJ_TEXT_GAP / S
    x, y, w, h = msg['rect']
    if x0 < x:
        msg['rect'] = [x0, y, w + (x - x0), h]
    bx, by, bw, bh = box['rect']; ox, _, ow, _ = box.get('rect0', box['rect'])
    if not OBJ_WIDE:   # правка 48: пузырь шириной оригинала; текст не шире пузыря
        msg['rect'][2] = min(msg['rect'][2], bx + bw - msg['rect'][0])
        return
    widen = (ox + ow) * (DST_W / SRC_W) / S - (bx + bw)   # якорь L без сдвига: правый край -> доля экрана рамки Scrooby 4:3
    widen *= OBJ_WIDEN_K
    if widen > 0:
        box['hwiden'] = widen
        msg['rect'][2] += widen


# Аватар уведомлений = высота пузыря (Роб 28.09 по видео с PSP, 2:42 и 3:01, правка 22: «аватарка маленькая, по размеру прямо
# как пузырь»; во всех уведомлениях с аватаром одно правило). Данные: значки заданий msnicons — все 101x95, круг-рамка одного места
# (у персонажей непрозрачное 13..62 x 35..85; у мест и предметов рисунок вылезает вверх/вправо, круг тот же — берётся самая
# частая рамка по всем значкам заданий на диске). Правило порта:
#  * круг значка по высоте = видимой высоте пузыря (альфа картинки helptext), центр круга — по центру пузыря по высоте, по
#    горизонтали — где был (круг наезжает на левый край пузыря, как в Scrooby); масштаб один на все значки (не скачет);
#  * значок задания рисуется рамкой ext (пакет: px, py, ang, clip у T_SLOT) — msnart_export кладёт значки ровно этого размера
#    (1:1, своим увеличителем); q*.png пакета на экран не выходят — их слоты прежние;
#  * текст — за самым широким значком заданий при этом масштабе + OBJ_TEXT_GAP; пузырь шире вправо на столько же (строк
#    помещается столько же), но не ближе AVATAR_RADAR_GAP px к радару. UI_AVATAR_OLD=1 — прежний размер (гейт tests/ui/avatar_check.py)
AVATAR_RADAR_GAP = 12
_CIRCLES = {}


def modal_bbox(ims):
    """самая частая рамка непрозрачного (альфа >= 0.5) среди картинок -> (x0, y0, x1, y1) (x1, y1 — не включая), размер картинки"""
    from collections import Counter
    c = Counter()
    for im in ims:
        bb = im.getchannel('A').point(lambda a: 255 if a >= 128 else 0).getbbox()
        if bb: c[(im.size, bb)] += 1
    (size, bb), _ = c.most_common(1)[0]
    return bb, size


def msnicon_images():
    import re as _re
    used = set()
    for f in (WEB / 'source/game/scripts/missions').glob('level0*/*.mfk'):
        used |= {m.lower() for m in _re.findall(r'SetHUDIcon\s*\(\s*"([^"]*)"', f.read_text(errors='replace'))}
    out = []
    for p in sorted((WEB / 'source/game/art/frontend/dynaload/images/msnicons').glob('*/*.p3d')):
        if p.stem.lower() in used:
            out += [decode_sprite_image(c)[1] for c in read(p).children if c.id == 0x19005]
    return out


def avatar_circle(kind):
    """'msn' — значки заданий -> (рамка круга, размер картинки)"""
    if kind not in _CIRCLES:
        _CIRCLES[kind] = modal_bbox(msnicon_images())
    return _CIRCLES[kind]


_EXTPX = None


def avatar_ext_px():
    """размер значка задания на экране PSP (рамка ext аватара HUD, px) -> (w, h); None — правило выключено (UI_AVATAR_OLD).
    tools/msnart_export.py кладёт значки msnicons ровно такими (1:1, своим увеличителем)"""
    global _EXTPX
    if _EXTPX is None:
        items, icons = build_items('hud')
        out, _ = adapt(items, icons, 'psp', Assets(), FontCache('psp'))
        ic = next((t for t in out if t['name'] == 'ObjectiveIcon'), None)
        _EXTPX = tuple(ic['extframe'][2:]) if ic and ic.get('extframe') else ()
    return _EXTPX or None


def avatar_fit(items):
    if os.environ.get('UI_AVATAR_OLD'):
        return
    obj = [it for it in items if it.get('unit') and it['unit'][1] == 'objective']
    icon = next((it for it in obj if it['name'] == 'ObjectiveIcon'), None)
    box = next((it for it in obj if it['name'] == 'MessageBox'), None)
    msg = next((it for it in obj if it['type'] == T_TEXT and it['name'] == 'Message'), None)
    if not (icon and box and msg):
        return
    bim = all_images()[box['frames'][0]]
    b0, b1 = bim.getchannel('A').point(lambda a: 255 if a >= 128 else 0).getbbox()[1::2]
    bx, by, bw, bh = box['rect']
    D = bh * (b1 - b0) / bim.height; cy = by + bh * (b0 + b1) / 2 / bim.height      # видимая высота пузыря, его центр
    box['vis_cy'] = cy   # правка 215: центр видимого пузыря — по нему текст задания по высоте (objective_text_centre)
    (c0x, c0y, c1x, c1y), (iw, ih) = avatar_circle('msn')
    ix, iy, iW, iH = icon['rect']; k0 = min(iW / iw, iH / ih)
    cx = ix + (iW - iw * k0) / 2 + (c0x + c1x) / 2 * k0                               # центр круга по x — где был
    k = D / (c1y - c0y)
    icon['ext_frame'] = [cx - (c0x + c1x) / 2 * k, cy - (c0y + c1y) / 2 * k, iw * k, ih * k]
    x0 = icon['ext_frame'][0] + iw * k * objective_icon_extent() + OBJ_TEXT_GAP / S
    x, y, w, h = msg['rect']; shift = x0 - x
    if shift <= 0:
        return
    radar = [it for it in items if it['tag'] == 'RADAR' and it['type'] == T_IMG]
    room = 1e9
    if radar:   # радар якорем R: x экрана = x * S + (DST_W - SRC_W * S); пузырь — якорем L: x * S
        rl = min(r['rect'][0] for r in radar) * S + (DST_W - SRC_W * S)
        room = (rl - AVATAR_RADAR_GAP) / S - (bx + bw + box.get('hwiden', 0))
    add = max(0.0, min(shift, room)) if OBJ_WIDE else 0.0   # правка 48: пузырь не шире оригинала, текст уже на сдвиг
    box['hwiden'] = box.get('hwiden', 0) + add
    msg['rect'] = [x0, y, w - shift + add, h]


# Правка Роба 215 (03.10): «текст внутри рамки выровнять, рамку не трогать; может идти во вторую строку — адаптивно, чтобы хорошо
# смотрелся на пиксельном экране PSP». Правило порта: строка текста задания не длиннее строки оригинала (ширина Message из Hud.pag
# при том же множителе читаемости — переносы как на PS2, «COLLECT THE ICE CREAM / AND COLA», запись PS2 3:19), блок из одной или
# двух строк — по центру видимого пузыря по высоте (верх текста и поле справа — прежние: viewer/ui.c fitW считает строки и ширину
# от отступа текста сверху). Пузырь и аватар на месте. UI_OBJ_TEXT_TOP=1 — прежнее: строка во всю ширину пузыря, прижата к верху
# Правка Роба 295 (04.10: «Talk to Marge опять по центру пузыря — верни как было: первая строка от скругления пилюли, с приличным
# отступом от верхнего края, системно, аккуратно»): по умолчанию снова раскладка до 215 — строка во всю ширину пузыря, у верха
# (движок: +3 px вниз, правка 156). Центр правки 215 — только с UI_OBJ_TEXT_CENTRE=1 (гейт tests/ui/objective_text.py его и держит)
OBJ_TEXT_TOP = bool(os.environ.get('UI_OBJ_TEXT_TOP')) or not os.environ.get('UI_OBJ_TEXT_CENTRE')
OBJ_PIX8_PAD = 2   # viewer/ui_layout.h PIX8_LH (10) − ячейка буквы pix8 (8)


def objective_text_centre(items):
    if OBJ_TEXT_TOP:
        return
    obj = [it for it in items if it.get('unit') and it['unit'][1] == 'objective']
    box = next((it for it in obj if it['name'] == 'MessageBox'), None)
    msg = next((it for it in obj if it['type'] == T_TEXT and it['name'] == 'Message'), None)
    if not (box and msg and msg.get('w_ps2')):
        return
    bx, by, bw, bh = box['rect']; x, y, w, h = msg['rect']
    cy = box.get('vis_cy', by + bh / 2)
    # центр рамки текста = центр пузыря, верх тот же; + (строка 10 − ячейка 8) px: блок строк грани pix8 (viewer/ui.c textGlyphs
    # центрирует строки по 10 px, буквы 8 px) — середина букв ровно в середине пузыря
    msg['rect'] = [x, y, min(w, msg['w_ps2']), 2 * (cy - y) + OBJ_PIX8_PAD / S if cy > y else h]
    msg['valign'] = 'centre'


# Брифинг миссии (MissionLoad.pag) — Роб 28.09 по видео с PSP (3:30–3:50, правка 39): «верхний шрифт (название) и две нижние
# кнопки (Continue / Cancel Mission) капельку крупнее, текст задания чуть ниже картинки». Правило порта (слово Роба, грани те же):
#  * название и надписи кнопок — та же грань, мягкий шаг: строка на BRIEF_LH_STEP px выше нынешней (наименьший множитель,
#    дающий такую строку), не крупнее MISSION_EFF_MAX (гейт читаемости tools/ui_export.py legibility держит границы);
#  * значки кнопок (accept / back) — во столько же раз, сколько выросла строка надписи, от своего центра; надпись — за значком
#    с прежним зазором, по центру значка по высоте;
#  * описание задания (и итог уличных гонок) — ниже картинки задания на одну строку своего шрифта (было ~3 px).
#  UI_BRIEF_OLD=1 — прежняя раскладка (отрицательный контроль гейта tests/ui/brief_layout.py).
BRIEF_LH_STEP = 2
BRIEF_BTN_FACE = 'boulder_16*1.5'   # правка 299: надписи кнопок брифинга на шаг меньше грани 1:1 (заглавная 10 px)


def briefing_loading(items):
    """Правка Роба 52 (эталон PS2 1:39–1:41, кадры 4/с): пока миссия грузится, на месте кнопок по центру планшета мигает
    «LOADING» (строка LOADING текстовой таблицы srr2, шрифт и строка надписи Continue, цвет названия миссии), затем
    появляется «✕ Continue» (группа LoadCompleted). Мигание и показ — код (viewer/msn_view.c), тег LOADING."""
    txt = next((it for it in items if it['type'] == T_TEXT and it['name'] == 'Continue'), None)
    bg = next((it for it in items if it['name'] == 'background' and it.get('page') == 'MissionLoad.pag'), None)
    title = next((it for it in items if it['type'] == T_TEXT and it['name'] == 'MissionTitle'), None)
    if not (txt and bg) or _BIBLE is None or not _BIBLE('LOADING'):
        return
    x, y, w, h = txt['rect']; cx = bg['rect'][0] + bg['rect'][2] / 2; W = 200
    items.append(dict(txt, name='BriefLoading', strings=[_BIBLE('LOADING')], tag='LOADING', halign='centre',
                      rect=[cx - W / 2, y, W, h], color=title['color'] if title else txt['color'], field='NONE'))


def briefing_layout(items):
    if os.environ.get('UI_BRIEF_OLD'):
        return
    fc = FontCache('psp')

    def step(face):
        base, _, mul = face.partition('*'); k0 = float(mul) if mul else 1.0
        want = fc.get(face)['lh'] + BRIEF_LH_STEP; k = k0
        while fc.get(face_mul(base, k))['lh'] < want and eff_scale(face_mul(base, round(k + 0.01, 2))) <= MISSION_EFF_MAX + 1e-6:
            k = round(k + 0.01, 2)
        return face_mul(base, k)
    for it in items:                                       # название: крупнее, центр строки на месте
        if it['type'] == T_TEXT and it['name'] in ('MissionTitle', 'PattyAndSelmaTitle'):
            if it['name'] == 'MissionTitle':   # правка 52: название миссии ПРОПИСНЫМИ (эталон PS2 1:40, 4:46); итог гонок
                it['strings'] = [t.upper() for t in it.get('strings') or []]   # (PattyAndSelmaTitle, «%d of %d races…») — как в данных: %d не %D
            f1 = step(it['font']); x, y, w, h = it['rect']; h1 = max(h, fc.get(f1)['lh'] / S)
            it['font'] = f1; it['rect'] = [x, y + h / 2 - h1 / 2, w, h1]; it['subset'] = True   # в атлас — буквы своих строк
    for nm in ('Continue', 'Abort'):                       # кнопки: значок и надпись одним множителем
        txt = next((it for it in items if it['type'] == T_TEXT and it['name'] == nm), None)
        ico = next((it for it in items if it['type'] == T_IMG and it['name'] == nm), None)
        if not (txt and ico):
            continue
        # правка Роба 210 (03.10: «Continue немного крупнее — буквы сливаются»): надпись кнопок карточки — грань PRESS START 1:1
        # (boulder_16@1, заглавная 11 px; как «✕ Start Mission / △ Cancel» правки 158), без пересэмпла; значок — тем же множителем
        # правка Роба 299 (04.10: «надписи Continue довольно крупные — капельку уменьшить»): на шаг меньше грани 1:1 — boulder_16 ×1,5
        # (заглавная 10 px вместо 11, строка 16 вместо 19); UI_BRIEF_BTN_299_OLD=1 — грань 1:1 правки 210
        f0 = txt['font']; f1 = step(f0) if os.environ.get('UI_CARD_BTN_OLD') else (LB_FACE if os.environ.get('UI_BRIEF_BTN_299_OLD') else BRIEF_BTN_FACE)
        k = fc.get(f1)['lh'] / fc.get(f0)['lh']
        ix, iy, iw, ih = ico['rect']; gap = txt['rect'][0] - (ix + iw)
        cx, cy = ix + iw / 2, iy + ih / 2; iw1, ih1 = iw * k, ih * k
        ico['rect'] = [cx - iw1 / 2, cy - ih1 / 2, iw1, ih1]
        h1 = fc.get(f1)['lh'] / S; x1 = cx + iw1 / 2 + gap
        txt['font'] = f1; txt['rect'] = [x1, cy - h1 / 2, txt['rect'][2], h1]; txt['subset'] = True
        # №318: «Continue нашим пиксельным шрифтом пузыря»; explicit panel face can override it
        if nm == 'Continue' and os.environ.get('TYPO_318_OLD') != '1':
            txt['pix8'] = True
            txt['pix8_default'] = True
    pic = next((it for it in items if it['name'] == 'MissionStartBitmap'), None)
    if pic:                                                # описание: на строку ниже картинки
        pb = pic['rect'][1] + pic['rect'][3]
        for it in items:
            if it['type'] == T_TEXT and it['name'] in ('MissionInfo', 'PattyAndSelmaInfo'):
                # правка Роба 299 (04.10: «текст задания — всегда тем же шрифтом, что в нижнем пузыре, как Talk to Marge»): пиксельная
                # грань pix8 (8 px, как текст пузыря); название миссии — как было. UI_BRIEF_PIX8_OLD=1 — прежняя грань
                if not os.environ.get('UI_BRIEF_PIX8_OLD'): it['pix8'] = True
                f = fc.get(it['font']); x, y, w, h = it['rect']; y = max(y, pb + f['lh'] / S)
                # ниже оказался угол рамки планшета (corner3 — узкая деталь у края): рамка текста уже на неё с обеих сторон
                # (текст по центру, строки те же), иначе перенос движка (ui.c fitW: строк — до детали ниже) теряет строку
                nl = max([len(wrap_lines(f, sv, w * S, True)) for sv in it.get('strings') or [] if sv] + [1])
                pad = 0.0
                for b in items:
                    bx_, by_, bw_, bh_ = b['rect']
                    if b is it or b['type'] != T_IMG or bw_ >= w * 0.1 or not (y < by_ < y + nl * f['lh'] / S):
                        continue
                    if bx_ < x + w and bx_ + bw_ > x:
                        pad = max(pad, (bx_ + bw_ - x) if bx_ + bw_ / 2 < x + w / 2 else (x + w - bx_))
                it['rect'] = [x + pad, y, w - 2 * pad, h]


BUBBLE_AA_SS = 8   # №515: подвыборок на сторону пикселя (покрытие = доля площади, как у шрифтов PSP)
BUBBLE_OFF = dict(bottom=3.0, right=2.5, shadow_dy=2.0)   # helptext.png 165x44: внешний край кольца (px исходника от края картинки) и тень вниз


def pill_aa(img, W, H):
    """№515 (Роб: у пузыря сообщения битые ломаные пиксели на скруглениях): рамка-«таблетка» пересобрана по профилям исходника,
    а не растянута из ступенчатых концов. Исходник = кольцо (золотая кромка + тёмная внутренняя линия + полупрозрачная заливка),
    свет сверху-слева (кромка сверху/слева ярче), и тёмная тень на 2 px ниже кольца (низ кольца на 3 px выше низа картинки).
    Профили (цвет и альфа по расстоянию d от внешнего края кольца) сняты в четырёх местах: верх, левый кончик, низ, правый кончик;
    между ними — плавный переход по углу нормали. Форма — стадион (концы-полукруги), масштаб H/ih на обе оси. Каждый пиксель — среднее
    8x8 подвыборок непрерывной формы: честное покрытие, края без ступенек на любом W, H. BUBBLE_AA_OLD=1 — прежний 3-срез."""
    iw, ih = img.size
    a = np.asarray(img.convert('RGBA')).astype(np.float64) / 255
    pm = a.copy(); pm[..., :3] *= pm[..., 3:4]
    ob, orr, sdy = BUBBLE_OFF['bottom'], BUBBLE_OFF['right'], BUBBLE_OFF['shadow_dy']
    n = int(ih / 2) + 2; mid = iw // 2; rows = [ih // 2 - 1, ih // 2]
    prof4 = [pm[:n + 1, mid],                                            # 0: верх (нормаль вверх)
             pm[rows, ::-1][:, int(orr):int(orr) + n + 1].mean(0),       # 1: правый кончик
             pm[::-1][int(ob):int(ob) + n + 1, mid],                     # 2: низ
             pm[rows, :n + 1].mean(0)]                                   # 3: левый кончик
    ofs = [0.0, orr - int(orr), ob - int(ob), 0.0]

    def prof(p, d):                                                       # ступенчатый профиль (пиксель исходника = полоса цвета): резкие насыщенные полосы,
        i = np.clip(np.floor(d).astype(int), 0, len(p) - 1)                # а плавность даёт покрытие подвыборками по кругу
        return p[i]
    k = H / float(ih)                                                     # px исходника -> px назначения
    Wr, Hr = W - orr * k, H - ob * k                                      # кольцо: левый и верхний край у границы, правый и нижний — внутри
    R = Hr / 2.0
    ss = BUBBLE_AA_SS
    xs = (np.arange(W * ss) + .5) / ss; ys = (np.arange(H * ss) + .5) / ss
    X, Y = np.meshgrid(xs, ys)
    cxl, cxr, cy = R, Wr - R, R
    def stad(Xs, Ys):                                                     # расстояние до осевого отрезка и вектор наружу
        px = np.clip(Xs, cxl, cxr); dx, dy = Xs - px, Ys - cy
        return np.hypot(dx, dy), dx, dy
    dist, dx, dy = stad(X, Y)
    in_ring = dist <= R
    dsh, _, _ = stad(X, Y - sdy * k)
    in_shadow = (dsh <= R) & ~in_ring
    d_src = np.maximum(R - dist, 0) / k                                   # расстояние от внешнего края кольца, px исходника
    psi = np.mod(np.arctan2(dx, -dy), 2 * np.pi)                          # угол нормали: 0 вверх, pi/2 вправо, pi вниз, 3pi/2 влево
    q = psi / (np.pi / 2); i0 = np.floor(q).astype(int) % 4; i1 = (i0 + 1) % 4; f = (q - np.floor(q))[..., None]
    f = f * f * (3 - 2 * f)                                               # плавный переход между профилями
    c0 = np.zeros(X.shape + (4,)); c1 = np.zeros(X.shape + (4,))
    for j in range(4):
        pj = prof(prof4[j], d_src)
        c0 = np.where((i0 == j)[..., None], pj, c0); c1 = np.where((i1 == j)[..., None], pj, c1)
    ring = c0 * (1 - f) + c1 * f
    shadow = np.array([6, 4, 4, 247]) / 255.0; shadow = np.concatenate([shadow[:3] * shadow[3], shadow[3:]])
    c = np.where(in_ring[..., None], ring, np.where(in_shadow[..., None], shadow, 0.0))
    c = c.reshape(H, ss, W, ss, 4).mean((1, 3))                           # покрытие: среднее по подвыборкам
    al = c[..., 3:4]
    rgb = np.where(al > 1e-4, c[..., :3] / np.maximum(al, 1e-4), 0)
    out = np.concatenate([np.clip(rgb, 0, 1), np.clip(al, 0, 1)], -1)
    return Image.fromarray((out * 255 + 0.5).astype(np.uint8), 'RGBA')


def hslice(img, W, H):
    """горизонтальный 3-срез: левый и правый концы (по высоте/2) — с сохранением пропорций, середина тянется по ширине"""
    iw, ih = img.size
    if img.size == (165, 44) and W >= H and os.environ.get('BUBBLE_AA_OLD') != '1':   # №515: пузырь задания helptext — пересборка с покрытием
        return pill_aa(img, W, H)
    cap = max(1, min(iw // 2 - 1, ih // 2)); capW = max(1, int(round(cap * H / ih)))
    out = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    out.paste(resample(img.crop((0, 0, cap, ih)), capW, H), (0, 0))
    out.paste(resample(img.crop((iw - cap, 0, iw, ih)), capW, H), (W - capW, 0))
    if W - 2 * capW > 0:
        out.paste(resample(img.crop((cap, 0, iw - cap, ih)), W - 2 * capW, H), (capW, 0))
    return out


def legibility(plant=False):
    """гейт: HUD/брифинг >= MISSION_CAP_MIN_PX, провал >= FAIL_CAP_MIN_PX; надписи в своей рамке. 0 = ок"""
    bad = 0; fc = FontCache('psp')
    for screen in ('hud', 'missionload', 'missionover'):
        items, icons = build_items(screen)
        if plant == 'tiny' and screen == 'missionload':   # отрицательный контроль: подложен мелкий шрифт описания миссии
            for it in items:
                if it['name'] == 'MissionInfo': it['font'] = face_mul(it['font'], 0.6)
        if plant == 'huge' and screen == 'missionover':   # отрицательный контроль: подложен раздутый шрифт причины провала
            for it in items:
                if it['name'] == 'MissionFailureReason': it['font'] = face_mul(it['font'], 1.6)
        out, _ = adapt(items, icons, 'psp', Assets(), fc)
        for t in out:
            if not is_mission_text(t, screen):
                continue
            f = fc.get(t['font']); c = cap_px(t['font']); e = eff_scale(t['font']); x, y, w, h = t['dst']
            ws = max([text_width(f, ln) for sv in t.get('strings') or [] if sv and t['field'] == 'NONE'
                      for ln in wrap_lines(f, sv, w, t.get('wrap'))] + [0])
            minimum, limit = (FAIL_CAP_MIN_PX, FAIL_EFF_MAX) if screen == 'missionover' else (MISSION_CAP_MIN_PX, MISSION_EFF_MAX)
            ok = c >= minimum and e <= limit + 1e-6 and ws <= w + 0.5 and f['lh'] <= h + 1
            bad += not ok
            print('%-5s %-12s %-22s %-18s x%.2f от 272/480, заглавная %2d px, строка %2d px, ширина %5.1f из %3d%s' % (
                'ok' if ok else 'FAIL', screen, t['name'], t['font'], e, c, f['lh'], ws, w, '' if t['field'] == 'NONE' else ' (' + t['field'] + ')'))
    print('тексты миссий: %s (HUD/брифинг >= %d px <= x%.2f; провал >= %d px <= x%.2f; грань своя)' % (
        'OK' if not bad else 'FAIL %d' % bad, MISSION_CAP_MIN_PX, MISSION_EFF_MAX, FAIL_CAP_MIN_PX, FAIL_EFF_MAX))
    return 1 if bad else 0


# ------------------------------------------------------------------------------------------
# 2) placement of sprites inside their Scrooby box (source space)

def sprite_rect(rect, img_size, jx, jy):
    """Scrooby box vs image: same aspect (<=6%) -> image fills the box (uniform scale),
    otherwise the image keeps its native size, justified in the box."""
    x, y, w, h = rect
    iw, ih = img_size
    if w > 0 and h > 0 and abs((w / h) / (iw / ih) - 1) < 0.06:
        return [x, y, w, h]
    ox = x if jx == 'left' else (x + w - iw if jx == 'right' else x + (w - iw) / 2)
    oy = y if jy == 'top' else (y + h - ih if jy == 'bottom' else y + (h - ih) / 2)
    return [ox, oy, iw, ih]


FRAME_OVERLAP = 2


def frame_piece_rect(name, rect, images, sizes):
    """Board frame (XSmallBoard/BigBoard): corners native, edges stretched between corners
    (edges reach FRAME_OVERLAP px under the corners, which are drawn after them)."""
    r = _frame_piece_rect(name, rect, sizes)
    if r is None:
        return None
    o = FRAME_OVERLAP
    if name in ('Frame_Top', 'Frame_Bottom'):
        return [r[0] - o, r[1], r[2] + 2 * o, r[3]]
    return [r[0], r[1] - o, r[2], r[3] + 2 * o]


def edge_slice(img, name):
    """Middle third of an edge image (the part that is uniform along the edge)."""
    w, h = img.size
    if name in ('Frame_Top', 'Frame_Bottom'):
        return img.crop((w // 3, 0, w - w // 3, h))
    return img.crop((0, h // 3, w, h - h // 3))


def _frame_piece_rect(name, rect, sizes):
    x, y, w, h = rect
    tl, tr, bl, br = sizes['frame_tl.png'], sizes['frame_tr.png'], sizes['frame_bl.png'], sizes['frame_br.png']
    if name == 'Frame_Top':
        return [x + tl[0], y, w - tl[0] - tr[0], sizes['frame_t.png'][1]]
    if name == 'Frame_Bottom':
        th = sizes['frame_b.png'][1]
        return [x + bl[0], y + h - th, w - bl[0] - br[0], th]
    if name == 'Frame_Left':
        return [x, y + tl[1], sizes['frame_l.png'][0], h - tl[1] - bl[1]]
    if name == 'Frame_Right':
        tw = sizes['frame_r.png'][0]
        return [x + w - tw, y + tr[1], tw, h - tr[1] - br[1]]
    return None


# ------------------------------------------------------------------------------------------
# 3) adaptation: source space -> target space

def item_bbox(it, images):
    x, y, w, h = it['rect']
    if it['type'] == T_IMG and not it.get('tvframe') and it['frames']:
        r = frame_piece_rect(it['name'], it['rect'], None, {k: v.size for k, v in images.items()}) \
            if it['name'].startswith('Frame_') else None
        if r is None:
            r = sprite_rect(it['rect'], images[it['frames'][0]].size, it['jx'], it['jy'])
        return r
    return [x, y, w, h]


# Правка Роба 157 (01.10): «кружочки карта и Мардж — на ~5 px ближе к краям». Аватар задания (Мардж, левый верхний угол) и радар
# (правый нижний) — сдвиг целой группы к своему углу, px PSP; UI_HUD_NUDGE=0 — прежнее место
HUD_NUDGE = {} if os.environ.get('UI_HUD_NUDGE') == '0' else {('Hud.pag', 'objective'): (-5, -5), ('Hud.pag', 'Foreground/Player0'): (5, 5)}


def compute_units(items, images):
    boxes = {}
    for it in items:
        if it['type'] == T_RECT and it.get('fill'):
            continue
        if it.get('tvframe'):
            continue
        x, y, w, h = item_bbox(it, images)
        b = boxes.setdefault(it['unit'], [1e9, 1e9, -1e9, -1e9, None])
        b[0] = min(b[0], x); b[1] = min(b[1], y); b[2] = max(b[2], x + w); b[3] = max(b[3], y + h)
        if it['forced']:
            b[4] = it['forced']
    units = {}
    for key, (x0, y0, x1, y1, forced) in boxes.items():
        cx = (x0 + x1) / 2
        anchor = forced or ('L' if cx < SRC_W / 3 else ('R' if cx > 2 * SRC_W / 3 else 'C'))
        ox = {'L': 0.0, 'C': (DST_W - SRC_W * S) / 2, 'R': DST_W - SRC_W * S}[anchor]
        # safe-area push (R4)
        tx0, tx1, ty0, ty1 = x0 * S + ox, x1 * S + ox, y0 * S, y1 * S
        dx = max(0, SAFE_X - tx0) - max(0, tx1 - (DST_W - SAFE_X)) if tx1 - tx0 < DST_W - 2 * SAFE_X else 0
        dy = max(0, SAFE_Y - ty0) - max(0, ty1 - (DST_H - SAFE_Y)) if ty1 - ty0 < DST_H - 2 * SAFE_Y else 0
        nx, ny = HUD_NUDGE.get(key, (0, 0))   # правка 157: круги «Мардж» и «карта» — на 5 px ближе к своим углам
        units[key] = dict(anchor=anchor, ox=ox + dx + nx, oy=dy + ny, bbox=[x0, y0, x1 - x0, y1 - y0])
    objective_on_radar(items, images, units)
    rh, ob = units.get(('Hud.pag', 'racehud')), units.get(('Hud.pag', 'objective'))
    if rh and ob:   # №552: левый край блока гонки = левый край круга задания в углу (поле угловых элементов)
        left = lambda u: u['bbox'][0] * S + u['ox']
        rh['ox'] += left(ob) - left(rh)
    return units


# Правка Роба 365 (05.10, голосом: «уведомления сильно выше карты-радара — отцентровать по высоте с кружком радара»). Замер: центр
# радара y 220 из 272, центр видимого пузыря задания/уведомления 205–209 (на записи PS2 пузырь ~7 px ниже центра радара). Правило
# порта: весь блок задания (пузырь, аватар, текст) по высоте — центр видимого пузыря (avatar_fit vis_cy) на центре кольца радара;
# низ блока не ниже края экрана. По x — как было. UI_OBJ_RADAR_OLD=1 — прежнее место (красный контроль tests/ui/bubble_radar.py)
OBJ_RADAR_OLD = bool(os.environ.get('UI_OBJ_RADAR_OLD'))


def objective_on_radar(items, images, units):
    if OBJ_RADAR_OLD:
        return None
    key = ('Hud.pag', 'objective')
    box = next((it for it in items if it.get('unit') == key and it['name'] == 'MessageBox' and it.get('vis_cy') is not None), None)
    radar = [it for it in items if it.get('tag') == 'RADAR' and it['type'] == T_IMG and it.get('unit') in units]
    if box is None or key not in units or not radar:
        return None
    ring = max(radar, key=lambda it: it['rect'][2] * it['rect'][3])   # кольцо радара — самая большая картинка радара
    x, y, w, h = item_bbox(ring, images)
    cy = (y + h / 2) * S + units[ring['unit']]['oy']                    # центр кольца на экране PSP, px
    u = units[key]; bx, by, bw, bh = u['bbox']
    oy = cy - box['vis_cy'] * S
    oy = min(oy, DST_H - (by + bh) * S)                                 # низ блока — не за краем экрана
    u['oy'] = oy
    return cy, box['vis_cy'] * S + oy


def tx_rect(r, u, scale=S):
    """Transform a source rect with the unit offset.  Size = round(size*scale) (so one image
    at one scale always gets the same bitmap), position = rounded centre - size/2."""
    x, y, w, h = r
    W, H = max(1, int(round(w * scale))), max(1, int(round(h * scale)))
    cx = (x + w / 2) * scale + u['ox']; cy = (y + h / 2) * scale + u['oy']
    return [int(math.floor(cx - W / 2 + 0.5)), int(math.floor(cy - H / 2 + 0.5)), W, H]


IDENTITY_UNIT = dict(anchor='-', ox=0.0, oy=0.0)


class Assets:
    """Bitmaps requested by the adapted layout, each at its exact on-screen size."""

    def __init__(self):
        self.bitmaps = {}      # key -> RGBA image
        self.images = all_images()

    def scaled(self, name, w, h, tag=''):
        key = '%s@%dx%d%s' % (name, w, h, tag)
        if key not in self.bitmaps:
            self.bitmaps[key] = resample(self.images[name], w, h)
        return key

    def put(self, key, img):
        self.bitmaps[key] = img
        return key


def ring_inner_radius(img, dst, centre, step=0.25):
    """Внутренний радиус кольца (px экрана): первый радиус от centre, где средняя по углу альфа картинки >= 50%."""
    a = np.asarray(img.convert('RGBA'))[..., 3].astype(np.float32) / 255
    h, w = a.shape
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    r = np.hypot(xx + 0.5 + dst[0] - centre[0], yy + 0.5 + dst[1] - centre[1])
    for r0 in np.arange(0.0, float(r.max()), step):
        m = (r >= r0) & (r < r0 + step)
        if m.any() and a[m].mean() >= 0.5:
            return float(r0)
    return float(r.max())


# Слабые (мелкие) исходники диска, которые на экране PSP больше себя (пузырь задания helptext 165x44 -> 3-срез 56 px,
# значки q*.png, значки заданий msnicons в аватаре), — Роб 28.09 (правка 22): «увеличить в хорошее качество, свой апскейл на
# Маке». Свой увеличитель без сети и нейросетей (детерминированный, numpy): Ланцош по премультиплицированным каналам ->
# нерезкая маска радиусом в долю шага увеличения (края рисунка снова резкие на пикселе PSP) -> анти-звон: каждый пиксель
# зажат между минимумом и максимумом своих 2x2 исходных соседей (без ореолов и светлых каёмок). UI_UPSCALE_PLAIN=1 — прежний
# простой Ланцош (отрицательный контроль гейта tests/ui/upscale_check.py).
UPSCALE_AMOUNT, UPSCALE_SIGMA = 1.0, 0.55     # сила маски; радиус (в шагах увеличения, px экрана = SIGMA * k)


def _blur(a, sigma):
    """гаусс по двум осям (каналы последней осью), края — повтор"""
    r = max(1, int(math.ceil(sigma * 3)))
    x = np.arange(-r, r + 1, dtype=np.float32); k = np.exp(-x * x / (2 * sigma * sigma)); k /= k.sum()
    for ax in (0, 1):
        p = [(0, 0)] * a.ndim; p[ax] = (r, r); ap = np.pad(a, p, mode='edge')
        a = sum(k[i] * np.take(ap, np.arange(i, i + a.shape[ax]), axis=ax) for i in range(2 * r + 1))
    return a


def _upscale_hq(pm, lz, w, h):
    """pm — исходник (премульт., HxWx4), lz — его Ланцош до (h, w); -> увеличенное резкое без звона"""
    sh, sw = pm.shape[:2]
    kx, ky = w / sw, h / sh
    out = lz + UPSCALE_AMOUNT * (lz - _blur(lz, max(0.35, UPSCALE_SIGMA * max(kx, ky))))
    # анти-звон: соседи 2x2 исходника вокруг точки выборки (как у билинейного)
    fx = (np.arange(w, dtype=np.float32) + .5) / kx - .5; fy = (np.arange(h, dtype=np.float32) + .5) / ky - .5
    x0 = np.clip(np.floor(fx).astype(int), 0, sw - 1); x1 = np.clip(x0 + 1, 0, sw - 1)
    y0 = np.clip(np.floor(fy).astype(int), 0, sh - 1); y1 = np.clip(y0 + 1, 0, sh - 1)
    q = [pm[y0][:, x0], pm[y0][:, x1], pm[y1][:, x0], pm[y1][:, x1]]
    lo = np.minimum(np.minimum(q[0], q[1]), np.minimum(q[2], q[3])); hi = np.maximum(np.maximum(q[0], q[1]), np.maximum(q[2], q[3]))
    return np.clip(out, lo, hi)


def upscales(src, dst):
    """увеличение (по обеим осям не меньше, хоть по одной больше 2%) — туда идёт свой увеличитель; растяжка вдоль одной оси
    при уменьшении по другой (края рамки паузы) — простой Ланцош"""
    return dst[0] >= src[0] and dst[1] >= src[1] and (dst[0] > src[0] * 1.02 or dst[1] > src[1] * 1.02)


def resample(img, w, h):
    """Premultiplied Lanczos (area for strong minification) -> crisp, no dark fringes.  Upscaling (either axis > 2%) ->
    our HQ upscaler (_upscale_hq: sharpened at the PSP pixel, no ringing)."""
    a = np.asarray(img.convert('RGBA')).astype(np.float32) / 255
    pm = a.copy(); pm[..., :3] *= pm[..., 3:4]
    chans = []
    for c in range(4):
        ch = Image.fromarray(pm[..., c], 'F')
        f = Image.BOX if (w < img.width * 0.5 or h < img.height * 0.5) else Image.LANCZOS
        chans.append(np.asarray(ch.resize((w, h), f)))
    out = np.clip(np.stack(chans, -1), 0, 1)
    if upscales(img.size, (w, h)) and not os.environ.get('UI_UPSCALE_PLAIN'):
        out = _upscale_hq(pm, out, w, h)
        out[..., :3] = np.minimum(out[..., :3], out[..., 3:4])      # премультиплицированный цвет не ярче своей альфы
    al = out[..., 3:4]
    rgb = np.where(al > 1e-4, out[..., :3] / np.maximum(al, 1e-4), 0)
    res = np.concatenate([np.clip(rgb, 0, 1), al], -1)
    return Image.fromarray((res * 255 + 0.5).astype(np.uint8), 'RGBA')


TV_EDGE = 3   # px: непрозрачная внешняя кромка рамки-телевизора (правка 102)


def widescreen_tvframe(img, W, H, scale):
    """3-slice horizontal extension of the 640x480 TV bezel: side columns keep aspect, the
    straight middle band is resampled to fill the width.  Returns an RGBA WxH image."""
    sw = int(round(img.width * scale)); sh = H
    base = resample(img, sw, sh)
    L = int(round(120 * scale))
    out = Image.new('RGBA', (W, H))
    out.paste(base.crop((0, 0, L, sh)), (0, 0))
    out.paste(base.crop((sw - L, 0, sw, sh)), (W - L, 0))
    mid = img.crop((120, 0, img.width - 120, img.height))
    out.paste(resample(mid, W - 2 * L, sh), (L, 0))
    # правка Роба 102 (30.09): у tvframe.png диска 3 последних столбца/строки прозрачные — после масштаба край рамки
    # полупрозрачный (альфа ~52 справа и снизу, ~115 слева и сверху), виден просвет в 1 px. Внешняя кромка TV_EDGE px —
    # непрозрачная, цветом пластика в TV_EDGE px от края
    a = np.asarray(out).copy(); e = TV_EDGE
    a[:, :e] = a[:, e:e + 1]; a[:, -e:] = a[:, -e - 1:-e]; a[:e, :] = a[e:e + 1, :]; a[-e:, :] = a[-e - 1:-e, :]
    a[:, :e, 3] = a[:, -e:, 3] = 255; a[:e, :, 3] = a[-e:, :, 3] = 255
    return Image.fromarray(a, 'RGBA')


def glyph_scale(font, mode):
    if mode == 'ref':
        return 1.0
    return max(S, FONT_MIN_PX / font['height'])


NATIVE_PIX_ON = os.environ.get('NATIVE_PIX_ON') == '1' and os.environ.get('NATIVE_PIX_OLD') != '1'   # №566: двухцветный растр родных граней ВЫКЛЮЧЕН (Роб 12.10: «ещё более погрызанное»); прежний площадной растр; новый — только NATIVE_PIX_ON=1
NATIVE_THIN = ('Tt2001m__14', 'swz721n_10', 'swz721mi_12')   # тонкие штрихи: порог ниже, чтобы не пропадали


class FontCache:
    """Glyph bitmaps (alpha) re-rasterised at the target scale (R7).

    Each glyph cell is padded by 2 source px on both sides and resampled with an area (box)
    filter at the exact font scale, then 'bolded' with a = 1-(1-a)^2 so thin stems survive the
    ~0.57x reduction, and quantised to 16 alpha levels (what the T4 atlas stores)."""
    PAD = 2

    def __init__(self, mode):
        self.mode = mode
        self.fonts = all_fonts()
        self.cache = {}

    def _pixel(self, face, base, f, k):
        a = np.asarray(f['image'].convert('RGBA'))[..., 3].astype(np.float32) / 255
        glyphs = {}
        for code, g in f['glyphs'].items():
            gw, x0, y0 = int(g['w']), int(g['x']), int(g['y'])
            bm = None
            if gw:
                cell = a[y0:y0 + int(g['h']), x0:x0 + gw]
                bm = np.repeat(np.repeat(cell, k, 0), k, 1) if k > 1 else cell
            glyphs[code] = dict(bm=bm, w=0 if bm is None else bm.shape[1], left=int(g['left']) * k, adv=int(g['adv']) * k)
        res = dict(face=face, scale=float(k), lh=int(f['height']) * k, base=f['baseline'] * k, glyphs=glyphs)
        self.cache[face] = res
        return res

    def get(self, face):
        if face in self.cache:
            return self.cache[face]
        core, caret, lead = face.partition('^')     # 'boulder_16@1+1^2' = та же грань, строка +2 px PSP (правка 312, ui_typo.json):
        if caret:                                   # буквы те же (атлас общий), ячейка буквы прежней высоты (gh, viewer/ui.c)
            f0, l = self.get(core), int(lead)
            res = dict(f0, face=face, lh=max(1, f0['lh'] + l), gh=f0.get('gh', f0['lh']), atlas=f0.get('atlas', core))
            self.cache[face] = res
            return res
        core, plus, trk = face.partition('+')       # 'boulder_16@1+2' = та же грань, трекинг +2 px PSP (правка 163)
        if plus:
            f0, t = self.get(core), int(trk)
            res = dict(f0, face=face, track=t, atlas=core,
                       glyphs={c: dict(g, adv=g['adv'] + t, left=g['left'] + t // 2) for c, g in f0['glyphs'].items()})
            self.cache[face] = res
            return res
        base, _, mul = face.partition('*')          # 'boulder_24*1.2' = same font, 20% larger
        base, at, one = base.partition('@')          # 'boulder_16@1' = грань диска 1:1, без FONT_MIN_PX и S (правка 136)
        f = self.fonts[base]
        if f.get('pixel'):                           # пиксельный шрифт: родной размер, кратные только целые (повтор пикселей без фильтра), метрики как у msx_thin
            return self._pixel(face, base, f, 1 if at else max(1, int(round(float(mul) if mul else 1.0))))
        fs = 1.0 if (at and self.mode == 'psp') else glyph_scale(f, self.mode) * (float(mul) if mul else 1.0)
        if fs != 1.0 and self.mode == 'psp' and NATIVE_PIX_ON:
            return self._native_hinted(face, f, fs)
        lh = max(1, int(round(f['height'] * fs)))
        a = np.asarray(f['image'].convert('RGBA'))[..., 3].astype(np.float32) / 255
        glyphs = {}
        P = self.PAD
        for code, g in f['glyphs'].items():
            gw = int(round(g['w']))
            bm = None
            left = g['left'] * fs
            if gw:
                x0, y0 = int(round(g['x'])), int(round(g['y']))
                cell = a[y0:y0 + int(round(g['h'])), x0:x0 + gw]
                if fs == 1.0:
                    bm = cell
                else:
                    pad = np.pad(cell, ((0, 0), (P, P)))
                    tw = max(1, int(round((gw + 2 * P) * fs)))
                    # №601 (Роб 15.10): толстые заголовочные Boulder — исходная мягкая кромка диска (билинейный масштаб, без «жирности» 1-(1-a)^2 и порога); остальные грани прежние.
                    # BOULDER_ORIG_OLD=1 — как TEST16 (вариант B для Boulder)
                    if os.environ.get('UI_FONT_SMOOTH_C') == '1' or (base.startswith('boulder') and os.environ.get('BOULDER_ORIG_OLD') != '1'):
                        bm = np.asarray(Image.fromarray(pad, 'F').resize((tw, lh), Image.BILINEAR))
                        bm = np.round(np.clip(bm, 0, 1) * 15) / 15
                    else:
                        bm = np.asarray(Image.fromarray(pad, 'F').resize((tw, lh), Image.BOX))
                        bm = 1 - (1 - np.clip(bm, 0, 1)) ** 2
                        bm = np.round(bm * 15) / 15
                    # trim empty columns (keeps atlas small), shift origin accordingly
                    cols = np.where(bm.max(0) > 0)[0]
                    if len(cols):
                        c0, c1 = cols[0], cols[-1] + 1
                        bm = bm[:, c0:c1]
                        left = g['left'] * fs - P * fs + c0
                    else:
                        bm = None
            # hinted metrics: whole-pixel bearing and advance -> spacing independent of sub-pixel phase
            adv = g['adv'] * fs if fs == 1.0 else max(1, int(round(g['adv'] * fs)))
            glyphs[code] = dict(bm=bm, w=0 if bm is None else bm.shape[1], left=int(math.floor(left + 0.5)), adv=adv)
        res = dict(face=face, scale=fs, lh=lh, base=f['baseline'] * fs, glyphs=glyphs)
        self.cache[face] = res
        return res

    def _native_hinted(self, face, f, fs):
        """№566 (Роб 11.10): родной шрифт диска при дробном масштабе без «пробитости». Раньше: площадной фильтр, «жирность» a = 1-(1-a)^2 и 16 уровней серого — края штрихов и
        заглавных на дробных долях пикселя, рваные стыки. Теперь: (1) вертикальная привязка к сетке: заглавная ровно CAP px (округлённый масштаб), базовая линия на целой строке
        одинаково у всех знаков; (2) площадной фильтр в этот размер и порог без серого (знак двухцветный, края на границах пикселей); (3) целые метрики. Размер заглавной тот же, что
        давал выбранный масштаб, строка прежней высоты — раскладка Роба не едет. NATIVE_PIX_OLD=1 — прежний растр"""
        a = np.asarray(f['image'].convert('RGBA'))[..., 3].astype(np.float32) / 255
        hg = f['glyphs'][ord('H')]; hc = a[int(round(hg['y'])):int(round(hg['y'])) + int(round(hg['h'])), int(round(hg['x'])):int(round(hg['x'])) + int(round(hg['w']))]
        rows = np.where(hc.max(1) > .3)[0]; caph = int(rows[-1] - rows[0] + 1)
        cap = max(1, int(round(caph * fs))); k = cap / caph
        lh = max(1, int(round(f['height'] * fs))); base = int(round(f['baseline'] * fs))
        thin = face.partition('*')[0].partition('@')[0] in NATIVE_THIN   # тонкие штрихи: серый сохраняется (порог съел бы штрих), но контрастнее и с той же привязкой к сетке
        P = self.PAD; glyphs = {}
        for code, g in f['glyphs'].items():
            gw = int(round(g['w'])); bm = None; left = g['left'] * k
            if gw:
                x0, y0, gh = int(round(g['x'])), int(round(g['y'])), int(round(g['h']))
                cell = np.pad(a[y0:y0 + gh, x0:x0 + gw], ((0, 0), (P, P)))
                tw = max(1, int(round((gw + 2 * P) * k))); hk = max(1, int(round(gh * k)))
                r = np.asarray(Image.fromarray(cell, 'F').resize((tw, hk), Image.BOX))
                r = np.round(np.clip((r - .08) / .55, 0, 1) * 15) / 15 if thin else (r >= .5).astype(np.float32)
                oy = base - int(round(f['baseline'] * k)); cv = np.zeros((lh, tw), np.float32)
                for y in range(hk):
                    yy = y + oy
                    if 0 <= yy < lh: cv[yy] = r[y]
                cols = np.where(cv.max(0) > 0)[0]
                if len(cols):
                    c0, c1 = cols[0], cols[-1] + 1
                    bm = cv[:, c0:c1]; left = g['left'] * k - P * k + c0
            adv = max(1, int(round(g['adv'] * k)))
            glyphs[code] = dict(bm=bm, w=0 if bm is None else bm.shape[1], left=int(math.floor(left + 0.5)), adv=adv)
        res = dict(face=face, scale=fs, lh=lh, base=f['baseline'] * fs, glyphs=glyphs)
        self.cache[face] = res
        return res


def text_width(font, s):
    w = 0.0
    for ch in s:
        g = font['glyphs'].get(ord(ch)) or font['glyphs'].get(ord('?'))
        w += g['adv']
    return w


def wrap_lines(font, s, maxw, wrap):
    if not wrap or maxw <= 0 or text_width(font, s) <= maxw:
        return [s]
    lines, cur = [], ''
    for word in s.split(' '):
        t = (cur + ' ' + word) if cur else word
        if text_width(font, t) <= maxw or not cur:
            cur = t
        else:
            lines.append(cur); cur = word
    lines.append(cur)
    return lines


def layout_text(font, s, box, halign, valign, wrap):
    """-> list of (code, x, y) glyph origins (ints).  Mirrors uiText() in viewer/ui.c."""
    x, y, w, h = box
    lines = wrap_lines(font, s, w, wrap)
    lh = font['lh']
    bh = lh * len(lines)
    y0 = y if valign == 'top' else (y + h - bh if valign == 'bottom' else y + (h - bh) // 2)
    out = []
    for i, ln in enumerate(lines):
        lw = text_width(font, ln)
        x0 = x if halign == 'left' else (x + w - lw if halign == 'right' else x + (w - lw) / 2)
        pen = int(math.floor(x0 + 0.5))
        for ch in ln:
            code = ord(ch) if ord(ch) in font['glyphs'] else ord('?')
            g = font['glyphs'][code]
            out.append((code, int(math.floor(pen + g['left'] + 0.5)), y0 + i * lh))
            pen += g['adv']
    return out


# Грань pix8 (правка 126, viewer/ui_layout.h) на Маке: шаг знака = ширина рисунка жирного глифа + 1 px, пробел 3, знаки кнопок 9.
# Правка 141: строки будки и гардероба рисуются pix8 (бит 3 res[0], ITEM_PIX8 в viewer/ui.c) — раскладке нужна их точная ширина
PIX8_LH, PIX8_GH = 10, 8
_PIX8 = {}


def pix8_adv(ch):
    if not _PIX8:
        import re as _re
        _PIX8['msx'] = bytes(int(v, 16) for v in _re.findall(r'0x([0-9a-fA-F]{2})', (ROOT / 'viewer/bootmsx.h').read_text().split('{', 1)[1]))
        _PIX8['fix'] = {m[0]: [int(x, 16) for x in m[1].split(',')] for m in
                        _re.findall(r"\{'(.)', \{([^}]*)\}\}", (ROOT / 'viewer/ui_layout.h').read_text())}   # pix8Fix
    o = ord(ch)
    if o in (0xa3, 0xbf, 0xae, 0xb5, 0xa7, 0xa9) or ch == '@': return 9      # pix8Cell: знаки кнопок — клетка 8 + 1
    c = o if 32 <= o <= 126 else ord('?')
    m = 0
    for y in range(8):
        r = _PIX8['msx'][c * 8 + y]
        m |= _PIX8['fix'][chr(c)][y] if chr(c) in _PIX8['fix'] else (r | ((r >> 1) & ~(r << 1))) & 0xff   # = pix8Row
    if not m: return 3
    a = next(x for x in range(8) if m & (0x80 >> x)); b = next(x for x in range(7, -1, -1) if m & (0x80 >> x))
    return b - a + 2


def pix8_width(s):
    """ширина строки pix8, px (сумма шагов; последний столбец шага пустой -> рисунок на 1 px уже)"""
    return sum(pix8_adv(ch) for ch in s)


def num_glyphs(it, scale, assets):
    """картинки букв спрайтового шрифта элемента T_NUM при масштабе scale x glyph_k -> (знак -> (ключ, w, h, сдвиг, шаг), значок монеты
    у счёта или None). Вынесено из adapt для моста твик-панели (tools/ui_typo.py: масштаб цифр gk) — тот же порядок картинок"""
    glyphs = {}
    scale = scale * it.get('glyph_k', 1.0)   # подсказки: картинки шрифта мельче (TUT_GLYPH_K)
    for ch, im in it['chars'].items():
        if ch == '?' and 'yquestion.png' in assets.images and os.environ.get('POS_UNKNOWN_OLD') != '1':
            im = 'yquestion'   # №615 (Роб 15.10): «?» места в гонке — золотой, как цифры (было серый question.png, мелкий); POS_UNKNOWN_OLD=1 — прежний
        src = assets.images[im.lower() + '.png']
        al = np.asarray(src)[..., 3]
        cols = np.where(al.max(0) > 20)[0]
        if len(cols) == 0:          # space: advance only
            glyphs[ch] = (None, 0, 0, 0, max(1, int(round(src.width * 0.6 * scale))))
            continue
        c0, c1 = int(cols[0]), int(cols[-1]) + 1
        crop = src.crop((max(0, c0 - 1), 0, min(src.width, c1 + 1), src.height))
        gw, gh = max(1, int(round(crop.width * scale))), max(1, int(round(src.height * scale)))
        key = assets.put('%s@%dx%d_t' % (im.lower(), gw, gh), resample(crop, gw, gh))
        adv = max(1, int(round((c1 - c0 + 2) * scale)))           # visible width + 2 src px
        glyphs[ch] = (key, gw, gh, -int(round(1 * scale)), adv)
    prefix = None
    if it['name'] == 'NumCoins':
        # the original draws a spinning 3D coin by code; stand-in = the disc's 2D coin (coins.png)
        gh = next(g[2] for g in glyphs.values() if g[0])
        coin_ = 'coins.png' if 'coins.png' in assets.images else 'money.png'   # №880: NTSC ingame.p3d — без coins.png, у счётчика знак $ (money.png)
        iw, ih = assets.images[coin_].size
        ch_ = max(1, min(ih, int(round(gh * COIN_ICON_K))))   # Роб 28.09: крупнее (было 0.62), но не выше родных 19 px — растянутая «битая»
        prefix = assets.scaled(coin_, max(1, int(round(iw * ch_ / ih))), ch_)
    return glyphs, prefix


def msgbox_fit(dl):
    """правка 500 (аудит панели 10.10, сцена 26): «✕ Accept» (значок и подпись) целиком внутри рамки окна сообщения, у правого нижнего угла с отступом; якорь подсказок
    ставил его на кромку. dl — список рисования экрана message после раскладки (пиксели PSP). UI_MSGBOX_FIT_OLD=1 — прежнее"""
    if os.environ.get('UI_MSGBOX_FIT_OLD') == '1':
        return dl
    bgd = [t for t in dl if t['page'] == 'MessageBox.pag' and t['name'] == 'Frame_Bgd']; acc = [t for t in dl if t['page'] == 'Accept.pag']
    if bgd and acc:
        bx, by, bw, bh = bgd[0]['dst']; right = max(t['dst'][0] + t['dst'][2] for t in acc); bottom = max(t['dst'][1] + t['dst'][3] for t in acc)
        dx = min(0, bx + bw - 10 - right); dy = min(0, by + bh - 8 - bottom)
        for t in acc: t['dst'] = [t['dst'][0] + dx, t['dst'][1] + dy] + list(t['dst'][2:])
    return dl


def adapt(items, icon_defs, mode, assets, fonts):
    """mode 'psp' (anchored, S) or 'ref' (identity 640x480).  -> list of target draw items."""
    scale = S if mode == 'psp' else 1.0
    assets.images.update(_STRIPS)   # правка 319: полосы звеньев (tile_chains_seamless)
    W, H = (DST_W, DST_H) if mode == 'psp' else (SRC_W, SRC_H)
    units = compute_units(items, assets.images) if mode == 'psp' else {}
    out = []
    sizes = {k: v.size for k, v in assets.images.items()}
    for it in items:
        u = units.get(it['unit'], IDENTITY_UNIT) if mode == 'psp' else IDENTITY_UNIT
        t = dict(it)
        t['anchor'] = u.get('anchor', '-')
        if it['type'] == T_RECT:
            t['dst'] = [0, 0, W, H] if it.get('fill') else tx_rect(it['rect'], u, scale)
            if it.get('fill'):
                t['dst'] = [0, tx_rect(it['rect'], u, scale)[1], W, tx_rect(it['rect'], u, scale)[3]]
                if it['rect'][1] <= 0 and it['rect'][1] + it['rect'][3] >= SRC_H:
                    t['dst'] = [0, 0, W, H]
        elif it['type'] == T_POLY:
            t['pts'] = [[p[0] * scale + u['ox'], p[1] * scale + u['oy']] for p in it['points']]
            t['dst'] = tx_rect(it['rect'], u, scale)
        elif it['type'] == T_IMG:
            if it.get('tvframe'):
                img = assets.images[it['frames'][0]]
                bm = widescreen_tvframe(img, W, H, scale) if mode == 'psp' else img
                if mode == 'psp':
                    for (x0, y0, x1, y1), nm in split_tvframe(bm):
                        pc = dict(t, name='TVFrame_' + nm, dst=[x0, y0, x1 - x0, y1 - y0], tvframe=False,
                                  bitmaps=[assets.put('tvframe_%s@%dx%d' % (nm, x1 - x0, y1 - y0), bm.crop((x0, y0, x1, y1)))])
                        out.append(pc)
                    continue
                t['dst'] = [0, 0, W, H]
                t['bitmaps'] = [assets.put('tvframe@%dx%d' % (W, H), bm)]
            elif not it['frames']:                          # картинка извне (MissionStartBitmap): только рамка места
                t['dst'] = tx_rect(it['rect'], u, scale); t['bitmaps'] = []
            else:
                K = it.get('k', 1.0)                        # pause board: corners/edges x K (pause_layout)
                szk = sizes if K == 1.0 else {n: (w * K, h * K) for n, (w, h) in sizes.items()}
                r = frame_piece_rect(it['name'], it['rect'], None, szk) if it['name'].startswith('Frame_') else None
                if r is None and K != 1.0:                  # corner: justified in the box, never stretched to it
                    x, y, w, h = it['rect']; iw, ih = szk[it['frames'][0]]
                    r = [x if it['jx'] == 'left' else (x + w - iw if it['jx'] == 'right' else x + (w - iw) / 2),
                         y if it['jy'] == 'top' else (y + h - ih if it['jy'] == 'bottom' else y + (h - ih) / 2), iw, ih]
                if r is None:
                    r = sprite_rect(it['rect'], sizes[it['frames'][0]], it['jx'], it['jy'])
                if it.get('hwiden') and mode == 'psp':      # рамка задания под 16:9 (widescreen_objective): шире 3-срезом
                    r = [r[0], r[1], r[2] + it['hwiden'], r[3]]
                d = tx_rect(r, u, scale)
                t['dst'] = d
                if it.get('hwiden') and mode == 'psp':
                    t['bitmaps'] = [assets.put('%s@%dx%d_hs' % (f, d[2], d[3]), hslice(assets.images[f], d[2], d[3])) for f in it['frames']]
                elif it['name'] in ('Frame_Top', 'Frame_Bottom', 'Frame_Left', 'Frame_Right'):
                    f = it['frames'][0]
                    t['bitmaps'] = [assets.put('%s@%dx%d_edge' % (f, d[2], d[3]),
                                               resample(edge_slice(assets.images[f], it['name']), d[2], d[3]))]
                else:
                    t['bitmaps'] = [assets.scaled(f, d[2], d[3]) for f in it['frames']]
        elif it['type'] in (T_BAR,):
            r = sprite_rect(it['rect'], sizes[it['image']], it['jx'], it['jy'])
            d = tx_rect(r, u, scale); t['dst'] = d
            t['bitmaps'] = [assets.scaled(it['image'], d[2], d[3])]
        elif it['type'] == T_SECTOR:
            box = it['rect']
            iw, ih = sizes[it['image']]
            r = sprite_rect(box, (iw, ih), it['jx'], it['jy'])
            d = tx_rect(r, u, scale)
            bw, bh = max(1, int(round(iw * scale))), max(1, int(round(ih * scale)))
            t['dst'] = [d[0], d[1], bw, bh]
            cx = (box[0] + box[2] / 2) * scale + u['ox']; cy = (box[1] + box[3] / 2) * scale + u['oy']
            t['pivot'] = [cx, cy]
            t['angle'] = it['sector'] * 45.0
            t['bitmaps'] = [assets.scaled(it['image'], bw, bh, '_pad')]
        elif it['type'] == T_SLOT:
            x, y, w, h = it['rect']
            bms = []
            # q*.png пакета движок не показывает (значки заданий — msnicons извне, рамка ext ниже); их размер прежний: крупнее —
            # +330 КБ страниц значков в куче ради картинок, которых нет на экране
            for f in it['images']:
                iw, ih = sizes[f]
                k = min(w / iw, h / ih)
                r = [x + (w - iw * k) / 2, y + (h - ih * k) / 2, iw * k, ih * k]
                d = tx_rect(r, u, scale)
                bms.append((assets.scaled(f, d[2], d[3]), d))
            t['slots'] = bms
            t['dst'] = tx_rect(it['rect'], u, scale)
            if it.get('ext_frame') and mode == 'psp':        # значок задания извне (msnicons) — ровно в эту рамку, 1:1
                t['extframe'] = tx_rect(it['ext_frame'], u, scale)
        elif it['type'] == T_TEXT:
            t['dst'] = tx_rect(it['rect'], u, scale)
            t['fontinfo'] = fonts.get(it['font'])
            if it.get('shadow'):
                sh = it['shadow']
                sdx = 0 if sh['dx'] == 0 else int(math.copysign(max(1, round(abs(sh['dx']) * scale)), sh['dx']))
                sdy = 0 if sh['dy'] == 0 else int(math.copysign(max(1, round(abs(sh['dy']) * scale)), -sh['dy']))
                if it.get('shadow_px') and mode == 'psp':      # правка 163: тень строки — целый сдвиг в px PSP (направление из данных)
                    sdx = int(math.copysign(it['shadow_px'], sdx)) if sdx else 0
                    sdy = int(math.copysign(it['shadow_px'], sdy)) if sdy else 0
                col = list(sh['color'])
                if mode == 'psp' and it.get('page') not in HUD_PAGES and not it.get('shadow_keep'):
                    col[3] = int(round(col[3] * SHADOW_SOFT))   # правка 192: тень в 1 px на PSP читается обводкой — мягче, как у оригинала
                t['sh'] = (sdx, sdy, col)
        elif it['type'] == T_NUM:
            t['dst'] = tx_rect(it['rect'], u, scale)
            if it.get('texts'): t['texts'] = it['texts']
            t['glyphs'], pre = num_glyphs(it, scale, assets)
            if pre: t['prefix'] = pre
        elif it['type'] == T_RADARMAP:
            t['dst'] = tx_rect(it['rect'], u, scale)
        elif it['type'] == T_RADARICONS:
            t['dst'] = tx_rect(it['rect'], u, scale)
            # icon bitmaps at native size * scale (centre-justified around their world position)
            t['icons'] = []
            for g in ICONS:
                d = icon_defs.get(g)
                if not d:
                    t['icons'].append(None); continue
                iw, ih = sizes[d['image']]
                bw, bh = max(1, int(round(iw * scale))), max(1, int(round(ih * scale)))
                t['icons'].append(dict(bitmap=assets.scaled(d['image'], bw, bh), w=bw, h=bh, color=d['color']))
        out.append(t)
    # radar geometry (centre / radius) from the Map0 hole  (hud only)
    for t in out:
        if t['type'] in (T_RADARMAP, T_RADARICONS):
            m = next(x for x in out if x['type'] == T_RADARMAP)
            t['centre'] = [m['dst'][0] + m['dst'][2] / 2, m['dst'][1] + m['dst'][3] / 2]
            t['radius'] = m['dst'][2] / 2
    # Круг радара (Роб 27.09): карта Map0 шире видимого круга, в оригинале её край закрывала дорожка шкалы Hit & Run.
    # Дорожку мы не рисуем, поэтому карту, значки и конус обрезаем по внутреннему краю кольца шкалы; масштаб карты
    # остаётся по Map0 (radius), обрезка отдельно (clip). Радиус меряется по альфе картинки кольца, одно правило для всех уровней
    rm = next((x for x in out if x['type'] == T_RADARMAP), None)
    ring = next((x for x in out if x.get('name') == 'HnRMeterBgd' and x.get('bitmaps')), None)
    if rm is not None and ring is not None:
        clip = ring_inner_radius(assets.bitmaps[ring['bitmaps'][0]], ring['dst'], rm['centre'])
        clip = min(rm['radius'], clip)
        for t in out:
            if t['type'] in (T_RADARMAP, T_RADARICONS):
                t['clip'] = clip
            elif t['type'] == T_POLY and t['tag'] == 'RADAR':      # конус обзора: дальние точки до края круга
                cx, cy = rm['centre']
                pts = []
                for qx, qy in t['pts']:
                    d = math.hypot(qx - cx, qy - cy)
                    k = clip / d if d > clip else 1.0
                    pts.append([cx + (qx - cx) * k, cy + (qy - cy) * k])
                t['pts'] = pts
    return out, units


# ------------------------------------------------------------------------------------------
# 4) level radar map (resource/pure3d/l<N>hudmap.p3d: flat road mesh) -> T4 texture

MAP_TEX = 512
MAP_COLORS = {'road': (8, 8, 8, 255), 'roadedge': (48, 48, 48, 255), 'inter': (8, 8, 8, 255)}


def bake_radar_map(level=LEVEL):
    import convert  # noqa  (MIT, hit-and-run-web)   geometry() of a prim group
    root = read(SCROOBY / ('resource/pure3d/l%dhudmap.p3d' % level))
    parts = []
    for c in walk(root):
        if c.id == 0x10002:
            sh, ps = convert.geometry(c)
            kind = 'roadedge' if 'edge' in sh.lower() else ('inter' if 'inter' in sh.lower() else 'road')
            for p in ps:
                parts.append((kind, p['position'][:, [0, 2]], p['indices'].reshape(-1, 3)))
    allp = np.concatenate([p[1] for p in parts])
    lo, hi = allp.min(0), allp.max(0)
    mpp = float(np.ceil(max(hi - lo) / (MAP_TEX - 8) * 100) / 100)   # metres per texel
    ctr = (lo + hi) / 2
    x0, z0 = ctr[0] - MAP_TEX / 2 * mpp, ctr[1] - MAP_TEX / 2 * mpp
    SS = 4
    img = Image.new('L', (MAP_TEX * SS, MAP_TEX * SS), 0)
    dr = ImageDraw.Draw(img)
    order = {'roadedge': 1, 'road': 2, 'inter': 3}
    for kind in ('roadedge', 'road', 'inter'):
        for k, pos, tri in parts:
            if k != kind:
                continue
            uv = (pos - [x0, z0]) / mpp * SS
            for a, b, c in tri:
                dr.polygon([tuple(uv[a]), tuple(uv[b]), tuple(uv[c])], fill=order[kind])
    return dict(img=img, ss=SS, mpp=mpp, x0=float(x0), z0=float(z0), lo=lo.tolist(), hi=hi.tolist())


def radar_map_rgba(m, col_edge=(24, 40, 24, 235), col_road=(10, 14, 10, 235)):
    """Supersampled class map -> RGBA at MAP_TEX, 16-level palette-friendly."""
    cls = np.asarray(m['img'])
    SS = m['ss']
    rgba = np.zeros(cls.shape + (4,), np.float32)
    rgba[cls == 1] = col_edge
    rgba[cls >= 2] = col_road
    a = rgba.reshape(MAP_TEX, SS, MAP_TEX, SS, 4)
    pm = a.copy(); pm[..., :3] *= pm[..., 3:4] / 255
    pm = pm.mean(axis=(1, 3))
    al = pm[..., 3:4]
    rgb = np.where(al > 0.5, pm[..., :3] / np.maximum(al, 1e-3) * 255, 0)
    out = np.concatenate([rgb, al], -1)
    return Image.fromarray(np.clip(out + 0.5, 0, 255).astype(np.uint8), 'RGBA')


# ------------------------------------------------------------------------------------------
# 5) software renderer of the adapted draw list (mock images; mirrors viewer/ui.c)

def fmt_value(it, v):
    fm = it.get('fmt')
    if fm == 'time':
        s = int(v) // 1000
        return '%d:%02d' % (s // 60, s % 60)
    if fm == 'frac':
        return '%d/%d' % v if isinstance(v, tuple) else str(v)
    return str(v)


def comp(canvas, img, x, y, color=(255, 255, 255, 255)):
    if img is None:
        return
    if color != (255, 255, 255, 255):
        a = np.asarray(img).astype(np.float32)
        a = a * np.array(color, np.float32) / 255
        img = Image.fromarray(np.clip(a + 0.5, 0, 255).astype(np.uint8), 'RGBA')
    canvas.alpha_composite(img, (int(x), int(y))) if x >= 0 and y >= 0 else canvas.paste(
        Image.alpha_composite(canvas.crop((int(x), int(y), int(x) + img.width, int(y) + img.height)), img),
        (int(x), int(y)))


def draw_alpha(canvas, bm, x, y, color):
    if bm is None or bm.size == 0:
        return
    h, w = bm.shape
    im = np.zeros((h, w, 4), np.uint8)
    im[..., 0], im[..., 1], im[..., 2] = color[0], color[1], color[2]
    im[..., 3] = np.clip(bm * color[3] + 0.5, 0, 255).astype(np.uint8)
    comp(canvas, Image.fromarray(im, 'RGBA'), x, y)


def render(dl, W, H, state, bitmaps, bg, radar_map=None, selected=-1):
    canvas = bg.copy().convert('RGBA')
    dr = ImageDraw.Draw(canvas, 'RGBA')
    shown = state.get('show', set())
    for t in dl:
        if t['tag'] != 'ALWAYS' and t['tag'] not in shown:
            continue
        if t.get('selonly') and t['menu'] != selected:
            continue
        ty = t['type']
        col = tuple(t.get('color', (255, 255, 255, 255)))
        if ty == T_RECT:
            x, y, w, h = t['dst']
            if t.get('colors'):   # vertical gradient (top two, bottom two)
                c = t['colors']
                g = Image.new('RGBA', (w, h))
                arr = np.zeros((h, w, 4), np.float32)
                for yy in range(h):
                    f = yy / max(1, h - 1)
                    arr[yy, :] = np.array(c[0]) * (1 - f) + np.array(c[2]) * f
                g = Image.fromarray(arr.astype(np.uint8), 'RGBA')
                comp(canvas, g, x, y)
            else:
                ov = Image.new('RGBA', canvas.size, (0, 0, 0, 0))
                ImageDraw.Draw(ov).rectangle([x, y, x + w - 1, y + h - 1], fill=col)
                canvas.alpha_composite(ov)
        elif ty == T_POLY:
            ov = Image.new('RGBA', canvas.size, (0, 0, 0, 0))
            ImageDraw.Draw(ov).polygon([tuple(p) for p in t['pts']], fill=tuple(t['colors'][1]))
            canvas.alpha_composite(ov)
        elif ty == T_IMG and not t['bitmaps']:
            pass                                          # картинка извне (MissionStartBitmap): рисует движок
        elif ty == T_IMG:
            k = state.get(t['field'], 0) if t['field'] != 'NONE' else 0
            bmk = t['bitmaps'][min(int(k), len(t['bitmaps']) - 1)]
            comp(canvas, bitmaps[bmk], t['dst'][0], t['dst'][1], col)
        elif ty == T_BAR:
            v = state.get(t['field'], 0) / 1000.0
            bm = bitmaps[t['bitmaps'][0]]
            cw = int(round(bm.width * v))
            if cw > 0:
                comp(canvas, bm.crop((0, 0, cw, bm.height)), t['dst'][0], t['dst'][1], col)
        elif ty == T_SECTOR:
            # dark sector = shutter over the unfilled part of the ring (clockwise from the top)
            lit = math.ceil(state.get('HNR', 0) * 8 / 1000 - 1e-6)
            if t['sector'] < lit:
                continue
            bm = bitmaps[t['bitmaps'][0]]
            # rotate about pivot: place bitmap on a big transparent canvas, rotate, composite
            px, py = t['pivot']
            big = Image.new('RGBA', canvas.size, (0, 0, 0, 0))
            big.alpha_composite(bm, (t['dst'][0], t['dst'][1]))
            big = big.rotate(-t['angle'], resample=Image.BILINEAR, center=(px, py))
            if col != (255, 255, 255, 255):
                a = np.asarray(big).astype(np.float32) * np.array(col, np.float32) / 255
                big = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), 'RGBA')
            canvas.alpha_composite(big)
        elif ty == T_SLOT:
            k = state.get(t['field'], 0)
            bmk, d = t['slots'][k % len(t['slots'])]
            comp(canvas, bitmaps[bmk], d[0], d[1], col)
        elif ty == T_TEXT:
            f = t['fontinfo']
            if t['field'] != 'NONE' and t['fmt'] in ('int',):
                s = str(state.get(t['field'], 0))
            elif t['field'] == 'MENU_ITEM':
                s = t['strings'][selected % len(t['strings'])] if selected >= 0 else t['strings'][0]
            elif t['field'] == 'POSITION':
                p = state.get('POSITION', 1); s = t['strings'][min(max(p, 1), 4) - 1] if p < 4 else t['strings'][3]
            elif t['field'] != 'NONE':
                s = t['strings'][state.get(t['field'], 0) % len(t['strings'])]
            else:
                s = t['strings'][t.get('sel', 0)] if t['strings'] else ''
            c = col
            if t['menu'] >= 0 and t['menu'] == selected:
                c = (255, 230, 20, 255)
            glyphs = layout_text(f, s, t['dst'], t['halign'], t['valign'], t['wrap'])
            if t.get('sh'):
                sdx, sdy, sc = t['sh']
                for code, gx, gy in glyphs:
                    draw_alpha(canvas, f['glyphs'][code]['bm'], gx + sdx, gy + sdy, sc)
            for code, gx, gy in glyphs:
                draw_alpha(canvas, f['glyphs'][code]['bm'], gx, gy, c)
        elif ty == T_NUM:
            if t['field'] != 'NONE':
                v = state.get(t['field'])
                if v is None:
                    continue
                if t['field'] == 'LAP':
                    v = (state.get('LAP', 1), state.get('LAPS', 3))
                if t['field'] == 'CARDS':
                    v = (state.get('CARDS', 0), state.get('CARDS_TOTAL', 7))
                if t['field'] == 'COLLECTED':
                    v = (state.get('COLLECTED', 0), state.get('COLLECT_TOTAL', 10))
                s = fmt_value(t, v)
            else:
                s = t['text']
            gl = [t['glyphs'].get(ch.upper()) for ch in s]
            gl = [g for g in gl if g]
            tw = sum(g[4] for g in gl)
            x, y, w, h = t['dst']
            x0 = x if t['halign'] == 'left' else (x + w - tw if t['halign'] == 'right' else x + (w - tw) // 2)
            if t.get('prefix'):
                pb = bitmaps[t['prefix']]
                comp(canvas, pb, x0 - pb.width - PREFIX_GAP, y + (h - pb.height) // 2)
            for g in gl:
                if g[0]:
                    gy = y + (h - g[2]) // 2 if t['valign'] == 'centre' else (y if t['valign'] == 'top' else y + h - g[2])
                    comp(canvas, bitmaps[g[0]], x0 + g[3], gy, col)
                x0 += g[4]
        elif ty == T_RADARMAP and radar_map is not None:
            cx, cy = t['centre']; r = t['radius']
            px, pz, hd, rng = state.get('RADAR', (220.0, 172.0, math.pi / 2, 150.0))
            k = r / rng
            yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
            ox, oy = xx + 0.5 - cx, yy + 0.5 - cy
            rc = t.get('clip', r)                                   # круг радара (обрезка), масштаб по r
            inside = ox * ox + oy * oy <= rc * rc
            rr, ff = ox / k, -oy / k
            c, s_ = math.cos(hd), math.sin(hd)
            dx = -c * rr + s_ * ff; dz = s_ * rr + c * ff
            u = (px + dx - radar_map['x0']) / radar_map['mpp']; v = (pz + dz - radar_map['z0']) / radar_map['mpp']
            mi = np.asarray(radar_map['rgba'])
            ui = np.clip(u.astype(int), 0, MAP_TEX - 1); vi = np.clip(v.astype(int), 0, MAP_TEX - 1)
            samp = mi[vi, ui].astype(np.float32)
            samp[~inside] = 0
            samp[(u < 0) | (v < 0) | (u >= MAP_TEX) | (v >= MAP_TEX)] = 0
            canvas.alpha_composite(Image.fromarray(samp.astype(np.uint8), 'RGBA'))
        elif ty == T_RADARICONS:
            cx, cy = t['centre']; r = t['radius']
            px, pz, hd, rng = state.get('RADAR', (220.0, 172.0, math.pi / 2, 150.0))
            k = r / rng
            for typ, wx, wz in state.get('ICONS', []):
                ic = t['icons'][typ]
                if not ic:
                    continue
                dx, dz = wx - px, wz - pz
                c, s_ = math.cos(hd), math.sin(hd)
                rr = -c * dx + s_ * dz; ff = s_ * dx + c * dz
                sx, sy = rr * k, -ff * k
                d = math.hypot(sx, sy); lim = t.get('clip', r) - max(ic['w'], ic['h']) * 0.35
                if d > lim:
                    sx, sy = sx * lim / d, sy * lim / d
                comp(canvas, bitmaps[ic['bitmap']], int(round(cx + sx - ic['w'] / 2)),
                     int(round(cy + sy - ic['h'] / 2)), tuple(ic['color']))
    return canvas


def backdrop(W, H, kind):
    """Neutral stand-in for the 3D scene behind the HUD/menus (identical in both mocks)."""
    y = np.linspace(0, 1, H)[:, None]
    x = np.linspace(0, 1, W)[None, :]
    sky = np.stack([120 + 60 * y, 170 + 40 * y, 230 - 20 * y], -1) * np.ones((1, W, 1))
    ground = np.stack([80 + 30 * x, 120 + 20 * x, 60 + 0 * x], -1) * np.ones((H, 1, 1))
    img = np.where((y > 0.55)[..., None], ground, sky)
    # checker on the ground so any horizontal stretch is visible
    cx = (np.floor(x * 16) + np.floor(y * 9)) % 2
    img = img * np.where((y > 0.55) & (cx == 0), 0.9, 1.0)[..., None]
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), 'RGB').convert('RGBA')


DEMO_B = dict(show={'RADAR', 'SPEED', 'HNR_METER', 'HNR_FULL', 'DAMAGE', 'HNR_TICKET', 'CARDS', 'POSITION', 'ACTION'},
              SPEED=132, SPEED_UNIT=1, COINS=57, DAMAGE=650, HNR=1000, HNR_LIGHT=1, HNR_TEXT=1, CARDS=3,
              CARDS_TOTAL=7, POSITION=2, RADAR=(220.0, 172.0, 0.3, 150.0), ICONS=[(9, 260.0, 200.0), (8, 180.0, 150.0)])
DEMO_STATE = dict(show={'RADAR', 'SPEED', 'HNR_METER', 'COINS', 'OBJECTIVE', 'TIMER'},
                  SPEED=88, SPEED_UNIT=0, COINS=1234, TIMER=83000, DAMAGE=650, HNR=375, HNR_LIGHT=0,
                  OBJECTIVE=2, OBJICON=0, RADAR=(220.0, 172.0, math.pi / 2, 150.0),
                  ICONS=[(10, 300.0, 120.0), (6, 150.0, 260.0), (4, 180.0, 150.0), (9, 240.0, 190.0),
                         (10, 900.0, 900.0), (5, 250.0, 120.0)])


# ------------------------------------------------------------------------------------------
# 6) texture pages (CLUT8, swizzled) + font page (T4) + map page (T4)

def kmeans_palette(pixels, k=255, iters=14, seed=1):
    """pixels: (N,4) uint8 RGBA.  Premultiplied k-means -> (k,4) uint8 palette (index 0 = clear)."""
    px = pixels.astype(np.float32)
    vis = px[px[:, 3] > 0]
    if len(vis) == 0:
        return np.zeros((k, 4), np.uint8)
    key = vis.astype(np.uint32)
    packed = (key[:, 0] << 24) | (key[:, 1] << 16) | (key[:, 2] << 8) | key[:, 3]
    uniq, inv, cnt = np.unique(packed, return_inverse=True, return_counts=True)
    cols = np.stack([(uniq >> 24) & 255, (uniq >> 16) & 255, (uniq >> 8) & 255, uniq & 255], -1).astype(np.float32)
    if len(cols) <= k:
        pal = np.zeros((k, 4), np.float32); pal[:len(cols)] = cols
        return pal.astype(np.uint8)
    feat = cols.copy(); feat[:, :3] *= feat[:, 3:4] / 255
    w = cnt.astype(np.float32)
    rng = np.random.default_rng(seed)
    # init: weighted sample + extremes of alpha
    idx = rng.choice(len(feat), size=k, replace=False, p=w / w.sum())
    cen = feat[idx].copy()
    for _ in range(iters):
        d = ((feat[:, None, :] - cen[None, :, :]) ** 2).sum(-1) if len(feat) * k < 4e7 else None
        if d is None:
            lab = np.empty(len(feat), np.int64)
            for s0 in range(0, len(feat), 20000):
                dd = ((feat[s0:s0 + 20000, None, :] - cen[None]) ** 2).sum(-1); lab[s0:s0 + 20000] = dd.argmin(1)
        else:
            lab = d.argmin(1)
        for j in range(k):
            m = lab == j
            if m.any():
                cen[j] = (feat[m] * w[m, None]).sum(0) / w[m].sum()
            else:
                cen[j] = feat[rng.integers(len(feat))]
    pal = cen.copy()
    a = np.maximum(pal[:, 3:4], 1e-3)
    pal[:, :3] = np.clip(pal[:, :3] * 255 / a, 0, 255)
    return np.clip(pal + 0.5, 0, 255).astype(np.uint8)


def map_to_palette(img_rgba, pal):
    """Nearest palette entry in premultiplied space; alpha 0 -> index 0 (clear)."""
    a = np.asarray(img_rgba).reshape(-1, 4).astype(np.float32)
    f = a.copy(); f[:, :3] *= f[:, 3:4] / 255
    P = pal.astype(np.float32); P[:, :3] *= P[:, 3:4] / 255
    full = np.concatenate([np.zeros((1, 4), np.float32), P], 0)
    out = np.empty(len(f), np.uint8)
    for s0 in range(0, len(f), 16384):
        d = ((f[s0:s0 + 16384, None, :] - full[None]) ** 2).sum(-1)
        out[s0:s0 + 16384] = d.argmin(1)
    out[a[:, 3] == 0] = 0
    return out.reshape(img_rgba.height, img_rgba.width)


def shelf_pack(sizes, W, H, pad=1):
    """sizes: dict key->(w,h).  -> dict key->(x,y) for one page, and leftover keys."""
    order = sorted(sizes, key=lambda k: (-sizes[k][1], -sizes[k][0]))
    pos, left = {}, []
    x = y = rowh = 0
    for k in order:
        w, h = sizes[k]
        w += pad * 2; h += pad * 2
        if x + w > W:
            x = 0; y += rowh; rowh = 0
        if y + h > H or w > W:
            left.append(k); continue
        pos[k] = (x + pad, y + pad); x += w; rowh = max(rowh, h)
    return pos, left


def swizzle(buf, width_bytes, height):
    a = np.asarray(buf, np.uint8).reshape(height // 8, 8, width_bytes // 16, 16)
    return a.transpose(0, 2, 1, 3).reshape(-1).tobytes()


def pow2(n, lo=8):
    p = lo
    while p < n:
        p *= 2
    return p


def build_pages(bitmaps, maxw=512, maxh=512):
    """Pack every bitmap into CLUT8 pages. -> pages list, where dict key -> (page, x, y, w, h)."""
    sizes = {k: v.size for k, v in bitmaps.items()}
    # big bitmaps (TV frame) get pieces first: cut to opaque bands to save memory
    where = {}
    pages = []
    todo = dict(sizes)
    while todo:
        pos, left = shelf_pack(todo, maxw, maxh)
        if not pos:
            raise SystemExit('bitmap too large for a page: %s' % left)
        used_w = max(pos[k][0] + todo[k][0] for k in pos) + 1
        used_h = max(pos[k][1] + todo[k][1] for k in pos) + 1
        pw, ph = pow2(used_w, 16), pow2(used_h, 8)
        canvas = Image.new('RGBA', (pw, ph), (0, 0, 0, 0))
        for k, (x, y) in pos.items():
            canvas.paste(bitmaps[k], (x, y))
            where[k] = (len(pages), x, y, todo[k][0], todo[k][1])
        pages.append(canvas)
        todo = {k: todo[k] for k in left}
    return pages, where


def split_tvframe(img, stripe=16, gap=16):
    """Opaque pieces of the widened TV frame: per 16-row stripe, runs of columns that hold any
    alpha (gaps < 16 px merged).  The empty screen area is not stored."""
    a = np.asarray(img)[..., 3]
    W, H = img.size
    out = []
    for y0 in range(0, H, stripe):
        y1 = min(H, y0 + stripe)
        cols = np.where(a[y0:y1].max(0) > 0)[0]
        if len(cols) == 0:
            continue
        run0 = prev = cols[0]
        for c in list(cols[1:]) + [None]:
            if c is None or c - prev > gap:
                out.append(((int(run0), y0, int(prev) + 1, y1), '%d_%d' % (y0, run0)))
                if c is not None:
                    run0 = c
            if c is not None:
                prev = c
    return out


# ------------------------------------------------------------------------------------------
# catalogue / mocks / pack

def catalog():
    out_md = ['# Scrooby UI catalogue (from the PS2 disc)', '',
              'Coordinates: Scrooby authoring space 640x480, **y up, origin bottom-left** (as stored).',
              'Anchor = result of the thirds rule / overrides (L/C/R) for the PSP layout.', '']
    js = {}
    for screen in SCREENS:
        items, icons = build_items(screen)
        units = compute_units(items, all_images())
        out_md += ['## Screen `%s` (pages: %s)' % (screen, ', '.join(p for _, p in SCREEN_PAGES[screen])), '',
                   '| page | group path | element | kind | x,y,w,h (4:3, y up) | image / font / text | dynamic | tag | anchor |',
                   '|---|---|---|---|---|---|---|---|---|']
        js[screen] = []
        for it in items:
            x, y, w, h = it['rect']
            yup = SRC_H - (y + h)
            kind = {T_RECT: 'rect', T_POLY: 'poly', T_IMG: 'sprite', T_TEXT: 'text', T_NUM: 'sprite-font',
                    T_RADARMAP: 'radar map', T_RADARICONS: 'radar icons', T_SLOT: 'image slot', T_BAR: 'bar',
                    T_SECTOR: 'sector'}[it['type']]
            what = ''
            if it['type'] == T_IMG:
                what = ','.join(it['frames'])
            elif it['type'] == T_TEXT:
                what = '%s: "%s"%s' % (it['font'], (it['strings'][0] if it['strings'] else '')[:40],
                                       ' (+%d)' % (len(it['strings']) - 1) if len(it['strings']) > 1 else '')
            elif it['type'] in (T_BAR, T_SECTOR):
                what = it['image']
            elif it['type'] == T_SLOT:
                what = '%d mission icons (q*.png)' % len(it['images'])
            elif it['type'] == T_NUM:
                what = 'digits/letters 45x50 sprite font'
            elif it['type'] == T_RADARICONS:
                what = ', '.join('%s=%s' % (k, v['image']) for k, v in icons.items())
            elif it['type'] in (T_RECT, T_POLY):
                what = 'rgba%s%s' % (tuple(it.get('color') or it['colors'][0]), ' full-width' if it.get('fill') else '')
            dyn = it['field'] if it['field'] != 'NONE' else ('menu #%d' % it['menu'] if it['menu'] >= 0 else '')
            anc = 'fill' if it.get('fill') or it.get('tvframe') else units.get(it['unit'], {}).get('anchor', '')
            out_md.append('| %s | %s | %s | %s | %d,%d,%d,%d | %s | %s | %s | %s |' % (
                it['page'], it['path'], it['name'], kind, x, yup, w, h, what.replace('|', '/'), dyn, it['tag'], anc))
            js[screen].append(dict(page=it['page'], path=it['path'], name=it['name'], kind=kind,
                                   rect_yup=[x, yup, w, h], what=what, dynamic=dyn, tag=it['tag'], anchor=anc))
        out_md.append('')
        out_md.append('Anchor units: ' + '; '.join('%s → %s' % (k[1], v['anchor']) for k, v in units.items()))
        out_md.append('')
    SCRATCH.mkdir(parents=True, exist_ok=True)
    (SCRATCH / 'catalog.md').write_text('\n'.join(out_md))
    (SCRATCH / 'catalog.json').write_text(json.dumps(js, indent=1))
    print('catalog ->', SCRATCH / 'catalog.md')


def mock():
    SCRATCH.mkdir(parents=True, exist_ok=True)
    rmap = bake_radar_map()
    rmap['rgba'] = radar_map_rgba(rmap)
    for screen in SCREENS + ['hud_b']:
        items, icons = build_items('hud' if screen == 'hud_b' else screen)
        state = dict(DEMO_B if screen == 'hud_b' else DEMO_STATE)
        sel = 0 if screen in ('pause', 'pausemsn') else (5 if screen == 'main' else -1)
        if screen in ('pause', 'pausemsn'):
            state['show'] = {'COINS', 'OBJECTIVE'}
        # reference 4:3 (identity), then naive stretch
        ra = Assets(); rf = FontCache('ref')
        ref, _ = adapt(items, icons, 'ref', ra, rf)
        bg43 = backdrop(SRC_W, SRC_H, screen)
        img43 = render(ref, SRC_W, SRC_H, state, ra.bitmaps, bg43, rmap, sel)
        stretched = img43.resize((DST_W, DST_H), Image.LANCZOS)
        pa = Assets(); pf = FontCache('psp')
        psp, units = adapt(items, icons, 'psp', pa, pf)
        imgw = render(psp, DST_W, DST_H, state, pa.bitmaps, backdrop(DST_W, DST_H, screen), rmap, sel)
        sheet = Image.new('RGBA', (DST_W * 2 + 12, DST_H + 22), (30, 30, 30, 255))
        sheet.paste(stretched, (0, 22)); sheet.paste(imgw, (DST_W + 12, 22))
        d = ImageDraw.Draw(sheet)
        d.text((4, 4), '4:3 original stretched to 16:9 (wrong)', fill=(255, 120, 120, 255))
        d.text((DST_W + 16, 4), 'anchored adaptation 480x272 (ours)', fill=(140, 255, 140, 255))
        sheet.save(SCRATCH / ('mock_%s.png' % screen))
        imgw.save(SCRATCH / ('mock_%s_psp.png' % screen))
        img43.save(SCRATCH / ('mock_%s_43.png' % screen))
        sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST).save(SCRATCH / ('mock_%s_x2.png' % screen))
        print('mock', screen, '->', SCRATCH / ('mock_%s.png' % screen))


# ------------------------------------------------------------------------------------------
# 7) binary pack  build/ui/ui.pak   (format documented in viewer/ui.h)

PAK_MAGIC = b'UIP1'
PSM_T4, PSM_T8 = 4, 5
DEFAULT_TAGS = {'hud': {'ALWAYS', 'RADAR', 'SPEED', 'HNR_METER', 'COINS'},
                'pause': {'ALWAYS', 'COINS'},
                'pausemsn': {'ALWAYS', 'COINS'},
                'levelstats': {'ALWAYS', 'SKIP'},
                'tutorial': {'ALWAYS'},
                'message': {'ALWAYS'},
                'main': {'ALWAYS', 'SKIP'},
                'missionload': {'ALWAYS', 'PRESS_START'},   # правка 52: Continue по умолчанию; код прячет его на время LOADING
                'missionover': {'ALWAYS'},
                'letterbox': {'ALWAYS'}}


def abgr(c):
    r, g, b, a = [int(v) for v in c]
    return (a << 24) | (b << 16) | (g << 8) | r


OBJ_ICONS = set()


# №888 (Роб 23.10: «стрелки ◀ ▶ у New Game / Resume Game / Load Game битые, пиксели поломаны в центре, не как в остальных меню»): стрелки главного меню лежали
# на общей странице ui1 с 255 цветами на всё — k-means размазывал зелёную рампу граней, в центре стрелки появлялись тёмные штрихи. Теперь они на своей
# маленькой странице со своей палитрой (как в пакетах меню, где стрелки чистые). ARROWS_PAGE_OLD=1 — прежнее (общая страница)
ARROWS_PAGE_OLD = os.environ.get('ARROWS_PAGE_OLD') == '1'
ARROW_IMGS = ('larrow.png', 'rarrow.png', 'larrowg.png', 'rarrowg.png')


def page_group(key):
    if key.startswith('tvframe'):
        return 'tv'
    if not ARROWS_PAGE_OLD and key.split('@')[0] in ARROW_IMGS:
        return 'arrows'
    if key.split('@')[0] in OBJ_ICONS:
        return 'icons'
    return 'ui'


class Blob:
    def __init__(self):
        self.b = bytearray()

    def align(self, n=64):
        while len(self.b) % n:
            self.b.append(0)
        return len(self.b)

    def add(self, data, align=64):
        off = self.align(align)
        self.b += data
        return off


def quantize_page(img, ncol=255):
    if os.environ.get('UI_QUANT_OPAQUE_OLD') != '1':
        return quantize_page_opaque(img, ncol)
    px = np.asarray(img).reshape(-1, 4)
    pal = kmeans_palette(px, ncol)
    idx = map_to_palette(img, pal)
    full = np.concatenate([np.zeros((1, 4), np.uint8), pal], 0)
    if len(full) < 256:
        full = np.concatenate([full, np.zeros((256 - len(full), 4), np.uint8)], 0)
    return idx, full[:256]


def quantize_page_opaque(img, ncol=255):
    """№683 (Роб 17.10: «у некоторых машин в будке снизу пиксельный шум» — Лимо, седан): общий k-means смешивал непрозрачные тёмные
    цвета с полупрозрачными краями (центр кластера с альфой 200–250), и непрозрачный кузов чёрной машины ложился в кадр частично
    прозрачным — сквозь него просвечивал фон (на PSP — крапинки). Правило для всех страниц CLUT8: непрозрачные пиксели — своя часть
    палитры (k-means только по ним, альфа 255) и только она; полупрозрачные края — своя часть (доля по числу их различных цветов,
    16..ncol/3) и ближайший по всей палитре; альфа 0 — индекс 0. UI_QUANT_OPAQUE_OLD=1 — прежний общий k-means"""
    a = np.asarray(img); H, W = a.shape[:2]
    px = a.reshape(-1, 4); al = px[:, 3]
    op = al == 255; semi = (al > 0) & (al < 255)
    if not semi.any() or not op.any():
        pal = kmeans_palette(px, ncol)
    else:
        nu_s = len(np.unique(px[semi].view(np.uint32))); nu_o = len(np.unique(px[op].view(np.uint32)))
        ns = int(np.clip(round(ncol * nu_s / (nu_s + nu_o)), min(16, nu_s), ncol // 3))
        po = kmeans_palette(px[op], ncol - ns); po = po[po[:, 3] > 0]; po[:, 3] = 255
        ps = kmeans_palette(px[semi], ns); ps = ps[(ps[:, 3] > 0) & (ps[:, 3] < 255)]
        pal = np.concatenate([po, ps], 0)
    full = np.concatenate([np.zeros((1, 4), np.uint8), pal], 0)
    idx = map_to_palette(img, pal).reshape(-1)
    if op.any() and semi.any():
        P = po[:, :3].astype(np.float32); f = px[op][:, :3].astype(np.float32); out = np.empty(len(f), np.uint8)
        for s0 in range(0, len(f), 16384):
            out[s0:s0 + 16384] = 1 + ((f[s0:s0 + 16384, None, :] - P[None]) ** 2).sum(-1).argmin(1)
        idx[op] = out
    if len(full) < 256:
        full = np.concatenate([full, np.zeros((256 - len(full), 4), np.uint8)], 0)
    return idx.reshape(H, W).astype(np.uint8), full[:256]


def snap565(c):
    """цвет -> ближайший точный цвет кадра 5650 (R, B — шаг 8, G — шаг 4; GE отбрасывает младшие биты)"""
    c = np.asarray(c, np.int32).copy()
    c[..., 0] = np.clip((c[..., 0] + 4) >> 3 << 3, 0, 248); c[..., 1] = np.clip((c[..., 1] + 2) >> 2 << 2, 0, 252)
    c[..., 2] = np.clip((c[..., 2] + 4) >> 3 << 3, 0, 248)
    return c


def quantize_page_565(img, ncol=255):
    """№409: страница -> CLUT8, палитра — точные цвета кадра 5650, непрозрачные пиксели рассеяны Флойдом–Стейнбергом (ошибка
    считается от цвета, который реально ляжет в кадр 5650 без дизеринга GE). Палитра: k-means как quantize_page + цвета входа,
    округлённые до 5650, самые частые (чтобы плавные свечения имели все ступени своей рампы). Прозрачные/полупрозрачные пиксели —
    ближайший, как раньше"""
    a = np.asarray(img).astype(np.int32); H, W = a.shape[:2]
    px = a.reshape(-1, 4); op = px[:, 3] == 255
    sn = snap565(px[op][:, :3]); u, cnt = np.unique(sn, axis=0, return_counts=True)
    km = kmeans_palette(px.astype(np.uint8), ncol)
    kmv = km[km[:, 3] > 0].astype(np.int32)
    semi = kmv[kmv[:, 3] < 255]                                  # края с альфой — из k-means как есть
    room = ncol - len(semi)
    order = np.argsort(-cnt); opq = [tuple(c) for c in u[order]]
    if len(opq) > room:                                          # частые точные цвета + точные k-means непрозрачных
        kmo = [tuple(c) for c in snap565(kmv[kmv[:, 3] == 255][:, :3])]
        keep = list(dict.fromkeys(kmo))[:room // 2]
        for c in opq:
            if len(keep) >= room: break
            if c not in keep: keep.append(c)
        opq = keep
    pal = np.zeros((ncol, 4), np.uint8)
    pal[:len(opq), :3] = np.array(opq, np.int32); pal[:len(opq), 3] = 255
    pal[len(opq):len(opq) + len(semi)] = semi
    npal = len(opq) + len(semi)
    full = np.concatenate([np.zeros((1, 4), np.uint8), pal], 0)
    idx = map_to_palette(img, pal).astype(np.int32).reshape(H, W)
    P = np.array(opq, np.float32); err = np.zeros((H + 1, W + 2, 3), np.float32)
    rgb = a[..., :3].astype(np.float32)
    for y in range(H):
        for x in range(W):
            if a[y, x, 3] != 255: continue
            c = rgb[y, x] + err[y, x + 1]
            k = int(((P - c) ** 2).sum(1).argmin()); idx[y, x] = 1 + k
            e = c - P[k]
            err[y, x + 2] += e * (7 / 16); err[y + 1, x] += e * (3 / 16); err[y + 1, x + 1] += e * (5 / 16); err[y + 1, x + 2] += e * (1 / 16)
    if len(full) < 256:
        full = np.concatenate([full, np.zeros((256 - len(full), 4), np.uint8)], 0)
    return idx.astype(np.uint8), full[:256]


HOME_ICON_OLD = os.environ.get('HUD_HOME_ICON_OLD') == '1'   # №623: 1 — без значка дома вне миссии (прежний пакет)


def home_icon(dl, assets):
    """№623 (Роб 15.10: «сделай как в оригинале»; PS2, внешнее видео ~100 кадров HUD): у оригинала кружок слева вверху есть всегда, вне миссии это значок дома
    (msnicons/location/simpsons), время, круг и место стоят правее него; кружок пропадает только в меню, диалогах и затемнении. Значок дома — элемент HomeIcon
    пакета ui.shr (res[0] бит 11, тег ALWAYS; движок рисует его, пока кружка задания в углу нет, и прячет вместе с радаром), размер и место ровно как у свёрнутого
    значка задания (рамка аватара, верх по полю от видимого левого края: viewer/ui_edge_padding.h uiIconTopShift). HUD_HOME_ICON_OLD=1 — без значка"""
    if HOME_ICON_OLD or os.environ.get('HUD_HOME_ICON_OLD') == '1':   # флаг читается и при вызове (окружение могли выставить после импорта модуля)
        return
    ic = next((t for t in dl if t['name'] == 'ObjectiveIcon' and t.get('extframe')), None)
    tmpl = next((t for t in dl if t['name'] == 'HnRMeterFrame' and t['type'] == T_IMG), None)
    if not (ic and tmpl):
        return
    from p3d import read
    root = read(WEB / 'source/game/art/frontend/dynaload/images/msnicons/location/simpsons.p3d')
    sprite = next((c for c in root.children if c.id == 0x19005), None)
    if sprite is None:
        return
    _, im = decode_sprite_image(sprite)
    bx, by, bw, bh = (int(round(v)) for v in ic['extframe'])
    r = resample(im, bw, bh)
    a = np.asarray(r)[..., 3]
    cols = np.where((a >= 32).any(axis=0))[0]; rows = np.where((a >= 32).any(axis=1))[0]
    if not len(cols):
        return
    shift = max(2.0, bx + float(cols[0])) - (by + float(rows[0]))       # uiIconTopShift: верх видимого круга = поле, равное видимому левому краю (не меньше 2 px)
    if os.environ.get('HUD_HOME_RING_OLD') != '1':
        # №724 (Роб 17.10, видео IMG_7280 5:05: «значок дома по старым замерам, слишком низко»): правило выше ставило по полю верх КАРТИНКИ —
        # трубу и антенну дома над кольцом, и кольцо дома стояло на 8 px ниже кольца свёрнутой аватарки задания (эмулятор: жёлтое кольцо
        # дома y 28, аватарки y 20, обе x 20). Теперь по тому же правилу ставится само кольцо (жёлтый обод): верх кольца = его левое поле,
        # как у аватарки; картинка над кольцом не выше 2 px от края. HUD_HOME_RING_OLD=1 — прежнее
        rgb = np.asarray(r)[..., :3].astype(int)
        ring = (a >= 128) & (rgb[..., 0] > 180) & (rgb[..., 1] > 130) & (rgb[..., 2] < 90) & (rgb[..., 0] - rgb[..., 2] > 120)
        rr = np.where(ring.any(axis=1))[0]; rc = np.where(ring.any(axis=0))[0]
        if len(rr) and len(rc):
            shift = max(max(2.0, bx + float(rc[0])) - (by + float(rr[0])), 2.0 - (by + float(rows[0])))
    key = assets.put('home_icon', r)
    OBJ_ICONS.add('home_icon')
    t = dict(tmpl, name='HomeIcon', tag='ALWAYS', field='NONE', dst=[bx, int(math.floor(by + shift + .5)), bw, bh], bitmaps=[key], home=True, home_right=bx + int(cols[-1]) + 1)   # home_right — видимый правый край (дом выступает за кольцо): px записи
    for k in ('frames', 'rect', 'unit', 'anchor', 'path'):
        t.pop(k, None)
    dl.append(t)


def pack(out=None, spec=None):
    """spec (tools/load_export.py): другой набор экранов тем же форматом UIP1 — dict screens (порядок), build(sc, assets,
    fonts) -> список рисования, tags {экран: теги по умолчанию}, group(key) -> группа страниц, groups (порядок групп),
    radar (карта радара), layout (имя json в build/ui), assets/fonts (готовые).  Без spec — ui.shr как раньше."""
    OUTDIR.mkdir(parents=True, exist_ok=True)
    SCRATCH.mkdir(parents=True, exist_ok=True)
    sp = spec or {}
    scr_list = sp.get('screens', SCREENS)
    assets = sp.get('assets') or Assets()
    fonts = sp.get('fonts') or FontCache('psp')
    screens = {}
    icon_sets = {}
    for sc in scr_list:
        if 'build' in sp:
            screens[sc] = sp['build'](sc, assets, fonts)
            continue
        items, icons = build_items(sc)
        assets.images.update(EXTRA_IMAGES)   # №613: синтетические картинки (плашка под картинкой задания)
        dl, units = adapt(items, icons, 'psp', assets, fonts)
        if sc == 'message': msgbox_fit(dl)
        if sc == 'hud': home_icon(dl, assets)
        screens[sc] = dl
    # правка 312: настройки твик-панели типографики (build/ui_typo.json, tools/ui_typo.py) — после раскладки экрана; без файла ничего
    import ui_typo as UT
    tcfg = UT.load()
    if tcfg:
        for sc in scr_list:
            screens[sc] = UT.apply(sc, screens[sc], assets, fonts, tcfg)
        UT.prune(screens, assets)
    if sp.get('post'):   # раскладка после значений панели Роба (будка: счёт монет на линии названия машины)
        sp['post'](screens)
    # ---- sprite pages (CLUT8), grouped
    groups = {}
    grp_of = sp.get('group', page_group)
    for k, im in assets.bitmaps.items():
        groups.setdefault(grp_of(k), {})[k] = im
    tex = []            # dicts: psm, w, h, pixels(bytes, swizzled), clut(bytes), nclut
    where = {}          # bitmap key -> (tex, x, y, w, h)
    preview = []
    for gname in sp.get('groups', ('ui', 'icons', 'tv', 'arrows')):
        if gname not in groups:
            continue
        pages, wh = build_pages(groups[gname])
        base = len(tex)
        for pi, pg in enumerate(pages):
            idx, clut = quantize_page_565(pg) if gname in sp.get('fs565', ()) else quantize_page(pg)   # №409: группы с рассеянием 5650
            h, w = idx.shape
            if os.environ.get('UI_QUANT_STATS') == '1':   # №683: замер страницы (пакет не меняет): непрозрачных пикселей, ставших прозрачнее, и ошибка цвета
                _s = np.asarray(pg).astype(int); _q = clut[idx].astype(int); _o = _s[..., 3] == 255
                print('QSTAT %s%d op=%d leak=%d err=%.2f' % (gname, pi, int(_o.sum()), int((_q[..., 3][_o] < 255).sum()), float(np.abs(_q[..., :3][_o] - _s[..., :3][_o]).mean()) if _o.any() else 0.0))
            # store only the rows in use (+8 for bilinear neighbours); TSIZE still says pow2 h
            used = int(np.where(idx.max(1) > 0)[0].max()) + 1
            # №948б (Роб 25.10, повтор №809: «в меню будки у картинки сгоревшей машины снизу битые пиксели»): хранились строки по последнему непрозрачному индексу (+8), а у последней
            # картинки страницы низ холста прозрачный (у каркаса husk 18 пустых строк из 116) — хвост картинки глубже хранимых строк: GE читал за концом данных (мусор снизу).
            # Теперь страница хранится до низа самой нижней картинки (+1 строка билинейного соседа). UI_ROWS948_OLD=1 — как было
            if os.environ.get('UI_ROWS948_OLD') != '1': used = max([used] + [y_ + h_ + 1 for (pi_, x_, y_, w_, h_) in wh.values() if pi_ == pi])
            rows = min(h, (used + 8 + 7) // 8 * 8)
            tex.append(dict(psm=PSM_T8, w=w, h=h, rows=rows, pixels=swizzle(idx[:rows].reshape(-1), w, rows),
                            clut=clut.tobytes(), nclut=256, name='%s%d' % (gname, pi)))
            pal = clut.astype(np.uint8)
            preview.append(Image.fromarray(pal[idx], 'RGBA'))
        for k, (pi, x, y, w, h) in wh.items():
            where[k] = (base + pi, x, y, w, h)
    # quantised bitmaps back (for the verification render)
    qbitmaps = {}
    for k, (ti, x, y, w, h) in where.items():
        qbitmaps[k] = preview[ti].crop((x, y, x + w, y + h))
    if os.environ.get('UI_ROWS_STATS'):   # №948б (диагностика): страницы CLUT8 — хранимые строки и нижний край картинок, чей низ глубже хранимых строк
        for _i, _t in enumerate(tex):
            if _t.get('rows') is not None: print('PAGE %d %s w=%d h=%d rows=%d' % (_i, _t.get('name'), _t['w'], _t['h'], _t['rows']))
        for k, (ti, x, y, w, h) in where.items():
            if os.environ['UI_ROWS_STATS'] in str(k) and y + h > tex[ti]['rows']: print('SHORT %s page=%d bottom=%d rows=%d' % (k, ti, y + h, tex[ti]['rows']))
    if os.environ.get('UI_DUMP_Q'):   # №948б (диагностика, пакет не меняет): исходные и квантованные кадры картинок по ключу-подстроке в папку (замер краёв)
        _d = Path(os.environ['UI_DUMP_Q']); _d.mkdir(parents=True, exist_ok=True)
        for k, q in qbitmaps.items():
            if os.environ.get('UI_DUMP_KEY', 'pcar2d') in k:
                _n = ''.join(ch if (ch.isalnum() or ch in '_.-') else '_' for ch in str(k)); assets.bitmaps[k].convert('RGBA').save(_d / (_n + '.orig.png')); q.convert('RGBA').save(_d / (_n + '.quant.png'))
    # ---- font page (T4, alpha ramp)
    faces = []
    for dl in screens.values():
        for t in dl:
            if t['type'] == T_TEXT and t['font'] not in faces:
                faces.append(t['font'])
    # грань только для заголовков (title: «PAUSE MENU», крупно) — в атлас лишь её буквы (+ '?' для запасного знака):
    # полная крупная грань удвоила бы страницу шрифтов (+64 КБ в RAM и VRAM HUD)
    subset = {}
    for dl in screens.values():
        for t in dl:
            if t['type'] == T_TEXT:
                cur = subset.get(t['font'], set())
                subset[t['font']] = None if (cur is None or not (t.get('title') or t.get('subset'))) else \
                    cur | {ord(c) for sv in t['strings'] if sv for c in sv} | {ord('?'), ord(' ')} | \
                    {ord(c) for c in t.get('subset_chars', '')}   # правка 312: строки, которые ставит код (ui_typo.py)
    # №926 (Роб 24.10: «в Scrapbook слева внизу LEVEL и L/R, но там стоит знак (вопросительный?)»): подпись уровня альбома ставит код (viewer/main_save.h scrapRun,
    # «LEVEL N» и «L / R» гранью uiFontFind("boulder_16") основного пакета ui.shr — там «boulder_16*2», пунктов меню); её атлас — только буквы строк пакета и «?»:
    # цифр и «/» не было, вместо них запасной знак. Цифры, «/» и буквы L E V R (после №925 у пунктов паузы другая грань, и V из «LEVEL PROGRESS» в этой выпала) добавляются в атлас граней boulder_16 (15 ячеек). SCRAP926_OLD=1 — как было
    if os.environ.get('SCRAP926_OLD') != '1':
        for face in list(subset):
            if face.startswith('boulder_16') and subset[face] is not None:
                subset[face] = subset[face] | {ord(c) for c in 'LEVR0123456789/'}
    # правка 163: грань с трекингом ('boulder_16@1+2') — те же буквы, что у грани без '+': одна ячейка атласа на обе (akeys)
    akeys = []
    for face in faces:
        if fonts.get(face).get('atlas', face) not in akeys:
            akeys.append(fonts.get(face).get('atlas', face))
    aidx = {face: akeys.index(fonts.get(face).get('atlas', face)) for face in faces}
    asub = {}
    for face in faces:
        cur = asub.get(aidx[face], set())
        asub[aidx[face]] = None if (cur is None or subset.get(face) is None) else cur | subset[face]
    fsizes = {}
    for ai, akey in enumerate(akeys):
        f = fonts.get(akey)
        for code, g in f['glyphs'].items():
            if g['bm'] is not None and code < 256 and (asub.get(ai) is None or code in asub[ai]):
                fsizes[(ai, code)] = (g['bm'].shape[1], g['bm'].shape[0])
    fpos, left = shelf_pack(fsizes, 512, 512, pad=1)
    assert not left, 'font page overflow'
    fw = pow2(max(x + fsizes[k][0] for k, (x, y) in fpos.items()) + 1, 32)
    fh = pow2(max(y + fsizes[k][1] for k, (x, y) in fpos.items()) + 1, 8)
    fatlas = np.zeros((fh, fw), np.uint8)
    for (ai, code), (x, y) in fpos.items():
        bm = fonts.get(akeys[ai])['glyphs'][code]['bm']
        fatlas[y:y + bm.shape[0], x:x + bm.shape[1]] = np.round(bm * 15).astype(np.uint8)
    t4 = (fatlas[:, 0::2] | (fatlas[:, 1::2] << 4)).astype(np.uint8)
    fclut = np.array([[255, 255, 255, i * 17] for i in range(16)], np.uint8)
    font_tex = len(tex)
    tex.append(dict(psm=PSM_T4, w=fw, h=fh, rows=fh, pixels=swizzle(t4.reshape(-1), fw // 2, fh), clut=fclut.tobytes(),
                    nclut=16, name='fonts'))
    # ---- radar map page (T4)
    radar = sp.get('radar', True)
    if radar:
        rmap = bake_radar_map()
        mrgba = radar_map_rgba(rmap)
        mpal = kmeans_palette(np.asarray(mrgba).reshape(-1, 4), 15)
        midx = map_to_palette(mrgba, mpal)
        mclut = np.concatenate([np.zeros((1, 4), np.uint8), mpal], 0)[:16]
        mt4 = (midx[:, 0::2] | (midx[:, 1::2] << 4)).astype(np.uint8)
        map_tex = len(tex)
        tex.append(dict(psm=PSM_T4, w=MAP_TEX, h=MAP_TEX, rows=MAP_TEX, pixels=swizzle(mt4.reshape(-1), MAP_TEX // 2, MAP_TEX),
                        clut=mclut.tobytes(), nclut=16, name='radarmap'))
    else:                                      # экраны без радара (загрузка): карты нет, ui.c её не рисует
        rmap = dict(x0=0.0, z0=0.0, mpp=0.0); map_tex = -1
    # ---- tables
    strings, str_index = [], {}

    def add_str(sv):
        b = (sv or '').encode('latin-1', errors='replace')
        if b not in str_index:
            str_index[b] = len(strings); strings.append(b)
        return str_index[b]
    slots, pts, items_bin, screen_recs = [], [], [], []
    sfonts, sfont_index = [], {}
    icons_bin = [None] * len(ICONS)

    def add_slot(key, x, y):
        ti, u, v, w, h = where[key]
        slots.append(struct.pack('<7Hh', ti, u, v, w, h, x & 0xffff, y & 0xffff, 0))
        return len(slots) - 1
    layout_json = {}
    for si, sc in enumerate(scr_list):
        first = len(items_bin)
        nmenu = 0
        layout_json[sc] = []
        for t in screens[sc]:
            ty = t['type']
            flags = 0
            x, y, w, h = [int(v) for v in t.get('dst', [0, 0, 0, 0])]
            col, col2 = abgr(t.get('color', [255, 255, 255, 255])), 0
            sdx = sdy = 0
            font = menu = 0
            sfirst = scount = 0
            px = py = ang = 0.0
            if ty == T_RECT and t.get('colors'):
                flags |= 2; col, col2 = abgr(t['colors'][0]), abgr(t['colors'][2])
            if ty == T_POLY:
                sfirst = len(pts)
                for (qx, qy), qc in zip(t['pts'], t['colors']):
                    pts.append(struct.pack('<ffI', qx, qy, abgr(qc)))
                scount = len(t['pts'])
            if ty in (T_IMG, T_BAR, T_SECTOR):
                sfirst = len(slots)
                for k in t['bitmaps']:
                    add_slot(k, x, y)
                scount = len(t['bitmaps'])
                if ty == T_SECTOR:
                    px, py = t['pivot']; ang = t['angle']; flags |= 16
            if ty == T_SLOT:
                sfirst = len(slots)
                for k, d in t['slots']:
                    add_slot(k, d[0], d[1])
                scount = len(t['slots'])
                if t.get('extframe'):          # рамка значка задания извне (viewer/ui.c drawObjIcon): px, py, ang = w, clip = h
                    px, py, ang = float(t['extframe'][0]), float(t['extframe'][1]), float(t['extframe'][2])
            if ty == T_TEXT:
                font = faces.index(t['font'])
                sfirst = len(strings)
                idxs = [add_str(sv) for sv in t['strings']]
                # strings of one item must be consecutive: re-add if dedupe broke the run
                if idxs != list(range(idxs[0], idxs[0] + len(idxs))) if idxs else False:
                    sfirst = len(strings)
                    for sv in t['strings']:
                        strings.append((sv or '').encode('latin-1', errors='replace'))
                else:
                    sfirst = idxs[0] if idxs else 0
                scount = len(t['strings'])
                if t.get('sel'):
                    sfirst += t['sel']; scount -= t['sel']
                if t.get('wrap'):
                    flags |= 1
                if t.get('sh'):
                    flags |= 4; sdx, sdy, sc_ = t['sh']; col2 = abgr(sc_)
                if t['menu'] >= 0:
                    flags |= 8; menu = t['menu']; nmenu = max(nmenu, menu + 1)
            if t.get('selonly'):
                flags |= 32; menu = t['menu']
            if t.get('fuse'):                  # фитиль (load_export.py): сдвиг вправо на PROGRESS * ang пикселей PSP
                flags |= 64; ang = float(t['fuse'])
            if t.get('rate'):                  # кадр / строка по часам экрана загрузки: px = кадров в секунду
                flags |= 128; px = float(t['rate'])
            if ty == T_IMG and t.get('tag') == 'ACTION' and t.get('bitmaps'):
                # №387: runtime держит поле от видимого △, прозрачный холст не считается
                box_ = qbitmaps[t['bitmaps'][0]].getchannel('A').point(lambda a: 255 if a >= 32 else 0).getbbox()
                t = dict(t, typo_res=(box_[1] + 1) if box_ else 0)
            if t.get('slide'):                 # меню (menu_export.py): бегунок ползунка строки slide-1, ang = дорожка, px PSP
                ang = float(t['track'])
            res0 = 0
            if ty == T_NUM and t.get('prefix'):
                res0 = add_slot(t['prefix'], 0, 0) + 1
            if ty == T_NUM:
                key = tuple(sorted((ch, g[0], g[1], g[2], g[3], g[4]) for ch, g in t['glyphs'].items()))
                if key not in sfont_index:
                    tab = bytearray(12 * 256)
                    for ch, g in t['glyphs'].items():
                        c = ord(ch)
                        if c > 255:
                            continue
                        if g[0]:
                            ti, u, v, gw, gh = where[g[0]]
                            struct.pack_into('<HHHBBbBH', tab, 12 * c, ti, u, v, gw, gh, g[3], g[4], 1)
                        else:
                            struct.pack_into('<HHHBBbBH', tab, 12 * c, 0xffff, 0, 0, 0, 0, 0, g[4], 1)
                    sfont_index[key] = len(sfonts); sfonts.append(bytes(tab))
                font = sfont_index[key]
                sfirst = add_str(t.get('text', ''))
                scount = 1
                if t.get('texts'):             # подсказки обучения: строки TUTORIAL_NNN подряд (viewer/ui.c uiDrawTutorial)
                    sfirst = len(strings)
                    for sv in t['texts']: strings.append((sv or '').encode('latin-1', errors='replace'))
                    scount = len(t['texts'])
            if t.get('home'):                  # №623: px — видимый правый край значка дома (px PSP); блок места и круга стоит правее него
                px = float(t['home_right'])
            if ty == T_RADARMAP:
                px, py = t['centre']; ang = t['radius']
            if ty == T_RADARICONS:
                px, py = t['centre']; ang = t['radius']
                for gi, ic in enumerate(t['icons']):
                    if ic:
                        ti, u, v, iw, ih = where[ic['bitmap']]
                        icons_bin[gi] = struct.pack('<HHHBBII', ti, u, v, iw, ih, abgr(ic['color']), 0)
            clipr = float(t.get('clip', 0.0)) if ty in (T_RADARMAP, T_RADARICONS) else 0.0
            if t.get('spin') is not None:      # правка 336: слой лучей экрана загрузки крутится (load_export.py, viewer/ui.c drawSpin):
                px, py = (float(v) for v in t['pivot']); ang = float(t['spin'])   # px, py — центр, px PSP; ang — °/с против часовой; res[0] бит 5
            if ty == T_SLOT and t.get('extframe'):
                clipr = float(t['extframe'][3])
            rec = struct.pack('<8B4h2I4B2H3fIf3I', ty, si, TAGS.index(t['tag']), FIELDS.index(t['field']),
                              FMT[t.get('fmt', 'none')], HALIGN.get(t.get('halign', 'left'), 0),
                              VALIGN.get(t.get('valign', 'top'), 0), flags, x, y, w, h, col, col2,
                              sdx & 0xff, sdy & 0xff, font, menu, sfirst, scount, px, py, ang, res0, clipr, (1 if t.get('ext') else 0) | (2 if t.get('title') else 0) | (4 if t.get('cone') else 0) | (8 if t.get('pix8') else 0) | (16 if t.get('nopix8') else 0) | (32 if t.get('spin') is not None else 0) | (64 if t.get('armL') else 0) | (128 if t.get('armR') else 0) | (256 if t.get('back') else 0) | (512 if t.get('variant') == 'avatar' else 1024 if t.get('variant') == 'plain' else 0) | (2048 if t.get('home') else 0), t.get('slide', 0) | t.get('typo_res1', 0), t.get('typo_res', 0))
            assert len(rec) == 64, len(rec)
            items_bin.append(rec)
            layout_json[sc].append(dict(name=t['name'], page=t['page'], type=ty, tag=t['tag'], field=t['field'],
                                        anchor=t.get('anchor'), dst=[x, y, w, h]))
        tags = 0
        for tg in sp.get('tags', DEFAULT_TAGS)[sc]:
            tags |= 1 << TAGS.index(tg)
        screen_recs.append(struct.pack('<4I', first, len(items_bin) - first, nmenu, tags))
    for i in range(len(icons_bin)):
        if icons_bin[i] is None:
            icons_bin[i] = struct.pack('<HHHBBII', 0xffff, 0, 0, 0, 0, 0, 0)
    # fonts
    font_bin = []
    for fi, face in enumerate(faces):
        f = fonts.get(face)
        hdr = struct.pack('<4H2I', font_tex, f['lh'], int(round(f['base'])), f.get('gh', 0) if f.get('gh', f['lh']) != f['lh'] else 0,
                          add_str(face), __import__('pixel_fonts').kern_res(face))   # res: парный кернинг (pixel_fonts.KERN, №552); pad = высота ячейки буквы, когда строка другой высоты (грань '^N', правка 312), иначе 0
        tab = bytearray(8 * 256)
        for code, g in f['glyphs'].items():
            if code > 255 or (subset.get(face) is not None and code not in subset[face]):
                continue                                   # вне набора заголовка: флаг 0 -> ui.c рисует '?'
            if g['bm'] is not None:
                x, y = fpos[(aidx[face], code)]
                struct.pack_into('<HHBbBB', tab, 8 * code, x, y, g['w'], g['left'], int(g['adv']), 1)
            else:
                struct.pack_into('<HHBbBB', tab, 8 * code, 0, 0, 0, 0, int(g['adv']), 2)
        font_bin.append(hdr + bytes(tab))
    # ---- write
    blob = Blob()
    blob.b += b'\0' * 128
    tex_recs = []
    for t in tex:
        po = blob.add(t['pixels'])
        co = blob.add(t['clut'])
        bufw = t['w']
        tex_recs.append(struct.pack('<4H6I', t['psm'], 1, t['w'], t['h'], po, co, t['nclut'], bufw, t['rows'], 0))
    off_tex = blob.add(b''.join(tex_recs), 16)
    off_item = blob.add(b''.join(items_bin), 16)
    off_screen = blob.add(b''.join(screen_recs), 16)
    off_font = blob.add(b''.join(font_bin), 16)
    str_offs = []
    sdata = bytearray()
    for sv in strings:
        str_offs.append(len(sdata)); sdata += sv + b'\0'
    off_str = blob.align(16)
    table_len = 4 * len(strings)
    blob.b += struct.pack('<%dI' % len(strings), *[off_str + table_len + o for o in str_offs]) + sdata
    off_sfont = blob.add(b''.join(sfonts), 16)
    off_icon = blob.add(b''.join(icons_bin), 16)
    off_slot = blob.add(b''.join(slots), 16)
    off_pts = blob.add(b''.join(pts), 16)
    blob.align(64)
    hdr = struct.pack('<4s19Ii3f', PAK_MAGIC, 1, len(tex), off_tex, len(items_bin), off_item, len(screen_recs), off_screen,
                      len(font_bin), off_font, len(strings), off_str, len(sfonts), off_sfont, len(icons_bin), off_icon,
                      len(slots), off_slot, len(pts), off_pts, map_tex,
                      rmap['x0'], rmap['z0'], rmap['mpp'])
    hdr += struct.pack('<I', len(blob.b))
    hdr += b'\0' * (128 - len(hdr))
    blob.b[0:128] = hdr
    out = Path(out) if out else ROOT / 'build' / PAK_NAME
    out.write_bytes(bytes(blob.b))
    import pixel_fonts; pixel_fonts.sidecar(out, faces)   # какие шрифты Lauke вошли в пакет (указание автора, tools/final-build.sh)
    lay = 'layout_planted.json' if TUT_PLANT else sp.get('layout', 'layout.json' if LEVEL == 1 else 'layout_l%d.json' % LEVEL)
    (OUTDIR / lay).write_text(json.dumps(dict(
        rules=dict(scale=S, safe=[SAFE_X, SAFE_Y], font_min_px=FONT_MIN_PX, level=LEVEL,
                   radar_map=dict(x0=rmap['x0'], z0=rmap['z0'], mpp=rmap['mpp'], tex=MAP_TEX)),
        textures=[dict(name=t['name'], psm=t['psm'], w=t['w'], h=t['h'], rows=t['rows'],
                       bytes=len(t['pixels']) + len(t['clut'])) for t in tex],
        screens=layout_json), indent=1))
    if spec is not None:       # load_export.py: свои проверочные картинки (страницы после квантования и атлас шрифтов)
        return dict(screens=screens, qbitmaps=qbitmaps, fonts=fonts, size=len(blob.b), tex=tex, preview=preview)
    # previews of what the PSP gets (quantised pages)
    for i, pv in enumerate(preview):
        pv.save(SCRATCH / ('page_%s.png' % tex[i]['name']))
    Image.fromarray(np.stack([np.full_like(fatlas, 255)] * 3 + [fatlas * 17], -1), 'RGBA').save(SCRATCH / 'page_fonts.png')
    Image.fromarray(np.concatenate([mclut[:, :3][midx], mclut[:, 3:][midx]], -1), 'RGBA').save(SCRATCH / 'page_radarmap.png')
    total = len(blob.b)
    print('pack -> %s  %d bytes (%.1f KB); textures: %s' % (out, total, total / 1024,
          ', '.join('%s %dx%d(%d rows) %s' % (t['name'], t['w'], t['h'], t['rows'], 'T8' if t['psm'] == PSM_T8 else 'T4')
                    for t in tex)))
    print('items %d, strings %d, slots %d, fonts %d, sprite-fonts %d' % (len(items_bin), len(strings), len(slots),
                                                                        len(font_bin), len(sfonts)))
    # verification render from the QUANTISED pages (what the GE will sample)
    rmap['rgba'] = Image.fromarray(np.concatenate([mclut[:, :3][midx], mclut[:, 3:][midx]], -1), 'RGBA')
    for sc in SCREENS:
        st = dict(DEMO_STATE)
        if sc in ('pause', 'pausemsn'):
            st['show'] = {'COINS', 'OBJECTIVE'}
        sel = 0 if sc in ('pause', 'pausemsn') else (5 if sc == 'main' else -1)
        img = render(screens[sc], DST_W, DST_H, st, qbitmaps, backdrop(DST_W, DST_H, sc), rmap, sel)
        img.save(SCRATCH / ('pack_%s.png' % sc))
    for tag in ('LOADING', 'PRESS_START'):   # правка 52: карточка миссии в двух состояниях (загрузка / ✕ Continue)
        st = dict(DEMO_STATE, show={'ALWAYS', tag})
        render(screens['missionload'], DST_W, DST_H, st, qbitmaps, backdrop(DST_W, DST_H, 'missionload'), rmap, -1).save(
            SCRATCH / ('pack_missionload_%s.png' % tag.lower()))


if __name__ == '__main__':
    argv, out = sys.argv[1:], None
    if '--out' in argv:                     # ship.sh: `ui_export.py --out {out}` = pack into that file
        i = argv.index('--out'); out = argv[i + 1]; del argv[i:i + 2]
    sys.argv[1:] = argv
    cmd = sys.argv[1] if len(sys.argv) > 1 else ('pack' if out else 'all')
    if cmd == 'dump':
        prj = project(sys.argv[2])
        for pg in sys.argv[3:]:
            print('=== page', pg)
            for path, e in iter_leaves(prj.pages[pg]):
                print('%-45s %-6s %-22s (%4d,%4d %4dx%-4d) j=%s/%s' % (
                    '/'.join(path), e['kind'], e['name'][:22], e.get('x', 0), e.get('y', 0), e.get('w', 0),
                    e.get('h', 0), e.get('jx', ''), e.get('jy', '')))
    if cmd in ('catalog', 'all'):
        catalog()
    if cmd in ('mock', 'all'):
        mock()
    if cmd in ('pack', 'all'):
        pack(out)
    if cmd == 'legibility':                 # гейт читаемости текстов миссий (tests/missions/legibility.sh)
        sys.exit(legibility('tiny' if '--plant-tiny' in sys.argv else ('huge' if '--plant-huge' in sys.argv else '')))
