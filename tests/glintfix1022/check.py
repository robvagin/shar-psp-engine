#!/usr/bin/env python3
"""№1022 гейт: звезда-свечение ключа не уходит в землю (не режется землёй, бордюром, изгородью), её ход плавный.

  python3 tests/glintfix1022/check.py RUN_DIR

RUN_DIR — прогон эмулятора по tests/glintfix1022/scenario.txt (flag glint1022log): строки GV1022 (каждый блик у глаза за кадр: центр c, размер sc,
пульс kA). Окна журнала: до SHOT wo00 — ключ как в TEST71 (flag glintfix1022 0), между SHOT wo.. и SHOT wn00 — новое.
Метрики ключа:
  clear — запас низа звезды над землёй: c.y − 1,25 · ½ · sc · kA − земля, м (кончик большого луча = 1,25 полуоси, №941); земля = c.y старого − WRGLOW_CY (1,8);
  jump  — наибольший скачок размера sc·kA между соседними кадрами, доля.
Зелёный (новое): clear ≥ CLEAR_MIN во всех кадрах, jump ≤ JUMP_MAX. Красный контроль: у старого clear < 0 хотя бы в одном кадре (звезда в земле). Последняя строка PASS/FAIL.
"""
import re, sys, os, statistics

CLEAR_MIN, JUMP_MAX, WRGLOW_CY = .3, .04, 1.8
rx = re.compile(r'^GV1022 f=(\d+) src=(\S+) m970=(\d) c=([-\d.]+),([-\d.]+),([-\d.]+) sc=([-\d.]+) kA=([-\d.]+)')


def windows(path):
    w = {'wo': [], 'wn': []}; cur = 'wo'
    for l in open(path, errors='replace'):
        if l.startswith('SHOT wo'): cur = 'wn'
        elif l.startswith('SHOT wn'): cur = None
        m = rx.match(l)
        if m and cur and m[2] == 'wrench':
            w[cur].append((int(m[1]), (float(m[4]), float(m[5]), float(m[6])), float(m[7]), float(m[8])))
    return w


def main():
    w = windows(os.path.join(sys.argv[1], 'smoke.txt'))
    if not w['wo'] or not w['wn']:
        print('нет строк GV1022 ключа'); print('FAIL'); return 1
    ground = statistics.median(c[1] for f, c, sc, k in w['wo']) - WRGLOW_CY
    res = {}
    for key, rows in w.items():
        clear = [c[1] - 1.25 * .5 * sc * k - ground for f, c, sc, k in rows]
        rows.sort(); jump = 0.0
        for (f0, c0, s0, k0), (f1, c1, s1, k1) in zip(rows, rows[1:]):
            if f1 - f0 == 1: jump = max(jump, abs(s1 * k1 - s0 * k0) / (s0 * k0))
        res[key] = (min(clear), max(clear), jump, statistics.median(sc for f, c, sc, k in rows), len(rows))
        print(f'{key}: кадров {len(rows)}, запас низа звезды над землёй {min(clear):+.2f}…{max(clear):+.2f} м, скачок размера {jump * 100:.1f} %, размер в мире {res[key][3]:.2f} м')
    green = res['wn'][0] >= CLEAR_MIN and res['wn'][2] <= JUMP_MAX
    red = res['wo'][0] < 0.0
    print('зелёный (новое: звезда над землёй, ход плавный):', 'да' if green else 'нет', '; красный контроль (старое в земле):', 'да' if red else 'нет')
    print('PASS' if green and red else 'FAIL')
    return 0 if green and red else 1


if __name__ == '__main__':
    sys.exit(main())
