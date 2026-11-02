#!/usr/bin/env python3
"""Круг 6, g08-sound: состав и размер пакета звука на каждом уровне (Мак, CPU).
  pack_check.py DIR [LIMIT]   DIR содержит l1..l7 с pack_report.json и sfx.sab (tools/sound_export.py --level N --out DIR/lN)
Проверяет: резидентный банк sfx.sab не больше LIMIT (по умолчанию 2 162 688 Б = слот 2112 КБ + запас, правка №1069); звуки подвижных объектов
(sndAnimObj) на своих уровнях есть — коротким клипом в банке или дорожкой anim_<клип> в music.sap; четыре образца громкости (UI_EV_STING) в банке;
монетки подбора (monkey_collect на L2, nuclear_waste_collect на L7, level_N_pickup_sfx своего уровня). Код 0 — всё в порядке.
Красный контроль: запуск на пакете круга 4 (build/snd4) — там звуков объектов и образцов нет, проверка обязана упасть."""
import json, os, sys
d = sys.argv[1]; limit = int(sys.argv[2]) if len(sys.argv) > 2 else 2162688
# уровень -> (резидентные клипы, дорожки anim_*)
OBJ = {1: ({'trapdoor', 'platform_01', 'platform_02', 'plantdoor'}, {'anim_aplatform6', 'anim_duff_truck'}),
       2: (set(), {'anim_elevator', 'anim_pigeon_takeoff'}),
       3: (set(), {'anim_robo_arm'}),
       4: ({'trapdoor', 'platform_01', 'platform_02', 'plantdoor'}, {'anim_aplatform6', 'anim_duff_truck', 'anim_gag_library'}),
       5: ({'platform_02'}, {'anim_elevator', 'anim_pigeon_takeoff'}),
       6: (set(), {'anim_robo_arm'}),
       7: (set(), {'anim_duff_truck'})}
STING = {'music_stinger', 'sfx_stinger', 'car_stinger', 'dialog_stinger'}
PICK = {2: 'monkey_collect', 7: 'nuclear_waste_collect'}
bad = 0
def fail(m):
    global bad; bad += 1; print('FAIL', m)
for lv in range(1, 8):
    p = os.path.join(d, 'l%d' % lv)
    try: rep = json.load(open(os.path.join(p, 'pack_report.json'))); size = os.path.getsize(os.path.join(p, 'sfx.sab'))
    except Exception as e: fail('L%d: нет пакета (%s)' % (lv, e)); continue
    res = {x['res'] for x in rep['sfx']}; tracks = {x['name'] for x in rep.get('music', [])} | set(rep.get('positional', []))
    pos = {x['name'] for x in rep['music']} if rep.get('music') and isinstance(rep['music'][0], dict) else set()
    tracks |= pos
    print('L%d sfx.sab %d Б, запас %d Б' % (lv, size, limit - size))
    if size > limit: fail('L%d банк %d Б больше лимита %d' % (lv, size, limit))
    r, t = OBJ[lv]
    for n in r:
        if n not in res: fail('L%d нет клипа объекта %s в банке' % (lv, n))
    for n in t:
        if n not in tracks: fail('L%d нет дорожки %s в music.sap' % (lv, n))
    for n in STING:
        if n not in res: fail('L%d нет образца громкости %s в банке' % (lv, n))
    need = PICK.get(lv, 'level_%d_pickup_sfx' % lv)
    if need not in res and 'card_collect' not in res: fail('L%d нет клипа подбора %s и запасного card_collect' % (lv, need))   # одинаковые файлы экспорт сводит в один ресурс
    if 'amb_room_small_01' not in tracks: fail('L%d нет атмосферы комнаты amb_room_small_01 (комнаты и миссии «дом»)' % lv)
print('PASS' if not bad else 'FAIL (%d)' % bad)
sys.exit(1 if bad else 0)
