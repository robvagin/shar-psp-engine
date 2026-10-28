#!/usr/bin/env python3
"""Близнецы · правка Роба 342: эталон прыжков оригинала с трамплинов → tests/jumps/ramps_ps2.tsv (гейт tests/jumps/twin.sh).

Заезды оригинала — бот tools/twins/rampjump.py (вариант jump.py с vy, тангажом и снимком точки разгона): по дорогам до точки разгона, разгон по линии трамплина
с газом до скорости бота, кромка, полёт, посадка; трасса jump.tsv каждые 2 кадра (PS2 PAL 50 к/с, физика 25 Гц).
Из трассы берутся: точка синхронизации на линии разгона (A0 м до кромки: место, курс, скорость), скорость на кромке, вершина
полёта (начало модели оригинала; над низом колёс оно на 0,70 м — медиана 14 заездов, сравнение с землёй порта в точке
синхронизации, 0,67–0,76), удар (где по линии от кромки машина впервые коснулась: скачок vy вверх больше 2 м/с при
падении быстрее 1 м/с), время от кромки до удара, итог (где машина через 3 с после удара). Оси переводятся в оси движка
(z отзеркален, курс π − курс оригинала). Те же правила считает tests/jumps/twin.c для порта.

  python3 tools/twins/jumpref.py ID:трамплин:км/ч:путь/к/jump.tsv ... [--out tests/jumps/ramps_ps2.tsv]
Трамплины (кромка и направление — tools/cartest/routes.py, build/sandbox/routes-l1.tsv, оси движка) — таблица RAMPS ниже."""
import argparse, math, sys
from pathlib import Path

# трамплин: кромка (x, y, z движка), направление разгона (dx, dz движка), A0 — точка синхронизации, м до кромки (ровное место
# на линии разгона перед подъёмом; у jumpzone4 подъём длинный — на склоне)
RAMPS = {
    'jumpzone16': ((-120.5, 3.12, 554.33), (0.0, -1.0), -30.0),   # F13: Роб 05.10 «F13-3, как будто ускоритель»
    'jumpzone17': ((121.37, 8.71, 213.02), (0.0, 1.0), -30.0),    # J10: дом (правки 47, 247; Роб 05.10 «J10-5, будто переносят»)
    'jumpzone2': ((-53.76, 4.21, 629.42), (-1.0, 0.0), -35.0),    # G14
    'jumpzone4': ((-119.46, 10.56, 439.05), (0.0, -1.0), -45.0),  # F12: прыжок через провал моста
    'jumpzone3': ((127.89, 5.75, 160.65), (-1.0, 0.0), -25.0),    # J9-5 (№705 ч.2): подъезд по -x, ворота зоны поперёк въезда (x 136,8)
}


def read(path):
    rows = [l.rstrip('\n').split('\t') for l in open(path, encoding='utf-8')]
    hdr = rows[0]; ix = {k: i for i, k in enumerate(hdr)}
    out = []
    for r in rows[1:]:
        if len(r) < len(hdr): continue
        out.append(dict(f=int(r[ix['кадр']]), x=float(r[ix['x']]), y=float(r[ix['y']]), z=-float(r[ix['z']]),   # z движка
                        h=math.pi - math.radians(float(r[ix['курс']])), v=float(r[ix['v']]),
                        vy=float(r[ix['vy']]) if 'vy' in ix else None, phase=r[ix['фаза']]))
    return out


def metrics(rows, lip, d, a0):
    lx, ly, lz = lip; dx, dz = d
    for s in rows: s['a'] = (s['x'] - lx) * dx + (s['z'] - lz) * dz; s['t'] = s['f'] / 50.0
    run = [s for s in rows if s['phase'] != 'дорога']
    i0 = next(i for i, s in enumerate(run) if s['a'] >= a0)
    s0 = run[i0]; run = run[i0:]
    tlip = vlip = None
    for p, q in zip(run, run[1:]):
        if p['a'] < 0 <= q['a']:
            k = (0 - p['a']) / (q['a'] - p['a']); tlip = p['t'] + k * (q['t'] - p['t']); vlip = p['v'] + k * (q['v'] - p['v']); break
    # удар: скачок vy вверх больше 2 м/с при падении быстрее 1 м/с; касание вскользь (за 1 с после него машина поднялась выше
    # места удара на 0,5 м, а за 3 с ушла ниже него на 1,5 м дальше 5 м по линии — задела конёк крыши и улетела) не посадка
    apex, timp, land, first = -1e9, None, None, None
    for k in range(1, len(run)):
        p, q = run[k - 1], run[k]
        if q['a'] <= -8: continue
        if tlip is not None and q['t'] - tlip > 3.0 and first is None: break
        if first is None and q['y'] > apex: apex = q['y']
        if tlip is not None and q['t'] >= tlip and q['vy'] - p['vy'] > 2.0 and p['vy'] < -1.0:
            if first is None: first = k
            nxt = [s for s in run[k:] if s['t'] <= q['t'] + 3.0]
            up = max(s['y'] for s in nxt if s['t'] <= q['t'] + 1.0) - q['y'] > 0.5
            down = any(s['y'] < q['y'] - 1.5 and s['a'] > q['a'] + 5.0 for s in nxt)
            if up and down: continue   # вскользь: подскочила и улетела дальше вниз (конёк крыши)
            timp, land = q['t'], q['a']; break
    end = run[-1]['a']
    if timp is not None:
        after = [s for s in run if s['t'] >= timp + 3.0]
        if after: end = after[0]['a']
    return dict(x0=s0['x'], y0=s0['y'], z0=s0['z'], h0=math.atan2(math.sin(s0['h']), math.cos(s0['h'])), v0=s0['v'],
                vlip=vlip or 0.0, rise=apex - s0['y'], apex=apex, land=land if land is not None else 0.0, tland=(timp - tlip) if timp and tlip else 0.0, end=end)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('runs', nargs='+'); ap.add_argument('--out', default=None); a = ap.parse_args()
    lines = ['# правка 342: прыжки оригинала (близнец PS2, tools/twins/jumpref.py); оси движка; см. knowledge/facts/ps2-ramps.yaml',
             '# id\tтрамплин\tкромка x y z\tdx dz\tсинхр. x y z\tкурс\tv0 м/с\tцель м/с\tкромка м/с\tвершина (начало модели) м\tудар м\tдо удара с\tитог м']
    for spec in a.runs:
        rid, ramp, kmh, path = spec.split(':', 3)
        lip, d, a0 = RAMPS[ramp]; m = metrics(read(path), lip, d, a0)
        lines.append('\t'.join([rid, ramp, *(f'{v:.2f}' for v in lip), *(f'{v:.3f}' for v in d), f"{m['x0']:.3f}", f"{m['y0']:.3f}", f"{m['z0']:.3f}",
                                f"{m['h0']:.4f}", f"{m['v0']:.3f}", f'{float(kmh) / 3.6:.3f}', f"{m['vlip']:.2f}", f"{m['apex']:.3f}", f"{m['land']:.2f}",
                                f"{m['tland']:.2f}", f"{m['end']:.1f}"]))
        print(f"{rid:12s} {ramp:11s} кромка {m['vlip'] * 3.6:5.1f} км/ч  подъём {m['rise']:5.2f} м  удар {m['land']:6.1f} м через {m['tland']:.2f} с  итог {m['end']:6.1f} м")
    if a.out: Path(a.out).write_text('\n'.join(lines) + '\n'); print('→', a.out)


if __name__ == '__main__':
    main()
