#!/usr/bin/env python3
"""Окно g07b-msnview, круг 6 (P17-19): список машин соперников бонус-гонки (bnsAiCars в viewer/bonus_play.h).
Проверяем по ДИСКУ (не по исходнику): 67 имён, без повторов, у каждого есть art/cars/<имя>.p3d, и каждое попадает в пакет msncars-b.shr по рецепту tools/bonus/export.py
(VEHICLES + RACE_VEHICLES + AI_VEHICLES). Красный контроль: --old убирает AI_VEHICLES из рецепта (прежний пакет) — тест обязан показать недостающие машины.
  aicars_check.py [--old] [корень диска game/]"""
import re, sys, pathlib
here = pathlib.Path(__file__).resolve().parents[2]
old = '--old' in sys.argv; args = [a for a in sys.argv[1:] if a != '--old']
disc = pathlib.Path(args[0]) if args else pathlib.Path.home() / 'Downloads/shar-psp/hit-and-run-web/source/game'
bp = (here / 'viewer/bonus_play.h').read_text(); a = bp.index('bnsAiCars[] = {'); b = bp.index('};', a)
names = re.findall(r'"([^"]+)"', bp[a:b])
ex = (here / 'tools/bonus/export.py').read_text()
def lst(var):
    m = re.search(r'^' + var + r'\s*=\s*\[(.*?)\]', ex, re.S | re.M); return re.findall(r"['\"]([^'\"]+)['\"]", m.group(1)) if m else []
pack = set(x.lower() for x in lst('VEHICLES') + lst('RACE_VEHICLES') + ([] if old else lst('AI_VEHICLES')))
on_disc = {p.stem.lower() for p in (disc / 'art/cars').glob('*.p3d')}
errs = []
if len(names) != 67: errs.append('имён %d, ждём 67' % len(names))
if len(set(n.lower() for n in names)) != len(names): errs.append('есть повторы')
miss_disc = [n for n in names if n.lower() not in on_disc]; miss_pack = [n for n in names if n.lower() not in pack]
if miss_disc: errs.append('нет на диске: %s' % miss_disc)
if miss_pack: errs.append('не попадут в пакет msncars-b.shr: %s' % miss_pack)
print('имён %d, на диске %d, в рецепте пакета %d' % (len(names), len(names) - len(miss_disc), len(names) - len(miss_pack)))
for e in errs: print('  ', e)
print('FAIL' if errs else 'OK'); sys.exit(1 if errs else 0)
