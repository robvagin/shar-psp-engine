#!/usr/bin/env python3
"""№1111: ни одна надпись не лежит поверх соседней надписи (экраны интерфейса ui_export.SCREENS). Для каждой надписи считается, где она реально рисуется (выравнивание, перенос, жёсткие
переносы «\\», значение из данных — по прикидке), пары с пересечением больше 1,5 px по обеим осям — провал, кроме пар, которые так же пересекаются у английского (пустые заготовки, перечни
вариантов одной рамки). Запуск: LANG_BIBLE=F python3 tests/lang1111/overlap.py   код 1 — есть новые пересечения; LANGFIT1111_OLD=1 — красный контроль."""
import json, os, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import ui_export as U

def pairs():
    fc = U.FontCache('psp'); res = {}
    for screen in U.SCREENS:
        try:
            items, icons = U.build_items(screen); out, _ = U.adapt(items, icons, 'psp', U.Assets(), fc)
        except Exception as ex:
            print('пропуск', screen, type(ex).__name__); continue
        ink = U.text_ink(out, fc)
        for i, (t, x0, x1, y0, y1) in enumerate(ink):
            for (o, ox0, ox1, oy0, oy1) in ink[i + 1:]:
                if t['name'] == o['name']:
                    continue
                dx, dy = min(x1, ox1) - max(x0, ox0), min(y1, oy1) - max(y0, oy0)
                if dx > 1.5 and dy > 1.5:
                    res['%s\x1f%s\x1f%s' % (screen, *sorted((t['name'], o['name'])))] = [round(dx, 1), round(dy, 1), (t.get('strings') or [''])[0][:30], (o.get('strings') or [''])[0][:30]]
    return res

if __name__ == '__main__':
    cur = pairs(); lang = U.LANG
    if '--json' in sys.argv:
        print(json.dumps(cur)); sys.exit(0)
    base = {}
    if lang != 'E':
        p = subprocess.run([sys.executable, __file__, '--json'], env=dict(os.environ, LANG_BIBLE='E'), capture_output=True, text=True)
        base = json.loads(p.stdout)
    bad = 0
    for k, v in sorted(cur.items()):
        if k in base or lang == 'E':
            continue
        bad += 1; print('НОВ.', k.replace('\x1f', ' | '), 'пересечение', v[0], 'x', v[1], '«%s» / «%s»' % (v[2], v[3]))
    print('язык', lang, 'пар с пересечением', len(cur), 'новых против английского', bad)
    sys.exit(1 if bad else 0)
