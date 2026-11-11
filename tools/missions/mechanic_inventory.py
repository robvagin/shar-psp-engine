#!/usr/bin/env python3
"""Опись миссий для бота «Механик» (Опора-93): по каждой миссии L1-L7 и каждой стадии — тип механики, точка цели,
предметы, персонажи, машины, разговоры, ролики, условия, итоги. Источник один — скрипты диска через tools/missions/compile.py
(те же данные играет порт); текста оригинала здесь нет.

  python3 tools/missions/mechanic_inventory.py [--out docs/state/mechanic_inventory.json] [--md docs/handoff/МЕХАНИК_ОПИСЬ_МИССИЙ_93.md]

Координаты — мировые оси порта, как у локаторов диска (x, y, z), без пересчёта: ровно то, что видит игра при телепорте.
"""
import argparse, json, re, sys, tempfile
from pathlib import Path
from collections import Counter

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
GAME = ROOT / 'hit-and-run-web/source/game'
sys.path.insert(0, str(HERE))
import compile as C

MISSION_FLAGS = [(1, 'bonus'), (2, 'streetrace'), (4, 'gamble'), (8, 'bonusmission'), (16, 'noscript'), (32, 'levelover'), (64, 'gameover'), (128, 'forcedcar'), (256, 'transition')]
STAGE_FLAGS = [(1, 'final'), (2, 'checkpoint'), (4, 'locked'), (8, 'showcomplete'), (16, 'mustaction'), (32, 'gotodialogoff'), (64, 'elapsed'), (128, 'musicon')]
MECH_RU = {
    'goto': 'доехать/дойти до точки', 'talkto': 'поговорить (подойти к персонажу)', 'dialogue': 'сцена разговора (катсцена на движке)',
    'race': 'гонка (чекпойнты, позиции)', 'getin': 'сесть в машину', 'timer': 'таймер/пауза', 'interior': 'войти в интерьер',
    'destroy': 'уничтожить машину', 'delivery': 'собрать предметы (подбор на карте)', 'dump': 'подобрать предметы, выпавшие из машины цели',
    'losetail': 'оторваться от погони', 'follow': 'ехать за машиной', 'coins': 'заплатить монетами', 'buycar': 'купить машину (экран)',
    'fmv': 'ролик', 'gooutside': 'выйти из интерьера', 'buyskin': 'купить костюм (экран)', 'pickupitem': 'подобрать предмет-состояние',
    'destroyboss': 'уничтожить босса'}
DROP_MECH = {'dump'}
LEVEL_NAMES = {1: 'Springfield (лето)', 2: 'Springfield (дождь/ночь)', 3: 'Shelbyville', 4: 'Springfield (осень)', 5: 'Halloween', 6: 'Springfield (зима)', 7: 'Springfield (весна)'}
CONV_LINE = re.compile(r'c_(.+?)_(\d+)_(convinit|convrespond|[a-z]+)_([a-z]+)_')


def flags(v, table):
    return [n for bit, n in table if v & bit]


def pos_of(a):
    return a['pos'] if isinstance(a, dict) and 'pos' in a else None


def loc_of(a):
    return a['loc'] if isinstance(a, dict) and 'loc' in a else None


def port_chr_files():
    """Список chars/*.chr, который грузит порт (msn_view.c npcFiles); modelFor(имя) берёт chars/<имя>.chr."""
    src = (ROOT / 'viewer/msn_view.c').read_text(errors='ignore')
    m = re.search(r'npcFiles\[\]\s*=\s*\{(.*?)\};', src, re.S)
    return set(re.findall(r'"chars/([^"]+)\.chr"', m.group(1))) if m else set()


PORT_CHR = port_chr_files()


def port_model(name):
    """Что уйдёт в modelFor: имя NPC; port_ok — лежит ли chars/<имя>.chr в списке порта (иначе модель-заглушка/замена)."""
    if not isinstance(name, str): return None
    if name == 'npd':   # общий риг анимаций (art/chars/npd_m.p3d 2,4 КБ: скелет без сетки и кожи), в оригинале тоже не рисуется: гость скрыт (hidden_moe, StayInBlack)
        return {'name': name, 'port_ok': True, 'skinless_rig': True, 'expected_visible': False}
    return {'name': name, 'port_ok': name in PORT_CHR}


def model_file(name):
    """art/chars/<имя>_m.p3d (имена длиннее 5 знаков урезаны на диске: kearney -> kearne_m)."""
    for cand in (name, name[:5], name[:4]):
        p = GAME / 'art/chars' / f'{cand}_m.p3d'
        if p.exists():
            return f'art/chars/{p.name}'
    return None


def car_file(name):
    p = GAME / 'art/cars' / f'{name}.p3d'
    return f'art/cars/{p.name}' if p.exists() else None


def parse_rewards():
    out = {}
    txt = (GAME / 'scripts/missions/rewards.mfk').read_text(errors='ignore')
    for m in re.finditer(r'BindReward\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*(\d+)(?:\s*,\s*(\d+)\s*,\s*"([^"]+)")?\s*\)', txt):
        name, f, typ, quest, lv, cost, seller = m.groups()
        out.setdefault(int(lv), []).append({'name': name, 'file': f.replace('\\', '/'), 'type': typ, 'quest': quest, 'cost': int(cost) if cost else None, 'seller': seller})
    gags = {int(a): int(b) for a, b in re.findall(r'SetTotalGags\(\s*(\d+)\s*,\s*(\d+)\s*\)', txt)}
    return out, gags


def kind_of(mid, mflags):
    if 'streetrace' in mflags: return 'streetrace'
    if 'gamble' in mflags: return 'wager_race'
    if 'bonusmission' in mflags or mid.startswith('bm'): return 'bonus_mission'
    if 'levelover' in mflags or 'gameover' in mflags: return 'final'
    return 'story'


def process_level(level, d, rewards):
    convs = d['conversations']
    out = {'level': level, 'name': LEVEL_NAMES.get(level), 'missions': []}
    for mi, m in enumerate(d['missions']):
        mid = m['id']; mfl = flags(m['flags'], MISSION_FLAGS); kind = kind_of(mid, mfl)
        md = {'mi': mi, 'id': mid, 'title': m['title'], 'kind': kind, 'flags': mfl, 'start': {}, 'stages': [], 'end': {}}
        npcs = {}; vehicles = {}; idx = 0
        player = None
        for ph in m['phases']:
            intro = ph['name'].endswith('sd')
            # --- старт фазы
            for c in ph['setup']:
                op, a = c['op'], c['args']
                if op in ('SetMissionResetPlayerInCar', 'SetMissionResetPlayerOutCar') and a:
                    md['start'].setdefault('player_reset', []).append({'phase': ph['name'], 'pos': pos_of(a[0]), 'loc': loc_of(a[0]), 'in_car': op.endswith('InCar')})
                elif op in ('SetMissionStartCameraName', 'SetAnimatedCameraName') and a:
                    md['start'].setdefault('camera', {})[ph['name']] = a[0]
                elif op == 'SetForcedCar':
                    md['start']['forced_car'] = a[0] if a else True
                elif op == 'InitLevelPlayerVehicle' and len(a) >= 2:
                    md['start'].setdefault('level_vehicle', []).append({'car': a[0], 'pos': pos_of(a[1]), 'loc': loc_of(a[1]), 'mode': a[2] if len(a) > 2 else None})
                elif op == 'PlacePlayerCar' and len(a) >= 2:
                    md['start'].setdefault('place_car', []).append({'car': a[0], 'pos': pos_of(a[1]), 'loc': loc_of(a[1])})
                elif op == 'SetDynaLoadData' and a:
                    md['start'].setdefault('dynaload', []).append(a[0])
            loads = [c['args'][0] for c in ph['load'] if c['op'] == 'LoadDisposableCar' and c['args']]
            if loads:
                md['start'].setdefault('disposable_cars', []).extend(x.replace('\\', '/') for x in loads)
            for si, st in enumerate(ph['stages']):
                s = {'i': idx, 'mi': mi, 'phase_idx': (0 if ph['name'].endswith('sd') else 1), 'stage_idx': si, 'rt_key': f"{mid}.{0 if ph['name'].endswith('sd') else 1}.{si}", 'phase': ph['name'], 'intro_phase': intro, 'mech': st['obj'], 'mech_ru': MECH_RU.get(st['obj'], st['obj']),
                     'text': st['text'], 'stage_flags': flags(st['flags'], STAGE_FLAGS), 'conditions': list(st['conds']), 'target': None,
                     'waypoints': [], 'items': None, 'npcs': [], 'vehicles': [], 'src': st['cmds'][0]['src'] if st['cmds'] else None}
                idx += 1
                colls = []; ops = {}
                stage_time = None; add_time = []; laps = None; fee = None; par = None; req_pos = None; cond = {}
                for c in st['cmds']:
                    op, a = c['op'], c['args']; ops.setdefault(op, []).append(a)
                    if op == 'AddNPC' and len(a) >= 2:
                        npcs[a[0]] = {'name': a[0], 'model': model_file(a[0]), 'port_model': port_model(a[0]), 'pos': pos_of(a[1]), 'loc': loc_of(a[1])}
                        s['npcs'].append(npcs[a[0]])
                    elif op == 'AddStageVehicle' and len(a) >= 2:
                        v = {'name': a[0], 'model': car_file(a[0]) if isinstance(a[0], str) else None, 'pos': pos_of(a[1]), 'loc': loc_of(a[1]),
                             'ai_path': (a[3].replace('\\', '/') if len(a) > 3 and isinstance(a[3], str) and a[3] not in ('', 'NULL') else None),
                             'driver': (a[4] if len(a) > 4 else None)}
                        vehicles[a[0]] = v; s['vehicles'].append(v)
                    elif op == 'AddStageCharacter' and len(a) >= 2:
                        s.setdefault('characters', []).append({'name': a[0], 'port_model': port_model(a[0]), 'pos': pos_of(a[1]), 'car_pos': pos_of(a[4]) if len(a) > 4 else None})
                    elif op == 'AddStageWaypoint' and a:
                        p = pos_of(a[0])
                        if p: s['waypoints'].append(p)
                    elif op == 'AddCollectible' and a:
                        colls.append({'pos': pos_of(a[0]), 'loc': loc_of(a[0]), 'shape': a[1] if len(a) > 1 else None})
                    elif op == 'AddCollectibleStateProp' and len(a) >= 2:
                        colls.append({'pos': pos_of(a[1]), 'loc': loc_of(a[1]), 'stateprop': a[0], 'state': a[2] if len(a) > 2 else None})
                    elif op == 'SetStageTime' and a: stage_time = a[0]
                    elif op == 'AddStageTime' and a: add_time.append(a[0])
                    elif op == 'SetRaceLaps' and a: laps = a[0]
                    elif op in ('SetRaceEnteryFee', 'SetCoinFee') and a: fee = a[0]
                    elif op == 'SetParTime' and a: par = a[0]
                    elif op == 'SetConditionPosition' and a: req_pos = a[0]
                    elif op == 'SetCondTime' and a: cond['time_ms'] = a[0]
                    elif op == 'SetCondMinHealth' and a: cond['min_health'] = a[0]
                    elif op == 'SetCondTargetVehicle' and a: cond['target_vehicle'] = a[0]
                    elif op == 'SetCompletionDialog' and a: s['completion_dialog'] = resolve_conv(convs, a[0], mid, who=a[1] if len(a) > 1 else None)
                    elif op == 'SetFMVInfo' and a: s['fmv'] = a[0]
                    elif op == 'SetDurationTime' and a: s['duration_s'] = a[0]
                    elif op == 'SetLevelOver': s['level_over'] = True
                    elif op == 'SetGameOver': s['game_over'] = True
                    elif op == 'MustActionTrigger': s['must_action_trigger'] = True
                    elif op == 'SetPresentationBitmap' and a: s['presentation_bitmap'] = a[0]
                    elif op == 'SetHUDIcon' and a: s['hud_icon'] = a[0]
                    elif op == 'SetMaxTraffic' and a: s['max_traffic'] = a[0]
                    elif op == 'NoTrafficForStage': s['no_traffic'] = True
                    elif op == 'SetCollectibleEffect' and a: s['collect_effect'] = a[0]
                    elif op == 'SetObjDistance' and a: s['lose_distance'] = a[0]
                    elif op == 'SetFollowDistances' and len(a) >= 2: s['follow_distances'] = [a[0], a[1]]
                    elif op == 'StartCountdown': s['countdown'] = True
                    elif op == 'ShowStageComplete': s['show_stage_complete'] = True
                    elif op == 'SetSwapPlayerLocator' and a: s['swap_player'] = {'pos': pos_of(a[0]), 'loc': loc_of(a[0])}
                    elif op == 'SetFadeOut' or op == 'SetIrisWipe' or op == 'StayInBlack': s.setdefault('screen_fx', []).append(op)
                    elif op == 'AddObjectiveNPCWaypoint' and len(a) >= 2: s['waypoints'].append(pos_of(a[1])) if pos_of(a[1]) else None
                    elif op == 'SetDialoguePositions': s['dialogue_positions'] = [pos_of(x) for x in a if pos_of(x)]
                # время и условия
                if stage_time is not None: s['time_limit_s'] = stage_time
                if add_time: s['add_time_s'] = add_time
                if laps is not None: s['laps'] = laps
                if fee is not None: s['fee'] = fee
                if par is not None: s['par_time'] = par
                if req_pos is not None: s['required_position'] = req_pos
                if cond: s['condition_params'] = cond
                # цель и предметы по механике
                o = st['obj']; tgt = None
                if o in ('goto', 'interior', 'gooutside'):
                    a = (ops.get('SetDestination') or [[None]])[0]
                    if a and a[0] is not None:
                        tgt = {'kind': 'destination', 'pos': pos_of(a[0]), 'loc': loc_of(a[0]), 'trigger': a[1] if len(a) > 1 else None}
                elif o == 'talkto':
                    a = (ops.get('SetTalkToTarget') or [[None]])[0]
                    n = npcs.get(a[0]) if a and a[0] else None
                    tgt = {'kind': 'npc', 'name': a[0] if a else None, 'model': model_file(a[0]) if a and a[0] else None, 'port_model': port_model(a[0]) if a else None, 'loc': n['loc'] if n else None, 'pos': n['pos'] if n else (s['waypoints'][0] if s['waypoints'] else None)}
                elif o == 'dialogue':
                    a = (ops.get('SetDialogueInfo') or [[None]])[0]
                    if a and a[0] is not None:
                        s['dialogue'] = {'who': a[0], 'with': a[1], 'models': [model_file(a[0]) if isinstance(a[0], str) else None, model_file(a[1]) if isinstance(a[1], str) else None], 'port_models': [port_model(a[0]), port_model(a[1])],
                                         'conversation': resolve_conv(convs, a[2], mid, who=None)}
                        n = npcs.get(a[1])
                        pos = n['pos'] if n else ((s.get('dialogue_positions') or [None, None])[1])
                        tgt = {'kind': 'dialogue', 'name': a[1], 'pos': pos, 'loc': n['loc'] if n else None}
                elif o == 'race':
                    cps = [c for c in colls if c.get('pos')]
                    s['checkpoints'] = [{'n': i, 'pos': c['pos'], 'loc': c['loc']} for i, c in enumerate(cps)]
                    tgt = {'kind': 'race_finish', 'pos': cps[-1]['pos'] if cps else None, 'loc': cps[-1]['loc'] if cps else None, 'checkpoint_count': len(cps)}
                    s['opponents'] = [v for v in s['vehicles'] if v.get('ai_path') or v.get('driver')]
                    s['opponent_count'] = len(s['opponents'])
                    colls = []
                elif o in ('getin', 'destroy', 'follow', 'losetail', 'dump'):
                    a = (ops.get('SetObjTargetVehicle') or [[None]])[0]
                    vn = a[0] if a else None
                    v = vehicles.get(vn)
                    if vn == 'current' and o == 'getin':   # машина игрока: где она стоит на старте фазы (или у последнего PlacePlayerCar)
                        cars = [r for r in md['start'].get('player_reset', []) if r['in_car'] and r['phase'] == ph['name']] or [r for r in md['start'].get('player_reset', []) if r['in_car']]
                        pc = md['start'].get('place_car', [])
                        v = {'pos': (cars[-1]['pos'] if cars else (pc[-1]['pos'] if pc else None)), 'loc': (cars[-1]['loc'] if cars else (pc[-1]['loc'] if pc else None))}
                    tgt = {'kind': 'vehicle', 'name': vn, 'model': car_file(vn) if isinstance(vn, str) else None, 'pos': v['pos'] if v else None, 'loc': v['loc'] if v else None}
                elif o == 'destroyboss':
                    a = (ops.get('SetObjTargetBoss') or [[None]])[0]
                    tgt = {'kind': 'boss', 'name': a[0] if a else None, 'pos': None}
                elif o == 'pickupitem':
                    a = (ops.get('SetPickupTarget') or [[None]])[0]
                    first = next((c for c in colls if c.get('pos')), None)
                    tgt = {'kind': 'pickup_item', 'name': a[0] if a else None, 'pos': first['pos'] if first else None}
                elif o == 'delivery':
                    first = next((c for c in colls if c.get('pos')), None)
                    tgt = {'kind': 'items', 'pos': first['pos'] if first else None}
                if tgt and tgt.get('pos') and tgt['pos'][1] <= -15: tgt['interior_coords'] = True   # локаторы комнат лежат ниже города (порт: «комнаты на −20»)
                s['target'] = tgt
                s['expect'] = expect_of(s, o, st)
                if o in ('delivery', 'dump', 'pickupitem') or (o not in ('race',) and colls):
                    bind = ops.get('BindCollectibleTo')
                    s['items'] = {'count': len(colls), 'positions': [c['pos'] for c in colls if c.get('pos')], 'drop_from_target_car': o in DROP_MECH or bool(bind),
                                  'bound_to_vehicle': bool(bind), 'effect': s.get('collect_effect'), 'locs': [c.get('loc') for c in colls if c.get('pos')],
                                  'what': ([c['stateprop'] for c in colls if c.get('stateprop')] or [c.get('shape') for c in colls][:1] or None),
                                  'from_vehicle': (tgt or {}).get('name') if (o in DROP_MECH or bind) else None}
                # зависимости: убрать пустые списки
                for k in ('waypoints', 'npcs', 'vehicles'):
                    if not s[k]: del s[k]
                md['stages'].append(s)
        # разговоры начала (стадии введения) и конца
        md['start']['intro_conversations'] = [s['dialogue']['conversation'] for s in md['stages'] if s['intro_phase'] and s.get('dialogue')]
        md['end'] = {'conversations': [s['completion_dialog'] for s in md['stages'] if s.get('completion_dialog')],
                     'fmv': [s['fmv'] for s in md['stages'] if s.get('fmv')],
                     'level_over': any(s.get('level_over') for s in md['stages']), 'game_over': any(s.get('game_over') for s in md['stages']),
                     'last_stage_text': md['stages'][-1]['text'] if md['stages'] else None}
        # что открывается (rewards.mfk)
        unlock = []
        rw = rewards.get(level, [])
        if kind == 'bonus_mission':
            unlock = [r for r in rw if r['quest'] == 'bonusmission']
        md['end']['unlocks'] = unlock
        out['missions'].append(md)
    return out


def expect_of(s, o, st):
    """Что должно быть в стадии (для сверки ботом): диалог (ключ, число строк), ролик, персонажи, предметы, машины."""
    e = {}
    d = s.get('dialogue')
    if d: e['dialogue'] = {'key': d['conversation'].get('key'), 'lines': len(d['conversation'].get('lines', []))}
    cd = s.get('completion_dialog')
    if cd: e['completion_dialog'] = {'key': cd.get('key'), 'lines': len(cd.get('lines', []))}
    if s.get('fmv'): e['fmv'] = s['fmv']
    if s.get('npcs'): e['npcs'] = [n['name'] for n in s['npcs']]
    if s.get('vehicles'): e['vehicles'] = [v['name'] for v in s['vehicles']]
    if s.get('items'): e['items'] = s['items']['count']
    if s.get('checkpoints'): e['checkpoints'] = len(s['checkpoints'])
    if s.get('opponent_count') is not None: e['opponents'] = s['opponent_count']
    return e


def resolve_conv(convs, name, mid, who=None):
    if not isinstance(name, str): return {'key': name, 'lines': []}
    for key in (f'{name}/{mid}',):
        if key in convs: return conv_obj(key, convs[key])
    for key, v in convs.items():
        if key.split('/')[0].lower() == name.lower(): return conv_obj(key, v)
    return {'key': name, 'lines': [], 'note': 'нет озвучки на диске'}


def conv_obj(key, files):
    lines = []
    for f in files:
        m = re.search(r'conversations/c_(.+?)_(\d+)_[a-z]+_([a-z0-9]+)_', f)
        lines.append({'file': f, 'n': int(m.group(2)) if m else None, 'actor': m.group(3) if m else None})
    return {'key': key, 'lines': lines}


def build(out_json, out_md):
    rewards, gags = parse_rewards()
    levels = []
    with tempfile.TemporaryDirectory() as td:
        for lv in range(1, 8):
            comp = C.Compiler(lv); comp.compile()
            jp = Path(td) / f'l{lv}.json'
            C.dump_json(comp.pack, jp)
            d = json.loads(jp.read_text())
            L = process_level(lv, d, rewards)
            L['rewards'] = rewards.get(lv, []); L['total_gags'] = gags.get(lv)
            levels.append(L)
    # итоги: гонки открывают машину «streetrace» (победа во всех уличных гонках уровня)
    for L in levels:
        sr = [r for r in L['rewards'] if r['quest'] == 'streetrace']
        L['street_race_reward'] = sr[0] if sr else None
    data = {'version': 1, 'source': 'tools/missions/compile.py по скриптам диска (hit-and-run-web/source/game/scripts/missions)',
            'axes': 'мировые оси порта (x,y,z локаторов диска), без отзеркаливания', 'levels': levels}
    Path(out_json).parent.mkdir(parents=True, exist_ok=True)
    Path(out_json).write_text(json.dumps(data, ensure_ascii=False, indent=1))
    write_md(data, out_md)
    return data


def write_md(data, out_md):
    L = ['# Опись миссий для бота «Механик» (Данные-93)', '',
         'Машиночитаемо: `docs/state/mechanic_inventory.json`; генератор `tools/missions/mechanic_inventory.py` (повторяемый, источник — скрипты диска через compile.py, те же данные, что играет порт).', '',
         'Оси: мировые (x, y, z локаторов диска), как их видит игра при переносе; без отзеркаливания. Поля стадии: `mech` (тип механики), `target.pos` (куда телепортироваться), `items` (сколько, где, падают ли из машины), `npcs` (кто стоит и с какой моделью), `vehicles`, `dialogue`/`completion_dialog` (разговоры), `fmv`, `conditions`, `time_limit_s`, `laps`, `fee`, `required_position`, `src` (файл:строка сценария).', '']
    tot = Counter(); n_st = 0
    for lv in data['levels']:
        L += [f"## Уровень {lv['level']} ({lv['name']})", '']
        for m in lv['missions']:
            seq = ' › '.join(s['mech'] for s in m['stages'] if not s['intro_phase']) or '-'
            n_items = sum((s['items'] or {}).get('count', 0) for s in m['stages'])
            n_st += len(m['stages'])
            for s in m['stages']: tot[s['mech']] += 1
            L.append(f"- **{m['id']}** «{m['title']}» [{m['kind']}] стадий {len(m['stages'])}, предметов {n_items}: {seq}")
        sr = lv.get('street_race_reward')
        L += ['', f"Награды уровня: " + ', '.join(f"{r['name']} ({r['quest']}{', ' + str(r['cost']) + ' у ' + r['seller'] if r['cost'] else ''})" for r in lv['rewards']), '']
    L += ['## Сводка по механикам', '', ', '.join(f'{k} {v}' for k, v in tot.most_common()), '', f'Всего стадий: {n_st}.', '',
          '## Открывается в конце', '',
          '- Бонусная миссия (`bm1`) открывает машину «bonusmission» уровня; победа во всех трёх уличных гонках (`sr1-sr3`) открывает машину «streetrace» уровня (по rewards.mfk и нашей логике Win_3Street_Races); конец последней миссии (`levelover`) открывает следующий уровень; `gameover` — титры.',
          '- Ролики: стадии `fmv` (поле `fmv`) и вступительные/финальные разговоры в `start.intro_conversations`/`end.conversations`.', '',
          '## Не покрыто', '',
          '- Босс (`destroyboss`): у стадии нет локатора, `target.pos` = null. Бот находит его по рантайму: строка `MSN stage <id>.<фаза>.<стадия> obj=destroyboss` и босс-цель из состояния миссии (позицию берёт у живого босса).',
          '- `getin` с целью «current» (машина игрока): позиция подставлена со старта фазы; 4 стадии без неё. Бот находит машину игрока по рантайму (`car` в логе, строки `MCAR`).',
          '- `talkto`/`dialogue`/`pickupitem` без позиции (1+1+2): позицию бот берёт у рантайма (`MSN marker ... at=x,y,z`, строки `MSN npc ...`).',
          '- `npd` — не персонаж, а общий риг анимаций без кожи (art/chars/npd_m.p3d 2,4 КБ: только скелет). В оригинале он тоже не рисуется: L1 m5 (стадия timer под чёрным экраном) и L5 m1sd (скрытый собеседник `hidden_moe`, диалог redeem apu/npd, голос). Порт не грузит модель и сдаётся без ожидания (`MSN npc npd model=no pending-give-up`). Поле `port_model.skinless_rig=true`, `expected_visible=false`: «модель на месте» для него проверять не надо.',
          '- Модели NPC: имя из сценария + файл `art/chars/<имя>_m.p3d`; `port_ok` — лежит ли `chars/<имя>.chr` в списке порта.',
          '- AI-пути гонок и погонь лежат в `.con` (поле `ai_path`); сама трасса AI здесь не расписана, только чекпойнты и waypoints.']
    Path(out_md).write_text('\n'.join(L) + '\n')


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=str(ROOT / 'docs/state/mechanic_inventory.json'))
    ap.add_argument('--md', default=str(ROOT / 'docs/handoff/МЕХАНИК_ОПИСЬ_МИССИЙ_93.md'))
    a = ap.parse_args()
    d = build(a.out, a.md)
    print('levels', len(d['levels']), 'missions', sum(len(l['missions']) for l in d['levels']), 'stages', sum(len(m['stages']) for l in d['levels'] for m in l['missions']))
