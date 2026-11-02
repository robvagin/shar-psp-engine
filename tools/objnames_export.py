#!/usr/bin/env python3
"""Имена анимированных объектов, события-триггеры и привязка объёмов к суставам: маленький файл рядом с уровнем (objnames.shr).

Зачем (сверка g11, круг 5): в level.shr (ANM1 и SPR1) у объектов нет имён, а движку нужны:
  * звук подвижных платформ, подъёмников, люков, крана и робо-руки по имени объекта (P11-15) и стая голубей по событию
    локатора flyingpidgeon (P11-16, событие 47): взлёт стаи pidgeon_B_group вместо сидящей pidgeon_A_group;
  * объём столкновения ящика, автомата, обезьяньего гэга L2 и паука L7 едет за суставом (P11 Д2, P12-12).
level.shr не трогаем: тот же сбор типов и объектов, что у psp_export.py, но без рисунка (пустые функции упаковки) даёт ТОТ ЖЕ порядок
объектов ANM1 и типов SPR1; движок сверяет число и признаки каждой записи и при расхождении файл не берёт.

Формат OBJN (little-endian, всё выровнено на 16):
  заголовок 32 Б: 'OBJN', u32 версия 1, u32 объектов, u32 триггеров, u32 типов, u32 смещение объектов, u32 смещение триггеров, u32 смещение типов
  объект 32 Б: char имя[24], u16 кадров клипа, u8 действие, u8 объёмов (не больше 255), u16 типов SPR1 у объекта, u16 0
  триггер 80 Б: u32 событие, u32 ящик(1)/сфера(0), f32 центр[3], f32 полуразмеры[3] (сфера: радиус в [0]), f32 оси[9], u32 0 x3
  тип 16 Б: u8 сустав, за которым едут объёмы (номер + 1; 0 нет), u8 суставов, u16 объёмов, u16 кадров клипа, char имя[10]
Запуск: python tools/objnames_export.py --level N --out PATH
"""
import sys, os, struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import numpy as np                           # noqa: E402
import animobj_export as AO                  # noqa: E402
import prp2_export as PX                     # noqa: E402
import p3dlocs                               # noqa: E402
from p3d import read, walk, string           # noqa: E402

EV_BIRDS = 47          # событие локатора flyingpidgeon (стая голубей взлетает), l2z1 и l5z1
# имя типа (часть) -> сустав, за которым едут объёмы. Сдвиг сустава в мире берётся только если он за клип уходит больше FOLLOW_MIN, м
FOLLOW = (('crate', 3), ('vending', 1), ('l2_monkiesgag', 1), ('l7_spider', 1))
FOLLOW_MIN = 0.2


def follow_joint(T):
    n = T['name'].lower()
    for key, j in FOLLOW:
        if key in n and j < len(T['joints']) and T['cols']:
            nfr, joints, keys = max(T['nfr'], 1), T['joints'], T['keys']
            ntr = max([jt['track'] for jt in joints] + [-1]) + 1
            W0 = AO.pose_world(joints, keys, ntr, 0)[j][:3, 3]
            far = max(float(np.abs(AO.pose_world(joints, keys, ntr, f)[j][:3, 3] - W0).max()) for f in range(nfr)) if ntr else 0.0
            return j + 1 if far > FOLLOW_MIN else 0
    return 0


def event_triggers(files):
    out = []
    for f in files:
        try: root = read(f)
        except Exception: continue
        for c in walk(root):
            if c.id != 0x3000005: continue
            name, p = string(c.data); kind, n = struct.unpack_from('<II', c.data, p)
            if kind != 0 or n < 1: continue
            ev = struct.unpack_from('<I', c.data, p + 8)[0]
            if ev != EV_BIRDS: continue
            locs, _ = p3dlocs.read_locators(f, str(f))
            L = next((l for l in locs if l.name == name), None)
            if L is not None and L.triggers:
                isr, ctr, h, axes = L.triggers[0]
                out.append((ev, 1 if isr else 0, ctr, h, np.asarray(axes, np.float64).reshape(-1)))
    return out


def build(level, art):
    sp = PX.StateProps(level, art, lambda f, m: [], lambda f, s: (-1, 0))
    ntype_sp = len(sp.types)
    types = [(follow_joint(T), len(T['joints']), len(T['cols']), T['nfr'], T['name']) for T in sp.types]
    ao = AO.AnimObjects(level, art, sp, None, verbose=False)
    objs = ao.objs
    trigs = event_triggers(PX.level_files(Path(art), level))
    head = 32
    off_o = head; off_t = off_o + 32 * len(objs); off_y = off_t + 80 * len(trigs)
    out = bytearray(b'OBJN' + struct.pack('<7I', 1, len(objs), len(trigs), len(types), off_o, off_t, off_y))
    for o in objs:
        out += struct.pack('<24sHBBHH', o['name'].encode('latin1', 'replace')[:23], o['nfr'] & 0xFFFF, o['action'], min(len(o['vols']), 255), len(o['types']), 0)
    for ev, rect, ctr, h, ax in trigs:
        ax9 = np.r_[ax, np.zeros(max(0, 9 - len(ax)))][:9]
        out += struct.pack('<II3f3f9f3I', ev, rect, *[float(v) for v in ctr], *[float(v) for v in h], *[float(v) for v in ax9], 0, 0, 0)
    for fj, nj, nc, nfr, name in types:
        out += struct.pack('<BBHH10s', fj, min(nj, 255), nc, nfr & 0xFFFF, name.encode('latin1', 'replace')[:9])
    out += b'\0' * (-len(out) % 16)
    return bytes(out), len(objs), len(trigs), types


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser(); ap.add_argument('--level', type=int, default=1); ap.add_argument('--out', default='')
    a = ap.parse_args()
    art = ROOT / 'hit-and-run-web/source/game/art'
    if not art.exists(): art = Path(os.path.expanduser('~/Downloads/shar-psp/hit-and-run-web/source/game/art'))
    data, no, nt, types = build(a.level, art)
    if a.out: Path(a.out).write_bytes(data)
    print(f'objnames L{a.level}: объектов {no}, триггеров {nt}, типов {len(types)}, следуют за суставом: {[(t[4], t[0]) for t in types if t[0]]}, {len(data)} Б')
