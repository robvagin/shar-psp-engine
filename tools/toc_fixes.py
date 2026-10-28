#!/usr/bin/env python3
"""№1006 (Роб 26.10: «ты долго всё ищешь — сделай себе оглавление»): оглавление правок по коду.
Каждый выключатель прежнего поведения `#ifndef X_OLD` в viewer/ — строка: номер правки из ближайшего комментария выше, выключатель, файл:строка,
раздел (по файлу) и начало комментария. Плюс обратный указатель: номер правки → все её выключатели и файлы.

  python3 tools/toc_fixes.py [выход.md]   (по умолчанию docs/handoff/ОГЛАВЛЕНИЕ_ПРАВОК.md главной рабочей копии)
"""
import os, re, sys
from collections import defaultdict
from pathlib import Path

W = Path(__file__).resolve().parent.parent
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.home() / 'Downloads/shar-psp/docs/handoff/ОГЛАВЛЕНИЕ_ПРАВОК.md'

# раздел по имени файла: первое совпадение по началу имени
AREAS = [
    ('Интерфейс: меню, пузыри, карточки, HUD', ('ui', 'msn_view', 'msnart', 'main_frontend', 'main_save', 'bootseq', 'bootscreen', 'bootbeep', 'bootmarks', 'bonus_play', 'tutorial')),
    ('Мультик: земля, заливки, бордюры, цвет', ('toonflat', 'ground969', 'tone', 'curb')),
    ('Контур и персонажи', ('skin', 'homer', 'npc', 'peds')),
    ('Свет, тени, блики', ('spritelight', 'lightsprite', 'lamppool', 'glint', 'sparkle', 'fx', 'neon')),
    ('Даль, небо, горизонт', ('horizon', 'hlod', 'silh', 'far', 'sky')),
    ('Рендер и скорость', ('main_render', 'main_stream', 'render', 'gg', 'dl')),
    ('Камера', ('main_camera', 'cam', 'roomcam')),
    ('Машины, физика, столкновения', ('car', 'col', 'physics', 'wheel', 'carfill')),
    ('Мир: пропы, стейтпропы, уровни', ('props', 'stateprops', 'main_levels', 'main_collect', 'propbake', 'world')),
    ('Миссии и игра', ('game_msn', 'mission', 'msn', 'hnr', 'main_loop')),
    ('Звук и ролики', ('snd', 'sound', 'audio', 'fmv', 'movie', 'me_')),
]


def area(fn):
    b = os.path.basename(fn).lower()
    for name, pre in AREAS:
        if any(b.startswith(p) for p in pre):
            return name
    return 'Прочее'


rx_if = re.compile(r'^\s*#ifndef\s+([A-Z0-9_]+_OLD)\b')
rx_no = re.compile(r'№\s?(\d{3,4}[а-яa-z]?)')
rows = []
for p in sorted((W / 'viewer').rglob('*')):
    if p.suffix not in ('.c', '.h') or '/build' in str(p):
        continue
    try:
        L = p.read_text(errors='replace').split('\n')
    except Exception:
        continue
    for i, l in enumerate(L):
        m = rx_if.match(l)
        if not m:
            continue
        # номер правки: цифры в имени выключателя (ROADINK1008_OLD -> 1008), иначе первый № сверху в сплошном комментарии над #ifndef
        blk = []
        for j in range(i - 1, max(-1, i - 25), -1):
            s_ = L[j].strip()
            if s_.startswith('//') or s_.startswith('/*') or s_.startswith('*'):
                blk.append(s_)
            elif s_ == '' or s_.startswith('#'):
                if blk:
                    break
                continue
            else:
                break
        blk.reverse()
        md = re.search(r'(\d{3,4})', m.group(1))
        num = md.group(1) if md and 300 <= int(md.group(1)) <= 1999 else ''
        if not num:
            for s_ in blk:
                mm = rx_no.search(s_)
                if mm:
                    num = mm.group(1); break
        note = re.sub(r'^[/*\s]+', '', blk[0]) if blk else ''
        rows.append((num, m.group(1), str(p.relative_to(W)), i + 1, area(p.name), note[:150]))

byno = defaultdict(list)
for r in rows:
    if r[0]:
        byno[r[0]].append(r)

def key(n):
    m = re.match(r'(\d+)(.*)', n)
    return (int(m.group(1)), m.group(2)) if m else (0, n)

out = ['# Оглавление правок по коду', '',
       'Собрано `tools/toc_fixes.py` из рабочей копии integ: каждый выключатель прежнего поведения `*_OLD` в `viewer/`, номер правки из комментария над ним. '
       'Перегенерировать после каждой сборки TEST. Номер есть в `ПРАВКИ_РОБА.md`, а здесь — где он живёт в коде.', '',
       f'Выключателей: {len(rows)}, с номером правки: {sum(1 for r in rows if r[0])}, правок: {len(byno)}', '',
       '## Как искать', '',
       '- по номеру правки — раздел «Номер → код» ниже (поиск `№1004`)',
       '- по теме — разделы по областям, внутри по файлам',
       '- выключить правку на PSP — файл `<имя в нижнем регистре без _OLD>old` рядом с EBOOT или флаг сценария (в комментарии у правки)', '',
       '## Номер → код', '', '| № | выключатели | файл:строка |', '|---|---|---|']
for n in sorted(byno, key=key, reverse=True):
    rs = byno[n]
    out.append(f'| {n} | {", ".join(sorted({r[1] for r in rs}))} | {", ".join(f"`{r[2]}:{r[3]}`" for r in rs[:4])}{" …" if len(rs) > 4 else ""} |')
out += ['', '## По областям', '']
for name, _ in AREAS + [('Прочее', ())]:
    rs = [r for r in rows if r[4] == name]
    if not rs:
        continue
    out += [f'### {name} ({len(rs)})', '', '| № | выключатель | файл:строка | о чём |', '|---|---|---|---|']
    for r in sorted(rs, key=lambda r: (r[2], r[3])):
        out.append(f'| {r[0] or "—"} | {r[1]} | `{r[2]}:{r[3]}` | {r[5].replace("|", "/")} |')
    out.append('')
OUT.write_text('\n'.join(out) + '\n')
print(f'{OUT}: выключателей {len(rows)}, правок {len(byno)}')
