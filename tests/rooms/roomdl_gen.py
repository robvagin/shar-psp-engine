#!/usr/bin/env python3
"""ТЕСТ90 (листы комнат): собирает сценарий tests/rooms/roomdl1190.txt из камер комнат tests/rooms/emu_cams_l1.txt и emu_cams_l4.txt: блоки дверей берутся как есть, в каждой комнате по 2 точки
(одна и та же точка трижды: листы a, прямой путь b, листы c, по три кадра; сравнение только неподвижных пикселей: уборщица и прочее медленное движение отсекается кадрами a и c, между которыми стоит b). Запуск: python3 tests/rooms/roomdl_gen.py"""
import re
from pathlib import Path
R = Path(__file__).resolve().parent

def blocks(fn):
    cur = None; out = []
    for ln in (R / fn).read_text(encoding='utf-8').splitlines():
        if ln.startswith('scene '):
            cur = [ln.split()[1], []]; out.append(cur)
        elif cur is not None and ln.strip() and not ln.startswith('#'):
            cur[1].append(ln)
    return out

def points(name, foot):
    s = []
    for tag, mode in (('a', 1), ('b', 0), ('c', 1)):   # листы, прямой путь, листы ещё раз; по три кадра с интервалом 0,5 с (неподвижные пиксели — комната, меняющиеся — Гомер, прохожие, значок)
        s += ['flag roomdl %d' % mode, 'scene RDL%s_%s' % (tag, name), foot, 'wait 2', 'shot %s1_%s' % (tag, name), 'wait 0.5', 'shot %s2_%s' % (tag, name), 'wait 0.5', 'shot %s3_%s' % (tag, name), 'wait 2']
    return s

def points_pause(name, foot):
    """картинка: пауза замораживает Гомера и прохожих, в паузе переключаем путь рисования: листы a, прямой b, листы c — по три кадра"""
    s = ['scene RDLp_%s' % name, foot, 'wait 2', 'press start 0.3', 'wait 1.5']
    for tag, mode in (('a', 1), ('b', 0), ('c', 1)):
        s += ['flag roomdl %d' % mode, 'wait 1', 'shot %s1_%s' % (tag, name), 'wait 0.5', 'shot %s2_%s' % (tag, name), 'wait 0.5', 'shot %s3_%s' % (tag, name)]
    s += ['flag roomdl 1', 'press start 0.3', 'wait 1.5']
    return s

def level(fn, rooms, pts=points):
    out = []; bl = blocks(fn); i = 0
    while i < len(bl):
        nm, lines = bl[i]
        if nm.endswith('_door') and any(nm.startswith(r) for r in rooms):
            out += ['scene ' + nm] + lines
            k = 0; j = i + 1
            while j < len(bl) and not bl[j][0].endswith('_door') and k < 2:
                nm2, l2 = bl[j]; foot = next((x for x in l2 if x.startswith('foot')), None)
                if foot and not nm2.startswith('L'):
                    out += pts(nm2, foot); k += 1
                j += 1
        i += 1
    return out

def build(pts, title):
    lines = ['# ТЕСТ90 (листы комнат, ROOMDL), создан tests/rooms/roomdl_gen.py: ' + title, 'flag camdet 1', 'scene start', 'wait 3']
    lines += level('emu_cams_l1.txt', ['SimpsonsHouse', 'KwikEMart', 'SpringfieldElementary'], pts)
    lines += ['flag roomdl 1', 'scene L4_load', 'flag golevel 4', 'wait 20']
    lines += level('emu_cams_l4.txt', ['SimpsonsHouse', 'BartRoom'], pts)
    return lines + ['end']

(R / 'roomdl1190.txt').write_text('\n'.join(build(points_pause, 'картинка в паузе, листы a и c, прямой b; гейт tests/rooms/roomdl1190.py')) + '\n', encoding='utf-8')
(R / 'roomdl1190_cost.txt').write_text('\n'.join(build(points, 'цена кадра: листы a и c, прямой b, по 3 с в каждом режиме (PHSEC сцен RDLa_/RDLb_/RDLc_)')) + '\n', encoding='utf-8')
print('готово')
