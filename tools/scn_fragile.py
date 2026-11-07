#!/usr/bin/env python3
"""Статический разбор сценария тура: действие, зависящее от положения героя, раньше, чем PSP успевает закончить переход
   (arch-fable, 05.11.2026).  python3 ЧИСТОТА_TEST90_scn_fragile.py tests/smoke_scenes/test90_game.txt
Переходы и сколько ждать на PSP: △ пешком у двери (шаги, ирис, чтение комнаты 0,2 с, персонажи до 1,5 с) — 6 с; △ в машине (выход) — 2 с;
flag msn/msnmainat (старт задания: газета, ирис) — 4 с; lvwin/realend — 8 с; movie — 3 с; golevel и goto — блокирующие, 1 с запаса.
ОПАСНО = foot с Y ниже города (в комнату) раньше 6 с после двери: герой не в комнате -> homerPlace в город на y комнаты -> падение (№1209)."""
import sys
PSP = {'door': 6.0, 'exitcar': 2.0, 'golevel': 1.0, 'msn': 4.0, 'lvwin': 8.0, 'movie': 3.0, 'goto': 1.0}
rc = 0
for path in sys.argv[1:]:
    scene, last, acc, onfoot, out = None, None, 0.0, 0, []
    for n, ln in enumerate(open(path, errors='replace'), 1):
        t = ln.split('#')[0].split()
        if not t: continue
        c = t[0]
        if c == 'scene': scene = t[1]; continue
        if c == 'wait': acc += float(t[1]); continue
        kind = None
        if c == 'foot': onfoot = 1
        if c == 'goto': onfoot = 0
        if c == 'press' and len(t) > 1 and t[1] == 'triangle': kind = 'door' if onfoot else 'exitcar'; onfoot = 1
        elif c == 'golevel' or (c == 'flag' and len(t) > 1 and t[1] == 'golevel'): kind = 'golevel'
        elif c == 'flag' and len(t) > 1 and t[1] in ('msn', 'msnmainat', 'msngo', 'msnsel'): kind = 'msn'
        elif c == 'flag' and len(t) > 1 and t[1] in ('lvwin', 'realend'): kind = 'lvwin'
        elif c == 'flag' and len(t) > 1 and t[1] == 'movie': kind = 'movie'
        elif c == 'goto': kind = 'goto'
        posdep = c in ('foot', 'goto') or (c == 'flag' and len(t) > 1 and t[1] in ('msn', 'msnmainat', 'msngo', 'roomdl', 'startcam'))
        if last and posdep and acc < PSP[last[0]]:
            sev = 'ОПАСНО' if (c == 'foot' and len(t) > 4 and float(t[4]) < -10 and last[0] == 'door') else 'проверить'
            out.append((sev, f"{scene} стр.{n}: «{ln.strip()[:46]}» через {acc:.0f} с после {last[0]} (стр.{last[1]}); PSP ≥ {PSP[last[0]]:.0f} с"))
        if kind: last = (kind, n); acc = 0.0
    print(f"== {path}: {len(out)} ({sum(1 for s, _ in out if s == 'ОПАСНО')} ОПАСНО)")
    for s, m in out: print(f"   {s:9s} {m}")
    if any(s == 'ОПАСНО' for s, _ in out): rc = 1
sys.exit(rc)
