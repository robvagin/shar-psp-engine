#!/usr/bin/env python3
"""Правка Роба 312 (04.10.2026): мост твик-панели типографики (build/ТИПОГРАФИКА_ПАНЕЛЬ/index.html) в игру.

Панель сохраняет build/ui_typo.json. Экспортёры пакетов UIP1 (tools/ui_export.py pack: ui.shr и ui_l<N>.shr, а через тот же pack —
fsave/psave, shop, phone, menu, pmenu …) после раскладки каждого экрана зовут apply(экран, список рисования, картинки, грани): настройки
ложатся на те же группы строк, что в панели, в пикселях PSP, после всех правил раскладки (якоря, доски паузы, углы подсказок).
Нет файла (или UI_TYPO=0) — список не трогается: пакеты побайтно прежние (нулевой контроль tests/ui/typo_bridge.py).

Группа (GROUPS) — набор строк одной роли: экраны, страницы и имена элементов Scrooby, вид:
  face  строка гранью пакета: face — итоговое имя грани ('boulder_24*1.3+1^2': масштаб, межбуквенное +N px, межстрочное ^N px),
        dx/dy — сдвиг, w — ширина рамки строки (перенос, выравнивание), pitch — шаг строк списка (пункты паузы, LEVEL PROGRESS)
  pix8  строка пиксельной гранью 8×8 движка (viewer/ui_layout.h): trk/lead — межбуквенное и межстрочное (−8…7 px), у пузыря
        задания добавки к полям текста (viewer/ui.c objTextStart, fitW): padL — от аватара, padN — без значка (от скругления
        пузыря), padT — сверху, padR — справа; font — грань пакета вместо pix8
  num   число спрайтовым шрифтом (монеты, таймер, место): gk — масштаб картинок букв (как TUT_GLYPH_K, MSN_DONE_K)
  img   статичная картинка (пузырь, квитанция, значки): k — масштаб от центра, dx/dy
Значения, равные нынешним, ничего не меняют. Правила сдвигов те же, что у панели (её data.js несёт GROUPS этого файла):
  * грань сменилась, строка одна и прижата к верху рамки — центр строки на месте: dy += (старая строка − новая) // 2
  * w: центр рамки (centre) или правый край (right) на месте
  * pitch: строки списка по возрастанию y, центр блока на месте, шаг новый
  * подсказки кнопок в правом нижнем углу (corner): строка «значок + подпись» шире — группы сдвигаются влево, правый край на месте
  * padT у пузыря задания: движок опускает строку и не меняет правое поле (fitW вычитает padT из отступа)
"""
import json
import math
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PATH = ROOT / 'build' / 'ui_typo.json'
# Роб 17.10 (TEST31 сводной ветки собрался БЕЗ его панели — старые шрифты в магазине машин и «VEHICLE DESTROYED»): build/ui_typo.json не в git,
# в свежей рабочей копии его нет. Теперь панель Роба лежит в git (docs/typography/ui_typo_ROB.json) и берётся, если build/ui_typo.json нет.
# Без настроек — только явно UI_TYPO=0. UI_TYPO_TRACKED_OLD=1 — прежнее (нет файла — тихо без панели)
TRACKED_PATH = ROOT / 'docs' / 'typography' / 'ui_typo_ROB.json'

HUD = ('hud',)
PAUSES = ('pause', 'pausemsn')
SAVES = ('fslots', 'fprompt', 'fnotice', 'fmessage', 'pslots', 'pprompt', 'pnotice', 'pmessage')
HINT_PAGES = ('Accept.pag', 'Back.pag', 'Accept3.pag', 'Continue.pag')
HINT_SCREENS = ('pause', 'pausemsn', 'main', 'missionover', 'levelstats', 'tutorial', 'message') + SAVES

# id, подпись, зачем (аргумент), экраны, выбор (страница, имя; имя 'X*' — начало имени, '*' — любое), вид, флаги
_HK = dict(sel=[(p, '*') for p in HINT_PAGES], kind='face', corner=True, icons=True, btn=True)
HINT_SPLIT = [
    dict(id='hints_main', title='Подписи кнопок · главное меню', why='✕ Accept справа внизу на главном меню (рамка телевизора)', screens=('main',), inherit='hints', **_HK),
    dict(id='hints_message', title='Подписи кнопок · уведомление', why='✕ Accept окна сообщения (Congratulations, отключён контроллер)', screens=('message',), inherit='hints', **_HK),
    dict(id='hints_over', title='Подписи кнопок · провал миссии', why='✕ Accept под вопросом «Retry Mission?»', screens=('missionover',), inherit='hints', **_HK),
    dict(id='hints_tut', title='Подписи кнопок · подсказки Барта', why='подписи кнопок на экране подсказки Барта', screens=('tutorial',), inherit='hints', **_HK),
    dict(id='hints_pause', title='Подписи кнопок · пауза', why='✕ Accept и △ Back паузы, LEVEL PROGRESS', screens=('pause', 'pausemsn', 'levelstats'), inherit='hints', **_HK),
    dict(id='hints_save', title='Подписи кнопок · сохранение', why='✕ Accept, △ Back и Continue экранов сохранения', screens=SAVES, inherit='hints', **_HK),
    dict(id='hints_msel', title='Подписи кнопок · MISSION SELECT', why='Accept и Back списка миссий паузы (раньше настроить было нельзя)', screens=('missionselect',), **_HK),
    dict(id='pause_cell', title='Пауза: код клетки', why='№598: «L1 K9-7» слева внизу паузы (шрифт Bart vs the Space Mutants)', screens=PAUSES, sel=[('PlaceCode.pag', 'PlaceCode')], kind='face'),
    dict(id='hints_vcards', title='Подписи кнопок · VIEW CARDS', why='№596: ✕ Accept и △ Back сетки и открытой карточки VIEW CARDS (шрифт как у всех подсказок, раньше группы не было)', screens=('viewcards',), **_HK),
    dict(id='hints_wardrobe', title='Подписи кнопок · гардероб', why='№695: Accept и Back гардероба — как у будки (значения будки без сдвига dx: у будки он под её раскладку)', screens=('wardrobe',), **_HK),
    # Роб 17.10 вечер («везде ли обновлены шрифты… accept и cancel старый шрифт»): экраны настроек (OPTIONS, звук, ролики; в паузе — OPTIONS, звук,
    # SETTINGS) не входили ни в одну группу — подписи шли шрифтом диска строчными «Accept / Back». Теперь как у паузы. UI_TYPO_HINTS_MENU_OLD=1 — без группы
    *([] if os.environ.get('UI_TYPO_HINTS_MENU_OLD') == '1' else [dict(id='hints_menu', title='Подписи кнопок · настройки', why='✕ Accept и △ Back экранов OPTIONS, звука, титров, роликов (главное меню и пауза)', screens=('options', 'sound', 'viewcredits', 'viewmovies', 'pauseoptions', 'pausesound', 'pausesettings', 'pausecamera', 'pausegame', 'pausepicture', 'pausecartoon') + (() if os.environ.get('SCRAPFONT1121_OLD') == '1' else ('scrapbook', 'scrapcontents', 'scrapstats', 'missiongallery', 'skingallery', 'vehiclegallery')), inherit='hints_pause', **_HK)]),   # №1121: Accept / Back альбома SCRAP BOOK   # №777: подстраницы SETTINGS
    dict(id='hints_phone', title='Подписи кнопок · будка', why='Back экрана телефонной будки (раньше настроить было нельзя)', screens=('phone',), **_HK),
]
HINT_ONE = [dict(id='hints', title='Подписи кнопок', why='✕ Accept, △ Back, Continue — одна грань на всех экранах меню',
                 screens=HINT_SCREENS, sel=[(p, '*') for p in HINT_PAGES], kind='face', corner=True, icons=True)]
HINT_GROUPS = HINT_ONE if os.environ.get('UI_TYPO_HINTS_SPLIT_OLD') == '1' else HINT_SPLIT
# Роб 17.10 вечер (сквозная проверка шрифтов): заголовок VIEW CARDS не входил ни в одну группу — шёл гранью по умолчанию, а не той, что Роб поставил
# заголовкам паузы; теперь — часть «Заголовок паузы» (своя группа pause_title_vc, пока не трогали — значения паузы). VC_TITLE_OLD=1 — прежнее
VC_TITLE_OLD = os.environ.get('VC_TITLE_OLD') == '1'
HINT_LINE = 13   # высота строки подписи PS2 (опорная для centre)
HINT_ALIGN = {   # Роб 11.10: углы как у паузы PS2 (8 / 9) и «внизу справа» главного меню; где у PS2 иначе (окно сообщения, будка, лавка, бонус-экраны) — не трогаем
    'hints_pause': dict(right=8, bottom=9), 'hints_pause_ls': dict(right=8, bottom=9), 'hints_minipause': dict(right=8, bottom=9),
    'hints_save': dict(right=8, bottom=9), 'hints_save_prompt': dict(right=8, bottom=9), 'hints_save_notice': dict(right=8, bottom=9),
    'hints_main': dict(x=402, y=233), 'hints_over': dict(x=402, y=233), 'hints_tut': dict(x=402, y=233),
}
LEGACY = ('hints', 'hud_bubble', 'st_bubble')   # прежний идентификатор группы: принимается в json (наследование), страница его больше не пишет
GROUPS = [
    dict(id='boot_loading', title='Экран запуска: надпись LOADING', why='надпись LOADING внизу экрана запуска',
         screens=('bootload', 'license'), sel=[('LoadingText.pag', 'Loading')], kind='face'),
    dict(id='boot_license', title='Экран запуска: логотип и копирайт', why='блок Simpsons Hit & Run, Fox и синий текст копирайта',
         screens=('bootload', 'license'), sel=[('License.pag', 'License')], kind='img'),
    dict(id='title_start', title='Press START', why='приглашение начать игру на первом экране',
         screens=('splash',), sel=[('Splash.pag', 'PressStart')], kind='face'),
    dict(id='bonus_head', title='Бонус: заголовки итогов', why='названия столбцов и итогов заезда',
         screens=('msbase',), sel=[('MiniSummary.pag', '*'), ('Continue.pag', '*')], kind='face',
         parts=[dict(id='cols', title='Заголовки столбцов', sel=[('MiniSummary.pag', 'Wins'), ('MiniSummary.pag', 'Points'), ('MiniSummary.pag', 'TotalTime'),
                                                              ('MiniSummary.pag', 'BestLap')]),
                dict(id='cont', title='✕ Continue', sel=[('Continue.pag', '*')])]),
    dict(id='bonus_rows', title='Бонус: результаты игроков', why='место, победы, очки и время каждого участника',
         screens=tuple('msrow%d' % i for i in range(4)), sel=[('MiniSummary.pag', '*')], kind='face',
         parts=[dict(id='rank', title='Место (1st)', sel=[('MiniSummary.pag', 'Rank*')]),
                dict(id='plr', title='Игрок (P1)', sel=[('MiniSummary.pag', 'Player*')]),
                dict(id='nums', title='Числа столбцов', sel=[('MiniSummary.pag', 'NumWins*'), ('MiniSummary.pag', 'NumPoints*'), ('MiniSummary.pag', 'TotalTime*'),
                                                              ('MiniSummary.pag', 'BestLap*')])]),
    # ---- игра (HUD)
    # Роб 11.10 (№528): пузырь с аватаркой (Talk to Marge) и без значка (Vehicle destroyed) — разные состояния игры (viewer/ui.c: значок задан или нет), у каждого свои настройки;
    # в пакете их два набора строки и рамки (признак варианта в res[0], ITEM_AVATAR/ITEM_PLAIN), пока не тронуты — берут значения прежней общей группы hud_bubble. UI_TYPO_BUBBLEVAR_OLD=1 — одна группа
    dict(id='hud_bubble_avatar', title='Пузырь задания · с аватаркой', why='главный текст игры со значком-аватаркой (Talk to Marge)', inherit='hud_bubble', variant='avatar',
         screens=HUD, sel=[('Hud.pag', 'Message'), ('Hud.pag', 'MissionObjective')], kind='pix8', pads=True),
    dict(id='hud_bubble_plain', title='Пузырь задания · без значка', why='главный текст игры без значка (Vehicle destroyed, Go to the Kwik-E-Mart)', inherit='hud_bubble', variant='plain',
         screens=HUD, sel=[('Hud.pag', 'Message'), ('Hud.pag', 'MissionObjective')], kind='pix8', pads=True),
    dict(id='hud_action', title='Подпись под △', why='что сделает треугольник: поговорить, сесть в машину, войти',
         screens=HUD, sel=[('Hud.pag', 'ActionText')], kind='pix8'),
    dict(id='hud_busted', title='Штраф полиции: BUSTED!', why='правка 310: уведомление об аресте и штрафе — шрифт крупнее',
         screens=HUD, sel=[('Hud.pag', 'HitNRun')], kind='face'),
    dict(id='hud_hitrun', title='Hit & Run!', why='предупреждение о погоне, крупно и красным, как у оригинала',
         screens=HUD, sel=[('Hud.pag', 'HitNRunBig')], kind='face'),
    dict(id='hud_banner', title='Надписи этапа', why='TASK COMPLETE, карточка, осы, колода — короткие победные строки по центру',
         screens=HUD, sel=[('Hud.pag', 'StageComplete'), ('Hud.pag', 'WaspsComplete'), ('Hud.pag', 'CardGet'), ('Hud.pag', 'ItemsComplete')],
         kind='face'),
    dict(id='hud_mdone', title='MISSION COMPLETE!', why='правка 500: вспышка конца миссии мультяшными буквами диска, на всю ширину кадра',
         screens=HUD, sel=[('Hud.pag', 'MissionComplete')], kind='num'),
    dict(id='load_text', title='Загрузка: LOADING', why='правка 500: мигающая надпись на газете уровня и точки после неё (загрузка меню)',
         screens=('loading', 'loadingfe'), sel=[('Loading.pag', 'Loading'), ('LoadingFE.pag', 'Loading')], kind='face'),
    dict(id='mini_menu', title='Бонус: выбор персонажа, машины и трассы', why='правка 500: подписи экранов выбора мини-игры (SELECT TRACK, круги, направление)',
         screens=('mmchar', 'mmtrack'), sel=[('MiniMenu.pag', '*')], kind='face'),
    dict(id='mini_hints', title='Бонус: подписи кнопок', why='правка 500: △ Back и ✕ Accept под выбором трассы',
         screens=('mmhints',), sel=[('Back2.pag', '*'), ('Accept2.pag', '*')], kind='face'),
    dict(id='hints_minipause', title='Подписи кнопок · пауза бонус-режима', why='✕ Accept паузы бонус-гонки (угол как у паузы PS2, правка 11.10)', screens=('minipause',), sel=[('Accept.pag', '*')], kind='face', corner=True, icons=True, inherit='mini_pause'),
    dict(id='mini_pause', title='Бонус: пауза', why='правка 500: PAUSE MENU, CONTINUE и QUIT GAME мини-игры',
         screens=('minipause',), sel=[('Pause.pag', '*'), ('MiniPause.pag', '*')] + ([('Accept.pag', '*')] if os.environ.get('BONUSUI1145_OLD') == '1' else []), kind='face', pitch=True),   # №1145: подпись ✕ Accept — своя группа hints_minipause (грань паузы основной игры на неё не переходит)
    # недоработки, найденные проходом по кадрам (10.10): тексты без группы, настроить которые было нельзя — группы без наследования (без ключей в json пакеты те же)
    dict(id='bpause_items', title='Пауза бонус-режима: пункты', why='CONTINUE / QUIT GAME паузы бонус-гонки (страница PauseMission.pag; в группе «Пункты паузы» их нет, чтобы не менять её)', screens=('minipause',), sel=[('PauseMission.pag', '*')], kind='face', pitch=True),
    dict(id='ms_title', title='MISSION SELECT: заголовок', why='название экрана над списком миссий', screens=('missionselect',), sel=[('MissionSelect.pag', 'MissionSelectTitle')], kind='face'),
    dict(id='over_title', title='Провал миссии: FAILED', why='крупная надпись FAILED над причиной', screens=('missionover',), sel=[('MissionOver.pag', 'MissionFailed')], kind='face'),
    dict(id='phone_damage', title='Будка: Damaged', why='«Damaged: N %» слева внизу экрана будки', screens=('phone',), sel=[('PhoneBooth.pag', 'VehicleDamaged'), ('PhoneBooth.pag', 'VehicleDamaged_Value')], kind='face'),
    dict(id='hud_card', title='Название карточки', why='что за карточку собрал (пиксельный шрифт, с переносом)',
         screens=HUD, sel=[('Hud.pag', 'CardTitle')], kind='pix8'),
    dict(id='hud_unlock', title='Открыто новое', why='строка «New bonus game track unlocked!» под надписью колоды',
         screens=HUD, sel=[('Hud.pag', 'ItemsUnlocked')], kind='face'),
    dict(id='hud_dropped', title='Item Dropped!', why='уведомление о выпавшем предмете миссии (пиксельный шрифт)',
         screens=HUD, sel=[('Hud.pag', 'ItemDropped')], kind='pix8'),
    dict(id='hud_small', title='Подписи гонки', why='LAPS и окончания места (st, nd, rd) рядом с крупными цифрами',
         screens=HUD, sel=[('Hud.pag', 'Laps'), ('Hud.pag', 'PositionOrdinal')], kind='face',
         parts=[dict(id='ord', title='«rd» у места', sel=[('Hud.pag', 'PositionOrdinal')])]),
    dict(id='hud_nums', title='Цифры HUD', why='монеты, таймер, круги, место — мультяшные цифры диска',
         screens=('hud', 'pause', 'pausemsn', 'purchase', 'phone'),
         sel=[('Coins.pag', 'NumCoins'), ('Hud.pag', 'Timer'), ('Hud.pag', 'ParTime'), ('Hud.pag', 'Lap'), ('Hud.pag', 'Position'),
              ('Hud.pag', 'ItemsCount'), ('Hud.pag', 'CountDown'), ('Hud.pag', 'Collectibles'), ('Rewards.pag', 'NumCoins'),
              ('PhoneBooth.pag', 'NumCoins')], kind='num',
         parts=[dict(id='place', title='Место «3»', sel=[('Hud.pag', 'Position')]),
                dict(id='pause', title='Монеты в паузе', screens=PAUSES, sel=[('Coins.pag', 'NumCoins')])]),
    # ---- пауза и её экраны
    dict(id='pause_coins', title='Монеты в паузе (значок и цифры)', why='правка 327: счёт монет в паузе — вместе со значком, ближе к правому углу',
         screens=PAUSES, sel=[('Coins.pag', '*')], kind='move'),
    dict(id='pause_title', title='Заголовок паузы', why='PAUSE MENU и LEVEL PROGRESS над рамкой: крупнее пунктов, мельче шапки',
         screens=PAUSES + ('levelstats',) + (() if VC_TITLE_OLD else ('viewcards',)), sel=[('Pause.pag', 'Paused'), ('LevelStats.pag', 'LevelProgress')] + ([] if VC_TITLE_OLD else [('PauseViewCards.pag', 'ViewCardsTitle')]), kind='face'),
    dict(id='pause_items', title='Пункты паузы', why='CONTINUE … QUIT GAME: читаются с расстояния вытянутой руки, шаг строк — воздух списка',
         screens=PAUSES, sel=[('PauseSunday.pag', '*'), ('PauseMission.pag', '*')], kind='face', pitch=True),
    dict(id='ls_rows', title='Строки LEVEL PROGRESS', why='таблица прогресса уровня: подпись слева, число справа',
         screens=('levelstats',), sel=[('LevelStats.pag', '*')], kind='face', pitch=True, exclude=['LevelProgress']),
    # Правка 10.10 (подписи кнопок одной группой на все экраны — сдвиг на главном меню уезжал на уведомлении): группа «Подписи кнопок» теперь своя на каждый экран
    # (HINT_SPLIT); пока в json нет новой группы, её значения берутся у прежней «hints» (inherit) — без новых групп в json пакеты побайтно те же. MISSION SELECT и будка
    # подписей раньше не имели вовсе (нечего наследовать). UI_TYPO_HINTS_SPLIT_OLD=1 — прежняя одна группа «hints»
    *(HINT_GROUPS),
    # ---- MISSION SELECT паузы (правка 348: pextra.shr, tools/ui_export.py missionselect_layout / missionselect_psp). rowcarry — что
    # едет со своей строкой при шаге (по номеру в конце имени): значок состояния и полоса выбора строки k — за названием строки k
    dict(id='ms_rows', title='MISSION SELECT: строки', why='правка 348: номер и название миссии; шаг строк — воздух списка (значок и полоса выбора едут со строкой)',
         screens=('missionselect',), sel=[('MissionSelect.pag', 'Mission*')], kind='face', pitch=True, exclude=['MissionSelectTitle'],
         rowcarry=[('MissionSelect.pag', 'MissionStatus*'), ('MissionSelect.pag', 'HighlightBar*')]),
    # ---- №597 (15.10): открытая коллекционная карточка (VIEW CARDS, крупно) — заголовок, эпизод, длинное описание; QR рисует viewer/main_save.h vcQrDraw
    dict(id='vc_title', title='Карточка: заголовок', why='№597: название карточки жёлтым над эпизодом', screens=('viewcards',), sel=[('PauseViewCards.pag', 'CardTitle*')], kind='face'),
    dict(id='vc_episode', title='Карточка: эпизод', why='№597: «Episode 4F22: …» под заголовком', screens=('viewcards',), sel=[('PauseViewCards.pag', 'CardEpisode*')], kind='face'),
    dict(id='vc_desc', title='Карточка: длинный текст', why='№597: описание карточки, колонка на ширину рамки минус QR (80 px)', screens=('viewcards',), sel=[('PauseViewCards.pag', 'CardDesc*')], kind='face'),
    dict(id='ms_level', title='MISSION SELECT: LEVEL N', why='правка 348: уровень на планке со стрелками ◄ ► (листается на экране)',
         screens=('missionselect',), sel=[('MissionSelect.pag', 'Level')], kind='face'),
    dict(id='ms_status', title='MISSION SELECT: Status', why='правка 348: заголовок столбца значков (галочка, крестик, черта)',
         screens=('missionselect',), sel=[('MissionSelect.pag', 'Status')], kind='face'),
    # ---- главное меню
    dict(id='main_item', title='Пункт главного меню', why='NEW GAME в рамке телевизора — одна строка карусели',
         screens=('main',), sel=[('MainMenu.pag', 'MainMenu*')], kind='face'),
    dict(id='main_level', title='Уровень в меню', why='строка «L1 Suburbs» над пунктом',
         screens=('main',), sel=[('Level.pag', 'Level')], kind='face'),
    # ---- карточка миссии, разговор, провал
    dict(id='brief_title', title='Название миссии', why='THE COLA CAPER — шапка карточки',
         screens=('missionload',), sel=[('MissionLoad.pag', 'MissionTitle'), ('MissionLoad.pag', 'PattyAndSelmaTitle')], kind='face'),
    dict(id='brief_info', title='Текст задания на карточке', why='правка 299: тем же пиксельным шрифтом, что пузырь задания',
         screens=('missionload',), sel=[('MissionLoad.pag', 'MissionInfo'), ('MissionLoad.pag', 'PattyAndSelmaInfo')], kind='pix8'),
    dict(id='brief_btn', title='Кнопки карточки', why='✕ Continue, Cancel Mission; №632: Continue (значок + зазор 5 px + слово) стоит по центру карточки, dx группы — сдвиг поверх центра', shadow=1, btn=True,
         screens=('missionload',), sel=[('MissionLoad.pag', 'Continue'), ('MissionLoad.pag', 'Abort')],
         kind='face'),
    dict(id='brief_loading', title='LOADING на карточке', why='№632: мигающее LOADING по центру карточки (центр надписи), шрифт Bart vs the Space Mutants; dx — сдвиг поверх центра', shadow=1,
         screens=('missionload',), sel=[('MissionLoad.pag', 'BriefLoading')], kind='face', inherit='brief_btn'),
    # Роб 17.10: строки гонки на ставку (Entry Fee / Time to Beat / Payout) — шрифтом кнопок карточки (Bart), пока не тронуты. WAGER_CARD_OLD=1 — без группы
    *([] if os.environ.get('WAGER_CARD_OLD') == '1' else [dict(id='brief_wager', title='Карточка: строки ставки', why='Entry Fee, Time to Beat, Payout гонки на ставку справа на чёрной плашке', screens=('missionload',), sel=[('MissionLoad.pag', 'EntryFee*'), ('MissionLoad.pag', 'TimeToBeat*'), ('MissionLoad.pag', 'Payout*')], kind='face', inherit='brief_btn')]),
    dict(id='lb_buttons', title='Кнопки разговора', why='✕ Start Mission, △ Cancel, ✕ SKIP внизу экрана разговора', shadow=1, btn=True,
         screens=('letterbox',), sel=[('LetterBoxButtons.pag', '*')], kind='face'),
    dict(id='over_text', title='Причина провала и подсказка', why='почему провалил и как пройти',
         screens=('missionover',), sel=[('MissionOver.pag', 'MissionFailureReason'), ('MissionOver.pag', 'MissionFailureHint')], kind='face'),
    dict(id='over_retry', title='Retry Mission?', why='вопрос над YES / NO',
         screens=('missionover',), sel=[('MissionOver.pag', 'RetryMission')], kind='face'),
    dict(id='over_answers', title='YES / NO провала', why='ответы на вопрос, пункты меню',
         screens=('missionover',), sel=[('MissionOver.pag', 'Response*')], kind='face'),
    # ---- подсказки Барта, окно сообщения
    dict(id='tut_text', title='Подсказки Барта', why='обучение: как ходить, прыгать, сесть в машину (буквы картинками диска)',
         screens=('tutorial',), sel=[('Tutorial.pag', 'TutorialMessage')], kind='num'),
    dict(id='tut_board', title='Рамка подсказки Барта', why='Роб 04.10: «рамочка капельку больше» — шире и выше от центра, углы те же',
         screens=('tutorial',), sel=[('SmallBoard.pag', '*')], kind='board'),
    dict(id='tut_disable', title='Disable Tutorials', why='подпись □ под рамкой подсказки',
         screens=('tutorial',), sel=[('Tutorial.pag', 'DisableTutorial')], kind='face'),
    dict(id='msgbox', title='Окно сообщения', why='Congratulations, отключён контроллер (пиксельный шрифт)',
         screens=('message',), sel=[('Message.pag', 'Message')], kind='pix8'),
    # ---- сохранение (экраны помощника правки 309: tools/save_export.py; мост тот же — pack)
    dict(id='save_head', title='Сохранение: куда', why='Save to: / Load from: и Memory Stick над слотами',
         screens=SAVES, sel=[('SelectMemoryDevice.pag', 'LoadSave'), ('SelectMemoryDevice.pag', 'CurrentMemoryDevice')], kind='face'),
    dict(id='save_slots', title='Сохранение: слоты', why='< EMPTY SLOT >, уровень и дата — пункты меню сохранения (правка 309)',
         screens=SAVES, sel=[('GameSlots.pag', 'Slot*')], kind='face', pitch=True),
    dict(id='save_msg', title='Сохранение: сообщение', why='вопрос и итог записи (Save Successful)',
         screens=SAVES, sel=[('GenericPrompt.pag', 'Message'), ('Message.pag', 'Message')], kind='face'),
    dict(id='save_answers', title='Сохранение: ответы', why='YES / NO / Continue на вопрос',
         screens=SAVES, sel=[('Prompt.pag', 'Response*')], kind='face'),
    # правка Роба 343 (05.10: «accept и back справа внизу на рамке меню — опустить и вправо»): подсказки экранов сохранения стоят
    # на правом крае доски (значки поверх рамки); сдвиг значков и подписей вместе — в угол экрана, как у подсказок паузы
    dict(id='save_hints', title='Сохранение: подписи кнопок', why='✕ Accept и △ Back у экранов сохранения — из-под рамки в угол, как в паузе',
         screens=SAVES, sel=[(p, '*') for p in HINT_PAGES], kind='move'),
    # ---- лавка и будка
    dict(id='shop_name', title='Лавка: машина', why='название машины над подиумом',
         screens=('purchase',), sel=[('Rewards.pag', 'PreviewName')], kind='face'),
    dict(id='shop_price', title='Лавка: цена', why='150 coins / to purchase под машиной',
         screens=('purchase',), sel=[('Rewards.pag', 'Price'), ('Rewards.pag', 'ToPurchase')], kind='face'),
    dict(id='shop_stats', title='Лавка: характеристики', why='SPEED, ACCELERATION, TOUGHNESS, HANDLING над звёздами',
         screens=('purchase',), sel=[('Rewards.pag', 'Speed'), ('Rewards.pag', 'Acceleration'), ('Rewards.pag', 'Toughness'),
                                     ('Rewards.pag', 'Stability')], kind='face'),
    dict(id='shop_buttons', title='Лавка: кнопки', why='Purchase, Cancel, Show Stats внизу экрана',
         screens=('purchase',), sel=[('Rewards.pag', 'ToggleView'), ('Cancel.pag', 'Back'), ('Buy.pag', 'Accept')], kind='face'),
    dict(id='phone_name', title='Будка: машина', why='название машины в телефонной будке',
         screens=('phone',), sel=[('PhoneBooth.pag', 'PreviewName')], kind='face'),
    dict(id='wardrobe_name', title='Гардероб: костюм', why='№695: название костюма — шрифт названия машины будки, без её сдвига (у будки имя слева, у гардероба по центру сверху)',
         screens=('wardrobe',), sel=[('PhoneBooth.pag', 'PreviewName')], kind='face'),
    dict(id='phone_repair', title='Будка: REPAIR: 50 COINS', why='цена починки разбитой машины под картинкой (по умолчанию Bart 16bit)', screens=('phone',),
         sel=[('PhoneBooth.pag', 'RepairOk'), ('PhoneBooth.pag', 'RepairNo')], kind='face'),
    dict(id='phone_stats_text', title='Будка: SPEED… (шрифт)', why='подписи SPEED, ACCELERATION, TOUGHNESS, HANDLING: шрифт отдельно, сдвиг — в группе ниже', screens=('phone',),
         sel=[('PhoneBooth.pag', n) for n in ('Speed', 'Acceleration', 'Toughness', 'Stability')], kind='pix8'),
    dict(id='phone_stats', title='Будка: характеристики', why='Роб 04.10: блок SPEED… со звёздами слишком у края — сдвиг целиком',
         screens=('phone',), sel=[('PhoneBooth.pag', n) for n in ('Speed', 'Acceleration', 'Toughness', 'Stability', 'Spe_*', 'Acc_*', 'Tou_*', 'Sta_*')],
         kind='move'),
    # ---- статичные картинки (масштаб от центра)
    dict(id='st_bubble_avatar', title='Пузырь задания · плашка с аватаркой', why='плашка текста задания: от её размера зависит перенос строк', inherit='st_bubble', variant='avatar',
         screens=HUD, sel=[('Hud.pag', 'MessageBox')], kind='img'),
    dict(id='st_bubble_plain', title='Пузырь задания · плашка без значка', why='плашка текста задания: от её размера зависит перенос строк', inherit='st_bubble', variant='plain',
         screens=HUD, sel=[('Hud.pag', 'MessageBox')], kind='img'),
    dict(id='st_ticket', title='Квитанция штрафа', why='правка 310: бумажка «50» под BUSTED!',
         screens=HUD, sel=[('Hud.pag', 'HitNRunTicket')], kind='img'),
    dict(id='st_action', title='Значок △', why='треугольник действия над подписью',
         screens=HUD, sel=[('Hud.pag', 'ActionButton')], kind='img'),
    dict(id='st_shop_stars', title='Лавка: звёзды', why='Роб 04.10: звёзды, промежутки и подписи увеличиваются пропорционально',
         screens=('purchase',), sel=[('Rewards.pag', n) for n in ('Spe_*', 'Acc_*', 'Tou_*', 'Sta_*')], kind='img', block=True,
         carry=['shop_stats'], carryfont=True),
]
# Правка 10.10 (вторая половина: «одна настройка на разные экраны»): группы, стоявшие на нескольких экранах, делятся по экрану. Первая запись таблицы остаётся
# под прежним id (её значения в json Роба не теряются), остальные получают id «прежний_суффикс» и inherit=прежний id: пока их не трогали, берут значения первой.
# Составные экраны бонус-меню (mmhints поверх mmchar/mmtrack — один экран пакета) и подписи кнопок бонуса разделить нельзя: движок рисует их одним элементом.
# UI_TYPO_HINTS_SPLIT_OLD=1 — как до правки (те же группы на нескольких экранах)
SPLITS = {
    'hints_pause': [(None, '', ('pause', 'pausemsn')), ('ls', ' · LEVEL PROGRESS', ('levelstats',))],
    'hints_save': [(None, ' · слоты', ('fslots', 'pslots')), ('prompt', ' · вопрос', ('fprompt', 'pprompt')), ('notice', ' · уведомление', ('fnotice', 'pnotice', 'fmessage', 'pmessage'))],
    'save_hints': [(None, ' · слоты', ('fslots', 'pslots')), ('prompt', ' · вопрос', ('fprompt', 'pprompt')), ('notice', ' · уведомление', ('fnotice', 'pnotice', 'fmessage', 'pmessage'))],
    'hud_nums': [(None, ' · HUD', ('hud',)), ('pause', ' · пауза', ('pause', 'pausemsn')), ('shop', ' · лавка', ('purchase',)), ('phone', ' · будка', ('phone',))],
    'pause_title': [(None, ' · пауза', ('pause', 'pausemsn')), ('ls', ' · LEVEL PROGRESS', ('levelstats',))] + ([] if VC_TITLE_OLD else [('vc', ' · VIEW CARDS', ('viewcards',))]),
    'mini_menu': [(None, ' · персонаж и машина', ('mmchar',)), ('track', ' · трасса', ('mmtrack',))],
}


def _split_groups(groups):
    if os.environ.get('UI_TYPO_HINTS_SPLIT_OLD') == '1':
        return groups
    out = []
    for g in groups:
        sp = SPLITS.get(g['id'])
        if not sp:
            out.append(g)
            continue
        for k, (suf, ttl, scr) in enumerate(sp):
            n = dict(g, screens=scr)
            if suf:
                n.update(id=g['id'] + '_' + suf, inherit=g['id'])
            n['title'] = g['title'] + ttl
            if g.get('parts'):
                n['parts'] = [pt for pt in g['parts'] if (set(pt['screens']) & set(scr) if pt.get('screens') else k == 0)]
            out.append(n)
    return out


if os.environ.get('UI_TYPO_PHONE_TEXT_OLD') == '1':
    GROUPS = [g for g in GROUPS if g['id'] != 'phone_stats_text']
GROUPS = _split_groups(GROUPS)
for _g in GROUPS:
    if _g['id'] in HINT_ALIGN:
        _g['align'] = HINT_ALIGN[_g['id']]
GROUP = {g['id']: g for g in GROUPS}
T_RECT, T_IMG, T_TEXT, T_NUM = 0, 2, 3, 4
DST_W, SAFE_X, DST_H = 480, 8, 272
PIX8_LH = 10
ASCII = ''.join(chr(c) for c in range(32, 127))
CODE_FIELDS = {'OBJECTIVE', 'ACTION_TEXT', 'MSGBOX', 'MSN_TITLE', 'MSN_INFO', 'MSN_FAIL', 'MSN_HINT'}   # строку ставит код (viewer/ui.c)


def load(path=None):
    """настройки панели или None (нет файла / UI_TYPO=0 / пусто)"""
    env = os.environ.get('UI_TYPO')
    if env == '0':
        return None
    p = Path(path or env or DEFAULT_PATH)
    if not p.exists() and not (path or env) and os.environ.get('UI_TYPO_TRACKED_OLD') != '1' and TRACKED_PATH.exists():
        p = TRACKED_PATH
    if not p.exists():
        return None
    cfg = json.loads(p.read_text())
    hp = (cfg.get('groups') or {}).get('hints_phone')
    if hp and 'hints_wardrobe' not in cfg['groups'] and os.environ.get('WARDROBE_TYPO_OLD') != '1':   # №695: гардероб берёт подписи кнопок будки (кроме сдвига dx)
        cfg['groups']['hints_wardrobe'] = {k: v for k, v in hp.items() if k != 'dx'}
        if 'dy' in cfg['groups']['hints_wardrobe']: cfg['groups']['hints_wardrobe']['dy'] -= 5   # нижняя строка гардероба стоит на 5 px ниже, чем у будки при той же дельте
    # №925 (Роб 24.10: «текст в меню паузы по-прежнему крупнее, чем я просил: размер как в Options, расстановку — как сейчас»): пункты паузы — грань пунктов
    # Options меню (U.ITEM_FACE, boulder_16 1:1, заглавная 11 px), а не boulder_16 x2 (12 px, на снимке 12 против 11); шаг строк (pitch) и низ букв прежние.
    # Только если в панели стоит прежнее значение (другое значение руки Роба не трогаем). UI_TYPO_PAUSE925_OLD=1 — boulder_16*2, как было
    pi = (cfg.get('groups') or {}).get('pause_items')
    if pi and pi.get('face') == 'boulder_16*2' and os.environ.get('UI_TYPO_PAUSE925_OLD') != '1':
        pi['face'] = 'boulder_16@1'
    pn = (cfg.get('groups') or {}).get('phone_name')
    if pn and 'wardrobe_name' not in cfg['groups'] and os.environ.get('WARDROBE_TYPO_OLD') != '1':
        cfg['groups']['wardrobe_name'] = {k: v for k, v in pn.items() if k not in ('dx', 'dy')}
    # №1145 (Роб 02.11: «бонус-игра: интерфейс старым шрифтом, сделай как в основной игре»): пауза бонус-гонки берёт грани паузы основной игры — заголовок PAUSE MENU гранью
    # заголовка паузы (pause_title, толстые мультяшные буквы), CONTINUE / QUIT GAME гранью и шагом пунктов паузы (pause_items). Значения берутся из панели Роба каждый раз заново,
    # поэтому правка паузы основной игры уходит и в бонус. Подпись ✕ Accept (hints_minipause) своя, не трогается. BONUSUI1145_OLD=1 — прежние группы mini_pause / bpause_items
    gs = cfg.get('groups') or {}
    if os.environ.get('BONUSUI1145_OLD') != '1':
        pt, pit = gs.get('pause_title'), gs.get('pause_items')
        if pt and pt.get('face'):
            gs['mini_pause'] = {k: v for k, v in pt.items() if k in ('face', 'dy')}
            if 'hints_minipause' not in gs and (gs.get('hints_pause') or {}).get('face'): gs['hints_minipause'] = dict(gs['hints_pause'])
        if pit and pit.get('face'):
            gs['bpause_items'] = {k: v for k, v in pit.items() if k in ('face', 'pitch', 'dy')}
    return cfg if ((cfg.get('groups') or {}) or (cfg.get('elements') or {})) else None   # №574: бывают только настройки отдельных элементов


def _scr(sc, g=None):
    # №695: гардероб Гомера — та же страница PhoneBooth, что и будка: типографика будки (панель Роба, подписи кнопок) действует и на него;
    # WARDROBE_TYPO_OLD=1 — прежнее (гардероб без настроек панели)
    return 'phone' if sc == 'wardrobe' and os.environ.get('WARDROBE_TYPO_OLD') != '1' and not (g and (g.get('id') in ('hints_phone', 'hints_wardrobe', 'phone_name', 'wardrobe_name') or str(g.get('id')).startswith('hud_nums'))) else sc


def _name_ok(pat, name):
    return pat == '*' or (pat.endswith('*') and name.startswith(pat[:-1])) or pat == name


def matches(g, sc, t, var=None):
    if _scr(sc, g) not in g['screens']:
        return False
    if g.get('variant') and (var or t.get('variant')) != g['variant']:      # пузырь задания: вариант с аватаркой / без значка (№528)
        return False
    want = {'face': (T_TEXT,), 'pix8': (T_TEXT,), 'num': (T_NUM,), 'img': (T_IMG,), 'move': (T_TEXT, T_IMG, T_NUM), 'board': (T_IMG, T_RECT)}[g['kind']]
    if t['type'] not in want or t['name'] in g.get('exclude', ()):
        return False
    return any(t.get('page') == pg and _name_ok(nm, t['name']) for pg, nm in g['sel'])


def engine_pix8(sc, t):
    """рисует ли движок строку пиксельной гранью 8×8 (viewer/ui.c pix8Want)"""
    if t['type'] != T_TEXT or t.get('nopix8'):
        return False
    if t.get('field') in ('OBJECTIVE', 'MSGBOX') or t.get('pix8'):
        return True
    return sc == 'hud' and (t.get('tag') == 'ITEM_DROPPED' or (t.get('tag') == 'CARD_TEXT' and bool(t.get('wrap'))))


def _s8(v):
    v = int(v)
    return max(-128, min(127, v)) & 0xff


def typo_res(trk=0, lead=0, padT=0, padL=0, padR=0):
    """res[2] записи элемента (viewer/ui.c TYPO_*): биты 0–3 trk, 4–7 lead (−8…7), байты 1–3 padT, padL, padR"""
    trk, lead = max(-8, min(7, int(trk))), max(-8, min(7, int(lead)))
    return (trk & 0xf) | (lead & 0xf) << 4 | _s8(padT) << 8 | _s8(padL) << 16 | _s8(padR) << 24


def typo_res1(padN=0):
    """res[1] строки (у текста не занят; у картинок — бегунок меню): младший байт — добавка к полю текста задания без значка (TYPO_PADN)"""
    return _s8(padN)


def _lh(fonts, t, sc):
    return PIX8_LH if engine_pix8(sc, t) else fonts.get(t['font'])['lh']


def _part_matches(part, sc, t):
    return (_scr(sc) in part.get('screens', (_scr(sc),))) and (not part.get('sel') or any(t.get('page') == pg and _name_ok(nm, t['name'])
                                                                                for pg, nm in part['sel']))


def _parts(g, st, its, sc):
    """части группы со своим сдвигом (<часть>_dx / _dy): место «3» среди цифр HUD, «rd» среди подписей гонки, монеты паузы"""
    for part in g.get('parts', ()):
        dx, dy = int(st.get(part['id'] + '_dx', 0)), int(st.get(part['id'] + '_dy', 0))
        if not (dx or dy):
            continue
        for t in its:
            if _part_matches(part, sc, t):
                t['dst'] = [t['dst'][0] + dx, t['dst'][1] + dy] + list(t['dst'][2:])


def _row_no(name):
    """номер строки списка в конце имени элемента (Mission3, MissionStatus3, HighlightBar3 -> 3), None — нет"""
    i = len(name)
    while i and name[i - 1].isdigit():
        i -= 1
    return int(name[i:]) if i < len(name) else None


def _bubble_variants(dl):
    """текст и плашка пузыря задания (Hud.pag Message, MissionObjective, MessageBox) в двух экземплярах: avatar — рисуется, когда у задания есть значок, plain — когда нет (viewer/ui.c ITEM_AVATAR/ITEM_PLAIN).
    Пока группы не тронуты, оба экземпляра одинаковы, вид прежний"""
    if any(t.get('variant') for t in dl):
        return
    i = 0
    while i < len(dl):
        t = dl[i]
        if t.get('page') == 'Hud.pag' and t.get('name') in ('Message', 'MissionObjective', 'MessageBox'):
            c = dict(t); c['dst'] = list(t['dst']); c['variant'] = 'plain'
            for k in ('bitmaps', 'frames', 'strings'):
                if k in t and isinstance(t[k], list): c[k] = list(t[k])
            t['variant'] = 'avatar'; dl.insert(i + 1, c); i += 1
        i += 1


BUBBLE_RIGHT = 8   # поле справа у экрана, px
BUBBLE_RADAR_GAP = 6   # №548: просвет между правым краем пузыря задания и радаром, px

def _bubble_grow(dl, delta, assets, U, text_right, variant=None):
    """рамка пузыря задания (Hud.pag MessageBox, 3-срез hslice) растёт вправо на delta, не дальше поля у края экрана; -> фактический прирост"""
    for q in dl:
        if q.get('page') == 'Hud.pag' and q.get('name') == 'MessageBox' and q['type'] == T_IMG and q.get('variant') == variant:
            x, y, w, h = q['dst']; over = max(0, text_right - (x + w))   # текст рамки уже выступает за неё на over (поле справа): рамка растёт на прирост и на это
            lim = DST_W - BUBBLE_RIGHT
            if os.environ.get('BUBBLE_FIT_OLD') != '1':   # №548 (Роб 11.10): правый край пузыря всегда левее радара не меньше чем на BUBBLE_RADAR_GAP px
                rx = [r['dst'][0] for r in dl if r.get('tag') == 'RADAR' and r['type'] == T_IMG and r['dst'][2] >= 40]
                if rx: lim = min(lim, min(rx) - BUBBLE_RADAR_GAP)
            d = max(0, min(int(delta) + over, lim - (x + w)))
            if d:
                if q.get('frames') and q.get('bitmaps'):
                    bms = []
                    for f, key in zip(q['frames'], q['bitmaps']):
                        bms.append(assets.put('%s@%dx%d_hs' % (f, w + d, h), U.hslice(assets.images[f], w + d, h)) if key.endswith('_hs') else assets.scaled(f, w + d, h))
                    q['bitmaps'] = bms
                q['dst'] = [x, y, w + d, h]
            return max(0, d - over)
    return int(delta)


def _pair_row(its, face_width, trk=0, gap=4):
    its.sort(key=lambda t: t['dst'][0])
    ws = [int(max([pix8_width(sv, trk) for sv in t['strings'] if sv] + [0])) if t.get('pix8') else int(face_width(t['font'], t['strings'])) for t in its]
    l, r = its[0]['dst'][0], max(t['dst'][0] + t['dst'][2] for t in its)
    x = int(round((l + r) / 2.0 - (sum(ws) + gap * (len(ws) - 1)) / 2.0))
    for t, w in zip(its, ws):
        t['dst'] = [x, t['dst'][1], w, t['dst'][3]]; x += w + gap


def _carried(g, sc, t):
    """элемент едет со своей строкой (rowcarry группы, правка 348)"""
    return _scr(sc, g) in g['screens'] and any(t.get('page') == pg and _name_ok(nm, t.get('name', '')) for pg, nm in g.get('rowcarry', ()))


def _text(g, st, its, dl, fonts, sc, face_width, assets, U):
    corner = bool(g.get('corner'))
    shv = st.get('sh', g.get('shadow', 0) if os.environ.get('BART_SHADOW_OLD') != '1' else None)   # №551: тёмная подложка (тень 1 px вниз-вправо, как у подписи под △): 1 — есть, 0 — нет; по умолчанию — по группе (shadow=1: надписи поверх мира)
    if shv is not None and os.environ.get('BART_SHADOW_OLD') == '1': shv = st.get('sh')
    va = st.get('va')   # правка 507: выравнивание строки по вертикали в её рамке (0 сверху, 1 по центру, 2 снизу); нет — как в раскладке экрана
    for t in its:
        x, y, w, h = t['dst']
        lh0 = _lh(fonts, t, sc)
        if va in (0, 1, 2):
            t['valign'] = ('top', 'centre', 'bottom')[int(va)]
        if shv == 1 and not t.get('sh'):
            t['sh'] = (1, 1, [0, 0, 0, 200])
        elif shv == 0 and st.get('sh') == 0:
            t['sh'] = None
        pix = engine_pix8(sc, t); font0 = t['font']
        if (t.get('pix8_default') or (pix and g['kind'] == 'face' and os.environ.get('UI_TYPO_FACEPIX_OLD') != '1')) and st.get('face') and st.get('font') != 'pix8':   # 11.10: выбранная грань сильнее пиксельной 8×8 движка (Damaged и BACK в будке)
            t['nopix8'] = True; t['pix8'] = False; pix = False
        if g['kind'] == 'pix8':
            font = st.get('font', 'pix8')
            if font and font != 'pix8':                     # грань пакета вместо pix8 (res[0] бит 4)
                t['font'] = font; t['nopix8'] = True; t['pix8'] = False; pix = False
        elif st.get('font') == 'pix8':                      # строка гранью пакета -> пиксельная 8×8 движка (res[0] бит 3)
            t['pix8'] = True; pix = True
        if pix and st.get('size') and int(st['size']) != 8 and os.environ.get('UI_TYPO_SIZE_OLD') != '1':
            size = max(1, min(64, int(st['size'])))
            scale = size / 8 / U.glyph_scale(fonts.fonts['pix8_8'], 'psp')
            t['font'] = 'pix8_8*%.8f' % scale
            if st.get('trk'): t['font'] += '+' + str(int(st['trk']))
            if st.get('lead'): t['font'] += '^' + str(int(st['lead']))
            t['nopix8'] = True; t['pix8'] = False; pix = False
        if pix:
            r = typo_res(st.get('trk', 0), st.get('lead', 0), st.get('padT', 0), st.get('padL', 0), st.get('padR', 0))
            if r:
                t['typo_res'] = r
        face = st.get('face')
        if face and not pix and face != t['font']:
            t['font'] = face
        if not pix and t['font'] != font0:                  # новая грань — в атлас только её буквы (страница шрифтов одна, 512x512):
            t['subset'] = True                              # строки элемента; текст из кода — весь ASCII, число — цифры
            if t.get('field') in CODE_FIELDS or t.get('field', '').startswith('ROW') or (sc in SAVES and t.get('menu', -1) >= 0):
                t['subset_chars'] = ASCII
            elif t.get('fmt') in ('int', 'frac', 'pct', 'time'):
                t['subset_chars'] = '0123456789/:%-'
        if not pix and g.get('pads') and (st.get('padT') or st.get('padL') or st.get('padR')):
            t['typo_res'] = typo_res(0, 0, st.get('padT', 0), st.get('padL', 0), st.get('padR', 0))
        if g.get('pads') and st.get('padN'):
            t['typo_res1'] = typo_res1(st['padN'])
        lh1 = _lh(fonts, t, sc)
        if lh1 != lh0 and t.get('valign', 'top') == 'top' and not t.get('wrap'):
            y += (lh0 - lh1) // 2                           # центр одной строки на месте
        nw = st.get('w')
        if nw is not None and int(nw) != w:
            nw = int(nw)
            if t.get('halign') == 'centre':
                x += (w - nw) // 2
            elif t.get('halign') == 'right':
                x += w - nw
            if g['id'].startswith('hud_bubble') and nw > w and os.environ.get('UI_TYPO_BUBBLEW_OLD') != '1':
                nw = w + _bubble_grow(dl, nw - w, assets, U, x + w, g.get('variant'))       # Роб 11.10: «ширина строки» растит и рамку пузыря (конусы-полукруги не искажаются), в пределах экрана
            w = nw
        if not corner:
            x += int(st.get('dx', 0)); y += int(st.get('dy', 0))
        t['dst'] = [x, y, w, h]
    if g['id'] == 'phone_damage' and len(its) > 1 and os.environ.get('UI_TYPO_PAIRROW_OLD') != '1':
        _pair_row(its, face_width, int(st.get('trk', 0)))   # 11.10: «DAMAGED» и «90%» снова подряд по ширине выбранной грани (Bart шире pix8 — налезали)
    p = st.get('pitch')
    if g.get('pitch') and p:
        ys = sorted({t['dst'][1] for t in its})
        n = len(ys)
        if n >= 2:
            p0 = int(round((ys[-1] - ys[0]) / (n - 1))); p = int(p)
            # №856 (Роб 20.10: «в меню паузы сверху и снизу очень странные большие отступы — больше межстрок или крупнее»): рамка паузы миссии
            # постоянная, на PAUSE_MIN_ROWS строк паузы свободной езды (правка 232), а пунктов в ней 5 — поля были по 49 px против 25 у паузы.
            # Пункты паузы миссии занимают ту же высоту, что 7 строк паузы: первая и последняя строки на тех же местах, поля как у паузы,
            # между строками больше воздуха (шаг 26 · 6 / 4 = 39). UI_TYPO_PAUSEMSN856_OLD=1 — шаг панели как у паузы
            if g['id'] == 'pause_items' and sc == 'pausemsn' and os.environ.get('UI_TYPO_PAUSEMSN856_OLD') != '1':
                rows_ = int(getattr(U, 'PAUSE_MIN_ROWS', 7) or 7)
                if 2 <= n < rows_: p = int(round(p * (rows_ - 1) / (n - 1)))
            if p != p0:
                y0 = ys[0] + ((n - 1) * (p0 - p)) // 2
                row = {yy: i for i, yy in enumerate(ys)}
                dy = {}
                for t in its:
                    ny = y0 + row[t['dst'][1]] * p
                    k = _row_no(t['name'])
                    if k is not None:
                        dy[k] = ny - t['dst'][1]
                    t['dst'] = [t['dst'][0], ny] + list(t['dst'][2:])
                for q in dl:                                # правка 348: значок и полоса строки k — за строкой k (rowcarry)
                    k = _row_no(q.get('name', ''))
                    if k in dy and _carried(g, sc, q):
                        q['dst'] = [q['dst'][0], q['dst'][1] + dy[k]] + list(q['dst'][2:])
    if corner:
        icons = _corner(its, dl, face_width, fonts, int(st.get('trk', 0)))
        if g.get('align') and os.environ.get('UI_TYPO_ALIGN_OLD') != '1':
            _align(its, dl, g['align'], face_width, int(st.get('trk', 0)), fonts, sc)
        if g.get('btn'):
            _btn_rule(its, dl, fonts, sc)                   # №555: общее правило кнопки (значок + подпись)
        dx, dy = int(st.get('dx', 0)), int(st.get('dy', 0))
        k = float(st.get('icon_k', 1.0))
        for t in its:
            t['dst'] = [t['dst'][0] + dx, t['dst'][1] + dy] + list(t['dst'][2:])
        for q in icons:                                     # значки подсказок: тот же сдвиг, свой масштаб от центра
            if abs(k - 1.0) > 1e-9:
                _scale_img(q, k, assets, U.hslice)
            q['dst'] = [q['dst'][0] + dx, q['dst'][1] + dy] + list(q['dst'][2:])
    if g.get('btn') and not corner:
        _btn_rule(its, dl, fonts, sc, int(st.get('dy', 0)))   # №555: кнопки поверх мира (разговор, карточка); dy группы — добавка к центру
    _parts(g, st, its, sc)


# Правило подписей кнопок (Роб 11.10: на промежуточных экранах Accept «гуляет»). Где в оригинале (раскладка без настроек) кнопки на одном месте — после настроек шрифта они
# на одном месте тоже: align = dict(right=R, bottom=B) — правый край подписи (по её ширине в выбранной грани) на R от правого края экрана, нижняя строка на B от нижнего,
# строки выше — через pitch; align = dict(x=X, y=Y) — левый верх нижней строки в точке. Значок едет с подписью. UI_TYPO_ALIGN_OLD=1 — без правила
def _align(its, dl, al, face_width, trk=0, fonts=None, sc=None):
    ys = sorted({t['dst'][1] for t in its}, reverse=True)       # строки подсказок: нижняя первая (Accept над Back)
    pitch = int(al.get('pitch', 20))
    for t in its:
        x, y, w, h = t['dst']; i = ys.index(y)
        lh = _lh(fonts, t, sc) if fonts is not None else h
        nw = int(max([pix8_width(sv, trk) for sv in t['strings'] if sv] + [0])) if t.get('pix8') else int(face_width(t['font'], t['strings']))
        if 'right' in al:
            nx, cy = DST_W - int(al['right']) - nw, DST_H - int(al['bottom']) - HINT_LINE / 2.0 - i * pitch
        else:
            nx, cy = int(al['x']), int(al['y']) + HINT_LINE / 2.0 - i * pitch
        ny = int(math.floor(cy - lh / 2.0 + 0.5))           # центр строки подписи — на опорной линии при любой высоте шрифта, значок по той же линии
        for q in dl:
            if q.get('page') == t.get('page') and q['type'] == T_IMG:
                q['dst'] = [q['dst'][0] + nx - x, int(math.floor(cy - q['dst'][3] / 2.0 + 0.5))] + list(q['dst'][2:])
        t['dst'] = [nx, ny, nw, h]


# №555 (Роб 11.10): общее правило кнопки «значок + подпись» для всех экранов (Accept, Back, Start Mission, Cancel, Skip, Continue): середина ЗАГЛАВНЫХ подписи — на середине значка по
# вертикали (не середина строки грани: у разных граней заглавная стоит в строке по-разному), зазор значок -> подпись один (BTN_GAP px; двигается значок, подпись и край группы на месте).
# Группы: ключ btn=True. BTN_ALIGN_OLD=1 — прежнее (середина строки, зазор как в раскладке экрана)
BTN_GAP = 4
_CAPMID = {}


def _cap_mid(fonts, t, sc):
    """середина заглавных от верха строки грани, px (pix8 — строки 0..6 буквы H msx)"""
    if engine_pix8(sc, t):
        return 3.5
    face = t['font']
    if face not in _CAPMID:
        bm = fonts.get(face)['glyphs'][ord('H')]['bm']
        rows = [i for i in range(bm.shape[0]) if bm[i].max() >= .5] if bm is not None else []
        _CAPMID[face] = (rows[0] + rows[-1] + 1) / 2.0 if rows else fonts.get(face)['lh'] / 2.0
    return _CAPMID[face]


def _btn_rule(its, dl, fonts, sc, dy=0):
    if os.environ.get('BTN_ALIGN_OLD') == '1':
        return
    for t in its:
        icons = [q for q in dl if q.get('page') == t.get('page') and q['type'] == T_IMG and (t.get('page') in HINT_PAGES or q.get('name') == t.get('name'))]
        if not icons:
            continue
        x, y, w, h = t['dst']; lh = _lh(fonts, t, sc)
        for q in icons:
            qx, qy, qw, qh = q['dst']
            cy = qy + qh / 2.0
            t['dst'] = [x, int(math.floor(cy - _cap_mid(fonts, t, sc) + 0.5)) + dy, w, lh]; t['valign'] = 'top'   # dy группы — ручная добавка поверх правила (0 — ровно по центру)
            q['dst'] = [x - BTN_GAP - qw, qy, qw, qh]


def _corner(its, dl, face_width, fonts, trk=0):
    """подсказки кнопок в правом нижнем углу: подпись шире/уже — её группа (значок + подпись) и группы левее сдвигаются, правый
    край строки на месте (как tools/ui_export.py hints_to_corner). -> значки подсказок экрана"""
    units = []
    for t in its:
        icon = [q for q in dl if q.get('page') == t.get('page') and q['type'] == T_IMG]
        units.append((t, icon))
    icons = [q for _, ic in units for q in ic]
    right = max(t['dst'][0] + t['dst'][2] for t, _ in units) if units else 0
    if right < DST_W - SAFE_X - 1:
        return icons                                        # не угол экрана (окно сообщения, слоты): подпись растёт вправо
    acc = 0
    for t, icon in sorted(units, key=lambda u: -u[0]['dst'][0]):
        x, y, w, h = t['dst']
        nw = int(max([pix8_width(sv, trk) for sv in t['strings'] if sv] + [0])) if t.get('pix8') else int(face_width(t['font'], t['strings']))
        acc += nw - w
        t['dst'] = [x - acc, y, nw, h]
        for q in icon:
            q['dst'] = [q['dst'][0] - acc] + list(q['dst'][1:])
    return icons


def pix8_width(s, trk=0):
    import ui_export as U
    return sum(max(1, U.pix8_adv(ch) + trk) for ch in s)


HUD_STACK_TAGS = ('TIMER', 'PARTIME', 'LAP', 'POSITION', 'COLLECTIBLES')   # оверлеи миссии стопкой вниз (viewer/ui.c OV_ORDER), шаг — высота числа


def _num(g, st, its, assets, num_glyphs, S, sc):
    gk, trk = st.get('gk'), int(st.get('trk', 0))
    dx, dy = int(st.get('dx', 0)), int(st.get('dy', 0))
    for t in its:
        if gk is not None and abs(float(gk) - t.get('glyph_k', 1.0)) > 1e-9:
            old_k = t.get('glyph_k', 1.0)
            t['_glyphs0'] = t.get('glyphs')
            if ((sc == 'hud' and t.get('tag') in HUD_STACK_TAGS) or (sc in PAUSES and t.get('tag') == 'COINS' and os.environ.get('UI_TYPO_PAUSECOINS_OLD') != '1')) and os.environ.get('UI_TYPO_STACK_OLD') != '1':   # №530: монеты паузы тоже растут и убывают от центра рамки
                # Правка Роба 507/аудит (10.10, «4th» налезает на «1/3»): стопка оверлеев шагает по высоте рамки числа, а цифры крупнее — рамка прежняя; рамка растёт
                # вместе с буквами от своего центра (число в рамке по центру — на месте), следующий оверлей встаёт ниже. UI_TYPO_STACK_OLD=1 — прежнее
                x_, y_, w_, h_ = t['dst']; nh = int(math.floor(h_ * float(gk) / old_k + 0.5))
                t['dst'] = [x_, y_ - (nh - h_) // 2, w_, nh]
            t['glyph_k'] = float(gk)
            t['glyphs'], pre = num_glyphs(t, S, assets)
            if pre:
                t['prefix'] = pre
        if trk:                                             # межбуквенное спрайтового шрифта: шаг каждой буквы (таблица шрифта пакета)
            t['glyphs'] = {ch: (gl[0], gl[1], gl[2], gl[3], max(1, gl[4] + trk)) for ch, gl in t['glyphs'].items()}
        if dx or dy:
            t['dst'] = [t['dst'][0] + dx, t['dst'][1] + dy] + list(t['dst'][2:])
    _parts(g, st, its, sc)


def _scale_img(t, k, assets, hslice):
    x, y, w, h = t['dst']
    w1, h1 = max(1, int(round(w * k))), max(1, int(round(h * k)))
    x = int(math.floor(x + w / 2.0 - w1 / 2.0 + .5)); y = int(math.floor(y + h / 2.0 - h1 / 2.0 + .5))
    if t.get('frames') and t.get('bitmaps'):
        bms = []
        for f, key in zip(t['frames'], t['bitmaps']):
            if key.endswith('_hs'):
                bms.append(assets.put('%s@%dx%d_hs' % (f, w1, h1), hslice(assets.images[f], w1, h1)))
            else:
                bms.append(assets.scaled(f, w1, h1))
        t['bitmaps'] = bms
    t['dst'] = [x, y, w1, h1]


def _carry_font(t, k, fonts, U):
    # №318: rerasterise the disc face; never stretch a label bitmap
    core, caret, lead = t['font'].partition('^')
    core, plus, trk = core.partition('+')
    base = core.split('*')[0].split('@')[0]
    mul = fonts.get(t['font'])['scale'] * k / U.glyph_scale(fonts.fonts[base], fonts.mode)
    t['font'] = base + '*' + ('%.4f' % mul).rstrip('0').rstrip('.') + (plus + trk) + (caret + lead)
    t['subset'] = True
    x, y, w, h = t['dst']; w1, h1 = max(1, round(w * k)), max(1, round(h * k))
    t['dst'] = [int(math.floor(x + (w - w1) / 2 + .5)), int(math.floor(y + (h - h1) / 2 + .5)), w1, h1]


def _img(g, st, its, dl, assets, hslice, sc, fonts, U):
    k = float(st.get('k', 1.0)); dx, dy = int(st.get('dx', 0)), int(st.get('dy', 0))
    if g.get('block') and abs(k - 1.0) > 1e-9:              # блок (звёзды лавки): размеры x k и промежутки x k от центра блока
        x0 = min(t['dst'][0] for t in its); y0 = min(t['dst'][1] for t in its)
        x1 = max(t['dst'][0] + t['dst'][2] for t in its); y1 = max(t['dst'][1] + t['dst'][3] for t in its)
        cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
        def move(t):
            x, y, w, h = t['dst']; mx, my = x + w / 2.0, y + h / 2.0
            nx, ny = cx + (mx - cx) * k, cy + (my - cy) * k
            t['dst'] = [int(math.floor(nx - w / 2.0 + .5)), int(math.floor(ny - h / 2.0 + .5)), w, h]
        for t in its:
            _scale_img(t, k, assets, hslice); move(t)
        for cid in g.get('carry', ()):
            for t in dl:
                if matches(GROUP[cid], sc, t):
                    if g.get('carryfont') and os.environ.get('TYPO_318_OLD') != '1':
                        _carry_font(t, k, fonts, U)
                    move(t)
    elif abs(k - 1.0) > 1e-9:
        for t in its:
            _scale_img(t, k, assets, hslice)
    if dx or dy:
        for t in its:
            t['dst'] = [t['dst'][0] + dx, t['dst'][1] + dy] + list(t['dst'][2:])
        for cid in g.get('carry', ()):
            for t in dl:
                if matches(GROUP[cid], sc, t):
                    t['dst'] = [t['dst'][0] + dx, t['dst'][1] + dy] + list(t['dst'][2:])


def _move(g, st, its):
    dx, dy = int(st.get('dx', 0)), int(st.get('dy', 0))
    for t in its:
        t['dst'] = [t['dst'][0] + dx, t['dst'][1] + dy] + list(t['dst'][2:])


def _board(g, st, its, assets, U):
    """рамка доски шире/выше на gw/gh px с каждой стороны: углы наружу, края длиннее (кусок края диска заново), заливка больше"""
    gw, gh = int(st.get('gw', 0)), int(st.get('gh', 0))
    if not (gw or gh):
        return
    for t in its:
        x, y, w, h = t['dst']; nm = t['name']
        sx = -gw if 'Left' in nm else (gw if 'Right' in nm else 0)
        sy = -gh if 'Top' in nm else (gh if 'Bottom' in nm else 0)
        if nm in ('Frame_Top', 'Frame_Bottom'):
            x, w = x - gw, w + 2 * gw; y += sy
        elif nm in ('Frame_Left', 'Frame_Right'):
            y, h = y - gh, h + 2 * gh; x += sx
        elif nm == 'Frame_Bgd':
            x, y, w, h = x - gw, y - gh, w + 2 * gw, h + 2 * gh
        else:                                               # углы
            x += sx; y += sy
        if t['type'] == T_IMG and t.get('bitmaps') and t['bitmaps'][0].endswith('_edge') and (w, h) != tuple(t['dst'][2:]):
            f = t['frames'][0]
            t['bitmaps'] = [assets.put('%s@%dx%d_edge' % (f, w, h), U.resample(U.edge_slice(assets.images[f], nm), w, h))]
        t['dst'] = [x, y, w, h]


def _shift_item(it, dx, dy):
    """сдвиг узла пакета на (dx, dy) px: рамка, ось поворота, рамка внешнего значка, слоты"""
    for key in ('dst', 'extframe'):
        if it.get(key):
            it[key] = [it[key][0] + dx, it[key][1] + dy] + list(it[key][2:])
    if it.get('pivot'):
        it['pivot'] = [it['pivot'][0] + dx, it['pivot'][1] + dy]
    if it.get('slots'):
        it['slots'] = [(b, [d[0] + dx, d[1] + dy] + list(d[2:])) for b, d in it['slots']]


def _scale_img_xy(t, kx, ky, assets, hslice):
    x, y, w, h = t['dst']
    w1, h1 = max(1, int(round(w * kx))), max(1, int(round(h * ky)))
    x = int(math.floor(x + w / 2.0 - w1 / 2.0 + .5)); y = int(math.floor(y + h / 2.0 - h1 / 2.0 + .5))
    if t.get('frames') and t.get('bitmaps'):
        bms = []
        for f, key in zip(t['frames'], t['bitmaps']):
            bms.append(assets.put('%s@%dx%d_hs' % (f, w1, h1), hslice(assets.images[f], w1, h1)) if key.endswith('_hs') else assets.scaled(f, w1, h1))
        t['bitmaps'] = bms
    t['dst'] = [x, y, w1, h1]


def _scale_text(t, k, fonts, U, sc):
    """№574 срез 2b-2: масштаб одной надписи (грань пакета перерастрируется x k, центр строки на месте; зеркало elTextScale в панели)"""
    lh0 = _lh(fonts, t, sc)
    font0 = t['font']
    if engine_pix8(sc, t):
        t['font'] = 'pix8_8*%.8f' % (k / U.glyph_scale(fonts.fonts['pix8_8'], 'psp'))
        t['nopix8'] = True; t['pix8'] = False
    else:
        core, caret, lead = t['font'].partition('^')
        core, plus, trk = core.partition('+')
        base = core.split('*')[0].split('@')[0]
        mul = fonts.get(t['font'])['scale'] * k / U.glyph_scale(fonts.fonts[base], fonts.mode)
        t['font'] = base + '*' + ('%.4f' % mul).rstrip('0').rstrip('.') + (plus + trk) + (caret + lead)
    if t['font'] != font0:
        t['subset'] = True
        if t.get('field') in CODE_FIELDS or t.get('field', '').startswith('ROW') or (sc in SAVES and t.get('menu', -1) >= 0):
            t['subset_chars'] = ASCII
        elif t.get('fmt') in ('int', 'frac', 'pct', 'time'):
            t['subset_chars'] = '0123456789/:%-'
    lh1 = _lh(fonts, t, sc)
    if lh1 != lh0 and t.get('valign', 'top') == 'top' and not t.get('wrap'):
        x, y, w, h = t['dst']
        t['dst'] = [x, y + (lh0 - lh1) // 2, w, h]


def _elements(sc, dl, elements, assets=None, U=None, fonts=None):
    """№574 (вторая версия): настройки отдельных элементов, ключ «экран|вид|имя» (вид: text | num | img) -> dx, dy, k (масштаб; у надписи — размер шрифта); после групп (зеркало typo_panel_page.html applyTypo)"""
    kinds = {T_TEXT: 'text', T_NUM: 'num', T_RECT: 'rect'}
    for key, e in (elements or {}).items():
        parts = key.split('|', 2)
        if len(parts) != 3 or parts[0] != sc or not isinstance(e, dict):
            continue
        dx, dy = int(e.get('dx', 0)), int(e.get('dy', 0))
        k, kw, kh = float(e.get('k', 1.0)), float(e.get('w', 1.0)), float(e.get('h', 1.0))
        if not (dx or dy or abs(k - 1) > 1e-9 or abs(kw - 1) > 1e-9 or abs(kh - 1) > 1e-9):
            continue
        for it in dl:
            if it.get('name') != parts[2] or kinds.get(it['type'], 'img') != parts[1]:
                continue
            if parts[1] == 'img' and assets is not None and (abs(k * kw - 1) > 1e-9 or abs(k * kh - 1) > 1e-9):
                _scale_img_xy(it, k * kw, k * kh, assets, U.hslice)       # масштаб, ширина и высота картинки/плашки (зеркало scaleRect2 в панели)
            elif parts[1] == 'num' and assets is not None and abs(k - 1) > 1e-9:
                it['glyph_k'] = it.get('glyph_k', 1.0) * k                 # масштаб надписи спрайтовым шрифтом: картинки букв x k
                it['glyphs'], pre = U.num_glyphs(it, U.S, assets)
                if pre:
                    it['prefix'] = pre
            elif parts[1] == 'rect' and (abs(k * kw - 1) > 1e-9 or abs(k * kh - 1) > 1e-9) and os.environ.get('UI_TYPO_FLAT_OLD') != '1':
                x, y, w, h = it['dst']                                   # №574 2b-3: плоская плашка — масштаб, ширина, высота от центра (зеркало scaleRect2 в панели)
                w1, h1 = max(1, round(w * k * kw)), max(1, round(h * k * kh))
                it['dst'] = [int(math.floor(x + w / 2.0 - w1 / 2.0 + .5)), int(math.floor(y + h / 2.0 - h1 / 2.0 + .5)), w1, h1]
            elif parts[1] == 'text' and fonts is not None and abs(k - 1) > 1e-9 and os.environ.get('UI_TYPO_ELTEXT_OLD') != '1':
                _scale_text(it, k, fonts, U, sc)                          # №574 2b-2: размер шрифта надписи x k (откат UI_TYPO_ELTEXT_OLD=1)
            if dx or dy:
                _shift_item(it, dx, dy)


BRIEF_GAP = 5   # №632: зазор значка ✕ и слова Continue, px


def _brief_center(sc, dl, fonts, gs, U):
    """№632 (Роб 15.10): на карточке миссии (все виды, экран missionload) LOADING — центр самой надписи, Continue — центр блока «значок ✕ + зазор 5 px + слово» — ровно по центру карточки
    (центр синей основы background); dx групп brief_btn / brief_loading — сдвиг поверх центра. BRIEF_CENTER_OLD=1 — прежние места (зеркало briefCenter в typo_panel_page.html)"""
    if sc != 'missionload' or os.environ.get('BRIEF_CENTER_OLD') == '1':
        return
    bg = next((t for t in dl if t['type'] == T_RECT and t['name'] == 'background'), None)
    if not bg:
        return
    cx = bg['dst'][0] + bg['dst'][2] / 2.0

    def gdx(gid):
        st = gs.get(gid)
        k = GROUP.get(gid, {})
        while st is None and k.get('inherit'):
            st = gs.get(k['inherit']); k = GROUP.get(k['inherit'], {})
        return int((st or {}).get('dx', 0))
    for t in dl:
        if t['type'] == T_TEXT and t['name'] == 'BriefLoading':
            x, y, w, h = t['dst']; t['halign'] = 'centre'
            t['dst'] = [int(math.floor(cx + gdx('brief_loading') - w / 2.0 + .5)), y, w, h]
    txt = next((t for t in dl if t['type'] == T_TEXT and t['name'] == 'Continue'), None)
    ic = next((q for q in dl if q['type'] == T_IMG and q['name'] == 'Continue'), None)
    if txt and ic and txt.get('strings'):
        ww = int(U.face_width(txt['font'], txt['strings'])); qx, qy, qw, qh = ic['dst']
        x0 = int(math.floor(cx + gdx('brief_btn') - (qw + BRIEF_GAP + ww) / 2.0 + .5))
        ic['dst'] = [x0, qy, qw, qh]
        x, y, w, h = txt['dst']; txt['halign'] = 'left'; txt['dst'] = [x0 + qw + BRIEF_GAP, y, ww, h]


def apply(sc, dl, assets, fonts, cfg=None, U=None):
    """настройки панели на список рисования экрана sc (после раскладки, пиксели PSP). Без настроек — dl как есть"""
    if cfg is None:
        cfg = load()
    if not cfg:
        return dl
    if U is None:
        import ui_export as U
    gs = cfg.get('groups') or {}
    if sc == 'hud' and os.environ.get('UI_TYPO_BUBBLEVAR_OLD') != '1':
        _bubble_variants(dl)
    for g in GROUPS:
        st = gs.get(g['id'])
        k = g
        while st is None and k.get('inherit'):   # правка 10.10: новая группа берёт значения прежней (по цепочке), пока её не трогали; {} — явный сброс группы, не наследует
            st = gs.get(k['inherit'])
            k = GROUP.get(k['inherit'], {})
        if not st:
            continue
        its = [t for t in dl if matches(g, sc, t)]
        if not its:
            continue
        if g['kind'] in ('face', 'pix8'):
            _text(g, st, its, dl, fonts, sc, U.face_width, assets, U)
        elif g['kind'] == 'num':
            _num(g, st, its, assets, U.num_glyphs, U.S, sc)
        elif g['kind'] == 'img':
            _img(g, st, its, dl, assets, U.hslice, sc, fonts, U)
        elif g['kind'] == 'move':
            _move(g, st, its)
        elif g['kind'] == 'board':
            _board(g, st, its, assets, U)
    if sc == 'hud' and os.environ.get('UI_TYPO_STACK_OLD') != '1':
        # «th» у места стоит отдельной строкой справа от цифры данных; цифра выросла — подпись уходит вправо на прирост ширины цифры «4» (правка 507)
        for t in dl:
            g0 = t.get('_glyphs0')
            if t.get('name') == 'Position' and g0 and '4' in g0 and '4' in t.get('glyphs', {}):
                d = t['glyphs']['4'][4] - g0['4'][4]
                if d:
                    for q in dl:
                        if q.get('name') == 'PositionOrdinal':
                            q['dst'] = [q['dst'][0] + d] + list(q['dst'][1:])
    _elements(sc, dl, cfg.get('elements'), assets, U, fonts)   # №574: настройки отдельных элементов — самыми последними
    _brief_center(sc, dl, fonts, gs, U)                         # №632: Continue и LOADING по центру карточки (после элементов: ширина значка уже с масштабом)
    return dl


def prune(screens, assets):
    """картинки, на которые после настроек никто не ссылается (прежние размеры), — вон из пакета"""
    used = set()
    for dl in screens.values():
        for t in dl:
            used.update(t.get('bitmaps') or [])
            if t.get('prefix'):
                used.add(t['prefix'])
            for gl in (t.get('glyphs') or {}).values():
                if gl[0]:
                    used.add(gl[0])
            for b, _ in t.get('slots') or []:
                used.add(b)
            for ic in t.get('icons') or []:
                if ic:
                    used.add(ic['bitmap'])
    for k in [k for k in assets.bitmaps if k not in used]:
        del assets.bitmaps[k]


if __name__ == '__main__':
    import sys
    if len(sys.argv) > 1 and sys.argv[1] == 'groups':      # для панели и проверки: группы одним JSON
        print(json.dumps(GROUPS, ensure_ascii=False, indent=1))
