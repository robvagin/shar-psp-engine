#!/usr/bin/env python3
"""GitHub #4: подписи запасных экранов бонус-гонки (viewer/bonus_play.h: выбор трассы, итоги серии, «ещё заезд?», пауза) на выбранном языке -> bonuslabels.txt,
по строке на подпись в порядке BL_* из bonus_play.h, коды знаков шрифтов игры. Движок читает lang/<буква>/bonuslabels.txt; английские остаются в коде, пустая строка — тоже английская.
Ключи текстовой библиотеки диска: SELECT_TRACK_AND_NUM_LAPS, LAPS, PRESS_START, RACE_SUMMARY, WINS, POINTS, TOTAL_TIME, BEST_LAP, CONTINUE, QUIT_GAME, RACE_AGAIN?, YES, NO, LOADING, GO, CAMERA, места Rank0..3 итогов.
«TRACK», «CAR» и два вида камеры («сверху», «за машиной») отдельным словом на диске нет: берутся слова самого диска для этих понятий (Strecke / piste / pista, Auto / voiture / coche из «Change Track Direction», «Race Car», «Police Car»).
  LANG_BIBLE=F python3 tools/lang_bonus.py --out build/lang/F/bonuslabels.txt        (--list — показать строки)"""
import os, re, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import ui_export as U
import accents

# (метка, ключ диска или None, слово порта по языкам, приводить к заглавным)
LABELS = [
    ('TITLE',     'SELECT_TRACK_AND_NUM_LAPS', None, False),
    ('TRACK',     None, {'F': 'CIRCUIT', 'G': 'STRECKE', 'S': 'PISTA'}, False),
    ('LAPS',      'LAPS', None, False),
    ('PRESS',     'PRESS_START', None, False),
    ('STANDINGS', 'RACE_SUMMARY', None, False),
    ('CAR',       None, {'F': 'VOITURE', 'G': 'AUTO', 'S': 'COCHE'}, False),
    ('WINS',      'WINS', None, False),
    ('POINTS',    'POINTS', None, False),
    ('TOTAL',     'TOTAL_TIME', None, False),
    ('BEST',      'BEST_LAP', None, False),
    ('CONTINUE',  'CONTINUE', None, True),
    ('QUIT',      'QUIT_GAME', None, True),
    ('AGAIN',     '#818926', None, True),   # «Race again?»: имя ключа в данных диска не нашлось, строка берётся по её числу-метке в таблице
    ('YES',       'YES', None, True),
    ('NO',        'NO', None, True),
    ('LOADING',   'LOADING', None, True),
    ('GO',        'GO', None, False),
    ('CAMERA',    'CAMERA', None, False),
    ('CAMTOP',    None, {'F': 'dessus', 'G': 'oben', 'S': 'arriba'}, False),
    ('CAMBEHIND', None, {'F': 'derrière la voiture', 'G': 'hinter dem Auto', 'S': 'detrás del coche'}, False),
    # TEST90: HUD гонки на языке игры. «LAP» — единственное число слова диска LAPS (TOURS / RUNDEN / VUELTAS); NITRO в трёх языках то же слово; места — строки страницы итогов диска (1er, 2ème / 1, 2 / 1º, 2º)
    ('LAP',       None, {'F': 'TOUR', 'G': 'RUNDE', 'S': 'VUELTA'}, True),
    ('NITRO',     None, {'F': 'NITRO', 'G': 'NITRO', 'S': 'NITRO'}, True),
    ('PL1',       '#rank0', None, True),
    ('PL2',       '#rank1', None, True),
    ('PL3',       '#rank2', None, True),
    ('PL4',       '#rank3', None, True),
]

def one_line(t):
    t = t.replace('\\', ' ').replace('\r', ' ').replace('\n', ' ')
    t = re.sub(r'(?<=\w)- (?=[a-zäöüß])', '', t)   # «Gesamt- zeit» — перенос диска внутри слова: в одну строку слитно
    return re.sub(r'\s+', ' ', t).strip()

def by_hash(lang, num):
    """Строка таблицы языка по числу-метке (та же таблица, что в ui_export.load_bible)."""
    root = U.read(U.SCROOBY / 'resource/txtbible/srr2.p3d')
    for c in U.walk(root):
        if c.id == 0x1800E:
            _, p = U.string(c.data)
            if c.data[p:p + 1] != lang.encode():
                continue
            n, mod, size = struct.unpack_from('<III', c.data, p + 1); p += 13
            hs = struct.unpack_from('<%dI' % n, c.data, p); offs = struct.unpack_from('<%dI' % n, c.data, p + 4 * n)
            buf = c.data[p + 8 * n:]
            for h, o in zip(hs, offs):
                if h == num:
                    return buf[o:].decode('utf-16-le', errors='replace').split('\0')[0]
    return None

def rank_text(k):
    """строка места на странице итогов диска (MiniSummary.pag, Rank<k>) на языке LANG_BIBLE"""
    sys.path.insert(0, str(Path(__file__).resolve().parent / 'bonus'))
    import screens_export as S
    for it in S.build_items('msrow%d' % k):
        if it['name'] == 'Rank%d' % k:
            return it['strings'][0]


def lines(lang):
    B = U.load_bible(lang); out = []
    for name, key, own, up in LABELS:
        if key is None:
            t = own.get(lang, '')
        else:
            if key.startswith('#rank'):
                t = rank_text(int(key[5:]))
            else:
                t = by_hash(lang, int(key[1:])) if key.startswith('#') else B(key)
            if not t:
                sys.exit('нет ключа %s в таблице %s' % (key, lang))
        t = one_line(t)
        t = t.upper() if up else t
        if lang == 'G' and key and key.startswith('#rank'):
            t += '.'   # немецкий порядковый знак: «2.»; на диске у места только число
        out.append(t)
    return out

def main():
    L = U.LANG
    if L == 'E':
        sys.exit('английский остаётся в коде (LANG_BIBLE=F|G|S)')
    ls = lines(L)
    if '--list' in sys.argv:
        for (name, key, own, up), t in zip(LABELS, ls):
            print('%-9s %-28s %s' % (name, key or '(слово порта)', t))
        return
    out = sys.argv[sys.argv.index('--out') + 1]
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    Path(out).write_bytes(b'\n'.join(accents.font_bytes(t) for t in ls) + b'\n')

if __name__ == '__main__':
    main()
