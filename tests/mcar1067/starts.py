#!/usr/bin/env python3
"""№1067: стартовые точки машины игрока всех миссий L1–L7 по данным диска. starts.py OUT.tsv
Ищет в scripts/missions/levelNN/*.mfk вызовы SetMissionResetPlayerInCar("car") и SetMissionResetPlayerOutCar("игрок","car"), позицию берёт из локаторов art/missions/levelNN/*.p3d
(tools/missions/p3dlocs.py; ось z движка отзеркалена, курс движка = pi + курс локатора). Колонки: уровень, файл, миссия, вид (in/out), локатор машины, x y z h, локатор игрока, x y z"""
import re, sys, math
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/missions'))
import p3dlocs as P
G = Path.home() / 'Downloads/shar-psp/hit-and-run-web/source/game'
out = ['level\tfile\tmission\tkind\tcar_loc\tcx\tcy\tcz\tch\tplayer_loc\tpx\tpy\tpz']
for lv in range(1, 8):
    loc = {}
    for f in sorted((G / f'art/missions/level{lv:02d}').glob('*.p3d')):
        L, err = P.read_locators(f, f.name)
        for l in L: loc.setdefault(l.name.lower(), l)
    for f in sorted((G / f'scripts/missions/level{lv:02d}').glob('*.mfk')):
        txt = re.sub(r'//[^\n]*', '', f.read_text(errors='replace'))
        ms = re.search(r'SelectMission\(\s*"([^"]+)"', txt); name = ms.group(1) if ms else f.stem
        for m in re.finditer(r'SetMissionResetPlayerInCar\(\s*"([^"]+)"', txt):
            c = loc.get(m.group(1).lower())
            out.append('\t'.join(map(str, [lv, f.name, name, 'in', m.group(1)] + ([f'{c.pos[0]:.2f}', f'{c.pos[1]:.2f}', f'{c.pos[2]:.2f}', f'{math.pi + (c.heading or 0):.4f}'] if c else ['', '', '', '']) + ['', '', '', ''])))
        for m in re.finditer(r'SetMissionResetPlayerOutCar\(\s*"([^"]+)"\s*,\s*"([^"]+)"', txt):
            p = loc.get(m.group(1).lower()); c = loc.get(m.group(2).lower())
            out.append('\t'.join(map(str, [lv, f.name, name, 'out', m.group(2)] + ([f'{c.pos[0]:.2f}', f'{c.pos[1]:.2f}', f'{c.pos[2]:.2f}', f'{math.pi + (c.heading or 0):.4f}'] if c else ['', '', '', '']) + [m.group(1)] + ([f'{p.pos[0]:.2f}', f'{p.pos[1]:.2f}', f'{p.pos[2]:.2f}'] if p else ['', '', '']))))
Path(sys.argv[1]).write_text('\n'.join(out) + '\n'); print(len(out) - 1, 'стартов')
