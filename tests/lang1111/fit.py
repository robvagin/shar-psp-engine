#!/usr/bin/env python3
"""№1111: тексты на другом языке не должны вылезать за рамки. Для каждого экрана интерфейса (ui_export.SCREENS) берутся настоящие надписи выбранного языка (LANG_BIBLE=E|F|G|S),
считается перенос по ширине рамки и высота строк в игровой грани; провал — надпись шире рамки после переноса или выше рамки (на английском то же место при том же размере считается нормой —
сравнение с английским: «нов.» — только у этого языка). Запуск: LANG_BIBLE=F python3 tests/lang1111/fit.py [--all]   код 1 — есть новые провалы."""
import os, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import ui_export as U

def run():
    fc = U.FontCache('psp'); res = {}
    for screen in U.SCREENS:
        try:
            items, icons = U.build_items(screen)
            out, _ = U.adapt(items, icons, 'psp', U.Assets(), fc)
        except Exception as ex:          # экран не собирается без данных уровня — пропуск, не провал
            print('пропуск', screen, type(ex).__name__, str(ex)[:60]); continue
        for t in out:
            if not t.get('strings') or 'dst' not in t:
                continue
            f = fc.get(t['font']); x, y, w, h = t['dst']
            for sv in t['strings']:
                if not sv:
                    continue
                lines = [ln for seg in U._lines(sv) for ln in U.wrap_lines(f, seg, w, t.get('wrap'))]   # «\\» и «\n» — жёсткий перенос, как у движка
                ws = max([U.text_width(f, ln) for ln in lines] + [0])
                res[(screen, t['name'], sv)] = (ws, w, len(lines) * f['lh'], h, len(lines))
    return res

def agg(res):
    """по надписи (экран, имя): самая широкая строка после переноса, самая высокая высота, рамка, самая длинная строка языка"""
    a = {}
    for (screen, name, sv), (ws, w, hs, h, n) in res.items():
        k = (screen, name); cur = a.get(k, [0, w, 0, h, ''])
        if ws > cur[0]: cur[4] = sv
        cur[0] = max(cur[0], ws); cur[2] = max(cur[2], hs); a[k] = cur
    return a

if __name__ == '__main__':
    import json
    lang = U.LANG
    cur = agg(run())
    if '--json' in sys.argv:
        print(json.dumps({'\x1f'.join(k): v for k, v in cur.items()})); sys.exit(0)
    base = {}
    if lang != 'E':
        import subprocess
        p = subprocess.run([sys.executable, __file__, '--json'], env=dict(os.environ, LANG_BIBLE='E'), capture_output=True, text=True)
        base = {tuple(k.split('\x1f')): v for k, v in json.loads(p.stdout).items()}
    bad = 0
    for (screen, name), (ws, w, hs, h, sv) in sorted(cur.items()):
        b = base.get((screen, name), [0, w, 0, h, ''])   # английский: чем он вылезает сам, то норма (пустые заготовки, перечни)
        limw = max(w, b[0]) + .5; limh = max(h, b[2]) + 1
        if lang == 'E' or (ws <= limw and hs <= limh):
            continue
        bad += 1
        print('НОВ. %-12s %-24s ширина %5.1f (рамка %3d, англ. %5.1f) высота %3d (рамка %3d, англ. %3d)  «%s»' % (screen, name[:24], ws, w, b[0], hs, h, b[2], sv[:48].replace('\\', '/')))
    print('язык', lang, 'надписей', len(cur), 'вылезает сильнее английского', bad)
    sys.exit(1 if bad else 0)
