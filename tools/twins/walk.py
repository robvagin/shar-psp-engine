#!/usr/bin/env python3
"""Близнецы · О4: Гомер пешком к точке и действие (В12 будка). Бот: выйти из машины («треугольник»), идти левым стиком к цели
(стик — относительно камеры; направление «вперёд» берётся из последнего шага Гомера), у цели — кнопка действия, дальше кадры экрана.

  python3 tools/twins/walk.py --to X Z [--act triangle] [--after 600] [--out v12_booth]
Позиция Гомера — перенос матрицы 0xa24a40 (стоит вертикально, рядом с машиной игрока 0xa21870), найдена 01.10 по трём снимкам ровной ходьбы; 0x65da90 и 0x617f30 — НЕ Гомер."""
import argparse, math, os, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import common as C
import drive as D

HOMER = [('hx', 0xa24a70, 'f32'), ('hy', 0xa24a74, 'f32'), ('hz', 0xa24a78, 'f32')]
# камера оригинала (addrs.tsv, наблюдение 01.10): cam_pos 0x7aaba0, cam_fwd 0x7aabb8 — оси оригинала
CAM = [('cx', 0x7aaba0, 'f32'), ('cy', 0x7aaba4, 'f32'), ('cz', 0x7aaba8, 'f32'), ('fx', 0x7aabb8, 'f32'), ('fy', 0x7aabbc, 'f32'), ('fz', 0x7aabc0, 'f32')]


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--to', type=float, nargs='+', required=True); ap.add_argument('--act', default='triangle')
    ap.add_argument('--after', type=int, default=600); ap.add_argument('--out', default='v12_booth'); ap.add_argument('--near', type=float, default=1.6)
    ap.add_argument('--png', type=int, default=5); ap.add_argument('--seq', default='triangle@0'); ap.add_argument('--save', default='')
    ap.add_argument('--hold', default='', help='№1059: стик во время действия, имя@кадр от прихода: up@200,left@300 (up/down/left/right/none)')
    ap.add_argument('--cam', action='store_true', help='писать камеру оригинала (cam_pos, cam_fwd) в walk.tsv')
    ap.add_argument('--state', default='build/ps2ref/state_l1_incar.p2s'); ap.add_argument('--skip-exit', action='store_true', help='Гомер уже пешком: сразу идти (без выхода из машины)'); a = ap.parse_args()
    wps = [(a.to[i], a.to[i + 1]) for i in range(0, len(a.to) - 1, 2)]; wi = 0   # путь по точкам: --to x1 z1 x2 z2 …
    o = D.Original(C.OUT / 'o4' / a.out, state=a.state, extra=HOMER + (CAM if a.cam else [])); (o.out / 'png').mkdir(exist_ok=True)
    tsv = (o.out / 'walk.tsv').open('w'); tsv.write('кадр\tгх\tгz\tд\tфаза\tмашина x\tмашина z' + ('\tcam x\ty\tz\tfwd x\ty\tz\tгy' if a.cam else '') + '\n')
    phase, t0, prev, cam, cmd_ang, seg_start, hist, side_until, side, side_in_window = 'выход', 0, None, None, None, None, [], -1, 1, False
    while True:
        s = o.step()
        if s is None: break
        f = o.frame; hx, hz = s['hx'], s['hz']; tgt = wps[wi]; d = math.hypot(tgt[0] - hx, tgt[1] - hz); cmds = []
        if phase == 'выход' and a.skip_exit: phase, t0 = 'идти', f
        if phase == 'выход':
            cmds = ['pad triangle 1'] if 60 <= f < 66 else ['pad triangle 0']
            if f >= 210: phase, t0 = 'идти', f
        elif phase == 'идти':
            # кусками по 30 кадров: стик держится постоянным; по смещению за последние 20 кадров куска — куда «смотрит» стик
            # (камера), новый угол стика = направление на цель − камера (стик задаёт ход относительно камеры)
            if (f - t0) % 30 == 0:
                if seg_start is not None and cmd_ang is not None:
                    dx, dz = hx - seg_start[0], hz - seg_start[1]
                    if math.hypot(dx, dz) > 0.4 and not side_in_window: cam = D.ang(math.atan2(dx, dz) - cmd_ang)
                want = math.atan2(tgt[0] - hx, tgt[1] - hz)
                cmd_ang = D.ang(want - cam) if cam is not None else 0.0
                side_in_window = False
            if (f - t0) % 30 == 10: seg_start = (hx, hz)
            # упёрся (дверь машины, забор): за 20 кадров меньше 0,3 м — 40 кадров вбок (+90°), потом снова к цели
            hist.append((f, hx, hz)); hist[:] = hist[-11:]
            if f > side_until and len(hist) == 11 and math.hypot(hx - hist[0][1], hz - hist[0][2]) < 0.3: side_until = f + 40; side = -side
            if f <= side_until: side_in_window = True
            ang_now = (cmd_ang or 0.0) + (side * math.pi / 2 if f <= side_until else 0.0)
            ux, uy = math.sin(ang_now), math.cos(ang_now)
            cmds = [*D.axis('l', 'y', uy), *D.axis('l', 'x', ux)]
            if d < (a.near if wi == len(wps) - 1 else 1.2):
                if wi < len(wps) - 1: wi += 1; t0 = f - ((f - t0) % 30); cam_keep = cam
                else: phase, t0 = 'действие', f; cmds = [*D.axis('l', 'y', 0.0), *D.axis('l', 'x', 0.0)]
        elif phase == 'действие':   # последовательность кнопок от прихода: --seq triangle@0,cross@60 (каждая — 6 кадров)
            cmds = []
            for item in a.seq.split(','):   # кнопка@кадр[+сколько держать], по умолчанию 6 кадров
                b, at = item.split('@'); dur = 6
                if '+' in at: at, dur = at.split('+'); dur = int(dur)
                at = int(at)
                if at <= f - t0 < at + dur: cmds.append(f'pad {b} 1')
                elif at + dur <= f - t0 < at + dur + 2: cmds.append(f'pad {b} 0')
            if a.hold:
                cur_ = 'none'
                for item in a.hold.split(','):
                    nm, at = item.split('@')
                    if f - t0 >= int(at): cur_ = nm
                if cur_ != getattr(o, '_hold', 'none') or f - t0 == 0:
                    o._hold = cur_; ax_ = {'up': ('y', 1.0), 'down': ('y', -1.0), 'left': ('x', -1.0), 'right': ('x', 1.0)}.get(cur_)
                    cmds += [*D.axis('l', 'y', 0.0), *D.axis('l', 'x', 0.0)] if ax_ is None else [*D.axis('l', 'x' if ax_[0] == 'y' else 'y', 0.0), *D.axis('l', ax_[0], ax_[1])]
            if a.save and f - t0 in [int(x) for x in a.save.split(',')]: cmds.append(f"save {o.out / f'snap_{f - t0}.p2s'}")
            if f - t0 > a.after: o.p.stdin.write('end\n'); o.p.stdin.flush(); break
        prev = (hx, hz)
        if f % a.png == 0 and (phase == 'действие' or f % 50 == 0): cmds.append(f"png {o.out / 'png' / f'f{f:05d}.png'}")
        tsv.write(f"{f}\t{hx:.2f}\t{hz:.2f}\t{d:.2f}\t{phase}\t{s['x']:.2f}\t{s['z']:.2f}" +
                  (''.join(f"\t{s[k]:.3f}" for k, _, _ in CAM) + f"\t{s['hy']:.3f}" if a.cam else '') + '\n')
        if f > 3000: o.p.stdin.write('end\n'); o.p.stdin.flush(); break
        if 204 <= f <= 246 and os.environ.get('WALK_DEBUG'): print(f, round(hx, 2), round(hz, 2), cmds, flush=True)
        o.send(*cmds)
    o.close(); tsv.close()
    print(f'фаза {phase}, кадр {o.frame}, до цели {d:.2f} м → {o.out.relative_to(C.DATA)}')


if __name__ == '__main__':
    main()
