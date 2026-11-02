#!/usr/bin/env python3
"""№1121 (подписи): подпись никогда не под картинкой и не поверх неё — оглавление (CARDS и счётчик справа от картинки карточек), просмотр костюма и машины (подпись ниже рисунка, все 7 уровней),
просмотр миссии (название ниже рамки, номер выше). Размеры берутся по реальным картинкам диска каждого уровня. SCRAPCAPS1121_OLD=1 — красный контроль (обязан упасть)."""
import os, sys
import numpy as np
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import menu_export as M
import ui_export as U

fails = 0; total = 0
def chk(ok, msg):
    global fails, total
    total += 1; fails += not ok; print(('PASS ' if ok else 'FAIL ') + msg)

for sc in M.PACKS['scrapgal'] + M.PACKS['scrap']: U.SCREEN_PAGES[sc] = M.PAGES[sc]
S = U.S
def px(r): return [r[0] * S + 240 - 320 * S, r[1] * S, r[2] * S, r[3] * S]   # центр экрана: 240 px = 320 единиц
def prep(sc, its):
    M.scrap_fonts(sc, its)
    if not M.SCRAPCAPS1121_OLD: M.scrap_captions(sc, its)
    return its
def cap_ink(it):   # верх и низ букв подписи (по всем её строкам), px экрана
    ys = [U.glyph_ink(it['font'], t) for t in it['strings'] if t]
    top = it['rect'][1] * S + min(i[2] for i in ys); bot = it['rect'][1] * S + max(i[3] for i in ys)
    return top, bot
def cap_x(it):
    xs = [U.glyph_ink(it['font'], t) for t in it['strings'] if t]
    w = max(i[1] for i in xs); cx = px(it['rect'])[0] + px(it['rect'])[2] / 2
    return cx - w / 2, cx + w / 2

for L in range(1, 8):
    for sc, view, capn in (('skingallery', 'SkinView', 'SkinName'), ('vehiclegallery', 'VehicleView', 'VehicleName')):
        its = prep(sc, M.gallery_items(sc, L)); v = next(i for i in its if i['name'] == view); cap = next(i for i in its if i['name'] == capn)
        bottoms = []
        for fr in v['frames']:
            im = U.project('frontend.p3d').images.get(fr) if isinstance(fr, str) else fr
            a = np.asarray(im.convert('RGBA'))[..., 3] > 16; ys = np.where(a.any(1))[0]; h = a.shape[0]
            bottoms.append((v['rect'][1] + v['rect'][3] * (ys[-1] + 1) / h) * S)
        top, bot = cap_ink(cap)
        chk(top >= max(bottoms) + 3 and bot <= 244, 'L%d %s: подпись %.0f..%.0f px ниже рисунка (низ рисунка до %.0f px) и в окне ТВ' % (L, sc, top, bot, max(bottoms)))
    its = prep('missiongallery', M.gallery_items('missiongallery', L)); v = next(i for i in its if i['name'] == 'MissionView')
    t = next(i for i in its if i['name'] == 'MissionTitle'); n = next(i for i in its if i['name'] == 'MissionNum')
    chk(cap_ink(t)[0] >= (v['rect'][1] + v['rect'][3]) * S + 2 and cap_ink(n)[1] <= v['rect'][1] * S - 2, 'L%d missiongallery: название ниже рамки (%.0f >= %.0f), номер выше (%.0f <= %.0f)' % (L, cap_ink(t)[0], (v['rect'][1] + v['rect'][3]) * S, cap_ink(n)[1], v['rect'][1] * S))
its = prep('scrapcontents', M.scrap_items('scrapcontents', U.build_items('scrapcontents')[0]))
pic = next(i for i in its if i['name'] == 'CollectorCards' and i['type'] == U.T_IMG); pr = px(pic['rect'])
fr0 = pic['frames'][0]; im0 = U.project('frontend.p3d').images.get(fr0) if isinstance(fr0, str) else fr0   # картинка стоит по центру рамки в натуральную величину x 0,5667 (dst пакета 65x58): берём её настоящие края
w_, h_ = im0.size[0] * S, im0.size[1] * S; pr = [pr[0] + (pr[2] - w_) / 2, pr[1] + (pr[3] - h_) / 2, w_, h_]
for it in its:
    if it['type'] == U.T_TEXT and it['name'] in ('CollectorCards', 'CollectorCards_Unlocked'):
        top, bot = cap_ink(it); x0, x1 = cap_x(it)
        apart = x0 >= pr[0] + pr[2] + 2 or top >= pr[1] + pr[3] + 2
        chk(apart, 'оглавление: %s (x %.0f..%.0f, y %.0f..%.0f) не перекрыт картинкой карточек (x %.0f..%.0f, y %.0f..%.0f)' % (it['name'], x0, x1, top, bot, pr[0], pr[0] + pr[2], pr[1], pr[1] + pr[3]))
print('%s строк %d, упало %d' % ('FAIL' if fails else 'PASS', total, fails)); sys.exit(1 if fails else 0)
