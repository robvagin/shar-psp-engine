#!/usr/bin/env python3
"""№1121: SCRAP BOOK — все подписи нашим шрифтом bart_16bit (кегль кратный: @1 или *2), OPEN BOOK / GAME STATS по центру свободного места на обложке.
  python3 tests/scrap1105/fonts1121.py        # зелёный; SCRAPFONT1121_OLD=1 — красный контроль (обязан упасть)
Собирает пакеты scrap и scrapgal (мок-режим без файлов) и проверяет каждый текст в списках рисования всех шести экранов (в том числе Accept / Back)."""
import os, re, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import menu_export as M
import ui_export as U

fails = 0; total = 0
def chk(ok, msg):
    global fails, total
    total += 1; fails += not ok; print(('PASS ' if ok else 'FAIL ') + msg)

tmp = tempfile.mkdtemp()
shots = {}
for pack in ('scrap', 'scrapgal'):
    res = M.export(pack, os.path.join(tmp, pack + '.shr'), mocks=False)
    shots.update(res['screens'])
for sc, dl in shots.items():
    txt = [t for t in dl if t['type'] == U.T_TEXT and t['page'] != 'TVFrame.pag']
    TITLES = ('OpenBook', 'GameStats', 'Level', 'MissionNum')   # №1134: толстые заголовки страниц — прежний шрифт данных (boulder_24)
    small = [t for t in txt if t['name'] not in TITLES]; titles = [t for t in txt if t['name'] in TITLES]
    bad = [(t['name'], t.get('font')) for t in small if not re.fullmatch(r'bart_16bit(@1|\*2)', t.get('font') or '')]
    chk(small and not bad, '%s: %d мелких подписей, все bart_16bit с целым кеглем%s' % (sc, len(small), (' (не так: %s)' % bad[:3]) if bad else ''))
    chk(all(t.get('font') in ('boulder_24', None) or not t.get('font', '').startswith('bart') for t in titles), '%s: %d заголовков — прежний толстый шрифт, не bart' % (sc, len(titles)))
    hints = [t for t in txt if t['page'] in ('Accept.pag', 'Back.pag')]
    chk(hints and all(t['font'] == 'bart_16bit@1' for t in hints), '%s: Accept и Back — bart_16bit@1, как в паузе и OPTIONS' % sc)
# №1134: Accept и Back на всех экранах альбома и галерей — место и грань как в OPTIONS главного меню
res_menu = M.export('menu', os.path.join(tmp, 'menu.shr'), mocks=False)['screens']['options']
ref = {(t['name'], t['type']): (t.get('font'), t['dst']) for t in res_menu if t['page'] in ('Accept.pag', 'Back.pag')}
for sc, dl in shots.items():
    mine = {(t['name'], t['type']): (t.get('font'), t['dst']) for t in dl if t['page'] in ('Accept.pag', 'Back.pag')}
    chk(mine and all(ref[k] == v for k, v in mine.items()), '%s: Accept / Back (%d знаков) — грань и место как в OPTIONS' % (sc, len(mine)))
# центр пары OPEN BOOK / GAME STATS по буквам — в центре свободного места обложки (242; 188,5)
dl = shots['scrapbook']; pair = [t for t in dl if t['name'] in ('OpenBook', 'GameStats')]
cy, cx = [], []
for t in pair:
    ink = U.glyph_ink(t['font'], 'OPEN BOOK' if t['name'] == 'OpenBook' else 'GAME STATS')   # у boulder_24 грань данных (строка 16 px), как в пакете
    x, y, w, h = t['dst']
    cy.append((y + (ink[2] + ink[3]) / 2, y + ink[2], y + ink[3])); cx.append(x + w / 2)
mid = (min(c[1] for c in cy) + max(c[2] for c in cy)) / 2
chk(abs(mid - M.SCRAP_FREE_CY) <= 1.5, 'пара по высоте: центр букв %.1f, свободное место %.1f' % (mid, M.SCRAP_FREE_CY))
chk(all(abs(c - M.SCRAP_FREE_CX) <= 1.5 for c in cx), 'пара по ширине: центры %s, свободное место %.1f' % ([round(c, 1) for c in cx], M.SCRAP_FREE_CX))
print('%s строк %d, упало %d' % ('FAIL' if fails else 'PASS', total, fails)); sys.exit(1 if fails else 0)
