#!/usr/bin/env python3
"""Гейт №1271: ролик в рамке-телевизоре. check.py КАДР.png — снимок середины ролика (окно 480x272).
Рамка: по периметру (14 px) тёмная и сине-фиолетовая (средняя яркость < 90, синий > красного на 15+). Без рамки по краю картинка ролика или светлые поля.
Код выхода 0 — рамка на месте."""
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB'); W, H = im.size; px = im.load()
band = [px[x, y] for y in range(H) for x in range(W) if x < 14 or x >= W - 14 or y < 14 or y >= H - 14]
n = len(band); r = sum(p[0] for p in band) / n; g = sum(p[1] for p in band) / n; b = sum(p[2] for p in band) / n
ok = (r + g + b) / 3 < 90 and b > r + 15
print(f'периметр: яркость {(r+g+b)/3:.0f}, R {r:.0f} G {g:.0f} B {b:.0f}: {"РАМКА НА МЕСТЕ" if ok else "РАМКИ НЕТ"}')
sys.exit(0 if ok else 1)
