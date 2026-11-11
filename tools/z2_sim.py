#!/usr/bin/env python3
"""Шаг 8: симуляция работы области города на езде по уровню. python3 tools/z2_sim.py НАБОР [--level N] [--radius 220]
Едем по прямым через уровень 15 м/с (шаг 0,5 с, 20 минут), клетки круга радиуса r грузятся ближние первыми, отдаются за r+20 (гистерезис). Область города — отрезок байт; аллокаторы:
  плиты (cellpool.h): классы КБ, плита ≥ 4 ячеек (≤ 32), кратна 256 КБ, плита целиком в области лучшим подходящим, пустая плита возвращается;
  прямой (как zone3.h): клетка лучшим подходящим блоком области, заголовок 64 Б, выравнивание 64, склейка соседей.
Для каждого аллокатора ищется наименьшая ёмкость области (шаг 64 КБ), при которой за езду ни одна клетка не получила отказ. Это модель по таблицам клеток, не журнал железа."""
import sys, math, random, struct, bisect
from pathlib import Path

class Arena:
    def __init__(self, cap): self.cap = cap; self.free = [(0, cap)]; self.fails = 0   # свободные отрезки (старт, длина), по старту
    def alloc(self, n):
        best = None
        for k, (s, l) in enumerate(self.free):
            if l >= n and (best is None or l < self.free[best][1]): best = k
        if best is None: return None
        s, l = self.free[best]
        if l == n: self.free.pop(best)
        else: self.free[best] = (s + n, l - n)
        return s
    def release(self, s, n):
        k = bisect.bisect_left(self.free, (s, 0)); self.free.insert(k, (s, n))
        if k + 1 < len(self.free) and self.free[k][0] + self.free[k][1] == self.free[k + 1][0]:
            self.free[k] = (self.free[k][0], self.free[k][1] + self.free[k + 1][1]); self.free.pop(k + 1)
        if k > 0 and self.free[k - 1][0] + self.free[k - 1][1] == self.free[k][0]:
            self.free[k - 1] = (self.free[k - 1][0], self.free[k - 1][1] + self.free[k][1]); self.free.pop(k)

def slab_bytes(cell):
    b = 256 << 10
    while b // cell < 4: b += 256 << 10
    if b // cell > 32: b = 32 * cell
    return b

class SlabPool:
    def __init__(self, cap, classes_kb):
        self.a = Arena(cap); self.cls = [k << 10 for k in classes_kb]; self.slabs = []   # плита: dict(cls, start, n, used set)
    def alloc(self, size):
        c = next((c for c in self.cls if c >= size), None)
        if c is None:
            s = self.a.alloc((size + 63) // 64 * 64); return None if s is None else ('d', s, (size + 63) // 64 * 64)
        best = None
        for sl in self.slabs:
            if sl['c'] == c and len(sl['used']) < sl['n'] and (best is None or len(sl['used']) > len(best['used'])): best = sl
        if best is None:
            sb = slab_bytes(c); s = self.a.alloc(sb)
            if s is None: return None
            best = dict(c=c, s=s, n=sb // c, sb=sb, used=set()); self.slabs.append(best)
        k = next(i for i in range(best['n']) if i not in best['used']); best['used'].add(k); return ('s', best, k)
    def free(self, h):
        if h[0] == 'd': self.a.release(h[1], h[2]); return
        sl, k = h[1], h[2]; sl['used'].discard(k)
        if not sl['used']: self.a.release(sl['s'], sl['sb']); self.slabs.remove(sl)

class Direct:
    def __init__(self, cap): self.a = Arena(cap)
    def alloc(self, size):
        n = (size + 63) // 64 * 64 + 64; s = self.a.alloc(n); return None if s is None else (s, n)
    def free(self, h): self.a.release(*h)

def level_cells(path):
    d = Path(path).read_bytes(); toff, ncell, coff = struct.unpack_from('<III', d, 76); out = []
    for i in range(ncell):
        r = d[coff + 48 * i:coff + 48 * i + 48]; bb = struct.unpack_from('<6f', r, 0); owner, off, size = struct.unpack_from('<III', r, 32)
        if size and not owner: out.append(((bb[0] + bb[3]) / 2, (bb[2] + bb[5]) / 2, size))
    return out

def run(cells, make, cap, R, route):
    pool = make(cap); live = {}; fails = 0; peak = 0
    for x, z in route:
        want = sorted((math.hypot(c[0] - x, c[1] - z), i) for i, c in enumerate(cells) if math.hypot(c[0] - x, c[1] - z) < R)
        for i in [i for i in live if math.hypot(cells[i][0] - x, cells[i][1] - z) > R + 20]: pool.free(live.pop(i))
        for _, i in want:
            if i in live: continue
            h = pool.alloc(cells[i][2])
            if h is None: fails += 1
            else: live[i] = h
    return fails

def make_route(cells, steps, seed):
    random.seed(seed); xs = [c[0] for c in cells]; zs = [c[1] for c in cells]
    x, z = random.choice(cells)[:2]; hd = random.uniform(0, 6.28); out = []
    for k in range(steps):
        if k % 120 == 0: hd += random.uniform(-1.2, 1.2)
        x += math.cos(hd) * 7.5; z += math.sin(hd) * 7.5
        if not (min(xs) < x < max(xs) and min(zs) < z < max(zs)): hd += 3.14; x = min(max(x, min(xs)), max(xs)); z = min(max(z, min(zs)), max(zs))
        out.append((x, z))
    return out

def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]; S = Path(a[0])
    lv = int(sys.argv[sys.argv.index('--level') + 1]) if '--level' in sys.argv else 4
    R = float(sys.argv[sys.argv.index('--radius') + 1]) if '--radius' in sys.argv else 220.
    cells = level_cells(S / ('level.shr' if lv == 1 else f'l{lv}/level.shr')); route = make_route(cells, 600, lv)[::4]   # 150 точек: каждые 2 с езды
    allocs = [('плиты нынешние 16/32/64/128/192', lambda cap: SlabPool(cap, [16, 32, 64, 128, 192])),
              ('плиты 16/32/64/128/256', lambda cap: SlabPool(cap, [16, 32, 64, 128, 256])),
              ('плиты мельче 8..192', lambda cap: SlabPool(cap, [8, 12, 16, 24, 32, 48, 64, 96, 128, 192])),
              ('прямой лучший подходящий (как zone3.h)', lambda cap: Direct(cap))]
    print(f'L{lv}, радиус {R:.0f} м, клеток {len(cells)}, точек езды {len(route)}')
    for name, mk in allocs:
        lo, hi = 1 << 20, 24 << 20
        while hi - lo > (64 << 10):
            mid = (lo + hi) // 2 // (64 << 10) * (64 << 10)
            if run(cells, mk, mid, R, route) == 0: hi = mid
            else: lo = mid
        print(f'  {name:<42} наименьшая ёмкость без отказов: {hi / 1048576:5.2f} МБ')
if __name__ == '__main__': main()
