"""Графа «сторож» ночных отчётов (CLAUDE.md п.12, viewer/watchdog.h): строки `WATCHDOG <вид> actor=… t=… …` из журналов прогона.
Любое срабатывание — дыра, которую чинят в корне (сторож страховка, не повод оставить). Общий разбор для msnflow_report.py и night_report.py."""
import re
from collections import Counter
from pathlib import Path

def scan(*paths):
    """-> (Counter по (вид, actor), список (файл, сцена, строка)); сцена — последняя SCENE выше строки"""
    cnt, rows = Counter(), []
    for p in paths:
        p = Path(p)
        if not p.exists(): continue
        scene = 'start'
        for l in p.read_text(errors='replace').splitlines():
            if l.startswith('SCENE '): scene = (l.split() + ['?'])[1]; continue
            m = re.match(r'WATCHDOG (\w+) actor=(\w+)', l)
            if m: cnt[(m.group(1), m.group(2))] += 1; rows.append((p.parent.name, scene, l))
            m = re.match(r'FALL1209 who=(\w+)', l)   # прежний наблюдатель падения (ag/loadorder) — то же нарушение, пока нет строк сторожа в старых журналах
            if m: cnt[('nofloor(FALL1209)', {'hero': 'hero', 'car': 'car'}.get(m.group(1), m.group(1)))] += 1; rows.append((p.parent.name, scene, l))
    return cnt, rows

def summary(cnt):
    return 'сторож: чисто (0 срабатываний)' if not cnt else 'сторож: ДЫРА, %d срабатываний (%s)' % (sum(cnt.values()), ', '.join(f'{k}/{a} {n}' for (k, a), n in sorted(cnt.items())))

def lines(rows, limit=8):
    return [f'  [{r[0]} сцена {r[1]}] {r[2][:200]}' for r in rows[:limit]] + ([f'  … ещё {len(rows) - limit}'] if len(rows) > limit else [])
