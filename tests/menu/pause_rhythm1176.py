#!/usr/bin/env python3
"""№1176: ритм страниц паузы. Меряет в px PSP (по пробной раскладке экспорта pmenu): шаг строк, промежуток между буквами соседних строк,
поле от видимой рамки до первой/последней строки. Правило Роба: промежуток между строками не больше поля сверху и снизу.
  python3 -I tests/menu/pause_rhythm1176.py [--lang E|F|G|S] [--check]
--check: код выхода 1, если на странице паузы (pause*, кроме CAMERA-настроек №1151) промежуток строк больше поля. Переменные окружения
экспорта (OPTIONS_PITCH1176_OLD=1 ...) действуют как при обычной сборке пакета."""
import os, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
argv = sys.argv[1:]
lang = 'E'
if '--lang' in argv: lang = argv[argv.index('--lang') + 1]
if lang != 'E': os.environ['LANG_BIBLE'] = lang
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import ui_export as U, menu_export as M
PANEL_PITCH = M.pause_items_pitch()
rep = []
cur = {}
orig = U.board_fit
def fit(items, rows, page, K, **kw):
    fb = orig(items, rows, page, K, **kw)
    boxes, _ = U._row_inks(rows, kw.get('images'))
    hid = kw.get('hidden') or ()
    boxes = [b for i, b in enumerate(boxes) if i not in hid]   # строки, которых игрок не видит (RESET CAR), в промежутки не входят
    rep.append((cur.get('sc') or ('pause(главная)' if kw.get('min_rows') else None), page, kw.get('pitch'), kw.get('pad_y'), fb, boxes, bool(hid)))
    return fb
U.board_fit = fit
opf = M.pause_fit
def pf(sc, its, assets):
    cur['sc'] = sc
    return opf(sc, its, assets)
M.pause_fit = pf
with tempfile.TemporaryDirectory() as d:
    M.export('pmenu', os.path.join(d, 'pmenu.shr'), mocks=False)
    cur['sc'] = None
    U.pack(__import__('pathlib').Path(d) / 'ui.shr')   # главная страница паузы (пакет ui)
bad = 0
ref = {}   # sc -> (поле, наибольший промежуток) для сравнения главной и OPTIONS
print(f'язык {lang}: страница | строк | шаг заданный | поле сверху | поле снизу | промежутки между буквами строк (px PSP)')
for sc, page, pitch, pad, fb, bx, hid in rep:
    if not sc or len(bx) < 2: continue
    if sc == 'pause(главная)' and hid:
        # пауза свободной езды: в разметке остаётся слот скрытой службной строки RESET CAR (движок закрывает его при показе), поэтому по построению board_fit:
        # поле = pad_y сверху и снизу, промежуток = шаг - высота букв строки
        top = bot = pad; pitch = PANEL_PITCH; gaps = [pitch - (b[3] - b[1]) for b in bx[:-1]]   # на экране шаг главной — из панели Роба
    else:
        top = bx[0][1] - fb[1]; bot = fb[3] - bx[-1][3]; gaps = [bx[i + 1][1] - bx[i][3] for i in range(len(bx) - 1)]
    ok = max(gaps) <= min(top, bot)
    flag = '' if ok else '  НАРУШЕНИЕ: промежуток > поля'
    if sc in ('pausecamera', 'pausecamadj'): flag += '  (CAMERA, принята Робом, не проверяем)'; ok = True
    elif sc == 'pause(главная)' and not hid: flag = '  (пауза миссии: доска постоянной высоты, поля больше, справка)'; ok = True
    elif sc not in ('pauseoptions', 'pause(главная)'): flag = flag.replace('НАРУШЕНИЕ', 'вне №1176, справка'); ok = True
    if sc == 'pauseoptions' or (sc == 'pause(главная)' and hid): ref[sc] = (min(top, bot), max(gaps))
    if sc == 'pauseoptions' and pitch != PANEL_PITCH: flag += f'  НАРУШЕНИЕ: шаг OPTIONS {pitch} не равен шагу главной страницы {PANEL_PITCH}'; ok = False
    if not ok: bad += 1
    print(f'{sc:14s} | {len(bx):2d} | шаг {pitch} | сверху {top} | снизу {bot} | промежутки {gaps}{flag}')
if len(ref) == 2:
    print(f"главная страница паузы (по панели): промежуток {ref['pause(главная)'][1]}; OPTIONS: поле {ref['pauseoptions'][0]}, промежуток {ref['pauseoptions'][1]}")
    if ref['pauseoptions'][1] != ref['pause(главная)'][1]: print('НАРУШЕНИЕ: промежуток OPTIONS не равен промежутку главной страницы'); bad += 1
sys.exit(1 if '--check' in argv and bad else 0)
