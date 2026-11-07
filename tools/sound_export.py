#!/usr/bin/env python3
"""SHAR -> PSP, stage 6 (sound): level sound pack for viewer/sound.c (runtime mixer: viewer/audio.c).

  python tools/sound_export.py [--level 1] [--out build/audio] [--car famil_v]

Writes (all from the owner's disc, hit-and-run-web/source/game; original PS-ADPCM blocks copied bit-for-bit):
  sfx.sab    resident bank (~1.1 MB): car engine model of --car (car_tune.spt), crashes and prop hits (collide.spt),
             coins / pickups / mission stingers (world.spt), Homer feet / jump / stomp (homer.spt), frontend clicks,
             + extension "SXT1" (SAB header @80): see EXT below.
  music.sap  streamed from the memory stick: level music (l1_music.rms file set), ambience zone loops (ambience.rms),
             positional loops (positionalsettings.spt, streamed in the original too), Homer's in-car lines D_*_Hom
             (dialog.spt, streamed in the original).
  sound_report.json  what went in, sizes, what the disc does not have.

EXT (little-endian): 'SXT1', u32 nsec, then nsec x {char tag[4], u32 off (from EXT start), u32 count, u32 stride}
  MODE  36 B: name[16], f32 sfx, car, music, ambience, dialog        global.spt tuner: bus volumes per game mode
  OBJS  20 B: f32 x, y, z, r, u8 kind (0 DynaPhys, 1 AnimDynaPhys, 2 InstStatPhys), u8 pad, i16 res
              every level object with a sound in its 0x3000600 attribute (third field = collide.spt name)
  ZONE  96 B: u32 type (0 sphere, 1 box), u32 event, f32 half[3], R[9] (rows = local x, y, z axes in world),
              t[3], char track[28]                  ambience gates: event locators (id 7..81) + TriggerVolume -> region of ambience.rms
  EMIT  32 B: f32 x, y, z, radius, minDist, maxDist, u16 track (index in music.sap), u16 pad,
              f32 gain (clip SetTrim x PlaybackProbability; the probability is 1.0 for every level-1 emitter)
              positional loops: locators kind 1 + positionalsettings.spt
  MEVT  36 B: char event[28], i16 track[3], u8 n, u8 kind   музыка миссий: события l<N>_music.rms (mission_music)
  RAMB  56 B: char room[28], char track[28]   атмосфера комнаты (правка 345, room_ambience): имя комнаты порта -> дорожка атмосферы
music.sap also carries the mission conversation lines (voice_track_name, bus 2) and every mission music set of the level.
music.sap track bus field: 0 music, 1 ambience, 2 dialogue (Homer's lines), 3 positional loop.
Coordinates: game space of hit-and-run-web / level.shr (disc z mirrored).
"""
import argparse, json, os, re, struct, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / 'hit-and-run-web' / 'tools'))
import audio_export as AE                    # noqa: E402
from p3d import read, walk, string            # noqa: E402

ROOT = HERE.parent
GAME = ROOT / 'hit-and-run-web' / 'source' / 'game'
ART = GAME / 'art'
SND = GAME / 'sound'

# ---------------------------------------------------------------- resident effects (script resource names)
SFX = [
    # car (carsound.spt; engine/horn/fire of the car come from car_tune.spt automatically)
    'skid', 'dirt_skid', 'gearshift', 'car_jump', 'siren',
    # collisions (collide.spt: all of it; the objects' attribute names these)
    'small_car_crash', 'medium_car_crash', 'large_car_crash', 'car_hit_link_fence', 'car_hit_crate',
    'car_hit_garbage_can', 'car_hit_hydrant', 'car_hit_mailbox', 'car_hit_tree', 'hit_shrub', 'car_hit_streetlamp',
    'car_hit_phonestop', 'car_hit_stopsign', 'car_hit_telepole', 'hit_concrete', 'car_hit_cemetary_gate',
    'car_hit_const_signA', 'car_hit_L2pipe', 'car_hit_pumpkin', 'car_hit_pedestrian', 'powerbox_smash',
    'krustyglass_smash', 'explode_01', 'generic_car_explode', 'suspension_ronks', 'hydrant_spray',
    'generic_metal_hit', 'vending_machine_hit', 'monkey_crate_hit', 'dumpster_hit',
    # world.spt
    'coin_collect_01', 'coin_lose', 'wrench_collect', 'card_collect', 'character_zapped', 'pop_up',
    # осы-камеры (world.spt; В5, viewer/main.c collFrame): появление, жужжание (петля), выстрел, промах, уничтожение
    'wasp_charging', 'wasp_charged_idle', 'traffic_coffin',   # сверка g08-sound круг 2 (g11b P10-19, P10-31): стадии осы и гроб Хэллоуина (10-100 м)
    'wasp_fade_in', 'wasp_idle', 'wasp_attack', 'wasp_lasers', 'laser_miss', 'wasp_destroyed',
    'mission_complete', 'busted', 'stage_complete', 'switch', 'payphone_pickup', 'trampoline', 'rage_warning',
    # homer.spt
    'feet_concrete', 'feet_concrete_jump', 'feet_metal', 'feet_metal_jump', 'feet_wood', 'feet_wood_jump',
    'feet_dirt', 'feet_dirt_jump', 'jump', 'homer_stomp',
    # frontend.spt (locked_out — затемнённый пункт меню, как в паузе: 29.09, звуки главного меню и OPTIONS)
    'accept', 'back', 'scroll', 'pause_on', 'pause_off', 'locked_out',
    # сверка g08-sound (02.11.2026; СВЕРКА_ПОЛНАЯ/P09, P10, P05): мотор трафика и ИИ миссий (P10-1, P10-2), подбор предмета миссии по уровням (P09:23),
    # взлёт птиц (P09:24), звуки меню (P09:29, Д14), звуки подвижных объектов (P05:8). Имена, которых нет в скриптах диска, exporter пропускает
    'ferrari', 'mission_ai_vehicle',
    'level_1_pickup_sfx', 'level_2_pickup_sfx', 'level_3_pickup_sfx', 'level_4_pickup_sfx', 'level_5_pickup_sfx', 'level_6_pickup_sfx',
    'monkey_collect', 'nuclear_waste_collect', 'pigeon_takeoff', 'gull_takeoff',
    'phonebusy', 'cheat_success', 'cheat_fail', 'car_stinger', 'dialog_stinger', 'music_stinger', 'sfx_stinger', 'continue', 'cancel',
    'trapdoor', 'platform_01', 'platform_02', 'aplatform6', 'duff_truck', 'plantdoor', 'elevator', 'gag_library', 'hit_switch_quick',
    'robo_arm',   # круг 6 (g08, проверка проводки): g11 в круге 5 подключил звуки подвижных объектов по имени (objnames.shr), рука-робот L3 и L6 играет robo_arm
]

# ---------------------------------------------------------------- music (l<N>_music.rms -> роли движка)
# Композиция уровня sound/music/l<N>_music.rms (radmusic): двоичный граф событий не разобран, строки идут по порядку — пары
# «файл .rsd -> имя потока», затем регионы и события. Роли движка (viewer/sound.c tName) названы по дорожкам уровня 1 и на всех
# уровнях одни: дорожка пакета называется именем роли, файл — тот, что композиция уровня ставит в эту роль. Правила (одни на
# все уровни; на уровне 1 дают прежнюю таблицу до байта, проверка — tests/levels/music_check.py):
#   езда: потоки, объявленные между событием Level_Intro и регионом Sunday_Drive_region (регионы названы так на всех 7 уровнях):
#     *_Intro -> sunday_drive_intro (дальше sunday_drive), остальные не _End_Sus -> sunday_drive (петля; нет — петлёй идёт
#     вступление, уровень 5 — ПРАВИЛО ПОРТА); выход из машины sunday_drive_end_sus1..3 — файлы <главный>_End_Sus<k> на диске
#     (у уровня 1 Sus2 объявлен в композиции как M0_End_Pos), иначе потоки *_End_Sus этого блока;
#   миссия по умолчанию collection02_*: потоки группы M1 (как музыка миссий, mission_music) по суффиксу: Pickup (дальше Main),
#     Main, End_Pos, End_Neg, End_Sus (два первых);
#   магазины: поток KwikEmart -> kwik_e_mart_muzak, поток перед Store_region -> muzak (петли);
#   Hit & Run: потоки Hit_Run_Intro (дальше Main), Hit_Run_Main (петля), Hit_Run_End_Neg;
#   прочее: Harp_Music04/05 (OF_found_card, по порядку файлов), Horn_triumph, Horn_Blast, три первых потока после события
#     Newspaper_Spin (подпись героя уровня: Tuba_* Гомера, Bart_Guit_* Барта…; движком пока не играются), Simpsons_Theme (петля).
def composition(lv):
    """-> (строки композиции от my_comp, {ключ файла: путь}, [(индекс, поток, ключ файла)] объявления по порядку,
    {группа M<k>/Bonus: [(поток, ключ)]}) — тот же обход, что mission_music (ссылка на объявленный поток — не объявление)."""
    mdir = SND / 'music'
    files = {os.path.relpath(p, mdir)[:-4].replace('\\', '/').lower(): p for p in glob_rsd(mdir)}
    b = (mdir / f'l{lv}_music.rms').read_bytes()
    strs = [s.decode() for s in re.findall(rb'[\x20-\x7e]{3,}', b)]
    strs = strs[strs.index('my_comp'):] if 'my_comp' in strs else strs
    decl, known, groups, pending, i = [], {}, {}, [], 0
    while i < len(strs):
        s = strs[i]; key = s.replace('\\', '/').lower()
        if s in known: pending.append((s, known[s])); i += 1; continue
        if key in files and i + 1 < len(strs):
            known[strs[i + 1]] = key; decl.append((i, strs[i + 1], key)); pending.append((strs[i + 1], key)); i += 2; continue
        m = MEV_GROUP.match(s)
        if m:
            g = groups.setdefault(m.group(1), [])
            if pending: g += pending; pending = []
        elif not (s.endswith('_region') or s == 'music'): pending = []
        i += 1
    return strs, files, decl, groups


def level_skid_cars(lv):
    """Правка 413а: машины со своим звуком заноса (carSoundParameters с RoadSkidClipName/DirtSkidClipName — парящие frink_v, hbike_v,
    ship, witchcar, mono_v и остов huskA), которые на уровне lv может вести игрок: названы в скриптах scripts/missions/level0N/*.mfk
    (машины миссий, гонок, будки) или наградой уровня в rewards.mfk (BindReward … "car", …, N). Их профиль (мотор, занос) — в пакет:
    движок звучит машиной игрока (viewer/audio.c audioPlayerCar), а не машиной уровня. -> [имя профиля]"""
    _, cars, _, _ = AE.load_scripts()
    own = [n for n, c in cars.items() if 'RoadSkidClipName' in c or 'DirtSkidClipName' in c]
    txt = ''
    for f in sorted((GAME / 'scripts' / 'missions' / f'level{lv:02d}').glob('*.mfk')):
        txt += re.sub(r'//[^\n]*', '', f.read_text(errors='replace'))
    rw = re.sub(r'//[^\n]*', '', (GAME / 'scripts' / 'missions' / 'rewards.mfk').read_text(errors='replace'))
    for m in re.finditer(r'BindReward\s*\(([^)]*)\)', rw):
        ra = [x.strip().strip('"') for x in m.group(1).split(',')]
        if len(ra) >= 5 and ra[2] == 'car' and ra[4].isdigit() and int(ra[4]) == lv: txt += ' "%s" ' % ra[0]
    return [n for n in own if re.search(r'"%s"' % re.escape(n), txt, re.I)]


def level_player_cars(lv):
    """Правка 413 (хвост): все машины со своим профилем звука (car_tune.spt), которые игрок может вести на уровне lv — названы в
    скриптах scripts/missions/level0N/*.mfk (машины миссий и гонок, будка AddVehicleSelectInfo, угоняемые машины миссий) или наградой
    уровня в rewards.mfk. Их мотор, холостой, гудок и занос — в пакет: машина игрока звучит своим профилем (viewer/audio.c
    audioPlayerCar). Порядок — как в скриптах; парящие (level_skid_cars) входят сюда же. -> [имя профиля]"""
    _, cars, _, _ = AE.load_scripts()
    low = {n.lower(): n for n in cars}
    out = []
    for f in sorted((GAME / 'scripts' / 'missions' / f'level{lv:02d}').glob('*.mfk')):
        for m in re.finditer(r'"([A-Za-z0-9_]+)"', re.sub(r'//[^\n]*', '', f.read_text(errors='replace'))):
            n = low.get(m.group(1).lower())
            if n and n not in out: out.append(n)
    rw = re.sub(r'//[^\n]*', '', (GAME / 'scripts' / 'missions' / 'rewards.mfk').read_text(errors='replace'))
    for m in re.finditer(r'BindReward\s*\(([^)]*)\)', rw):
        ra = [x.strip().strip('"') for x in m.group(1).split(',')]
        if len(ra) >= 5 and ra[2] == 'car' and ra[4].isdigit() and int(ra[4]) == lv and ra[0] in cars and ra[0] not in out: out.append(ra[0])
    if os.environ.get('CAR_WORLD_AUDIO_OLD') != '1':
        # Loaded car paths also name models (notably huskA on every level).
        for f in sorted((GAME / 'scripts' / 'missions' / f'level{lv:02d}').glob('*.mfk')):
            text = re.sub(r'//[^\n]*', '', f.read_text(errors='replace'))
            for path in re.findall(r'"([^"\n]+)"', text):
                path = path.replace('\\', '/').lower()
                if path.startswith('art/cars/') and path.endswith('.p3d'):
                    canonical = low.get(path.rsplit('/', 1)[-1][:-4])
                    if canonical and canonical not in out: out.append(canonical)
        # CarStart stores the parked model after heading/parked words. These
        # cars have no MFK/reward entry, but can be driven on every level.
        for _, _, kind, data, _, _ in locators(lv):
            if kind == 3 and len(data) > 8:
                name = data[8:].split(b'\0')[0].decode('ascii')
                canonical = low.get(name.lower())
                if not canonical: raise ValueError('CarStart has no sound profile: ' + name)
                if canonical not in out: out.append(canonical)
    return out


def drive_plan(lv):
    """-> (ключ файла вступления или None, [ключи файлов петли]) региона Sunday_Drive_region композиции l<N>_music.rms;
    ключ — путь от sound/music без .rsd, строчными (как в composition)"""
    import rms
    C = rms.read(SND / 'music' / f'l{lv}_music.rms')
    ri = C.region_index('Sunday_Drive_region')
    if ri is None: return None, []
    key = lambda s: C.stream_file(s)[1].replace('\\', '/').lower()
    plan = C.region_plan(ri)
    intro = [key(alts[0]) for rep, alts in plan if not rep]
    loop = [key(s) for rep, alts in plan if rep for s in alts]
    if not loop and intro: loop = [intro.pop()]             # повтора нет — последний поток и есть петля (правило порта)
    return (intro[0] if intro else None), loop


def music_table(lv):
    """-> [(имя роли, файл без .rsd от sound/music, петля, следующая)] уровня lv по правилам выше."""
    strs, files, decl, groups = composition(lv)
    out = []
    def add(name, key, loop, nxt=None):
        if key and not any(o[0] == name for o in out): out.append((name, key, loop, nxt))
    first = lambda cond: next((k for _, st, k in decl if cond(st)), None)
    by_stream = lambda name: first(lambda st: st.lower() == name.lower())
    # езда: регион Sunday_Drive_region композиции, разобранный целиком (tools/rms.py, 29.09): вступление — потоки до
    # повтора, петля — поток в logic_repeat. Выбор logic_or внутри повтора (уровень 5: девять Sitar_Orch, случайно по
    # одному) — ПРАВИЛО ПОРТА: все по кругу в порядке композиции (sunday_drive -> sunday_drive~2 -> … -> sunday_drive).
    # До 29.09 блок брался по строкам между Level_Intro и Sunday_Drive_region: у уровня 5 так петлёй шло вступление
    # уровня 1 (sunday_drive_intro); у 1–4, 6, 7 разбор даёт те же файлы.
    lo = strs.index('Level_Intro') if 'Level_Intro' in strs else -1
    hi = strs.index('Sunday_Drive_region') if 'Sunday_Drive_region' in strs else -1
    blk = [(st, k) for i, st, k in decl if lo < i < hi] if 0 <= lo < hi else []
    sus_re = re.compile(r'_end_sus\d*$', re.I)
    intro, mains = drive_plan(lv)
    main = mains[0] if mains else None
    sus = sorted((k for k in files if main and re.fullmatch(re.escape(main) + r'_end_sus\d+', k)), key=lambda k: int(re.search(r'\d+$', k).group()))
    if not sus: sus = [k for st, k in blk if sus_re.search(st)]
    add('sunday_drive_intro', intro, False, 'sunday_drive' if main else None)
    if len(mains) <= 1:
        add('sunday_drive', main, True)
    else:
        ring = ['sunday_drive'] + ['sunday_drive~%d' % (j + 1) for j in range(1, len(mains))]
        for j, k in enumerate(mains): add(ring[j], k, False, ring[(j + 1) % len(ring)])
    for j, k in enumerate(sus[:3]): add(f'sunday_drive_end_sus{j + 1}', k, False)
    # миссия по умолчанию: группа M1
    m1 = groups.get('M1', [])
    roles = {}
    for st, k in m1: roles.setdefault(music_role(st), []).append(k)
    mm = (roles.get('main') or [None])[0]
    add('collection02_pickup', (roles.get('pickup') or [None])[0], False, 'collection02_main' if mm else None)
    add('collection02_main', mm, True)
    add('collection02_end_pos', (roles.get('win') or [None])[0], False)
    add('collection02_end_neg', (roles.get('lose') or [None])[0], False)
    for j, k in enumerate((roles.get('sus') or [])[:2]): add(f'collection02_end_sus{j + 1}', k, False)
    # магазины
    add('kwik_e_mart_muzak', by_stream('KwikEmart'), True)
    st_i = strs.index('Store_region') if 'Store_region' in strs else -1
    add('muzak', next((k for i, st, k in reversed(decl) if i < st_i), None) if st_i >= 0 else None, True)
    # Hit & Run
    hm = by_stream('Hit_Run_Main')
    add('hit_run_intro', by_stream('Hit_Run_Intro'), False, 'hit_run_main' if hm else None)
    add('hit_run_main', hm, True)
    add('hit_run_end_neg', by_stream('Hit_Run_End_Neg'), False)
    # прочее
    for nm in ('Harp_Music04', 'Harp_Music05', 'Horn_triumph', 'Horn_Blast'): add(nm.lower(), by_stream(nm), False)
    ns = strs.index('Newspaper_Spin') if 'Newspaper_Spin' in strs else -1
    for i, st, k in [d for d in decl if d[0] > ns][:3] if ns >= 0 else []: add(st.lower()[:27], k, False)
    add('simpsons_theme', by_stream('Simpsons_Theme'), True)
    # правка 20 (29.09) «звуки перелётов меню»: события композиции Loading_screen и Newspaper_Spin (tools/rms.py: play_region
    # FE_trans01 — выбор одного из FE_trans01..03, FE_trans02 — одного из NewsPaper_Spin01/02) -> дорожки loading_screen,
    # loading_screen~1.. и newspaper_spin, newspaper_spin~1 (движок берёт одну случайно, как logic_or; viewer/main.c feStinger)
    # правка Роба 69 (30.09): старт уровня — событие композиции Level_Intro -> Level_Intro_region (у уровня 1 одна из
    # Homer\\Tuba_056/058/060, короткая туба); музыка езды Sunday_Drive — только с событием Sunday_Drive_start (посадка в машину)
    for ev, nm in (('Loading_screen', 'loading_screen'), ('Newspaper_Spin', 'newspaper_spin'), ('Level_Intro', 'level_intro')):
        for j, k in enumerate(event_alts(lv, ev)): add(nm if not j else f'{nm}~{j}', k, False)
    # VIEW CREDITS (29.09, viewer/main.c optionsRun): событие композиции Credits -> Credits_region -> Land_of_Choc_Main, петля
    for j, k in enumerate(event_alts(lv, 'Credits')[:1]): add('credits', k, True)
    # правка 179 (близнец Б2, ступень 3; [диск] событие OF_explore_mission -> регион из 47 туб Homer\Tuba_0xx, logic_or): пока
    # Гомер пешком, оригинал каждые 15–20 с играет новую короткую тубу — дорожки of_explore, of_explore~1.. (viewer/sound.c tubaFrame)
    for j, k in enumerate(dict.fromkeys(event_alts(lv, 'OF_explore_mission'))): add('of_explore' if not j else f'of_explore~{j}', k, False)
    # правка 182 (близнец Б2: vivaldi.rsd прочитан у ворот stonecutters_hall_out, (−381; 85) порта; [диск] событие
    # Enter_StoneCutters_Tunnel -> push_region Vivaldi): дорожка vivaldi, петля, пока игрок в атмосфере тоннеля/зала Stonecutters
    for j, k in enumerate(event_alts(lv, 'Enter_StoneCutters_Tunnel')[:1]): add('vivaldi', k, True)
    return out


def event_alts(lv, event):
    """событие композиции l<N>_music.rms -> ключи файлов (путь от sound/music без .rsd, строчными) первого шага его региона:
    альтернативы logic_or по порядку композиции; нет события или региона — []"""
    import rms
    C = rms.read(SND / 'music' / f'l{lv}_music.rms')
    for name, acts in C.events():
        if name != event: continue
        ri = next((r for kind, r in acts if kind in ('play_region', 'push_region') and r is not None), None)
        steps = C.region_streams(ri) if ri is not None else []
        return [C.stream_file(s)[1].replace('\\', '/').lower() for s in steps[0]] if steps else []
    return []

# Атмосфера (29.09): композиция sound/ambience/ambience.rms разобрана целиком (tools/rms.py): событие -> play_region/
# push_region -> регион -> слой -> дерево событий -> поток -> файл. Первые четыре события композиции — состояния игры
# (frontend, movie, pause, unpause), дальше по порядку события локаторов-ворот: id события локатора = AMB_EVENT0 + номер
# (сверено: 9 suburbs, 14 PP_entrance, 29 country, 37..46 Stonecutters/Burns/PP, 57 cemetary* = Placeholder4 ->
# cemetary_day_region, 58 cemetary* уровня 4 = Placeholder5 -> cemetary_night, 60 ufo_ambience_* уровня 7 = Placeholder7 ->
# halloween_ufo_ambience_region, 71 halloween_cafeteria = Placeholde11 -> halloween_cafeteria_powerplant_region).
# До 29.09 файл брался по имени региона руками только для уровня 1 (уровни 2–7 молчали, две ошибки на уровне 1:
# PP_entrance — powplant_loop_exterior_01 вместо power_plant, тоннель Stonecutters — tunnel_loop_01 вместо burns_tunnel_loop).
# ПРАВИЛА ПОРТА (данных нет): 1) дорожка — на РЕГИОН композиции (несколько событий одного региона не перезапускают петлю);
# 2) регион с выбором logic_or (dockside_day — 2 петли, barn — 3) играет первую; 3) push_region = play_region (pop — только
# unpause); 4) событие без звука (push/play тихого региона: pause_region у DoNotUse*/Placeholder8/9 — фонари, голуби,
# «музыка» клубов) — не ворота атмосферы; 5) атмосфера на старте — регион с наибольшим числом объёмов ворот уровня
# (у уровня 1 — suburbs_day, как было).
AMB_EVENT0 = 7
AMB_GAME_EVENTS = ('frontend', 'movie', 'pause', 'unpause')


def amb_track_name(region):
    """'power_plant_tunnel_01_region' -> 'amb_pp_tunnel_01' (имя дорожки music.sap, <= 27 символов)"""
    n = region.lower()
    n = n[:-len('_region')] if n.endswith('_region') else n
    for a, b in (('power_plant', 'pp'), ('powerplant', 'pp'), ('_interior', '_int'), ('_exterior', '_ext')):
        n = n.replace(a, b)
    n = 'amb_' + n
    assert len(n) <= 27, n
    return n


def ambience_map(path=None):
    """-> {id события локатора: dict(event, region, track, file (путь .rsd или None — тихий регион), alts)}"""
    import rms
    C = rms.read(path or SND / 'ambience' / 'ambience.rms')
    evs = C.events()
    assert tuple(n for n, _ in evs[:len(AMB_GAME_EVENTS)]) == AMB_GAME_EVENTS, [n for n, _ in evs[:4]]
    out = {}
    for k, (name, acts) in enumerate(evs[len(AMB_GAME_EVENTS):]):
        ri = next((r for kind, r in acts if kind in ('play_region', 'push_region') and r is not None), None)
        if ri is None:
            continue
        steps = C.region_streams(ri)
        alts = [C.stream_file(s)[1] for s in steps[0]] if steps else []
        f = None
        if alts:
            f = SND / 'ambience' / (alts[0].replace('\\', '/') + '.rsd')
            if not f.exists():                                   # регистр имени файла на диске (DMV -> dmv.rsd)
                f = next((p for p in (SND / 'ambience').glob('*.rsd') if p.name.lower() == f.name.lower()), f)
        out[AMB_EVENT0 + k] = dict(event=name, region=C.region_name(ri), track=amb_track_name(C.region_name(ri)),
                                   file=str(f) if f else None, alts=alts)
    return out

# ---------------------------------------------------------------- голос разговоров миссий и музыка миссий (пакет «Миссии: озвучка»)
# Голос: реплики разговоров conversations/c_<имя>_<N>_<тип>_<кто>_l<уровень>[<миссия>].rsd — ровно те, что пакет миссий уровня
# кладёт в разговоры (tools/missions/compile.py: SetDialogueInfo / SetCompletionDialog / вступления бонусов). Дорожка music.sap
# (поток диалогов, bus 2) называется по правилу voice_track_name — то же правило в viewer/msn_audio.c (msnAudioVoiceName).
VOICE_KIND = (('_convinit_', '_ci_'), ('_noboxconv_', '_nb_'), ('_tutorial_', '_tu_'))

def voice_track_name(file):
    """'conversations/c_icecream_1_convinit_mrg_l1.rsd' -> 'v_icecream_1_ci_mrg_l1' (<= 27 символов, иначе None)"""
    b = re.split(r'[\\/]', file)[-1].lower()
    if b.endswith('.rsd'): b = b[:-4]
    if b.startswith('c_'): b = b[2:]
    for a, s in VOICE_KIND: b = b.replace(a, s)
    b = 'v_' + b
    return b if len(b) <= 27 else None

# Реплики к подсказкам обучения (Tutorial.scr, viewer/tutorial.c): conversations/c_<имя>_l1t1.rsd, PS2-варианты (ps2), где они есть.
# Какая реплика к какой подсказке — по записи PS2: текст на экране и расшифровка звука в тот же момент (1:05 startps2, 1:45 incarps2,
# 1:50 driveps2, 2:22 broken, 2:29 coin_1 Гомера + coin_2, 2:55 interiorps2, 4:15 wrench, 11:20 camera, 16:55 race, 22:55 bonus,
# 33:36 damaged); card, gag, timetrial — правило порта по смыслу реплики и строки библии
TUT_VOICES = ('startps2_1_tutorial_brt', 'incarps2_1_tutorial_brt', 'driveps2_1_tutorial_brt', 'broken_1_tutorial_brt',
              'coin_1_tutorial_hom', 'coin_2_tutorial_brt', 'interiorps2_1_tutorial_brt', 'wrench_1_tutorial_brt',
              'camera_1_tutorial_brt', 'race_1_tutorial_brt', 'bonus_1_tutorial_brt', 'damaged_1_tutorial_brt',
              'card_1_tutorial_brt', 'gag_1_tutorial_brt', 'timetrial_1_tutorial_brt',
              'rewardps2_1_tutorial_brt')   # правка 121+: «UNLOCKED CAR» (запись PS2 14:17)

def tutorial_mode(lv):
    """EnableTutorialMode(1 | true) в scripts/missions/level0<lv>/leveli.mfk"""
    f = GAME / 'scripts' / 'missions' / f'level0{lv}' / 'leveli.mfk'
    if not f.exists(): return False
    for line in f.read_text(errors='replace').splitlines():
        m = re.match(r'\s*EnableTutorialMode\s*\(\s*(\w+)\s*\)', line)
        if m: return m.group(1).lower() in ('1', 'true')
    return False

def mission_voice(lv):
    """[(дорожка, путь)] всех реплик разговоров пакета миссий уровня (порядок пакета, без повторов)"""
    import importlib.util
    sys.path.insert(0, str(HERE / 'missions'))
    spec = importlib.util.spec_from_file_location('msn_compile', HERE / 'missions' / 'compile.py')
    mc = importlib.util.module_from_spec(spec); spec.loader.exec_module(mc)
    c = mc.Compiler(lv); c.compile(); P = c.pack
    out, seen, bad = [], {}, []
    for clip in P.clips:
        f = P.strings[clip[0]]
        tn = voice_track_name(f); p = GAME / f
        if tn is None or not p.exists(): bad.append(f); continue
        if tn in seen:
            assert seen[tn] == str(p), ('два файла реплик на одно имя дорожки', tn, seen[tn], p)
            continue
        seen[tn] = str(p); out.append((tn, str(p)))
    return out, bad

# Музыка миссий: композиция уровня sound/music/l<N>_music.rms (radmusic). Двоичный граф событий не разобран; из неё читаются
# строки по порядку: пары «файл .rsd -> имя потока», затем регионы и события миссии (M0_start, M2_drama, M4_10sec_to_go,
# M1_win, M1_lose, M1_get_out_of_car, Bonus_start ...). Потоки, стоящие перед событиями группы, принадлежат ей; роль потока —
# по суффиксу его имени (Pickup -> Main, Drama_Pickup -> Drama, 10sec, End_Pos / End_Neg / End_Sus*). Событие -> дорожки:
# start = Pickup (дальше Main по цепочке) | Main | Intro; drama = Drama_Pickup (-> Drama) | Drama; 10sec_to_go = 10sec;
# win = End_Pos; lose = End_Neg; get_out_of_car = End_Sus* (случайная). Событие без потоков (M0_start) — «музыка не меняется».
MEV_KIND = {'start': 0, 'drama': 1, 'win': 2, 'lose': 3, 'get_out_of_car': 4, '10sec_to_go': 5}
MEV_GROUP = re.compile(r'^(M\d|Bonus|StreetRace0[123])_(start|drama|win|lose|get_out_of_car|10sec_to_go)$', re.I)   # ТЕСТ90: уличные гонки как и у music_graph.EVENT (путь по умолчанию); тут — прежний разбор строк и матрицы состояний

def music_role(stream):
    s = stream.lower()
    for suf, role in (('_drama_pickup', 'drama_pickup'), ('_pickup', 'pickup'), ('_drama', 'drama'), ('_main', 'main'),
                      ('_intro', 'intro'), ('_10sec', 'tensec'), ('_cymbal', 'cymbal')):
        if s.endswith(suf): return role
    m = re.search(r'_end_(pos|neg|sus)', s)
    return {'pos': 'win', 'neg': 'lose', 'sus': 'sus'}[m.group(1)] if m else None

def mission_music(lv):
    """-> (дорожки [(имя, путь, петля, следующая)], события [(имя, [дорожки], вид)], отчёт)"""
    if os.environ.get('MUSIC_GRAPH_OLD') != '1':
        from music_graph import mission_music as graph_music
        return graph_music(lv, SND / 'music')
    mdir = SND / 'music'
    files = {os.path.relpath(p, mdir)[:-4].replace('\\', '/').lower(): p for p in glob_rsd(mdir)}
    b = (mdir / f'l{lv}_music.rms').read_bytes()
    strs = [s.decode() for s in re.findall(rb'[\x20-\x7e]{3,}', b)]
    s0 = strs.index('my_comp') if 'my_comp' in strs else 0      # до имени композиции — словарь полей формата
    groups, pending, streams, i = {}, [], {}, s0
    while i < len(strs):
        s = strs[i]; key = s.replace('\\', '/').lower()
        if s in streams:                                  # ссылка на уже объявленный поток (Bonus: потоки Collection02 из M1)
            pending.append((s, streams[s])); i += 1; continue
        if key in files and i + 1 < len(strs):            # объявление: файл .rsd, затем имя потока
            streams[strs[i + 1]] = files[key]; pending.append((strs[i + 1], files[key])); i += 2; continue
        m = MEV_GROUP.match(s)
        if m:
            g = groups.setdefault(m.group(1), dict(streams=[], events=[]))
            if pending: g['streams'] += pending; pending = []
            g['events'].append((s, m.group(2).lower()))
        elif not (s.endswith('_region') or s == 'music'):
            pending = []                                  # событие не миссии (Sunday_Drive_start, KwikEmart, StreetRace01_start ...)
        i += 1
    tracks, events, rep = [], [], {}
    def tname(stream, path):
        for t in tracks:
            if t[1] == path: return t[0]
        return None
    for code, g in groups.items():
        roles = {}
        for stream, path in g['streams']:
            r = music_role(stream)
            if r: roles.setdefault(r, []).append((stream, path))
        def add(role, loop, nxt=None):
            out = []
            for stream, path in roles.get(role, []):
                n = tname(stream, path) or stream.lower()[:27]
                if not any(t[0] == n for t in tracks): tracks.append((n, path, loop, nxt))
                out.append(n)
            return out
        main = add('main', True) or add('intro', True)
        drama = add('drama', True)
        pick = add('pickup', False, main[0] if main else None)
        dpick = add('drama_pickup', False, drama[0] if drama else None)
        ev = dict(start=pick or main, drama=dpick or drama, **{'10sec_to_go': add('tensec', True)},
                  win=add('win', False), lose=add('lose', False), get_out_of_car=add('sus', False))
        for name, kind in g['events']:
            events.append((name, ev.get(kind, [])[:3], MEV_KIND[kind]))
    # Правка Роба 207 (03.10: «не начинается музыка уровня; когда начинается миссия, музыка меняется»): группы по порядку строк
    # теряли события, чьи потоки объявлены раньше под чужим именем — M0_start (миссия 0, у Мардж) оставался без дорожек, и музыка
    # не менялась. Пустые события миссий — по самой композиции (tools/rms.py: событие -> play_region -> слои -> потоки -> файлы):
    # M0_start -> Sunday_Drive_region [Sunday_Drive_Intro -> Sunday_Drive], M0_get_out_of_car -> Sus1/Sus3, M0_win -> Sus2
    if not os.environ.get('MEV_NAME_ORDER_ONLY'):
        import rms as RMS
        C = RMS.read(str(mdir / f'l{lv}_music.rms'))
        bypath = {t[1]: t[0] for t in tracks}
        def trk(fname, loop, nxt):
            p_ = files.get(fname.replace('\\', '/').lower())
            if not p_: return None
            n_ = bypath.get(p_)
            if not n_:
                n_ = os.path.basename(fname).lower()[:27]; tracks.append((n_, p_, loop, nxt)); bypath[p_] = n_
            return n_
        for k_, (name, ts, kind) in enumerate(events):
            if ts: continue
            acts = next((a for nm, a in C.events() if nm == name), [])
            ri = next((r for kd, r in acts if kd == 'play_region' and r is not None), None)
            if ri is None: continue
            seq = [[C.stream_file(si)[1] for si in alt] for alt in C.region_streams(ri)]
            if not seq: continue
            loop_kind = kind in (MEV_KIND['start'], MEV_KIND['drama'], MEV_KIND['10sec_to_go'])
            main_ = trk(seq[1][0], True, None) if len(seq) > 1 else None          # вступление -> петля (как Level_Intro / Sunday_Drive)
            first = [t for t in (trk(f, loop_kind and len(seq) == 1, main_) for f in seq[0]) if t]
            if first: events[k_] = (name, first[:3], kind)
            rep.setdefault('_rms', {})[name] = seq
        # Правка Роба 234 (03.10: «в миссии и пешком, пока не дошёл до цели, должна идти музыка миссии»): у get_out_of_car повтор —
        # из композиции (план региона, logic_repeat): M2/M3/M5/M6/M7 — петля, пока не сел в машину или миссия не кончилась; M0/M1/Bonus —
        # один проход (так в данных). Было: дорожки «пешком» всегда без петли
        for name, ts, kind in events:
            if kind != MEV_KIND['get_out_of_car'] or not ts: continue
            acts = next((a_ for nm, a_ in C.events() if nm == name), [])
            ri = next((r for kd, r in acts if kd == 'play_region' and r is not None), None)
            if ri is None or not any(rp for rp, _ in C.region_plan(ri)): continue
            for j, t in enumerate(tracks):
                if t[0] in ts and not t[2]: tracks[j] = (t[0], t[1], True, t[3])
            rep.setdefault('_sus_loop', []).append(name)
        rep[code] = dict(streams=[s for s, _ in g['streams']], events={n: ev.get(k, []) for n, k in g['events']},
                         unused=[s for s, _ in g['streams'] if music_role(s) in ('cymbal', None)])
    if os.environ.get('C3_CONTENT_MUSIC_OLD') != '1':
        tracks, events, state_report = state_music(lv, tracks, events)
        if state_report: rep['_states'] = state_report
    return tracks, events, rep


def state_music(lv, tracks, events):
    """RMS event_matrix: one authored state axis selects its action array.
    Matrixless events retain their existing export. Never flatten Stage1/2 into
    random alternatives. Unsupported multidimensional matrices fail loudly.
    """
    import rms
    C = rms.read(SND / 'music' / f'l{lv}_music.rms')
    tracks = list(tracks); events = list(events); report = []
    bypath = {str(Path(p).resolve()): n for n, p, _, _ in tracks}
    def add(si, loop, nxt=None):
        stream, fname = C.stream_file(si)
        path = str((SND / 'music' / (fname.replace('\\', '/') + '.rsd')).resolve())
        if path in bypath: return bypath[path]
        if not Path(path).is_file(): raise ValueError('state music source missing: ' + path)
        name = stream.lower()[:27]; names = {r[0] for r in tracks}; suffix = 1
        while name in names:
            name = stream.lower()[:23] + '~s' + str(suffix); suffix += 1
        bypath[path] = name; tracks.append((name, path, loop, nxt)); return name
    for event in C.pools['events']['els']:
        owner = C.owner(event[2])
        if not owner: continue
        if owner[0] != 'event_matricies': raise ValueError('invalid RMS state matrix')
        state_refs = C.store[C.el(event[2])[2][0]]
        if len(state_refs) != 1: raise ValueError('unsupported RMS multi-state matrix')
        state = C.el(state_refs[0]); state_name = C.store[state[2][0]]
        values = [C.store[v] for v in C.store[state[2][1]]]
        actions = C.store[event[1]]
        if len(actions) != len(values): raise ValueError('RMS matrix/action dimension mismatch')
        raw_name = C.store[event[0]]
        canonical = raw_name.replace('_getoutofcar', '_get_out_of_car').replace('_Win', '_win')
        match = MEV_GROUP.fullmatch(canonical)
        if not match: raise ValueError('unsupported authored matrix event: ' + raw_name)
        kind = match.group(2).lower(); canonical = match.group(1) + '_' + kind
        events = [row for row in events if row[0] not in (raw_name, canonical)]
        for vi, value in enumerate(values):
            regions = [C.owner(C.el(a)[2][1])[1] for a in C.store[actions[vi]] if C.el(a)[0] in ('play_region', 'push_region')]
            if len(regions) > 1: raise ValueError('unsupported simultaneous state regions')
            plan = C.region_plan(regions[0]) if regions else []
            tail = add(plan[1][1][0], plan[1][0]) if len(plan) > 1 and plan[1][1] else None
            selected = [add(si, plan[0][0], tail) for si in plan[0][1]] if plan else []
            if len(selected) > 3: raise ValueError('state alternatives exceed MEVT capacity')
            key = canonical + '@' + value
            if len(key) >= 28: raise ValueError('state event exceeds MEVT name size')
            row = (key, selected, MEV_KIND[kind]); events.append(row)
            if vi == 0: events.append((canonical, selected, MEV_KIND[kind]))
            report.append(dict(event=canonical, state=state_name, value=value,
                               region=C.region_name(regions[0]) if regions else None,
                               files=[[C.stream_file(si)[1] for si in alts] for _, alts in plan]))
    return tracks, events, report

def glob_rsd(d):
    return [str(p) for p in Path(d).rglob('*.rsd')]

MIRROR = None
AMB_MAP = None      # подмена разбора ambience.rms (гейт tests/levels/ambience_check.py --plant: пустая привязка)

def mirror(M):
    import numpy as np
    global MIRROR
    if MIRROR is None:
        MIRROR = np.diag([1.0, 1.0, -1.0, 1.0])
    return MIRROR @ M @ MIRROR

def level_files(level):
    return [ART / f'l{level}_terra.p3d'] + sorted(ART.glob(f'l{level}r*.p3d')) + sorted(ART.glob(f'l{level}z*.p3d'))

def locators(level):
    """-> [(file, name, kind, data bytes, pos(game), [TriggerVolume dict])]"""
    import numpy as np
    out = []
    files = level_files(level) + [ART / 'missions' / f'level0{level}' / 'level.p3d']
    for f in files:
        if not f.exists():
            continue
        for c in walk(read(f)):
            if c.id != 0x3000005:
                continue
            nm, p = string(c.data)
            kind, ln = struct.unpack_from('<II', c.data, p)
            d = c.data[p + 8:p + 8 + ln * 4]
            x, y, z = struct.unpack_from('<3f', c.data, p + 8 + ln * 4)
            vols = []
            for k in c.children:
                if k.id != 0x3000006:
                    continue
                tn, q = string(k.data)
                typ, = struct.unpack_from('<I', k.data, q)
                he = struct.unpack_from('<3f', k.data, q + 4)
                M = mirror(np.array(struct.unpack_from('<16f', k.data, q + 16), np.float64).reshape(4, 4))
                vols.append(dict(type=typ, half=he, R=M[:3, :3], t=M[3, :3]))
            out.append((f.name, nm, kind, d, (x, y, -z), vols))
    return out

def sound_objects(level, res):
    """Objects whose 0x3000600 attribute names a sound -> [(x, y, z, r, kind, name)]"""
    import numpy as np
    KIND = {0x3f00002: 0, 0x3f0000e: 1, 0x3f0000a: 2}
    out = []
    for f in level_files(level):
        for ent in read(f).children:
            if ent.id not in KIND:
                continue
            eff = next((c for c in ent.children if c.id == 0x3000600), None)
            if not eff:
                continue
            snd = string(eff.data, 8)[0]
            # №460 (Роб: заборчики/барьеры на хайвее L5 при сбивании звучат не так, как деревья): заборы и барьеры, которые ломаются обломками
            # (donotenter, railcross, gatesmasha, fence, tunnelfence), получают звук дерева car_hit_tree — тот же глухой «тук» с треском, что у деревьев (№459).
            # FENCE_BREAK_OLD=1 — звук с диска (crate / tree как в данных)
            if not os.environ.get('FENCE_BREAK_OLD') and re.search(r'(donotenter|railcross|gatesmasha|_fence_|tunnelfence)', string(ent.data, 0)[0], re.I) and 'car_hit_tree' in res:
                snd = 'car_hit_tree'
            if snd not in res:
                continue
            def rec(c, m, acc):
                if c.id == 0x120103:
                    _, p = string(c.data)
                    m = np.frombuffer(c.data, '<f4', 16, p + 4).reshape(4, 4).astype(np.float64) @ m
                if c.id == 0x120107:
                    acc.append(mirror(m))
                for s in c.children:
                    rec(s, m, acc)
            mats = []
            for c in ent.children:
                if c.id == 0x3000008:
                    rec(c, np.eye(4), mats)
            for M in mats:
                x, y, z = M[3, :3]
                out.append((float(x), float(y), float(z), 1.5, KIND[ent.id], snd))
    return out

def mode_table(gl):
    rows = [('Normal', [float(gl.get(k, 1)) for k in ('SfxVolume', 'CarVolume', 'MusicVolume', 'AmbienceVolume',
                                                       'DialogueVolume')])]
    for m in ('Pause', 'MissionScreen', 'Letterbox', 'Dialogue', 'Store', 'OnFoot', 'Minigame', 'JustMusic', 'Credits'):
        keys = [m + b + 'Volume' for b in ('Sfx', 'Car', 'Music', 'Ambience', 'Dialogue')]
        if all(k in gl for k in keys):
            rows.append((m, [float(gl[k]) for k in keys]))
    return rows

# ---------------------------------------------------------------- правка 345: реплики водителя уровня и атмосфера комнат
# Атлас миссий 332 (PS2, своя pcsx2-twins без окна, 05.10): через 3 кадра после ✕ Continue карточки миссии оригинал читает
# DIALOG.RCF homer/w_mstart_hom_03 (Continue на кадре 420) или _04 (Continue на 500) — выбор случайный; на выходе из машины
# (6–8 кадров после △) — homer/w_goc_hom_legs / _horse; в Kwik-E-Mart — AMBIENCE.RCF room_small_01.
# [диск] dialog.spt: семейства W_Mstart_<код>_* и W_GOC_<код>_* есть у каждого водителя уровней — Hom (L1, L7), Brt (L2, L6),
# Lis (L3), Mrg (L4), Apu (L5); водитель уровня — AddCharacter в скриптах уровня, код — как у реплик разговоров (viewer/game_msn.c
# pcCodes). В пакет — семейства водителя своего уровня (движок берёт их по префиксу w_mstart_ / w_goc_, viewer/sound.c).
PC_CODE = {'homer': 'hom', 'bart': 'brt', 'lisa': 'lis', 'marge': 'mrg', 'apu': 'apu'}
PC_EVENTS = ('mstart', 'goc', 'gic')
PC_OFF = False      # подмена для гейта (tests/sound/pcsay_check.py --plant): семейства водителя не кладутся — как до правки 345

VO_OLD = False      # подмена для гейта (tests/sound/vo_check.sh): реплики аудита 09.10 (W_Card… и соперники V_*) не кладутся

def rival_code(driver, codes):
    """ПРАВИЛО ПОРТА (связи «водитель -> код реплик» на диске нет): код семейства V_ — тот, чьи буквы идут по порядку в имени водителя
    AddStageVehicle и первая совпадает: skinner -> skn (и skin), smithers -> smi, cletus -> clt, snake -> snk, wiggum -> wig"""
    d = driver.lower(); out = []
    for c in sorted(codes):
        if not c or c[0] != d[:1]: continue
        k = 0
        for ch in d:
            if k < len(c) and ch == c[k]: k += 1
        if k == len(c): out.append(c)
    return out

def rival_codes(lv, res):
    """коды соперников уровня: водители AddStageVehicle(…, "<водитель>") скриптов уровня, у которых на диске есть реплики V_*"""
    vcodes = {m.group(2).lower() for n in res for m in [re.match(r'V_([A-Za-z]+)_([A-Za-z]+)_', n)] if m and res[n]['script'] == 'dialog'}
    drivers = set()
    for f in sorted((GAME / 'scripts' / 'missions' / f'level0{lv}').glob('*.mfk')):
        txt = re.sub(r'//[^\n]*', '', f.read_text(errors='replace'))
        drivers |= set(re.findall(r'AddStageVehicle\([^)]*,\s*"([A-Za-z]+)"\s*\)', txt))
    codes = set()
    for d in drivers: codes |= set(rival_code(d, vcodes))
    return codes

def player_code(lv):
    """код водителя уровня: AddCharacter("<имя>", …) скриптов уровня -> 'hom' / 'brt' / …; не нашли — ''"""
    names = set()
    for f in sorted((GAME / 'scripts' / 'missions' / f'level0{lv}').glob('*.mfk')):
        names |= set(re.findall(r'^\s*AddCharacter\(\s*"([A-Za-z]+)"', f.read_text(errors='replace'), re.M))
    codes = {PC_CODE[n.lower()] for n in names if n.lower() in PC_CODE}
    assert len(codes) <= 1, ('у уровня несколько водителей', lv, names)
    return codes.pop() if codes else ''

# Атмосфера комнаты. [диск] ambience.rms: события L1_Kwik_E_Mart / L1_School / L1_House -> room_small_01_region (room_small_01),
# L2_DMV -> room_dmv_region (DMV), L3_Krustylou_Studios -> krustylou_studios_region; локаторов этих событий нет ни в одном файле
# диска (ни в уровнях, ни в комнатах l<N>i0<k>.p3d — там только выход, событие 5): оригинал включает их входом в комнату.
# ПРАВИЛО ПОРТА: событие комнаты — по имени (без префикса уровня, без «_», без регистра): совпало целиком (KwikEMart, DMV) или
# имя события — конец имени комнаты (SimpsonsHouse <- House); Springfield Elementary <- School — по смыслу (ROOM_ALIAS).
# [PS2] 05.10 (agent345/ps2 house2: из state_l1_start к двери дома, △): в доме Симпсонов оригинал читает room_small_01 (кадр 521,
# Гомер в комнате с 507) — дом подтверждён, как Kwik-E-Mart (атлас m0d); школа и DMV не сняты.
# Комната без события (Moes, AndroidsDungeon, Observatory, BartRoom) — атмосфера улицы, как было (вопрос близнецу).
ROOM_ALIAS = {'springfieldelementary': 'school'}
CLUB_OFF = os.environ.get('SND_CLUB_OFF') == '1'   # правка 593: событие 61 (клуб Legitimate Businessmen) снова тихий регион, как до правки
CLUB_EVENT = 61            # локаторы legit_business_music_01 / legit_businessmens_music (L2, L5): композиция l<N>_music.rms, событие Social_Club -> Social_Club_region = social_club.rsd, петля
ROOM_AMB_OFF = False  # подмена для гейта (tests/sound/room_amb_check.py --plant): таблицы нет — как до правки 345

def room_ambience(lv, amap):
    """-> [(имя комнаты порта, дорожка атмосферы, путь .rsd, событие)] для комнат уровня (файлы art/l<N>i0<k>.p3d)"""
    sys.path.insert(0, str(HERE))
    from interior_export import NAMES
    key = lambda n: re.sub(r'[^a-z]', '', re.sub(r'^L\d_', '', n).lower())
    ev = {}
    for m in amap.values():
        if m['file'] and os.path.exists(m['file']): ev.setdefault(key(m['event']), m)
    out = []
    for f in sorted(ART.glob(f'l{lv}i*.p3d')):
        mm = re.match(rf'l{lv}i(\d+)\.p3d$', f.name)
        if not mm or int(mm.group(1)) not in NAMES: continue
        room = NAMES[int(mm.group(1))]; rk = key(room); rk = ROOM_ALIAS.get(rk, rk)
        m = ev.get(rk) or next((v for k, v in ev.items() if len(k) >= 4 and rk.endswith(k)), None)
        if m: out.append((room, m['track'], m['file'], m['event']))
    return out

def car_stream_overlays(names, res, cars):
    """Authored streaming OverlayClipName variants; silent entries retain their probability."""
    if os.environ.get('CAR_STREAM_OLD'): return []
    out = []
    for car in names:
        name = cars[car].get('OverlayClipName')
        if not name or name not in res or not res[name]['stream']: continue
        r = res[name]
        assert r['loop'] and r['pitch'] == (1.0, 1.0), ('unsupported overlay contract', name)
        for i, file in enumerate(r['files']):
            path = AE.game_path(file)
            assert path, ('missing overlay', car, file)
            track = 'ov_' + name + (f'_{i}' if len(r['files']) > 1 else '')
            assert len(track) <= 27
            out.append((car, track, path, r['trim']))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--level', type=int, default=1)
    ap.add_argument('--out', default=str(ROOT / 'build' / 'audio'))
    ap.add_argument('--lang', choices=['fr', 'ge', 'sp'], default=None, help='№1111: вместо пакета — dialog_<язык>.sap (озвучка диалогов, разговоров миссий и гэгов уровня на языке копии игры; дорожки по номерам английского music.sap)')
    ap.add_argument('--car', default=None, help='машина игрока (по умолчанию — InitLevelPlayerVehicle уровня: famil_v, honor_v, …)')
    a = ap.parse_args()
    lv = a.level
    if not a.car:   # звук мотора — машины игрока уровня (leveli.mfk InitLevelPlayerVehicle), как модель в tools/psp_export.py
        from level_sweep import find_spawn
        a.car = find_spawn(lv).get('car') or 'famil_v'
    res, cars, pos, gl = AE.load_scripts()
    mdir = SND / 'music'

    # music
    music = [(n, str(mdir / (f + '.rsd')), lp, nx) for n, f, lp, nx in music_table(lv)]   # роли движка -> файлы уровня
    music = [m for m in music if os.path.exists(m[1])]

    # ambience zones: gates (event locators with trigger volumes) -> регион композиции ambience.rms (ambience_map)
    locs = locators(lv)
    amap = AMB_MAP if AMB_MAP is not None else ambience_map()
    gates, zone_skip, amb_silent = [], {}, {}
    club_zones = []
    for fn, nm, kind, d, p3, vols in locs:
        if kind != 0 or len(d) < 4:
            continue
        ev, = struct.unpack_from('<I', d)
        m = amap.get(ev)
        if m is None:
            continue                                       # не событие атмосферы (камеры, телепорты, трамплины …)
        if ev == CLUB_EVENT and not CLUB_OFF:               # правка 593: музыка клуба — зона музыкальной шины (viewer/sound.c sndFrame), не атмосфера
            club_zones.append((nm, ev, vols)); continue
        if not m['alts']:
            amb_silent[m['event']] = amb_silent.get(m['event'], 0) + len(vols)   # правило 4: тихий регион — не ворота
            continue
        if not m['file'] or not os.path.exists(m['file']):
            zone_skip[m['region']] = zone_skip.get(m['region'], 0) + 1
            continue
        gates.append((m, nm, ev, vols))
    # правило 5: атмосфера на старте — регион с наибольшим числом объёмов ворот (первым в пакете: viewer/sound.c sndLevelStart)
    weight = {}
    for m, _, _, vols in gates:
        weight[m['track']] = weight.get(m['track'], 0) + len(vols)
    order = sorted(weight, key=lambda t: -weight[t])       # sorted устойчив: при равенстве — порядок локаторов
    amb, amb_region, zones = [], {}, []
    for m, _, _, _ in sorted(gates, key=lambda g: order.index(g[0]['track'])):
        if m['track'] not in amb_region:
            amb_region[m['track']] = dict(region=m['region'], file=os.path.relpath(m['file'], GAME), events=[])
            amb.append((m['track'], m['file']))
        if m['event'] not in amb_region[m['track']]['events']:
            amb_region[m['track']]['events'].append(m['event'])
    for m, nm, ev, vols in gates:
        for v in vols:
            zones.append(dict(name=nm, event=ev, track=m['track'], **v))
    club = os.path.join(str(SND), 'music', 'social_club.rsd')
    if club_zones and os.path.exists(club):                 # правка 593: дорожка клуба — на шине музыки (имя social_club), зоны с событием 61
        music.append(('social_club', club, True, None))
        for nm, ev, vols in club_zones:
            for v in vols:
                zones.append(dict(name=nm, event=ev, track='social_club', **v))
    # правка 345: атмосфера комнат — дорожки после атмосферы ворот (первая дорожка атмосферы пакета — старт уровня)
    rooms_amb = [] if ROOM_AMB_OFF else room_ambience(lv, amap)
    for room, tr, f, ev in rooms_amb:
        if tr not in [t for t, _ in amb]: amb.append((tr, f))
    # круг 6 (g08, P09:17, Д9, P09:22): комната вне списка и миссии «дом» играют атмосферу room_small_01 (sound.c sndRoom, sndMissionAmbience) на ЛЮБОМ уровне, а таблица комнат
    # даёт её только уровням, где есть комната дома (L1, L4, L7): на L2, L3, L5, L6 дорожки не было и комната молчала. HOUSEAMB_SV_OLD=1 — как было
    if not ROOM_AMB_OFF and os.environ.get('HOUSEAMB_SV_OLD') != '1':
        hm = next((m for m in amap.values() if m['file'] and os.path.exists(m['file']) and re.sub(r'[^a-z]', '', re.sub(r'^L\d_', '', m['event']).lower()) == 'house'), None)
        if hm and hm['track'] not in [t for t, _ in amb]: amb.append((hm['track'], hm['file']))

    # positional loops: locators kind 1 -> positionalsettings.spt
    pos_tracks, emits = [], []
    for fn, nm, kind, d, p3, vols in locs:
        if kind != 1:
            continue
        key = nm if nm in pos else re.sub(r'_\d+$', '', nm)
        st = pos.get(key)
        if not st:
            continue
        clip = res.get(st.get('ClipName', key))
        if not clip or not clip['files']:
            continue
        path = AE.game_path(clip['files'][0])
        if not path:
            continue
        tn = next((t for t, pp in pos_tracks if pp == path), 'pos_' + key[:24])   # same file -> one track
        if tn not in [t for t, _ in pos_tracks]:
            pos_tracks.append((tn, path))
        rad = max(max(v['half']) for v in vols) if vols else float(st.get('MaxDistance', 30))
        emits.append(dict(name=nm, track=tn, x=p3[0], y=p3[1], z=p3[2], r=rad, mn=float(st.get('MinDistance', 5)),
                          mx=float(st.get('MaxDistance', 30)), prob=float(st.get('PlaybackProbability', 1)),
                          trim=clip['trim']))

    # Driver's in-car lines for this level (dialog.spt D_<event>_Hom_*, streamed in the original). Правка 183 (близнец Б2, ступень 3): оригинал за
    # рулём читает и W_<event>_Hom_* (w_bcrash, w_burn, w_break, w_mcrash, w_hitp — build/twins/b2/*/missing.tsv), и D_* (d_dcar,
    # d_break) — реплика события из обоих наборов; W_ берём только для событий, которые движок озвучивает (viewer/sound.c sayKey)
    W_EVENTS = ('bcrash', 'mcrash', 'damage', 'burn', 'dcar', 'break', 'air', 'hitp', 'missp', 'carway', 'door', 'arrive', 'time')
    # Правки 180/184 (близнец Б2: build/twins/b2/*/missing.tsv — оригинал читает generic_*/p_nhitbyc_*, p_hitbyc_*, traffic1/
    # d_carway_vm1_*): голоса улицы — прохожие (P_HitByC — сбит машиной, P_NHitByC — чуть не сбит, P_HitByW — удар Гомера) голосами
    # generic_* (Mn1/Mn2, Fm1/Fm2, By1/By2, Gr1/Gr2) и водители трафика (D_CarWay / D_Mcrash / D_Bcrash голосами Vm1/Vm2/Vf1/Vf2).
    # Движок: viewer/sound.c sndPedSay / sndDriverSay (какой прохожий каким голосом — правило порта, в данных диска связи нет)
    STREET = re.compile(r'(P_(N?HitByC|HitByW)_(Mn|Fm|By|Gr)[12]_|D_(CarWay|Mcrash|Bcrash)_V[mf][12]_)', re.I)
    pc = '' if PC_OFF else player_code(lv)   # правка 345: W_Mstart_<код>_* и W_GOC_<код>_* водителя уровня
    # Аудит 09.10 п. 12/58: реплики героя и соперников, которые оригинал озвучивает, а пакет не брал. Смысл семейств — по самим
    # записям диска (расшифровка 09.10, whisper large-v3-turbo): W_Card «I can sell this on the internet» — карточка; W_Mvic «We're
    # number one» / W_Mfail «No!» — итог миссии; W_Springboard «Bouncy bouncy!» — подкидывающая точка; W_Fall «AHHH! I'm gonna go
    # boom» — падение пешком; W_NewAI «Another one? Lousy generic villains» — новая машина погони (близнец b2_loop: w_newai_hom_02
    # на кадре 11250 в погоне Hit & Run); W_Pass «Eat my dust» / W_Passed «He used a cheat code» — обгон. Соперники V_<семейство>_
    # <код>: V_Mvic «You and/or your children will pay for this» (игрок победил), V_Mfail «What a remedial driver» (игрок проиграл),
    # V_Damage «I dropped my travel mug» (машину соперника ударили), V_Tail «My first taste of victory» (соперник впереди).
    # Код соперника — из водителей AddStageVehicle уровня (rival_codes). VO_OLD = True — как до аудита (красный tests/sound/vo_check.sh)
    W_VO = () if VO_OLD else ('card', 'mvic', 'mfail', 'springboard', 'fall', 'newai', 'pass', 'passed', 'hitbyc', 'tail', 'aidestroy', 'turbo', 'dodge', 'longjump')
    V_VO = ('mvic', 'mfail', 'damage', 'tail', 'hitcar')
    rivals = set() if VO_OLD else rival_codes(lv, res)
    dialog = []
    for n, r in sorted(res.items()):
        if r['script'] != 'dialog':
            continue
        mw = re.match(r'W_([A-Za-z]+)_'+re.escape(pc or 'hom')+'_', n, re.I)
        mp = re.match(r'W_([A-Za-z]+)_([A-Za-z]+)_', n)
        drv = bool(pc and mp and mp.group(1).lower() in PC_EVENTS and mp.group(2).lower() == pc)
        mv = re.match(r'V_([A-Za-z]+)_([A-Za-z]+)_', n)
        vo = bool(mw and mw.group(1).lower() in W_VO) or bool(mv and mv.group(1).lower() in V_VO and mv.group(2).lower() in rivals)
        if not (re.match(r'D_[A-Za-z]+_'+re.escape(pc or 'hom')+'_', n, re.I) or (mw and mw.group(1).lower() in W_EVENTS) or STREET.match(n) or drv or vo):
            continue
        p = AE.game_path(r['files'][0]) if r['files'] else None
        if p:
            dialog.append((n.lower()[:27], p))

    # В5: звуки гэгов уровня (GagSetSound в level.mfk -> nis.spt / interactive_props.spt, в оригинале потоковые) —
    # дорожки потока диалогов; у ресурса с несколькими AddFilename — «имя», «имя~1», «имя~2»… (viewer/main.c gagSound)
    import missions.mfk as MFK
    gag_names = []
    for c in MFK.parse((GAME / f'scripts/missions/level0{lv}/level.mfk').read_text(errors='replace'))[0]:
        if c.op == 'GagSetSound' and c.args and str(c.args[0]) not in gag_names: gag_names.append(str(c.args[0]))
    gags, gag_missing, gag_keys = [], [], {}
    for n in gag_names:
        r = res.get(n)
        paths = [AE.game_path(f) for f in (r['files'] if r else [])]
        paths = [p_ for p_ in paths if p_]
        if not paths: gag_missing.append(n); continue
        # имя дорожки = поле звука гэга в CLT1 (tools/collect_export.py): строчные, до 15 символов — иначе движок (audioTrack,
        # точное совпадение) не найдёт длинные имена уровней 2, 3, 5, 6 (gag_lvl2_dumpster, DMV_gag_willie_chaes_afterpig)
        key = n.lower()[:15]
        assert gag_keys.setdefault(key, n) == n, ('два звука гэгов с одним ключом', key, gag_keys[key], n)
        for k, p_ in enumerate(paths):
            tn = key if k == 0 else f'{key}~{k}'
            assert len(tn) <= 27, tn
            gags.append((tn, p_))
    dialog += gags
    named_counts = {}; doorb = b''
    if not VO_OLD:
        import street_voice_export
        named, doorb, named_counts = street_voice_export.extra(lv, GAME, res, AE.game_path)
        dialog += named

    # голос разговоров миссий уровня (поток диалогов) и музыка миссий (композиция l<N>_music.rms)
    voice, voice_bad = mission_voice(lv)
    assert not voice_bad, ('реплики без файла или с длинным именем', voice_bad)
    assert not ({t for t, _ in voice} & {t for t, _ in dialog}), 'имя реплики совпало с другой дорожкой диалогов'
    dialog += voice
    # подсказки обучения Барта (29.09, viewer/tutorial.c): только у уровня с EnableTutorialMode(1) в leveli.mfk (уровень 1)
    if tutorial_mode(lv):
        for nm in TUT_VOICES:
            p_ = GAME / 'conversations' / f'c_{nm}_l1t1.rsd'
            tn = voice_track_name(str(p_))
            assert p_.exists() and tn and tn not in {t for t, _ in dialog}, ('реплика подсказки', nm)
            dialog.append((tn, str(p_)))
    # CBG ticket/movie conditional gags: owned global conversations, L3 only.
    if lv == 3 and not os.environ.get('TICKET_CONTENT_C3_OLD'):
        from ticket_audio import streams as ticket_streams
        have_ticket = {n for n, _ in dialog}
        dialog += [(n, p) for n, p in ticket_streams(lv) if n not in have_ticket]
    # сверка g08-sound круг 2 (g07b P09:Д8): провал гонки заменяется разговором с Патти C_fail1/2_*_noboxconv_Pat_L<уровень>R<гонка>; дорожки в пакет
    if not os.environ.get('RACEFAIL_SV_OLD') == '1':
        have_rf = {n for n, _ in dialog}
        for p_ in sorted((GAME / 'conversations').glob(f'c_fail[12]_*_noboxconv_*_l{lv}r*.rsd')):
            tn = voice_track_name(str(p_))
            if tn and tn not in have_rf: dialog.append((tn, str(p_))); have_rf.add(tn)
    mtracks, mevents, mrep = mission_music(lv)
    # сверка g08-sound (P09 Д3, Д4, 18, 17): вставки поверх музыки (карточка пешком OF_found_card, фанфары Win_3Street_Races / Destroy_Camera_Bonus /
    # Gated_Mission) и особые куски зон атмосферы (OF_Apu_Oasis, ScaryMusic01, DuffBeer, StoneCutters); kind 6 — один проход, 7 — держится до следующей зоны.
    # Дорожки: первые шаги региона события (event_alts), до трёх. Музыка дома (OF_House: Tuba_005/012) — дорожки of_house, of_house~1.. (viewer/sound.c sndRoom)
    if not os.environ.get('MUSIC_SVERKA_OLD') == '1':
        known = {t[0] for t in mtracks}
        for ev, kind, loop in (('OF_found_card', 6, False), ('Win_3Street_Races', 6, False), ('Destroy_Camera_Bonus', 6, False), ('Gated_Mission', 6, False),
                               ('OF_Apu_Oasis', 7, True), ('ScaryMusic01', 7, True), ('DuffBeer', 7, True), ('StoneCutters', 7, True)):
            alts = event_alts(lv, ev)[:3]; ts = []
            for j, k in enumerate(alts):
                path = str(SND / 'music' / (k + '.rsd'))
                if not Path(path).is_file(): continue
                nm = ('x_' + ev.lower())[:23] + ('' if not j else '~%d' % j)
                if nm not in known: mtracks.append((nm, path, loop, None)); known.add(nm)
                ts.append(nm)
            if ts: mevents.append((ev, ts, kind))
        for j, k in enumerate(list(dict.fromkeys(event_alts(lv, 'OF_House')))[:2]):
            path = str(SND / 'music' / (k + '.rsd'))
            nm = 'of_house' if not j else 'of_house~%d' % j
            if Path(path).is_file() and nm not in known: mtracks.append((nm, path, False, None)); known.add(nm)
    have = {m[1] for m in music}; nroles = len(music); names = {m[0] for m in music}
    graph = os.environ.get('MUSIC_GRAPH_OLD') != '1'
    for n, p, lp, nx in mtracks:
        if p in have and not graph:                     # legacy file-only alias lost loop/next semantics
            continue
        nn, k = n, 1
        while nn in names: nn = f'{n[:23]}~m{k}'; k += 1   # имя роли движка занято другим файлом (уровни 2–7: Collection02_End_Pos
        music.append((nn, p, lp, nx)); have.add(p); names.add(nn)   # бонуса — не файл роли) — у дорожки миссии своё имя
    byname = {}
    for n, p, lp, nx in music: byname.setdefault(p, n)
    alias = {n: n if graph else byname[p] for n, p, _, _ in mtracks}
    # «следующая» у ролей — имена ролей; у дорожек миссий — через alias (имя файла в пакете)
    music = music[:nroles] + [(n, p, lp, alias.get(nx, nx) if nx else nx) for n, p, lp, nx in music[nroles:]]
    mevents = [(e, [alias[t] for t in ts], k) for e, ts, k in mevents]

    # objects with sounds -> OBJS
    objs = sound_objects(lv, res)
    sfx = list(SFX)
    # круг 4 (дирижёр, банк в 2112 КБ, правка №1069): резидентный sfx.sab должен вмещаться в фиксированный участок на PSP. Из новых клипов круга 1-3 остаются только те,
    # что игра вызывает: звуки подвижных объектов (sndAnimObj не подключён: у AoObj нет имени) и взлёт птиц (событий нет) не выгружаются; подбор предмета миссии —
    # клип только своего уровня; monkey_collect — L2, nuclear_waste_collect и traffic_coffin — L7. SND4_FULL=1 — весь список как в круге 3
    if os.environ.get('SND4_FULL') != '1':
        drop = {'trapdoor', 'platform_01', 'platform_02', 'aplatform6', 'duff_truck', 'plantdoor', 'elevator', 'gag_library', 'hit_switch_quick',
                'pigeon_takeoff', 'gull_takeoff', 'robo_arm'}
        # круг 6 (g08, проверка проводки): g11 подключил звуки подвижных объектов (animobj.c, objnames.shr), поэтому клипы возвращаются на уровни, где такие
        # объекты есть (список снят с objnames.shr каждого уровня): L1 и L4 — платформы, люк, дверь, грузовик (L4 ещё библиотека), L2 — лифт, L3 и L6 — рука-робот,
        # L5 — кран (platform_02) и лифт, L7 — грузовик. Взлёт голубей (триггер flyingpidgeon) есть на L2 и L5; триггера чаек нет нигде — gull_takeoff не выгружается.
        # ANIMOBJ_DROP_OLD=1 — как в круге 4 (клипы объектов не выгружаются)
        if os.environ.get('ANIMOBJ_DROP_OLD') != '1':
            # резидентно остаются короткие (люк, платформы, дверь); длинные (лифт 160 КБ, грузовик 91, рука-робот 140, библиотека 59, aplatform6 38, взлёт голубей 20) не влезают
            # в банк 2112 КБ на L2/L4/L5, поэтому идут позиционным потоком из music.sap (дорожка anim_<клип>, sound.c sndAnimObj) на тех уровнях, где объект есть
            keep = {1: {'trapdoor', 'platform_01', 'platform_02', 'plantdoor'}, 4: {'trapdoor', 'platform_01', 'platform_02', 'plantdoor'}, 5: {'platform_02'}}.get(lv, set())
            drop -= keep
            animStreams = {1: ['aplatform6', 'duff_truck'], 2: ['elevator', 'pigeon_takeoff'], 3: ['robo_arm'], 4: ['aplatform6', 'duff_truck', 'gag_library'],
                           5: ['elevator', 'pigeon_takeoff'], 6: ['robo_arm'], 7: ['duff_truck']}.get(lv, [])
            for n in animStreams:
                clip = res.get(n)
                path = AE.game_path(clip['files'][0]) if clip and clip['files'] else None
                if path and ('anim_' + n) not in [t for t, _ in pos_tracks]: pos_tracks.append(('anim_' + n, path))
        drop |= {'level_%d_pickup_sfx' % k for k in range(1, 7) if k != lv}
        if lv != 2: drop.add('monkey_collect')
        if lv != 7: drop |= {'nuclear_waste_collect', 'traffic_coffin'}
        # не вызываются игрой: телефон занят, чит-звуки, «continue» и «cancel» меню (событий нет); wasp_charged_idle: g11b при отсутствии клипа играет wasp_idle (того же размера)
        drop |= {'continue', 'cancel', 'wasp_charged_idle'}
        # ps2-twin P05:25 (g07b-msnview): «занято» у будки при выключенных будках (миссия с принудительной машиной) играет △ у будки (main_actbooth.h) — клип 14 КБ остаётся в банке уровня
        # (запас слота у самого тесного L2 38 976 Б). PHONEBUSY_OLD=1 — как было (клип убран)
        if os.environ.get('PHONEBUSY_OLD') == '1': drop.add('phonebusy')
        # №1104 (коды оригинала, g10-flow): cheat_success / cheat_fail играют при вводе кода в OPTIONS паузы — остаются в банке уровня (~10 КБ). CHEATS_SND1104_OLD=1 — как было (убраны)
        if os.environ.get('CHEATS_SND1104_OLD') == '1': drop |= {'cheat_success', 'cheat_fail'}
        # круг 6 (g08, проверка проводки): g09a влил событие UI_EV_STING (ui.c: образец после остановки ползунка Sound), поэтому четыре образца громкости
        # (music/sfx/car/dialog_stinger, вместе 29 КБ) снова в банке. STING_DROP_OLD=1 — как в круге 4 (образцов нет)
        if os.environ.get('STING_DROP_OLD') == '1': drop |= {'car_stinger', 'dialog_stinger', 'music_stinger', 'sfx_stinger'}
        else: drop -= {'car_stinger', 'dialog_stinger', 'music_stinger', 'sfx_stinger'}
        sfx = [n for n in sfx if n not in drop]
    for o in objs:
        if o[5] not in sfx:
            sfx.append(o[5])

    # stream table order (build_pack): music, ambience, dialog, positional
    track_index = {n: i for i, n in enumerate([m[0] for m in music] + [t for t, _ in amb] + [t for t, _ in dialog]
                                              + [t for t, _ in pos_tracks])}

    # EXT
    modes = mode_table(gl)
    sec = []
    b = bytearray()
    for name, v in modes:
        b += AE.pack_name(name, 16) + struct.pack('<5f', *v)
    sec.append((b'MODE', bytes(b), len(modes), 36))
    sec.append((b'OBJS', None, len(objs), 20))
    b = bytearray()
    for zn in zones:
        R = zn['R']
        # rows of R are the local axes in world (row-vector convention of the p3d matrices); normalise, keep scale out
        import numpy as np
        ax = [R[i] / (np.linalg.norm(R[i]) or 1.0) for i in range(3)]
        half = [zn['half'][i] * float(np.linalg.norm(R[i])) for i in range(3)]
        b += struct.pack('<II3f9f3f', zn['type'], zn['event'], *half, *[float(v) for r_ in ax for v in r_],
                         *[float(v) for v in zn['t']]) + AE.pack_name(zn['track'], 28)
    sec.append((b'ZONE', bytes(b), len(zones), 96))
    b = bytearray()
    for e in emits:
        b += struct.pack('<6fHHf', e['x'], e['y'], e['z'], e['r'], e['mn'], e['mx'], track_index[e['track']],
                         0 if os.environ.get('POSLOOP_SV_OLD') == '1' else int(round(e['prob'] * 1000)) + 1,   # сверка g08-sound (P09:26): pad = вероятность·1000 + 1, громкость — трим (бросок при входе в объём)
                         e['trim'] if not os.environ.get('POSLOOP_SV_OLD') == '1' else e['trim'] * e['prob'])
    sec.append((b'EMIT', bytes(b), len(emits), 32))
    b = bytearray()   # MEVT 36 B: char event[28], i16 track[3] (индексы music.sap, -1 нет), u8 n, u8 kind (MEV_KIND)
    for e, ts, k in mevents:
        ids = [track_index[t] for t in ts] + [-1] * (3 - len(ts))
        b += AE.pack_name(e, 28) + struct.pack('<3hBB', *ids, len(ts), k)
    sec.append((b'MEVT', bytes(b), len(mevents), 36))
    b = bytearray()   # RAMB 56 B (правка 345): char room[28], char track[28]
    for room, tr, _, _ in rooms_amb:
        b += AE.pack_name(room, 28) + AE.pack_name(tr, 28)
    sec.append((b'RAMB', bytes(b), len(rooms_amb), 56))

    if doorb: sec.append((b'DORB', doorb, len(doorb)//96, 96))

    def ext_blob(resnames):
        idx = {n: i for i, n in enumerate(resnames)}
        ob = bytearray()
        for x, y, z, r, k, s in objs:
            ob += struct.pack('<4fBBh', x, y, z, r, k, 0, idx.get(s, -1))
        parts, off = [], 8 + 16 * len(sec)
        head = b'SXT1' + struct.pack('<I', len(sec))
        body = bytearray()
        for tag, data, cnt, stride in sec:
            data = bytes(ob) if tag == b'OBJS' else data
            assert len(data) == cnt * stride, (tag, len(data), cnt, stride)
            head += tag + struct.pack('<III', off + len(body), cnt, stride)
            body += data
            body += b'\0' * (-len(body) % 16)
        return head + bytes(body)

    from traffic_export import level_script
    traffic_cars = [name for name, _, _ in level_script(lv)[0]]
    traffic_cars += [n for n in level_player_cars(lv) if n.lower() not in [t.lower() for t in traffic_cars + [a.car]]]

    # the resource order is decided inside build_pack; run it twice (cheap) so OBJS carries the final indices
    overlays = car_stream_overlays(list(dict.fromkeys([a.car, *traffic_cars])), res, AE.load_scripts()[1])
    if a.lang:   # №1111: язык озвучки — только файл поверх music.sap (дорожки английского пакета; нет перевода у реплики — остаётся английская)
        lidx = AE.lang_index(a.lang)
        if not lidx:   # на диске нет озвучки этого языка — набор без файла (tools/ship_data.sh: необязательный)
            print('НЕОБЯЗАТЕЛЬНО: пропуск — на диске нет озвучки языка %s' % a.lang); return
        resL = AE.load_scripts(lang=a.lang)[0]
        gagSpot = {}
        for n in gag_names:
            r_ = resL.get(n); ps_ = [AE.game_path(f) for f in (r_['files'] if r_ else [])]; ps_ = [q for q in ps_ if q]
            for k_, q in enumerate(ps_): gagSpot[n.lower()[:15] if k_ == 0 else f"{n.lower()[:15]}~{k_}"] = q
        items, kept = [], 0
        for tn, p_ in dialog:
            loc = gagSpot.get(tn) or lidx.get(os.path.relpath(p_, GAME).replace('\\', '/').lower())
            if loc and tn in track_index and os.path.realpath(loc) != os.path.realpath(p_): items.append((track_index[tn], tn, loc))
            else: kept += 1
        os.makedirs(a.out, exist_ok=True)
        size = AE.build_lang_overlay(os.path.join(a.out, 'dialog_%s.sap' % a.lang), sorted(items))
        print('level %d lang %s: на языке %d дорожек диалогов из %d (остаются английскими %d), dialog_%s.sap %.1f МБ' % (lv, a.lang, len(items), len(dialog), kept, a.lang, size / 1048576))
        return
    rep = AE.build_pack(a.out, a.car, sfx, music, amb, dialog, ext=ext_blob([]), quiet=True, pos_sel=pos_tracks, extra_cars=traffic_cars, car_sel=overlays)
    rep = AE.build_pack(a.out, a.car, sfx, music, amb, dialog, ext=ext_blob(rep['resources']), pos_sel=pos_tracks, extra_cars=traffic_cars, car_sel=overlays)

    resident = rep['resident_audio_bytes']
    report = dict(level=lv, car=a.car, car_sound_profiles=rep['cars'], sab_bytes=rep['sab_bytes'], sap_bytes=rep['sap_bytes'], resident_audio_bytes=resident,
                  resources=len(rep['resources']), missing=rep['missing'], modes=[m for m, _ in modes],
                  music=[m[0] for m in music], ambience=[t for t, _ in amb], positional=[t for t, _ in pos_tracks],
                  named_voice_counts=named_counts, doorbells=len(doorb)//96,
                  dialog=len(dialog), gag_tracks=[t for t, _ in gags], gag_sounds_missing=gag_missing,
                  zones=len(zones), zones_without_file=zone_skip, ambience_regions=amb_region, ambience_silent_events=amb_silent,
                  emitters=len(emits),
                  objects=len(objs), object_sounds=sorted({o[5] for o in objs}),
                  voice_lines=len(voice), mission_music=mrep, music_events={e: ts for e, ts, _ in mevents},
                  driver=pc, driver_lines=sorted(t for t, _ in dialog if re.match(r'w_(mstart|goc)_', t)),
                  room_ambience={room: dict(track=tr, event=ev) for room, tr, _, ev in rooms_amb},
                  undefined_car_clips={n: {k: v for k, v in cars[n].items()
                                           if 'ClipName' in k and v and v not in res}
                                       for n in rep['cars']
                                       if any('ClipName' in k and v and v not in res for k, v in cars[n].items())})
    json.dump(report, open(os.path.join(a.out, 'sound_report.json'), 'w'), indent=1)
    print('level %d: %d resources resident (%.0f KB audio), %d music, %d ambience zones (%d gates), %d positional loops '
          '(%d emitters), %d Homer lines, %d sound objects' % (lv, len(rep['resources']), resident / 1024, len(music),
                                                               len(amb), len(zones), len(pos_tracks), len(emits),
                                                               len(dialog), len(objs)))
    if zone_skip:
        print('ambience regions without a file (silent gates):', zone_skip)

if __name__ == '__main__':
    main()
