#!/usr/bin/env python3
"""Родные экраны мини-игры (Super Sprint, правка Роба 420) с диска -> пакет bonus/mini.shr того же формата UIP1, что ui.shr
(рисует viewer/ui.c uiScr*; открыт только на время бонус-игры, viewer/bonus_play.h).

Данные: art/frontend/scrooby/minigame.p3d — экраны MiniMenu.scr (MiniMenu.pag, Back2.pag, Accept2.pag), MiniSummary.scr
(BigBoard.pag, MiniSummary.pag, Continue.pag), MiniPause.scr (XSmallBoard.pag, PauseFgd.pag, Pause.pag, MiniPause.pag,
Accept.pag); картинки и строки — оттуда же (значки трасс track1…track7, лица персонажей, значки 43 машин, рейтинг s0…s9,
лица итогов l/b/h/m/a_happy/mad/normal/sad, корона kingicon). Раскладка 16:9 — правила tools/ui_export.py (R1–R7: один масштаб
272/480, якоря по третям, ничего не растягивается, фон на всю ширину).

Экраны пакета (номера — viewer/bonus_play.h BUI_*):
  0 mmchar   MiniMenu.pag: фон + Foreground (рост-доска CharactersBgd, лица Character0..4, строка машины игрока P1
             Vehicle0_*: стрелки, значок машины, рейтинг); значок «P1» под выбранным лицом — 5 копий Player0 (метки BADGE_TAGS)
  1 mmtrack  MiniMenu.pag Overlay: затемнение, SELECT TRACK AND NUMBER OF LAPS, □ Change Track Direction, значки трасс
             Track0..6; подсветка TrackBgd и курсор TrackCursor (x N кругов со стрелками, направление trackcw/ccw) — 7 копий
             на местах значков (метки CUR_TAGS)
  2 mmhints  Back2.pag + Accept2.pag (△ Back, ✕ Accept)
  3 msbase   MiniSummary.scr: BigBoard + MiniSummary (заголовки столбцов, полосы строк, Race Series Standings) + Continue
  4..7 msrow0..3  строка k таблицы итогов: лицо Face k, P-метка, место, Wins, Points, Total Time, Best Lap; короны лучшего
             общего времени и лучшего круга — копии в каждой строке (метки KING_TOTAL / KING_LAP), корона места — у 1st
  8 minipause MiniPause.scr: доска XSmallBoard, PAUSE MENU, CONTINUE / QUIT GAME, ✕ Accept — раскладкой паузы порта (pause_layout)

Поля (значения ставит движок, uiScrSetRow): mmchar ROW0..4 — кадр лица k (0 обычное, 1 выбранное _s), ROW5 — машина (кадр
значка, порядок Vehicle0_Value), ROW6 — рейтинг s0..s9, ROW7 — кадр стрелок машины (0 яркие, 1 серые larrowg/rarrowg);
mmtrack ROW9 — число кругов (строки «x 0»…«x 9»), ROW10 — направление (0 trackcw, 1 trackccw); msrow ROW0 — персонаж лица
(кадры l, b, h, m, a), ROW1 — P1…P4, ROW2..5 — тексты Wins / Points / Total Time / Best Lap из кода (uiScrRowText).

ПРАВИЛА ПОРТА (данных нет; вопросы оригиналу — knowledge/questions.tsv):
  - у PSP один игрок: строки машин P2..P4, «PRESS START» вступления, таймер и «Waiting for other players…» не выводятся
  - картинка в рамке другой пропорции — своего размера по центру рамки (не вписывается с растяжкой, R1)
  - копии значка P1 и курсора трассы стоят на местах лиц / значков из данных (сдвиг = разница мест в данных)
  - столбцы итогов (Wins, Points, Total Time, Best Lap и числа) — по центру своего столбца данных (узкая грань PSP шире столбца),
    заголовки «Total Time» и «Best Lap» — в две строки по ширине столбца
  - шрифты: строки boulder_16 данных — грань диска 1:1 (boulder_16@1, как пункты меню порта, правка 136), Tt2001m__14 —
    узкая грань подписей порта HINT_FACE (swz721n_10@1, правка 233): дробный масштаб 0,57 даёт мыло (R7)
  - таблица итогов, доска и рост-доска выбора — единые композиции: один якорь на группу (MiniSummary/BigBoard — по центру,
    рост-доска с лицами — слева, машины — справа, по третям данных), «□ Change Track Direction» — к левому краю
    python tools/bonus/screens_export.py            # build/bonus/mini.shr + макеты build/bonus/screens/*.png
    python tools/bonus/screens_export.py --out X    # пакет в X (tools/ship_rules.txt)
    python tools/bonus/screens_export.py --check    # сверка ресурсов пакета с диском (гейт tests/bonus/screens.sh)
Читает только диск (hit-and-run-web/source/game) и hit-and-run-web/tools (MIT) через tools/ui_export.py.
"""
import json
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
os.environ.setdefault('UI_SCRATCH', str(ROOT / 'build/bonus/ui_scratch'))   # служебные картинки ui_export — в build
import ui_export as U  # noqa: E402

MG = 'minigame.p3d'
SCREENS = ['mmchar', 'mmtrack', 'mmhints', 'msbase', 'msrow0', 'msrow1', 'msrow2', 'msrow3', 'minipause']
PAGES = {
    'mmchar': [(MG, 'MiniMenu.pag')],
    'mmtrack': [(MG, 'MiniMenu.pag')],
    'mmhints': [(MG, 'Back2.pag'), (MG, 'Accept2.pag')],
    'msbase': [(MG, 'BigBoard.pag'), (MG, 'MiniSummary.pag'), (MG, 'Continue.pag')],
    'minipause': [(MG, 'PauseBgd.pag'), (MG, 'XSmallBoard.pag'), (MG, 'PauseFgd.pag'), (MG, 'Pause.pag'), (MG, 'MiniPause.pag'),
                  (MG, 'Accept.pag')],
}
for _k in range(4):
    PAGES['msrow%d' % _k] = [(MG, 'MiniSummary.pag')]
# метки видимости (одна на копию; любые свободные теги UIP1 — у второго пакета они только биты, viewer/bonus_play.h BUI_TAG*)
LOCK_TAGS = ['RADAR', 'SPEED', 'HNR_METER', 'OBJECTIVE', 'HNR_TICKET']
VEH_LOCK_TAG = 'ACTION'
WAIT_TAG = 'COUNTDOWN'
CUR_TAGS = ['TIMER', 'PARTIME', 'LAP', 'POSITION', 'DAMAGE', 'PROXIMITY', 'CARDS']   # курсор трассы k / значок P1 у лица k (k < 5)
TRACK_TAGS = ['COINS', 'WASPS', 'COLLECTIBLES', 'ITEMS_COMPLETE', 'STAGE_COMPLETE', 'MISSION_COMPLETE', 'MISSION_FAILED']
KING_TOTAL, KING_LAP = 'SKIP', 'STREETRACE'
FACE_B16 = os.environ.get('BONUS_UI_B16', 'boulder_16@1')
FACE_TT = os.environ.get('BONUS_UI_TT', U.HINT_FACE or 'Tt2001m__14@1')
OUT = ROOT / 'build/bonus/mini.shr'
MOCKS = ROOT / 'build/bonus/screens'
RANK_COLORS = ((255, 214, 0, 255), (232, 96, 40, 255), (88, 176, 48, 255), (255, 214, 0, 255))   # №1183: оригинал PS2
PLAYER_TAG = 'RADAR'   # метка видимости «P1» строки итогов (viewer/bonus_play.h BUI_PLAYER)
FACE_PX = (70, 55)   # №1183: лицо в итогах, единицы страницы 640x480: первое место, остальные (оригинал PS2: 0,146 и 0,115 высоты экрана)
ROW_RE_NAMES = ('Face', 'Player', 'Rank', 'NumWins', 'NumPoints', 'TotalTime', 'BestLap')


def _shift(it, dx=0.0, dy=0.0):
    x, y, w, h = it['rect']
    out = dict(it, rect=[x + dx, y + dy, w, h])
    if it.get('points'):
        out['points'] = [[px + dx, py + dy] for px, py in it['points']]
    return out


PAUSE_ROWS = int(os.environ.get('BONUS_PAUSE_ROWS', '5'))   # высота доски паузы в строках шага (оригинал: 0,17–0,71 высоты экрана)
MM_LABEL_W = 280   # px PSP
CHARLABEL_TAG = 'COINS'   # метка видимости подписи «Change Character» (экран выбора игрока)
MM_B16_NAMES = ('ChangeCharacter', 'Options', 'WaitingForOtherPlayers')   # №1182: подписи выбора — грань пунктов меню игры, не мелкая Tt2001m


def _faces(its, sc=None):
    for it in its:
        if it['type'] == U.T_TEXT:
            big = sc in ('mmchar', 'mmtrack') and not os.environ.get('BONUS_MMFACE1182_OLD') and it['name'] in MM_B16_NAMES
            if big:   # подпись крупной гранью — рамка шире (до значка кнопок справа), иначе длинный перевод сжимался обратно до мелкой грани
                x0, y0, w0, h0 = it['rect']; it['rect'] = [x0, y0, max(w0, MM_LABEL_W / U.S), h0]
                if it['name'] == 'ChangeCharacter' and sc == 'mmchar':
                    it['tag'] = CHARLABEL_TAG   # под выбором трассы эта подпись не рисуется (viewer/bonus_play.h)
            it['font'] = FACE_B16 if (it['font'].startswith('boulder') or big) else FACE_TT
            it['shadow'] = None if not it.get('shadow') else it['shadow']


def _crown(it):
    """№1197 + аудит п. 4: размер короны задаётся в рамке источника (картинка масштабируется при раскладке; размер, поставленный потом в dst, на картинку не действует) — 13 x 9 px PSP"""
    if os.environ.get('BONUS_SUMMARY_CROWN1197_OLD'):
        return it
    x0, y0, w0, h0 = it["rect"]; w1 = 13 / U.S; h1 = w1 * h0 / w0   # та же пропорция, что у картинки (no_stretch иначе вернёт её родной размер)
    it['rect'] = [x0 + (w0 - w1) / 2, y0 + (h0 - h1) / 2, w1, h1]
    return it


def _row_of(name):
    for p in ROW_RE_NAMES:
        if name.startswith(p) and name[len(p):].isdigit():
            return int(name[len(p):])
    return None


def build_items(sc):
    U.SCREEN_PAGES[sc] = PAGES[sc]
    its, _ = U.build_items(sc)
    if sc == 'mmchar':
        out = []
        chars = {it['name']: it for it in its if it['path'] == 'Foreground/Characters' and it['name'].startswith('Character')
                 and it['name'][9:].isdigit()}
        c0 = chars['Character0']['rect']
        for it in its:
            p, nm = it['path'], it['name']
            if p.startswith('Overlay'):
                continue
            if p == 'Foreground/Timer':
                it['tag'] = WAIT_TAG
                if nm == 'Timer_0': it.update(field='ROW8', fmt='select')
                out.append(it); continue
            if nm.startswith('PressStart'): continue
            if p.startswith('Foreground/Vehicles'):
                if not (nm.startswith('Vehicle0')):
                    continue
                if nm == 'Vehicle0_Selected':
                    it['tag'] = VEH_LOCK_TAG
                if nm == 'Vehicle0_Value':
                    it['field'] = 'ROW5'
                elif nm == 'Vehicle0_Rating':
                    it['field'] = 'ROW6'
                elif nm in ('Vehicle0_LArrow', 'Vehicle0_RArrow'):
                    it['field'] = 'ROW7'
                out.append(it); continue
            if p.startswith('Foreground/Characters/Player'):
                if not p.endswith('Player0'):
                    continue
                for k in range(5):   # значок P1 под лицом k: сдвиг = разница центров мест лиц в данных
                    r = chars['Character%d' % k]['rect']
                    dx = (r[0] + r[2] / 2) - (c0[0] + c0[2] / 2)
                    out.append(dict(_shift(it, dx), tag=LOCK_TAGS[k] if nm == 'Player0_Selected' else CUR_TAGS[k], name='%s_%d' % (nm, k), unit=chars['Character0']['unit']))
                continue
            if nm in chars:
                it['field'] = 'ROW%d' % int(nm[9:])
            out.append(it)
        its = out
    elif sc == 'mmtrack':
        out = []
        trk = {it['name']: it for it in its if it['path'] == 'Overlay/Tracks' and it['name'].startswith('Track') and it['name'][5:].isdigit()}
        t0 = trk['Track0']['rect']
        dxs = [trk['Track%d' % k]['rect'][0] - t0[0] for k in range(7)]
        unit = trk['Track0']['unit']
        for it in its:
            p, nm = it['path'], it['name']
            if not p.startswith('Overlay'):
                continue
            if nm in trk:
                it['tag'] = TRACK_TAGS[int(nm[5:])]
            if nm == 'TrackBgd' or p == 'Overlay/Tracks/TrackCursor':
                if nm == 'NumLaps':
                    it.update(field='ROW9', fmt='select')
                elif nm == 'Direction':
                    it['field'] = 'ROW10'
                for k in range(7):
                    out.append(dict(_shift(it, dxs[k]), tag=CUR_TAGS[k], name='%s_%d' % (nm, k), unit=unit))
                continue
            if p == 'Overlay/Options' and nm == 'Options':   # □ Change Track Direction — подсказка у левого края
                it.update(unit=(it['page'], 'Overlay/OptionsHint'), forced='L')
            out.append(it)
        its = out
    elif sc == 'msbase':
        its = [it for it in its if it['page'] != 'MiniSummary.pag' or (_row_of(it['name']) is None and 'KingIcon' not in it['name'])]
        for it in its:
            if it['page'] in ('BigBoard.pag', 'MiniSummary.pag'):
                it.update(unit=('MiniSummary', 'board'), forced='C')
    elif sc.startswith('msrow'):
        k = int(sc[5:])
        rows = {it['name']: it for it in its if it['name'].startswith('Row') and it['name'][3:].isdigit()}
        dy = rows['Row%d' % k]['rect'][1] - rows['Row0']['rect'][1]   # корона итога в данных — у строки 0
        out = []
        for it in its:
            nm = it['name']; r = _row_of(nm)
            if r == k:
                if it['type'] == U.T_TEXT and not os.environ.get('BONUS_SUMMARY_ROWFIX_OLD'):
                    # Правка Роба 507 (10.10): «1st», «P1» и числа стояли на разной высоте (рамки текста в данных смещены: числа центрированы на 6 единиц выше полосы
                    # строки, место и P1 — прижаты к верху своих рамок разной высоты). Теперь все тексты строки — одна рамка по полосе строки Row<k>, по центру
                    # (центр строки не уходит при смене размера грани, правило ui_typo). BONUS_SUMMARY_ROWFIX_OLD=1 — прежнее
                    band = rows['Row%d' % k]['rect']
                    it['rect'] = [it['rect'][0], band[1], it['rect'][2], band[3]]; it['valign'] = 'centre'
                if it['type'] == U.T_IMG and nm.startswith('Face') and not os.environ.get('BONUS_SUMMARY_FACE1183_OLD'):
                    # №1183 (Роб 04.11: «аватарки в итоговой сводке очень большие»): размер как у оригинала на PS2 (кадры emu-ps2: у первого места 70 px из 480 высоты, у остальных 55 px;
                    # центр лица прежний). В данных страницы лицо 92 px — вдвое выше строки, лица налезали на соседние строки
                    x0, y0, w0, h0 = it['rect']; w1 = FACE_PX[0] if k == 0 else FACE_PX[1]; h1 = w1 * h0 / w0
                    it['rect'] = [x0 + (w0 - w1) / 2, y0 + (h0 - h1) / 2, w1, h1]
                if it['type'] == U.T_TEXT and not os.environ.get('BONUS_SUMMARY_MARKS1183_OLD'):
                    if nm.startswith('Rank') and nm[4:].isdigit():   # цвет метки места как у оригинала PS2: 1st жёлтый, 2nd красно-оранжевый, 3rd зелёный, 4th жёлтый
                        it['color'] = list(RANK_COLORS[k])
                    elif nm.startswith('Player'):                   # метка «P1» красная и только у игрока (у соперников метки нет; движок показывает по тегу PLAYER_TAG)
                        it['strings'] = ['P1']; it['color'] = [230, 40, 40, 255]; it['tag'] = PLAYER_TAG
                fld = {'Face': 'ROW0', 'Player': 'ROW1', 'NumWins': 'ROW2', 'NumPoints': 'ROW3', 'TotalTime': 'ROW4', 'BestLap': 'ROW5'}
                for pfx, f in fld.items():
                    if nm.startswith(pfx):
                        it['field'] = f
                        if it['type'] == U.T_TEXT:
                            it['fmt'] = 'select'
                            if pfx in ('TotalTime', 'BestLap'):
                                it['strings'] = ['0:00.00']   # ширина для макета; текст — из кода
                out.append(it)
            elif nm == 'RankKingIcon' and k == 0:
                out.append(_crown(it))
            elif nm in ('TotalTimeKingIcon', 'BestLapKingIcon'):
                out.append(_crown(dict(_shift(it, 0, dy), tag=KING_TOTAL if nm.startswith('Total') else KING_LAP)))
        for it in out:
            it.update(unit=('MiniSummary', 'board'), forced='C')
        # та же опора якоря, что у msbase: рамка BigBoard (иначе строка без доски сдвинулась бы иначе) — невидимая точка-опора
        its = out
    elif sc == 'minipause':
        menu = sorted((it for it in its if it['page'] == 'MiniPause.pag'), key=lambda t: t['rect'][1])
        for i, it in enumerate(menu):
            it.update(page='PauseMission.pag', menu=i)   # раскладка пунктов паузы порта (pause_layout) — как у паузы игры
        keep = U.PAUSE_MIN_ROWS
        U.PAUSE_MIN_ROWS = 0 if os.environ.get('BONUS_PAUSE1182_OLD') else PAUSE_ROWS   # №1182: доска как у оригинала PS2 (0,17–0,71 высоты, два пункта шагом 0,104); прежде — по двум строкам
        try:
            its = U.pause_layout(its)
        finally:
            U.PAUSE_MIN_ROWS = keep
    if sc != 'minipause':
        _faces(its, sc)
    if sc == 'msbase' or sc.startswith('msrow'):   # правило порта: столбцы итогов — по центру своего столбца (у PS2 грань уже:
        for it in its:                            # «Total Time» узкой гранью PSP шире столбца 34 px и налезал на «Best Lap»)
            nm = it['name'].rstrip('0123456789')
            if it['type'] == U.T_TEXT and nm in ('Wins', 'Points', 'TotalTime', 'BestLap', 'NumWins', 'NumPoints'):
                it['halign'] = 'centre'
                if nm in ('TotalTime', 'BestLap') and it['strings'] and ' ' in it['strings'][0]:
                    it['wrap'] = True   # «Total / Time», «Best / Lap» в две строки по ширине столбца (иначе слипались)
    if sc == 'msbase' and not os.environ.get('BONUS_SUMMARY_ROWFIX_OLD'):
        # Правка 507: заголовки столбцов — общая нижняя линия: однострочные Wins / Points и вторая строка «Total Time» / «Best Lap» садятся на одну линию над
        # столбцами (рамка 90..124 одна на четыре, прижато к низу; в данных Wins/Points центрированы в 108..124, Total/Best — к верху 90)
        for it in its:
            if it['type'] == U.T_TEXT and it['page'] == 'MiniSummary.pag' and it['name'] in ('Wins', 'Points', 'TotalTime', 'BestLap'):
                it['rect'] = [it['rect'][0], 90, it['rect'][2], 34]; it['valign'] = 'bottom'
    U.hint_faces(its, sc) if sc in ('mmhints', 'msbase') else None
    return its


ANCHOR_REF = {}
CONTINUE_MARGIN = 8   # правка 507: отступ «✕ Continue» от внутреннего края правой рамки доски итогов, px PSP


def board_ref(assets):
    """рамка BigBoard msbase в источнике — общая опора якоря строк итогов (одна композиция)"""
    if 'board' not in ANCHOR_REF:
        its = build_items('msbase')
        b = [1e9, 1e9, -1e9, -1e9]
        for it in its:
            if it.get('unit') == ('MiniSummary', 'board'):
                x, y, w, h = U.item_bbox(it, assets.images)
                b = [min(b[0], x), min(b[1], y), max(b[2], x + w), max(b[3], y + h)]
        ANCHOR_REF['board'] = b
    return ANCHOR_REF['board']


def no_stretch(its, assets):
    """R1: картинка в рамке Scrooby другой пропорции — своего размера по выравниванию рамки, не вписана в рамку (ui_export.sprite_rect
    вписывает при расхождении до 6 %: homer.png 95x150 в рамке 100x150 тянулся на 5 % по ширине)"""
    for it in its:
        if it['type'] != U.T_IMG or not it.get('frames') or it['name'].startswith('Frame_'):
            continue
        iw, ih = assets.images[it['frames'][0]].size
        x, y, w, h = it['rect']
        if w > 0 and h > 0 and abs((w / h) / (iw / ih) - 1) > 0.005:
            it['rect'] = U.sprite_rect([x, y, w, h], (iw, ih), it['jx'], it['jy']) if abs((w / h) / (iw / ih) - 1) >= 0.06 else \
                [x if it['jx'] == 'left' else (x + w - iw if it['jx'] == 'right' else x + (w - iw) / 2),
                 y if it['jy'] == 'top' else (y + h - ih if it['jy'] == 'bottom' else y + (h - ih) / 2), iw, ih]


COLS = ('Wins', 'Points', 'TotalTime', 'BestLap')
COL_L, COL_R, COL_GAP = 211, 382, 2.0   # №1182: столбцы итогов — от метки P1 до правой рамки; ширина столбца — самое длинное слово шапки и самое широкое значение на этом языке (гранями после панели Роба)
TITLE_FACE = 'boulder_16@1'   # №1184а: заголовки экранов — толстая грань оригинала; нашими гранями (bart_16bit, msx_thin) — только мелкий системный текст


def col_stem(nm):
    n = nm.rstrip('0123456789')
    for a_, b_ in (('NumWins', 'Wins'), ('NumPoints', 'Points'), ('TotalTimeKingIcon', 'TotalTime'), ('BestLapKingIcon', 'BestLap')):
        if n == a_:
            return b_
    return n if n in COLS else None


def set_face(t, face, fonts):
    t['font'] = face; t['fontinfo'] = fonts.get(face)


def post(screens, fonts=None):
    """после панели типографики Роба: (1) заголовки экранов возвращаются на толстую грань оригинала; (2) подсказка паузы — нашей гранью; (3) столбцы итогов — по ширине слов
    шапки и значений на этом языке (грани уже окончательные), одним расчётом для шапки и всех четырёх строк"""
    fonts = fonts or U.FontCache('psp')
    if not os.environ.get('BONUS_SHADOW1198_OLD'):
        # аудит типографики 05.11 п. 6: одна тень на весь мелкий пиксельный текст — непрозрачный чёрный, 1 px вправо-вниз (у «× 1», P1, шапки итогов тени не было, у подсказок — полупрозрачная)
        for dl_ in screens.values():
            for t in dl_:
                if t.get('type') == U.T_TEXT and str(t.get('font', '')).split('@')[0].split('*')[0] in ('bart_16bit', 'msx_thin', 'msx_bold') and t.get('strings') is not None:
                    t['sh'] = (1, 1, [0, 0, 0, 255])
    if not os.environ.get('BONUS_LAPS1195_OLD'):
        # №1195 (Роб, фото с PSP: «× 1» между голубыми треугольниками, цифра налезает на правую стрелку): без пробела между × и цифрой, группа по центру между стрелками.
        # BONUS_LAPS1195_OLD=1 — «x 1» как в данных
        for t in screens.get('mmtrack', []):
            if t['name'].startswith('NumLaps') and t.get('strings'):
                t['strings'] = [x.replace(' ', '') for x in t['strings']]; t['halign'] = 'centre'
    if not os.environ.get('BONUS_OPTSQ1214_OLD'):
        # №1214 (Роб, фото с PSP 05.11: «кнопка с квадратом выше, чем надпись CHANGE TRACK DIRECTION, сделай ровно»): замер кадра эмулятора — середина □ 221,5,
        # середина заглавных подписи 226 (на 4 px ниже; у ✕ ACCEPT и △ BACK справа 1,5–2). Значок вниз на 4 px: середина 225,5 — вровень с подписью и с ✕ справа.
        # BONUS_OPTSQ1214_OLD=1 — как в данных
        for t in screens.get('mmtrack', []):
            if t['name'] == 'Options' and t.get('type') == U.T_IMG:
                t['dst'] = [t['dst'][0], t['dst'][1] + 4] + list(t['dst'][2:])
    if os.environ.get('BONUS_SUMMARY_COLS1182_OLD'):
        return
    sm = screens.get('msbase', [])
    for t in sm:
        if t['name'] == 'RaceSummary' and t.get('strings'):
            set_face(t, TITLE_FACE, fonts); t['wrap'] = False; t['valign'] = 'centre'; t['halign'] = 'left'
            t['dst'] = [t['dst'][0], 31, U.text_width(fonts.get(TITLE_FACE), t['strings'][0]) + 1, 10]
    if not os.environ.get('BONUS_PAUSEACC1198_OLD'):
        for t in screens.get('minipause', []):
            if t['name'] == 'Accept':
                t['dst'] = [t['dst'][0], t['dst'][1] - 2] + list(t['dst'][2:])   # аудит п. 13: ACCEPT бонусной паузы на 2 px выше (низ строки 259 — как у пауз основной игры). BONUS_PAUSEACC1198_OLD=1 — прежнее
    for t in screens.get('mmtrack', []):
        if t['name'] == 'SelectTrack' and t.get('strings'):
            set_face(t, TITLE_FACE, fonts)
    for t in screens.get('minipause', []):
        if t['name'] == 'PressStartResumePlay':
            set_face(t, 'bart_16bit@1', fonts)   # подсказка слева внизу: нашей гранью, в две строки, как у оригинала («Press START button to / Continue»)
            x0, y0, w0, h0 = t['dst']; t['wrap'] = True; t['valign'] = 'bottom'; t['dst'] = [x0, y0 - 10, 170, h0 + 10]
    # столбцы
    hd = {t['name']: t for t in sm if t['name'] in COLS and t.get('strings')}
    val = {}
    for sc in ('msrow0',):
        for t in screens.get(sc, []):
            st = col_stem(t['name'])
            if st and t.get('type') == U.T_TEXT:
                val[st] = U.text_width(fonts.get(t['font']), '0:00.00' if st in ('TotalTime', 'BestLap') else '00')
    def widths():
        out = []
        for nm in COLS:
            t = hd[nm]; f = fonts.get(t['font'])
            words = [w for sv in t['strings'] if sv for w in sv.replace('\\', ' ').split()]
            out.append(max([val.get(nm, 10)] + [U.text_width(f, w) for w in words]))
        return out
    ws = widths()
    if sum(ws) + 3 * COL_GAP > COL_R - COL_L:   # длинные слова шапки не помещаются гранью Bart (8 px на знак) — пиксельная msx_thin (6 px), та же, что у чисел столбцов
        for nm in COLS:
            set_face(hd[nm], 'msx_thin@1', fonts)
        ws = widths()
    gap = max(COL_GAP, (COL_R - COL_L - sum(ws)) / 3)
    x = COL_L; plan = []
    for w in ws:
        plan.append((x + w / 2, w)); x += w + gap
    if not os.environ.get('BONUS_SUMMARY_COLS1198_OLD'):
        plan[1] = (plan[1][0] + 3, plan[1][1])   # аудит 05.11 п. 16: между WINS и POINTS было 6 px (меньше пробела) — POINTS (шапка и числа) на 3 px правее. BONUS_SUMMARY_COLS1198_OLD=1 — прежнее
    for sc, dl in screens.items():
        if sc != 'msbase' and not sc.startswith('msrow'):
            continue
        for t in dl:
            st = col_stem(t['name'])
            if not st:
                continue
            cx, w = plan[COLS.index(st)]; x0, y0, w0, h0 = t['dst']
            if t['name'].endswith('KingIcon') and st in ('TotalTime', 'BestLap') and not os.environ.get('BONUS_SUMMARY_CROWN1183_OLD'):
                # корона лучшего времени / круга стояла поверх самих цифр (40 x 28) — теперь над ними, вдвое меньше
                w0, h0 = 22, 15; y0 = y0 - 4   # 70..85: над цифрами (глиф цифр с 90), под шапкой (она на 5 px выше, см. ниже)
                if not os.environ.get('BONUS_SUMMARY_CROWN1197_OLD'):
                    w0, h0 = 13, 9; y0 -= 3   # №1197 + аудит п. 4: высота короны 9 px (буквы 7 + 2), над значением в просвете между шапкой и строкой 1 (72..89): 77..86
            if t.get('type') == U.T_TEXT:
                w0 = w + 1
                if t['name'] == st:
                    t['wrap'] = True
            if t['name'] == st and sc == 'msbase' and not os.environ.get('BONUS_SUMMARY_CROWN1183_OLD'):
                y0 -= 5   # шапка столбцов выше на 5 px: между ней и цифрами встаёт корона
            t['dst'] = [cx - w0 / 2, y0, w0, h0]

    if not os.environ.get('BONUS_SUMMARY_CROWN1197_OLD') and not os.environ.get('BONUS_SUMMARY_CROWN1183_OLD'):
        # корона первого места — того же размера, над подписью места («1st»), а не поверх неё (40 x 28 на строке в 14 px)
        for t in screens.get('msrow0', []):
            if t['name'] == 'RankKingIcon':
                r_ = next((q for q in screens['msrow0'] if q['name'] == 'Rank0'), None)
                if r_:
                    t['dst'] = [r_['dst'][0] + (r_['dst'][2] - 13) / 2, r_['dst'][1] - 10, 13, 9]


def build(sc, assets, fonts):
    its = build_items(sc)
    if not os.environ.get('BONUS_UI_STRETCH'):   # красный сверки: как ui_export (вписать с растяжкой до 6 %)
        no_stretch(its, assets)
    if sc.startswith('msrow'):   # невидимая опора: рамка доски (единый сдвиг C у всех частей таблицы) — нулевая прозрачная плашка
        x0, y0, x1, y1 = board_ref(assets)
        its.append(dict(name='AnchorRef', page='MiniSummary.pag', path='', tag='ALWAYS', unit=('MiniSummary', 'board'), forced='C',
                        field='NONE', fmt='none', menu=-1, type=U.T_RECT, rect=[x0, y0, x1 - x0, y1 - y0], color=[0, 0, 0, 0]))
    dl, _ = U.adapt(its, {}, 'psp', assets, fonts)
    if sc == 'msbase' and not os.environ.get('BONUS_SUMMARY_ROWFIX_OLD'):
        # Правка 507: «✕ Continue» (значок и подпись) вылезал за правую рамку доски; теперь группа целиком внутри рамки, правым краем в CONTINUE_MARGIN px от неё
        # и прижата к нижней кромке (по высоте как была: между строками и нижней рамкой). Мера — рамка BigBoard в кадре PSP
        right = [t['dst'][0] for t in dl if t['name'] == 'Frame_Right']
        cont = [t for t in dl if t['page'] == 'Continue.pag']
        if right and cont:
            dx = int(right[0]) - (CONTINUE_MARGIN if os.environ.get('BONUS_SUMMARY_COLS1198_OLD') else 0) - max(t['dst'][0] + t['dst'][2] for t in cont)   # аудит п. 16: правый край CONTINUE на 380, как у таблицы
            for t in cont:
                t['dst'] = [t['dst'][0] + dx] + list(t['dst'][1:])
    return [t for t in dl if t['name'] != 'AnchorRef']


def mg_assets():
    a = U.Assets()
    a.images.update(U.project(MG).images)   # картинки страниц мини-игры — из minigame.p3d (back.png, рамки и стрелки там свои)
    return a


def export(out=None, mocks=True):
    assets = mg_assets()
    for sc in SCREENS:
        U.SCREEN_PAGES[sc] = PAGES[sc]
    spec = dict(screens=SCREENS, build=build, post=post, tags={sc: {'ALWAYS'} for sc in SCREENS}, group=lambda k: 'ui', radar=False,
                groups=('ui',), layout='layout_bonus_mini.json', assets=assets)
    Path(out or OUT).parent.mkdir(parents=True, exist_ok=True)
    res = U.pack(out or OUT, spec)
    print('mini.shr: %d B (%.1f KB), экраны %s; текстуры %s' % (res['size'], res['size'] / 1024, ', '.join(SCREENS),
          ', '.join('%s %dx%d/%d' % (t['name'], t['w'], t['h'], t['rows']) for t in res['tex'])))
    if mocks:
        mock(res)
    return res


DEMO = dict(track=3, laps=3, rev=0, chr=2, veh=27, rating=5)


def mock(res):
    from PIL import Image
    MOCKS.mkdir(parents=True, exist_ok=True)
    scr = res['screens']; bm = res['qbitmaps']

    def draw(img, sc, st, sel=-1):
        return U.render(scr[sc], U.DST_W, U.DST_H, st, bm, img, None, sel)
    bg = Image.new('RGBA', (U.DST_W, U.DST_H), (0, 0, 0, 255))
    st = {'show': {'ALWAYS', CUR_TAGS[DEMO['chr']]}, 'ROW5': DEMO['veh'], 'ROW6': DEMO['rating'], 'ROW7': 0}
    st['ROW%d' % DEMO['chr']] = 1
    a = draw(bg, 'mmchar', st); a = draw(a, 'mmhints', {'show': {'ALWAYS'}}); a.save(MOCKS / 'mmchar.png')
    st2 = {'show': {'ALWAYS', CUR_TAGS[DEMO['track'] - 1], *TRACK_TAGS}, 'ROW9': DEMO['laps'], 'ROW10': DEMO['rev']}
    b = draw(draw(draw(bg, 'mmchar', st), 'mmtrack', st2), 'mmhints', {'show': {'ALWAYS'}}); b.save(MOCKS / 'mmtrack.png')
    c = draw(Image.new('RGBA', (U.DST_W, U.DST_H), (40, 60, 40, 255)), 'msbase', {'show': {'ALWAYS'}})
    for k in range(4):
        c = draw(c, 'msrow%d' % k, {'show': {'ALWAYS'} | ({KING_TOTAL, KING_LAP} if k == 0 else set()), 'ROW0': (k + 2) % 5, 'ROW1': k})
    c.save(MOCKS / 'summary.png')
    draw(Image.new('RGBA', (U.DST_W, U.DST_H), (40, 60, 40, 255)), 'minipause', {'show': {'ALWAYS'}}, 0).save(MOCKS / 'minipause.png')
    print('  макеты', MOCKS.relative_to(ROOT))


# ---------------------------------------------------------------- сверка с диском (гейт tests/bonus/screens.sh)
def check(pack_path):
    """пакет собран из minigame.p3d: экраны, картинки и строки — те, что на диске; ничего не растянуто (каждая картинка — в
    пропорции своей картинки диска, допуск 1 px округления); всё внутри 480x272. -> (ok, отчёт)"""
    import struct
    prj = U.project(MG)
    lay = json.loads((U.OUTDIR / 'layout_bonus_mini.json').read_text())
    errs, rep = [], {}
    raw = Path(pack_path).read_bytes()
    if raw[:4] != U.PAK_MAGIC:
        errs.append('магия пакета %r' % raw[:4])
    nscreen = struct.unpack_from('<I', raw, 4 + 4 * 5)[0]
    rep['screens'] = nscreen
    if nscreen != len(SCREENS):
        errs.append('экранов %d вместо %d' % (nscreen, len(SCREENS)))
    need_pages = {pg for sc in SCREENS for _, pg in PAGES[sc]}
    rep['pages'] = sorted(need_pages)
    for pg in need_pages:
        if pg not in prj.pages:
            errs.append('нет страницы %s на диске' % pg)
    # каждая страница экрана — из списка .scr диска
    scr = prj.screens
    for name, pages in (('MiniMenu.scr', ['MiniMenu.pag', 'Back2.pag', 'Accept2.pag']),
                        ('MiniSummary.scr', ['BigBoard.pag', 'MiniSummary.pag', 'Continue.pag']),
                        ('MiniPause.scr', ['PauseBgd.pag', 'XSmallBoard.pag', 'PauseFgd.pag', 'Pause.pag', 'MiniPause.pag', 'Accept.pag'])):
        if scr.get(name) != pages:
            errs.append('%s на диске: %s' % (name, scr.get(name)))
    names = {sc: [it['name'] for it in lay['screens'][sc]] for sc in SCREENS}
    want = {'mmchar': ['Character%d' % k for k in range(5)] + ['CharactersBgd', 'Vehicle0_Value', 'Vehicle0_Rating', 'ChangeCharacter'],
            'mmtrack': ['Track%d' % k for k in range(7)] + ['SelectTrack', 'Options'] + ['NumLaps_%d' % k for k in range(7)] +
                       ['Direction_%d' % k for k in range(7)],
            'msbase': ['RaceSummary', 'Wins', 'Points', 'TotalTime', 'BestLap', 'Frame_Top'],
            'msrow0': ['Face0', 'Rank0', 'RankKingIcon'], 'minipause': ['Continue', 'Quit', 'Paused']}
    for sc, ws in want.items():
        miss = [w for w in ws if w not in names[sc]]
        if miss:
            errs.append('%s: нет %s' % (sc, miss))
    # картинки: имя = картинка диска, пропорции сохранены
    U.SCREEN_PAGES.update(PAGES)
    assets = mg_assets()
    fonts = U.FontCache('psp')
    imgs = 0; worst = 0.0
    for sc in SCREENS:
        dl = build(sc, assets, fonts)
        for t in dl:
            x, y, w, h = [int(v) for v in t.get('dst', [0, 0, 0, 0])]
            if t['type'] in (U.T_IMG,) and t.get('bitmaps') and not t['name'].startswith('Frame_'):
                for k in t['bitmaps']:
                    src = k.split('@')[0]
                    if src not in prj.images:
                        errs.append('%s: картинка %s не из minigame.p3d' % (sc, src)); continue
                    iw, ih = prj.images[src].size; bw, bh = assets.bitmaps[k].size
                    d = max(abs(bw - iw * bh / ih), abs(bh - ih * bw / iw))
                    worst = max(worst, d); imgs += 1
                    if d > 1.5:
                        errs.append('%s: %s растянута %dx%d из %dx%d' % (sc, src, bw, bh, iw, ih))
            if t['type'] != U.T_RECT and w > 0 and (x < -1 or y < -1 or x + w > U.DST_W + 1 or y + h > U.DST_H + 1):
                errs.append('%s: %s за краем экрана %s' % (sc, t['name'], [x, y, w, h]))
    rep['images'] = imgs; rep['aspect_worst_px'] = round(worst, 2)
    strs = {s for sc in SCREENS for it in build_items(sc) if it['type'] == U.T_TEXT for s in it['strings']}
    for s in ('SELECT TRACK AND NUMBER OF LAPS', 'Change Track Direction', 'Change Character', 'Race Series Standings', 'Wins',
              'Points', 'Total Time', 'Best Lap', 'CONTINUE', 'QUIT GAME', 'x 9'):
        if s not in strs:
            errs.append('нет строки диска «%s»' % s)
    rep['strings'] = len(strs)
    return not errs, rep, errs


if __name__ == '__main__':
    argv = sys.argv[1:]
    if '--check' in argv:
        p = argv[argv.index('--check') + 1] if len(argv) > argv.index('--check') + 1 else str(OUT)
        ok, rep, errs = check(p)
        print(('OK' if ok else 'FAIL') + ' сверка mini.shr с диском: ' + json.dumps(rep, ensure_ascii=False))
        for e in errs:
            print('  ' + e)
        sys.exit(0 if ok else 1)
    out = None
    if '--out' in argv:
        out = argv[argv.index('--out') + 1]
    export(out, mocks=out is None)
