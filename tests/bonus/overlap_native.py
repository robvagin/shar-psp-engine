#!/usr/bin/env python3
"""№1182: ни одна надпись родных экранов бонус-игры (bonus/mini.shr: выбор игрока и машины, выбор трассы, итоги, пауза) не лежит поверх соседней.
Считается так же, как tests/lang1111/overlap.py: где надпись реально рисуется (выравнивание, перенос, значение из данных по прикидке), пары с пересечением больше 1,5 px
по обеим осям — пересечение. Экраны, видимые одновременно, берутся вместе (выбор: mmchar+mmtrack+mmhints; итоги: msbase+строки; пауза). Надписи с общими метками видимости,
которые на экране не бывают вместе, не считаются (разные курсоры, значения одного поля).
  LANG_BIBLE=F python3 tests/bonus/overlap_native.py [--list]     код 1 — есть пересечения"""
import os, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'tools/bonus'))
import ui_export as U
import screens_export as S
import ui_typo as UT
TCFG = UT.load()

GROUPS = {'выбор игрока': ['mmchar', 'mmhints'], 'выбор трассы': ['mmtrack', 'mmhints'], 'итоги': ['msbase', 'msrow0'], 'пауза': ['minipause']}
for k in range(1, 4):
    GROUPS['итоги строка %d' % (k + 1)] = ['msbase', 'msrow%d' % k]

def ink():
    fc = U.FontCache('psp'); assets = S.mg_assets(); scr = {}
    for sc in S.SCREENS:
        dl = S.build(sc, assets, fc)
        if TCFG:   # настройки панели типографики Роба (грани, сдвиги) — как при сборке пакета, после раскладки экрана
            dl = UT.apply(sc, dl, assets, fc, TCFG)
        scr[sc] = dl
    S.post(scr, fc)   # раскладка после панели, как в пакете (spec post)
    res = {}
    for g, scs in GROUPS.items():
        allv = []
        for sc in scs:
            dl = [dict(t) for t in scr[sc]]
            for t in dl:   # значение из данных: прикидка «5 знаков» ui_export для узких столбцов даёт ложные пересечения — берём реальную ширину (числа до двух знаков)
                if t.get('type') == U.T_TEXT and t.get('field', 'NONE') != 'NONE':
                    t['field'] = 'NONE'
                    if t['name'].rstrip('0123456789') in ('NumWins', 'NumPoints'): t['strings'] = ['00']
            for t, x0, x1, y0, y1 in U.text_ink(dl, fc):
                allv.append((sc, t, x0, x1, y0, y1))
        res[g] = allv
    return res

def main():
    n = 0; lst = []
    for g, v in ink().items():
        for i, (sa, a, ax0, ax1, ay0, ay1) in enumerate(v):
            for (sb, b, bx0, bx1, by0, by1) in v[i + 1:]:
                if sa == sb and a['name'] == b['name']:
                    continue
                if a.get('tag') == S.WAIT_TAG or b.get('tag') == S.WAIT_TAG:
                    continue   # «Waiting for other players...» и число: после №1182 не показываются (BONUSWAIT1182_OLD=1 — как было)
                if a.get('tag') != b.get('tag') and a.get('tag') in S.CUR_TAGS + S.TRACK_TAGS and b.get('tag') in S.CUR_TAGS + S.TRACK_TAGS:
                    continue   # разные курсоры одного ряда: на экране виден один
                dx, dy = min(ax1, bx1) - max(ax0, bx0), min(ay1, by1) - max(ay0, by0)
                if dx > 1.5 and dy > 1.5:
                    n += 1; lst.append('%s | %s «%s» / %s «%s»: %.1f x %.1f' % (g, a['name'], (a.get('strings') or [''])[0][:28], b['name'], (b.get('strings') or [''])[0][:28], dx, dy))
    for l in lst:
        print('ПЕРЕСЕЧЕНИЕ', l)
    print('язык', U.LANG, 'пересечений', n)
    return 1 if n else 0

if __name__ == '__main__':
    sys.exit(main())
