#!/usr/bin/env python3
"""Тур Роба на 105: ролик из VIEW MOVIES идёт в нашей рамке-телевизоре, а не под списком роликов (чужой экран 3 пакета OPTIONS рисовался вместо рамки).
   check.py MID.png [--expect red|green]   MID.png — кадр середины ролика (сценарий viewmovies_l6_scenario.txt, строка shot mid).
Признак «списка поверх»: жёлтая рамка выбранного пункта и белые подписи в центре кадра (жёлтые пиксели 240+,200+,<110). Зелёное: их 0 и по периметру тёмно-фиолетовая рамка (средняя яркость полосы 14 px < 90)."""
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB'); W, H = im.size; px = im.load()
yel = sum(1 for y in range(H) for x in range(W) if px[x, y][0] > 235 and px[x, y][1] > 190 and px[x, y][2] < 120)
band = [sum(px[x, y]) / 3 for y in range(H) for x in range(W) if x < 14 or x >= W - 14 or y < 14 or y >= H - 14]
bm = sum(band) / len(band); ok = yel < 40 and bm < 90
print(f'жёлтых пикселей списка {yel}, средняя яркость полосы по периметру {bm:.0f}: {"РАМКА НА МЕСТЕ" if ok else "РАМКИ НЕТ (список поверх)"}')
want = sys.argv[3] if len(sys.argv) > 3 and sys.argv[2] == '--expect' else 'green'
sys.exit(0 if (ok == (want == 'green')) else 1)
