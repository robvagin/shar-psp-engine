#!/usr/bin/env python3
"""№1059 · замер PS2: стик зажат, камера игры меняется (выход из машины = облёт камеры). Пишет Гомера (поза + матрица), камеру, стик.
  python3 tools/twins/walkcam1059.py --out wc1059_a --on 100 --stick up [--off 400] [--switch 250:left] [--frames 520]
Время в кадрах (50/с, PAL), шаг записи 2 кадра."""
import argparse, math, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import common as C
import drive as D

HOM = [(f'm{i}', 0xa24a40 + 4 * i, 'f32') for i in range(16)]          # матрица Гомера: m12..m14 — позиция
SCR = [('scr', 0x8470c4, 'u32')]   # экран игры: 1 игра, 7 Start Mission, 6 переход, 5 карточка миссии (ps2-mission-state)
CAM = [('cx', 0x7aaba0, 'f32'), ('cy', 0x7aaba4, 'f32'), ('cz', 0x7aaba8, 'f32'), ('fx', 0x7aabb8, 'f32'), ('fy', 0x7aabbc, 'f32'), ('fz', 0x7aabc0, 'f32')]
STK = {'up': ('y', 1.0), 'down': ('y', -1.0), 'left': ('x', -1.0), 'right': ('x', 1.0), 'none': ('y', 0.0)}


def stick_cmds(name):
    if name[0] == 'a' and name[1:].lstrip('-').isdigit():   # aNN — NN° от «вверх» к «влево» (a45 = вверх-влево)
        r = math.radians(int(name[1:])); return [*D.axis('l', 'y', math.cos(r)), *D.axis('l', 'x', -math.sin(r))]
    if name == 'none': return [*D.axis('l', 'y', 0.0), *D.axis('l', 'x', 0.0)]
    ax, v = STK[name]; other = 'x' if ax == 'y' else 'y'
    return [*D.axis('l', other, 0.0), *D.axis('l', ax, v)]


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--out', required=True); ap.add_argument('--on', type=int, default=100)
    ap.add_argument('--stick', default='up'); ap.add_argument('--off', type=int, default=10 ** 9); ap.add_argument('--frames', type=int, default=520)
    ap.add_argument('--png', type=int, default=0, help='кадр экрана каждые N кадров в png/')
    ap.add_argument('--btn', default='', help='кнопка@кадр+длина через запятую: l2@300+60,r2@440+60')
    ap.add_argument('--switch', default='', help='кадр:направление[,кадр:направление] — смена стика на ходу'); ap.add_argument('--exit', type=int, default=60)
    ap.add_argument('--state', default='build/ps2ref/state_l1_incar.p2s'); a = ap.parse_args()
    sw = dict((int(k), v) for k, v in (x.split(':') for x in a.switch.split(',') if x))
    o = D.Original(C.OUT / 'o4' / a.out, state=a.state, extra=HOM + CAM + SCR); (o.out / 'png').mkdir(exist_ok=True)
    tsv = (o.out / 'wc.tsv').open('w'); tsv.write('кадр\tстик\tгх\tгy\tгz\tfwdx\tfwdz\tcx\tcy\tcz\tcfx\tcfy\tcfz\tscr\n'); cur = 'none'
    while True:
        s = o.step()
        if s is None: break
        f = o.frame; cmds = []
        if a.exit <= f < a.exit + 6: cmds.append('pad triangle 1')
        elif f == a.exit + 6: cmds.append('pad triangle 0')
        for item in (x for x in a.btn.split(',') if x):
            b, at = item.split('@'); at, du = at.split('+'); at, du = int(at), int(du)
            if at <= f < at + 2: cmds.append(f'pad {b} 1')
            elif at + du <= f < at + du + 2: cmds.append(f'pad {b} 0')
        if a.png and f % a.png == 0: cmds.append(f"png {o.out / 'png' / f'f{f:05d}.png'}")
        want = cur
        if a.on <= f < a.off: want = a.stick
        for k in sorted(sw):
            if f >= k: want = sw[k]
        if f >= a.off: want = 'none'
        if want != cur: cmds += stick_cmds(want); cur = want
        tsv.write(f"{f}\t{cur}\t{s['m12']:.3f}\t{s['m13']:.3f}\t{s['m14']:.3f}\t{s['m8']:.4f}\t{s['m10']:.4f}\t" + '\t'.join(f"{s[k]:.4f}" for k, _, _ in CAM) + f"\t{int(s['scr'])}\n")
        if f >= a.frames: break
        o.send(*cmds)
    o.close(); tsv.close(); print('готово', o.out)


if __name__ == '__main__':
    main()
