#!/usr/bin/env python3
"""GitHub #4: подписи запасных экранов бонус-гонки на F, G, S (снимки tests/bonus/lang4_run.sh).
  python3 tests/bonus/lang4_gate.py ПАПКА_НОВОЕ [ПАПКА_OLD]      ПАПКА/{E,F,G,S}/{fb_menu2,fb_pause,s6}.png
Проверяет: (1) таблицы lang_bonus.py: 26 непустых строк, у каждого языка почти все подписи отличаются от английских; (2) на снимках новой сборки
заголовок экрана и столбцы итогов у F, G, S отличаются от английских (метка «подпись переведена»), правее x=474 в полосе заголовков ничего нет (подгонка ширин);
(3) со сборкой BONUSLANG_OLD=1 (вторая папка) снимки F, G, S совпадают с английскими (до шума фона) — гейт на этих снимках обязан упасть."""
import os, subprocess, sys
from pathlib import Path
from PIL import Image, ImageChops
ROOT = Path(__file__).resolve().parents[2]
SHOTS = {'fb_menu2': (0, 20, 480, 60), 'fb_pause': (100, 110, 380, 170)}   # итоги (s6) не сравниваются: фон там — кадр гонки, у каждого прогона свой

def diff(a, b, box):
    A = Image.open(a).convert('RGB').crop(box); B = Image.open(b).convert('RGB').crop(box)
    bb = ImageChops.difference(A, B).convert('L').point(lambda v: 255 if v > 40 else 0).getbbox()
    return 0 if bb is None else sum(1 for p in ImageChops.difference(A, B).convert('L').getdata() if p > 40)

def edge(a, box):   # яркие точки в полосе правее x=474
    I = Image.open(a).convert('RGB').crop((474, box[1], 480, box[3]))
    return sum(1 for r, g, b in I.getdata() if max(r, g, b) > 150)

def main():
    new = Path(sys.argv[1]); old = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    bad = 0
    en = {}
    for L in 'FGS':
        out = subprocess.run([sys.executable, str(ROOT / 'tools/lang_bonus.py'), '--list'], env=dict(os.environ, LANG_BIBLE=L), capture_output=True, text=True).stdout.splitlines()
        if len(out) != 26 or any(len(x.split(None, 2)[-1].strip()) == 0 for x in out):
            print('FAIL таблица %s: %d строк' % (L, len(out))); bad = 1
    for L in 'FGS':
        for nm, box in SHOTS.items():
            d = diff(new / L / (nm + '.png'), new / 'E' / (nm + '.png'), box)
            e = edge(new / L / (nm + '.png'), box)
            ok = d > 300 and e == 0
            print('%s %s %-9s отличий от английского %5d, у правого края %d' % ('ok  ' if ok else 'FAIL', L, nm, d, e)); bad |= not ok
        if old:
            for nm, box in SHOTS.items():
                d = diff(old / L / (nm + '.png'), old / 'E' / (nm + '.png'), box)
                print('%s %s %-9s OLD: отличий от английского %d (шум фона до 200)' % ('ok  ' if d <= 200 else 'FAIL', L, nm, d)); bad |= d > 200
    print('FAIL' if bad else 'PASS'); return bad

if __name__ == '__main__':
    sys.exit(main())
