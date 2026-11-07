#!/usr/bin/env python3
"""Иконка игры в меню PSP (XMB), ICON0.PNG 144×80, из карточки Роба (build/assets/icon0-source-rob.png, его
«Group 11809436.png»). Правка Роба 28.09 вечер: «плитка как у Tony Hawk, дотяни серые поля, подбей вёрстку профессионально».

Эталон — ICON0 Tony Hawk's Underground 2 Remix с карты Роба (прочитан из образа, в проект не копируется): 144×80, RGB без
прозрачности, картинка во всю плитку, надпись почти от края до края. У прежней нашей иконки были прозрачные скругления и
прозрачные строки сверху/снизу — XMB показывал сквозь них свой серый фон («серые поля»), а знак занимал ~85% ширины.

Что делает: карточка Роба кладётся на её же чёрный фон (прозрачные углы — чёрные), обрезается по содержимому (знак,
«v.0.1», «PSP»), вписывается БЕЗ растяжения (общий масштаб) в плитку 144×80 с ровными полями --pad px по бокам
(сверху/снизу — сколько даёт пропорция, поровну), фон плитки — чёрный, как в карточке. Результат — RGB без альфы:
плитка непрозрачна целиком, серых полей нет.

Правка Роба 30 (видео IMG_7656.MOV 0:13–0:22, палец у правой части плитки): «начало надписи и грань PSP — вровень с нашим
логотипом». В карточке надписи «v.0.1» (сверху) и «PSP» (снизу) начинались правее плашки HIT&RUN (x 485 и 403 против 265
в карточке 603×325), правые края и так вровень (546 / 543 / 544). --align plate (по умолчанию): начало «v.0.1» и левая
грань «PSP» ставятся ровно на левый край плашки HIT&RUN — одна левая линия у колонки v.0.1 / HIT&RUN / PSP; по высоте
надписи на местах. --align none — карточка как есть (прежняя раскладка, отрицательный контроль tools/pbp_gate.py).
Правка Роба 146 (01.10): «превьюшка статичная: убрать надпись версии и PSP, только логотип Симпсонов на чёрном фоне, как
родное». --text none (по умолчанию): рамки «v.0.1» и «PSP» (их находит parts()) закрашиваются чёрным фоном карточки ДО
обрезки по краске — в плитке остаётся только знак (табличка The Simpsons + жёлтый ромб + плашка HIT&RUN), вписанный
по тем же правилам (общий масштаб, поля --pad по бокам поровну, чёрный фон, RGB без альфы). Решено терминалом за Роба:
источник логотипа — его же карточка (знак в ней 513 px шириной), а не build/assets/logo-source.png (340 px, мутнее).
--text keep — прежняя плитка с надписями (отрицательный контроль tools/pbp_gate.py, вместе с --align).
render() — та же плитка функцией: tools/pbp_gate.py перерисовывает её из источника и сверяет с ICON0.
  tools/icon0.py [--src build/assets/icon0-source-rob.png] [--out build/assets/icon0.png] [--pad 5] [--text none|keep]
                 [--align right|plate|none]"""
import argparse, sys
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
W, H = 144, 80
SRC = ROOT / 'build/assets/icon0-source-rob.png'


def parts(px):
    """Плашка HIT&RUN (зелёная) и надписи над ней справа («v.0.1») и под ней («PSP»): рамки (x0, y0, x1, y1) в карточке"""
    ink = px.max(axis=2) > 48
    g = (px[..., 1] > px[..., 0] + 30) & (px[..., 1] > px[..., 2] + 10)
    gy, gx = np.where(g); plate = (gx.min(), gy.min(), gx.max() + 1, gy.max() + 1)
    def box(y0, y1):   # самая правая группа столбцов с краской в полосе y0..y1 правее начала плашки (зазор ≥ 8 px)
        m = ink[y0:y1, plate[0]:]; cols = np.where(m.any(0))[0]
        start = cols[-1]
        for c in cols[::-1]:
            if start - c > 8: break
            start = c
        x0 = plate[0] + start; yy, xx = np.where(ink[y0:y1, x0:])
        return (x0, yy.min() + y0, xx.max() + x0 + 1, yy.max() + y0 + 1)
    return plate, box(0, plate[1] - 2), box(plate[3] + 2, px.shape[0])


def disc_card(out):
    """№1162: карточка знака с диска игрока — спрайт gamelogo (640×480) из art/frontend/scrooby/frontend.p3d, тот же знак (табличка The Simpsons, жёлтый
    ромб, плашка HIT&RUN), что и на карточке Роба; в PNG с прозрачным фоном нет — чёрный, как у карточки. Нужен только свой диск (hit-and-run-web/source)."""
    sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
    import ui_export as U
    from p3d import read
    for c in read(U.WEB / 'source/game/art/frontend/scrooby/frontend.p3d').children:
        if c.id == 0x19005:
            n, i = U.decode_sprite_image(c)
            if n.startswith('gamelogo'):
                a = np.asarray(i.convert('RGBA')).copy()
                lit = a[..., :3].max(axis=2) >= 64   # зелёное свечение вокруг знака на диске тёмное (≤ 40): в чёрный, как фон карточки Роба
                a[~lit, :3] = 0; a[..., 3] = 255
                Image.fromarray(a, 'RGBA').save(out); return Path(out)
    raise SystemExit('icon0: в frontend.p3d нет спрайта gamelogo')


def render(src=SRC, pad=5, text='none', align='right', log=print):
    """Карточка Роба -> плитка ICON0 144×80 RGB (PIL.Image)."""
    src = Image.open(src).convert('RGBA')
    bg = Image.new('RGBA', src.size, (0, 0, 0, 255)); card = Image.alpha_composite(bg, src).convert('RGB')
    px = np.asarray(card).astype(int).copy()
    if text == 'none':   # правка Роба 146: надписей «v.0.1» и «PSP» нет — только знак на чёрном
        plate, ver, psp = parts(px)
        for bx in (ver, psp):
            x0, y0, x1, y1 = (int(v) for v in bx); px[y0:y1, x0:x1] = 0
        card = Image.fromarray(px.astype(np.uint8), 'RGB')
        log(f'надписи убраны (правка 146): v.0.1 {tuple(int(v) for v in ver)}, PSP {tuple(int(v) for v in psp)}')
    elif text == 'disc': log('карточка с диска: надписей «v.0.1» и «PSP» в ней нет (№1162)')
    elif align == 'right':   # правка Роба 68 (видео IMG_7662 0:05–0:25): «зелёная грань справа… по правую сторону надо добить» —
        plate, ver, psp = parts(px)   # «v.0.1» и «PSP» правым краем точно в правый край плашки HIT&RUN (в карточке были на 2–3 px левее)
        for bx in (ver, psp):
            x0, y0, x1, y1 = (int(v) for v in bx); piece = px[y0:y1, x0:x1].copy(); w_ = x1 - x0
            px[y0:y1, x0:x1] = 0; nx = int(plate[2]) - w_; px[y0:y1, nx:nx + w_] = np.maximum(px[y0:y1, nx:nx + w_], piece)
        card = Image.fromarray(px.astype(np.uint8), 'RGB')
        log(f'надписи правым краем в правый край плашки x={plate[2]}: v.0.1 {ver[2]} -> {plate[2]}, PSP {psp[2]} -> {plate[2]}')
    elif align == 'plate':   # правка Роба 30: начало «v.0.1» и грань «PSP» — на левый край плашки
        plate, ver, psp = parts(px)
        for bx in (ver, psp):
            x0, y0, x1, y1 = (int(v) for v in bx); piece = px[y0:y1, x0:x1].copy()
            px[y0:y1, x0:x1] = 0; nx = int(plate[0]); px[y0:y1, nx:nx + (x1 - x0)] = np.maximum(px[y0:y1, nx:nx + (x1 - x0)], piece)
        card = Image.fromarray(px.astype(np.uint8), 'RGB')
        log(f'надписи от левого края плашки x={plate[0]}: v.0.1 {ver[0]} -> {plate[0]}, PSP {psp[0]} -> {plate[0]}')
    ink = px.max(axis=2) > 48                       # всё, что не чёрный фон карточки (знак и, если оставлены, надписи)
    ys, xs = np.where(ink)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    content = card.crop((x0, y0, x1, y1))
    cw, ch = content.size
    s = min((W - 2 * pad) / cw, (H - 2 * pad) / ch)   # один масштаб на обе оси — без растяжения
    # вёрстка на холсте 4× (576×320), потом весь холст -> 144×80: поля ровно поровну и пропорция без округления до
    # целого пикселя плитки (край содержимого может лечь на полпикселя — фон чёрный с обеих сторон, шва нет)
    K = 4; tw4, th4 = round(cw * s * K), round(ch * s * K)
    big = Image.new('RGB', (W * K, H * K), (0, 0, 0))
    ox4, oy4 = (W * K - tw4) // 2, (H * K - th4) // 2
    big.paste(content.resize((tw4, th4), Image.LANCZOS), (ox4, oy4))
    tile = big.resize((W, H), Image.LANCZOS)
    log(f'{W}x{H} RGB; содержимое карточки {cw}x{ch} -> {tw4 / K:g}x{th4 / K:g} (масштаб {s:.4f}, пропорция '
        f'{cw / ch:.4f} -> {tw4 / th4:.4f}), поля: слева/справа {ox4 / K:g}, сверху/снизу {oy4 / K:g}')
    return tile


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', default=str(SRC), help='PNG карточки; disc — знак gamelogo с диска игрока (№1162)')
    ap.add_argument('--out', default=str(ROOT / 'build/assets/icon0.png'))
    ap.add_argument('--pad', type=int, default=5, help='поле слева и справа, px плитки')
    ap.add_argument('--text', default='none', choices=['none', 'keep', 'disc'], help='disc — карточка с диска без надписей (ставится сама при --src disc); none — только логотип (правка 146); keep — с «v.0.1» и «PSP»')
    ap.add_argument('--align', default='right', choices=['right', 'plate', 'none'], help='при --text keep: right — правым краем точно в правый край плашки HIT&RUN (правка 68); plate — от левого края (правка 30)')
    a = ap.parse_args()
    if a.src == 'disc':   # №1162: знак с диска игрока (публичный путь, tools/xmb_assets.sh)
        import tempfile
        a.src = disc_card(Path(tempfile.mkdtemp(prefix='shar-icon0-')) / 'gamelogo.png'); a.text = 'disc'
    render(a.src, a.pad, a.text, a.align).save(a.out, optimize=True)
    print(f'{a.out}: готово')
