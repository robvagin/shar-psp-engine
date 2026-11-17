# worldhold1326.py ПАПКА_СНИМКОВ: считает кадры w1..w10, где интерфейс виден, а мир чёрный (после ✕ карточки). Печатает «плохих N из M» и по кадрам: доля тёмного в мире, яркие точки интерфейса.
import sys
from PIL import Image
d = sys.argv[1]; bad = 0; n = 0; rows = []
for i in range(1, 11):
    try: im = Image.open(f'{d}/w{i}.png').convert('RGB')
    except Exception: continue
    W, H = im.size
    def frac(box, pred):
        x0, y0, x1, y1 = (int(box[0] * W), int(box[1] * H), int(box[2] * W), int(box[3] * H)); c = t = 0
        for y in range(y0, y1, 2):
            for x in range(x0, x1, 2):
                p = im.getpixel((x, y)); t += 1; c += pred(p)
        return c / max(t, 1)
    world_dark = frac((0.15, 0.12, 0.85, 0.70), lambda p: max(p) < 28)       # центр кадра: мир
    hud = frac((0.80, 0.70, 1.00, 1.00), lambda p: max(p) > 70) + frac((0.00, 0.70, 0.55, 1.00), lambda p: max(p) > 70)   # низ: радар и подпись задания
    n += 1; b = world_dark > 0.9 and hud > 0.03; bad += b
    rows.append(f'w{i}: мир тёмный {world_dark:.2f}, интерфейс {hud:.2f}{" ПЛОХО" if b else ""}')
print('\n'.join(rows)); print(f'плохих {bad} из {n}')
sys.exit(0 if bad == 0 else 1)
