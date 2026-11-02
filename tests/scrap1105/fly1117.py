#!/usr/bin/env python3
"""№1117: числа полёта картинки просмотра в viewer/main_scrap.h (scrapFlyCall) = раскладка пакета галерей (слоты и место просмотра в пикселях PSP, после подъёма костюма №1121),
и математика полёта: центр картинки при s = s0 в центре слота, при s = 1 в месте просмотра, ход линейный. Красный контроль: --wrong (сдвиг таблицы на 3 px) обязан упасть."""
import os, re, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import menu_export as M
import ui_export as U
fails = 0; total = 0
def chk(ok, msg):
    global fails, total
    total += 1; fails += not ok; print(('PASS ' if ok else 'FAIL ') + msg)
src = open(os.path.join(ROOT, 'viewer/main_scrap.h'), encoding='utf-8').read()
body = src[src.index('static void scrapFlyCall'):src.index('uiScrFly(1, UI_F_ROW0 + 8')]
wrong = 3.0 if '--wrong' in sys.argv else 0.0
# из кода: kind0 / kind1 / kind2 — числа слотов и просмотра
def nums(txt): return [float(x) for x in re.findall(r'(\d+\.\d+|\d+)f', txt)]
k0 = nums(body[body.index('if (kind == 0)'):body.index('else if (kind == 1)')])
k1_ = body[body.index('else if (kind == 1)'):body.index('else { static')]
k2_ = body[body.index('else { static'):]
res = {}
tmp = tempfile.mkdtemp()
res.update(M.export('scrapgal', os.path.join(tmp, 'g.shr'), mocks=False)['screens'])
def dst(sc, name): return next(t['dst'] for t in res[sc] if t['name'] == name)
# числа берём ИЗ КОДА (выражения scrapFlyCall считаются здесь) и сверяем с пакетом: слоты и место просмотра, центр и ширина
def code_vals(kind, sel):
    seg = {0: body[body.index('if (kind == 0)'):body.index('else if (kind == 1)')], 1: body[body.index('else if (kind == 1)'):body.index('else { static')], 2: body[body.index('else { static'):]}[kind]
    env = {'sel': sel, 'X': [float(x) for x in re.findall(r'(\d+\.\d*)f', re.search(r'X\[3\] = \{([^}]*)\}', seg).group(1))] if 'X[3]' in seg else None}
    out = {}
    for k in ('sx', 'sy', 'sw', 'vx', 'vy', 'vw'):
        e = re.search(k + r' = ([^;]*);', seg).group(1); e = re.sub(r'(\d+\.\d*)f', r'\1', e).replace('/', '//') if False else re.sub(r'(\d+\.\d*)f', r'\1', e).replace(' / ', ' // ')   # деление целых, как в C
        out[k] = eval(e, {}, env) + (wrong if k in ('sx', 'vx') else 0)
    return out
cases = (('missiongallery', 0, 'Mission%d', 'MissionView', range(8)), ('skingallery', 1, 'Skin%d', 'SkinView', range(3)), ('vehiclegallery', 2, 'Vehicle%d', 'VehicleView', range(6)))
for sc, kind, slotn, viewn, sels in cases:
    v = dst(sc, viewn)
    for i in sels:
        d = dst(sc, slotn % i); c = code_vals(kind, i)
        ok = abs(d[0] + d[2] / 2 - c['sx']) <= 1.5 and abs(d[1] + d[3] / 2 - c['sy']) <= 1.5 and abs(d[2] - c['sw']) <= 1
        chk(ok, '%s: слот %d — пакет центр (%.1f,%.1f) ширина %d, код (%.1f,%.1f) %d' % (sc, i, d[0] + d[2] / 2, d[1] + d[3] / 2, d[2], c['sx'], c['sy'], c['sw']))
    c = code_vals(kind, 0)
    chk(abs(v[0] + v[2] / 2 - c['vx']) <= 1.5 and abs(v[1] + v[3] / 2 - c['vy']) <= 1.5 and abs(v[2] - c['vw']) <= 1.5, '%s: место просмотра — пакет центр (%.1f,%.1f) ширина %d, код (%.1f,%.1f) %d' % (sc, v[0] + v[2] / 2, v[1] + v[3] / 2, v[2], c['vx'], c['vy'], c['vw']))
# математика полёта
s0 = 60 / 149; C = (235.5, 126.5); P0 = (138.0, 96.5)
px, py = (P0[0] - s0 * C[0]) / (1 - s0), (P0[1] - s0 * C[1]) / (1 - s0)
for s, want in ((s0, P0), (1.0, C), ((s0 + 1) / 2, ((P0[0] + C[0]) / 2, (P0[1] + C[1]) / 2))):
    cx_, cy_ = px + s * (C[0] - px), py + s * (C[1] - py)
    chk(abs(cx_ - want[0]) < 1e-6 and abs(cy_ - want[1]) < 1e-6, 'полёт: при s=%.3f центр (%.2f,%.2f) = (%.2f,%.2f)' % (s, cx_, cy_, *want))
print('%s строк %d, упало %d' % ('FAIL' if fails else 'PASS', total, fails)); sys.exit(1 if fails else 0)
