#!/usr/bin/env python3
"""№1125: модель глубины 16 бит для окон вдали (не эмулятор): мигает ли плоское окно относительно фасада и видно ли оно сквозь передний дом. Данные: зазоры окно–фасад по настоящим
уровням (L4 текстуры 129/294, L7 132; те же окна, что в winspr-lN.shr), ноль зазора включён. Модель на каждое окно и расстояние d:
  шаг глубины LSB(d) = (far − near)·d² / (far·near·65535): основной проход near 1,5 (zfNear), far 260; дальний проход near = 0,85·farPlainD (100), far 260 (окна дальше farPlainD — в дальнем проходе);
  кадр = новые случайные ошибки вершин окна и фасада (ошибка float24 меняется с положением камеры), равномерно ±σ по нормали: старое — вершины в абсолютных координатах, σ = 2 см (500–900 м от начала координат,
  tools/seams_ge.py); новое — в осях камеры, σ = полшага float24 на расстоянии d (мантисса 15 бит); глубина пикселя округляется до целого (случайная фаза); окно проходит GEQUAL, если z_окна + смещение ≥ z_фасада.
  Мигающее окно: доля прошедших кадров из 40 между 5 % и 95 %. Сквозь дом: окно за передним фасадом на h м проходит, если h < вынос (WINZ_K·LSB + WINZ_C, по нормали; смещение глубины нового пути 0). Старое (TEST84): смещение 3 шага, вершины в абсолютных координатах; в дальнем проходе оно ещё и стояло не на месте.
python tests/g12/winz_model.py   — печатает таблицу и проверяет: новое — 0 мигающих на 20/60/120 м и L4, L7; старое — мигает; «красный контроль»: новое с выносом 0 и смещением 3 мигает."""
import sys, os, math, struct, collections
import numpy as np
from pathlib import Path
R = Path(__file__).resolve().parents[2]; sys.path[:0] = [str(R / 'tools')]
S83 = Path.home() / 'Downloads/shar-psp-integ/build/test83/PSP/GAME/SHARTEST15'
CACHE = Path('/private/tmp/claude-501/-Users-USER-Downloads-shar-psp/d09a937d-c6b5-4716-8b24-9b181c0e2b1e/scratchpad/g12w')
def gaps(L, tex):
    f = CACHE / f'gaps_l{L}.npy'
    if f.exists(): return np.load(f)
    import gap_windows as G
    p = S83 / ('level.shr' if L == 1 else f'l{L}/level.shr'); wi, g = G.analyse(str(p), tex)
    a = np.array([x for x in g if x is not None]); np.save(f, a); return a
NEAR_MAIN, NEAR_FAR, FAR = 1.5, 85.0, 260.0
def lsb(d, far_pass):
    n = NEAR_FAR if far_pass else NEAR_MAIN; return (FAR - n) * d * d / (FAR * n * 65535.0)
def zq(dv, far_pass, phase):
    n = NEAR_FAR if far_pass else NEAR_MAIN; z = 65535.0 * (1.0 - (FAR / (FAR - n)) * (1.0 - n / dv)); return np.floor(z + phase)
def flicker(g, d, far_pass, sigma, bias, eps, frames=40, seed=1):
    rng = np.random.default_rng(seed); n = len(g); passes = np.zeros(n)
    for _ in range(frames):
        nf = rng.uniform(-sigma, sigma, n); nw = rng.uniform(-sigma, sigma, n); ph = rng.uniform(0, 1, n)
        dv_wall = d + nw; dv_win = d - g - eps + nf   # окно ближе к камере на зазор и вынос
        passes += (zq(dv_win, far_pass, ph) + bias >= zq(dv_wall, far_pass, ph))
    p = passes / frames
    return int(np.sum((p > 0.05) & (p < 0.95))), p
def sigma_new(d): e = math.floor(math.log2(max(d, 1.0))); return 0.5 * 2.0 ** (e - 15)
def run():
    rows = []; bad = []
    for L, tex in ((4, [129, 294]), (7, [132])):
        g = gaps(L, tex); g = np.r_[g, np.zeros(max(5, len(g) // 20))]   # нулевой зазор (копланарные, 38+ окон) — всегда в выборке
        for d in (20.0, 60.0, 120.0):
            far = d > 100.0; l = lsb(d, far)
            old, _ = flicker(g, d, far, 0.02, 3, 0.0)
            eps = 4 * l + 0.02; new, _ = flicker(g, d, far, sigma_new(d), 0, eps)   # новое: смещение глубины 0, запас — вынос
            red, _ = flicker(g, d, far, sigma_new(d), 0, 0.0)   # красный контроль: ни выноса, ни смещения — окно в плоскости фасада
            rows.append((L, d, far, l, len(g), old, new, red, eps))
    return rows
if __name__ == '__main__':
    rows = run(); ok = True
    print('L  d,м  проход   LSB,мм   окон   мигает: старое  новое  без запаса   «сквозь дом» до, м (новое)')
    for L, d, far, l, n, old, new, red, thr in rows:
        print(f'{L}  {d:4.0f}  {"дальний" if far else "основной"}  {l * 1000:7.1f}  {n:5d}   {old:6d} {new:9d} {red:10d}   {thr:8.3f}')
        ok &= new == 0 and (old > 0 if d != 60.0 else True)   # на 60 м ошибка float24 (2 см) меньше трёх шагов глубины (11 см): старое там не мигало
    print('\nновое: 0 мигающих на 20, 60 и 120 м (L4, L7); старое мигает на 20 и 120; красный контроль:', 'сработал' if any(r[7] > 0 for r in rows if r[1] >= 60) else 'НЕ сработал')
    sys.exit(0 if ok and any(r[7] > 0 for r in rows if r[1] >= 60) else 1)
