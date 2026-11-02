#!/usr/bin/env python3
"""Homer on foot (stage 4) -> build/homer.shr.

Same CHR1 format as tools/char_export.py (loaded by viewer/skin.c skinLoad, documented in viewer/skin.h): the skin
(art/chars/homer_m.p3d, PS2 strips kept, GE hardware skinning batches of <= 8 bones), the skeleton (33 joints),
textures CLUT8 with the exact original palettes (char_swatches_lit + 4 eyeball blink frames, swizzled) and ONLY the
clips the stage-4 controller (viewer/homer.c) plays, with the original sparse keys and timings (30 fps).

  python tools/homer_export.py                  # -> build/homer.shr
  python tools/homer_export.py --out X.shr --preview DIR

Budget 1.5 MB; the file is ~0.26 MB. Clips are picked by name from homer.cho (the original choreo bank):
  loco (walkerLoco: idle / walk / run / dash), jump (take off / in air / land / double jump dash), kick on the ground
  (hom_break = "break"), jump kick, stomp (antic / in air / land), get in / out of the car on the driver side with the
  door (open / get in / close and back; "_high" variants for tall vehicles), in-car idle, electrocuted/hit (flail,
  get up), hit switch, victory, surf on a car roof, long idles.
"""
import argparse, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import char_export as ce                                       # noqa: E402  (same exporter, filtered clip list)

CLIPS = [
    # locomotion (homer.cho locomotion "walkerLoco": idle + 3 velocities)
    'hom_loco_idle_rest', 'hom_loco_walk', 'hom_loco_run', 'hom_loco_dash',
    # jump / double jump (cho: jump_idle_launch blendIn .01, jump_idle blendIn .05 / out .1, jump_idle_land blendIn .01,
    # jump_dash_in_air blendIn .5 / out .05)
    'hom_jump_idle_take_off', 'hom_jump_idle_in_air', 'hom_jump_idle_land', 'hom_jump_dash_in_air',
    # attacks: kick on the ground ("break"), jump kick, stomp
    'hom_break', 'hom_jump_kick', 'hom_stomp_antic', 'hom_stomp_in_air', 'hom_stomp_land',
    # car, driver side (door opens / closes), normal and high vehicles
    'hom_get_into_car_open_door_driver', 'hom_get_into_car_driver', 'hom_get_into_car_close_door_driver',
    'hom_get_out_of_car_open_door_driver', 'hom_get_out_of_car_driver', 'hom_get_out_of_car_close_door_driver',
    'hom_get_into_car_open_door_high_driver', 'hom_get_into_car_high_driver', 'hom_get_into_car_close_door_high_driver',
    'hom_get_out_of_car_open_door_high_driver', 'hom_get_out_of_car_high_driver', 'hom_get_out_of_car_close_door_high_driver',
    'hom_in_car_idle',
    # правки Роба 220/231 (03.10: «в машине толстый Гомер, руль сквозь живот, не анимирован»): за рулём оригинал играет набор
    # водителя ndr (homer.cho: «in car animations borrowed from ndr»: in_car_idle_driver -> ndr_in_car_idle, turn_left_driver ->
    # ndr_turn_left ...) из art/chars/ndr_a.p3d — тело на 17 см дальше от руля, руки на руле; hom_in_car_idle — поза пассажира
    'ndr_in_car_idle', 'ndr_turn_left', 'ndr_turn_right', 'ndr_accelerate', 'ndr_decelerate', 'ndr_crash',
    # круг 2, g04-homer (СВЕРКА P05-1): радость за рулём — малая и большая победа водителя, и взгляд назад (стрелка вниз в машине); клипы набора
    # ndr (в homer.cho: in_car_victory_small/large_driver, look_back_driver). HOMER_CLIPS_OLD=1 — без них (как было)
    # hit / electrocuted (wasp beam) / fall
    'hom_flail', 'hom_get_up',
    # world interaction, rewards, car roof
    'hom_hit_switch', 'hom_victory_small', 'hom_victory_large', 'hom_surf_in', 'hom_surf_cycle', 'hom_surf_out',
    # long idles of the regular skin (cho: homer_idle0..2)
    'hom_idle_scratch', 'hom_idle_check_shoes', 'hom_idle_chili',
    # разговор миссии (правка Роба 38; cho: dialogue_* -> hom_dialogue_*; реплика за репликой по AddAmbientPcAnimation скрипта)
    'hom_dialogue_thinking', 'hom_dialogue_hands_on_hips', 'hom_dialogue_hands_in_air', 'hom_dialogue_shaking_fist',
    'hom_dialogue_scratch_head', 'hom_dialogue_yes', 'hom_dialogue_no', 'hom_dialogue_cross_arms',
    'hom_dialogue_open_arm_hand_gesture', 'hom_dialogue_shake_hand_in_air',
    # сверка g04-homer круг 5: клипы, на которые .cho Гомера ссылается чужими именами — уворот при взрыве машины (dodge -> npd_dodge, art/chars/npd_a.p3d),
    # взгляд назад и покачивание (look_back_driver / sway_*_driver -> ndr_*), махание водителя (wave_driver / wave_goodbye_driver -> ndr_wave*),
    # радость за рулём (in_car_victory_*_driver -> ndr_in_car_victory_*)
    'npd_dodge', 'ndr_look_back', 'ndr_sway_left', 'ndr_sway_right', 'ndr_wave', 'ndr_wave_goodbye', 'ndr_in_car_victory_large', 'ndr_in_car_victory_small',
]


# правка Роба 352 (05.10: «на каждом уровне свой главный игрок»): [диск] игрок уровня — AddCharacter(имя, имя) в
# scripts/missions/levelNN/leveli.mfk (L1 homer, L2 bart, L3 lisa, L4 marge, L5 apu, L6 bart, L7 homer). У каждого свои
# art/chars/<имя>_m.p3d (модель и скелет, 33 сустава; у Мардж +3 сустава волос), <имя>_a.p3d (свои клипы с префиксом
# hom_/brt_/lsa_/mrg_/apu_ — те же имена движений, что у Гомера) и <имя>.cho; за рулём все берут набор водителя ndr
# («in car animations borrowed from ndr and nps anim sets» в каждом .cho). Длинные idle обычного костюма — <имя>_idle0..2 .cho
# (у Гомера hom_idle_scratch / check_shoes / chili). Пакет игрока: tools/homer_export.py --level N -> l<N>/player.shr
PLAYERS = ('homer', 'bart', 'lisa', 'marge', 'apu')

if __import__('os').environ.get('HOMER_CLIPS_OLD') != '1':
    CLIPS += ['ndr_in_car_victory_small', 'ndr_in_car_victory_large', 'ndr_look_back']
    # круг 2, g04-homer (P05 Д3): дверь пассажира — те же клипы без суффикса _driver (посадка и выход, обычные и «_high»)
    CLIPS += ['hom_get_into_car_open_door', 'hom_get_into_car', 'hom_get_into_car_close_door', 'hom_get_out_of_car_open_door', 'hom_get_out_of_car',
              'hom_get_out_of_car_close_door', 'hom_get_into_car_open_door_high', 'hom_get_into_car_high', 'hom_get_into_car_close_door_high',
              'hom_get_out_of_car_open_door_high', 'hom_get_out_of_car_high', 'hom_get_out_of_car_close_door_high']


def level_player(level):
    """[диск] игрок уровня: AddCharacter("имя", ...) из leveli.mfk уровня"""
    import re
    f = ce.GAME / f'scripts/missions/level{level:02d}/leveli.mfk'
    txt = re.sub(r'//[^\n]*', '', f.read_text(errors='replace'))
    m = re.search(r'AddCharacter\s*\(\s*"([^"]+)"', txt)
    if not m: raise SystemExit(f'no AddCharacter in {f}')
    name = m.group(1).lower()
    if name not in PLAYERS: raise SystemExit(f'unknown player {name} in {f}')
    return name


def cho_idles(char):
    """[диск] длинные idle обычного костюма: animation "<char>_idle0..2" { animation "<клип>"; } в art/chars/<char>.cho"""
    import re
    txt = (ce.GAME / f'art/chars/{char}.cho').read_text(errors='replace')
    out = {}
    for m in re.finditer(r'animation\s+"%s_idle(\d)"\s*\{\s*animation\s+"([^"]+)"' % re.escape(char), txt):
        out.setdefault(int(m.group(1)), m.group(2))
    return [out[k] for k in sorted(out)]

# сверка g04-homer круг 6: клипы круга 2 и круга 5 перечислены дважды (ndr_look_back, ndr_in_car_victory_*) — убрать повторы, порядок прежний
CLIPS = list(dict.fromkeys(CLIPS))


def clip_list(char, prefix):
    """CLIPS Гомера с префиксом персонажа; длинные idle — из .cho персонажа (у Гомера — те же три клипа, порядок прежний)"""
    idles = [c for c in CLIPS if c.startswith('hom_idle_')]
    out = []
    for c in CLIPS:
        if c in idles:
            if c == idles[0]: out += cho_idles(char)
            continue
        out.append(prefix + c[4:] if c.startswith('hom_') else c)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=str(ROOT / 'build/homer.shr'))
    ap.add_argument('--preview')
    ap.add_argument('--skin', help='правка Роба 107: модель костюма art/chars/<skin>.p3d (h_undr_m, h_fat_m, h_stcr_m) на скелете и клипах игрока')
    ap.add_argument('--char', choices=PLAYERS, help='правка 352: персонаж (по умолчанию homer или игрок уровня --level)')
    ap.add_argument('--level', type=int, help='правка 352: игрок уровня N (AddCharacter leveli.mfk) — пакет l<N>/player.shr')
    ap.add_argument('--skin-index', type=int, default=0, help='правка 352: костюм K уровня --level по rewards.mfk (0 — обычный; l<N>/skin<K>.shr)')
    a = ap.parse_args()
    char = a.char or (level_player(a.level) if a.level else 'homer')
    skin = a.skin
    if a.skin_index:
        if not a.level: raise SystemExit('--skin-index needs --level')
        import skins_export as SE   # noqa: E402  (BindReward "skin" уровня: [(имя, модель, цена)], первый — обычный)
        sk = SE.skins(a.level)
        if a.skin_index >= len(sk): raise SystemExit(f'level {a.level}: {len(sk) - 1} costumes, no #{a.skin_index}')
        skin = sk[a.skin_index][1]
    skin = skin or f'{char}_m'
    orig = ce.load_clips
    need = {}

    def load_needed(root, joints):
        allc = orig(root, joints) + orig(ce.read(ce.GAME / 'art/chars/ndr_a.p3d'), joints) + orig(ce.read(ce.GAME / 'art/chars/npd_a.p3d'), joints)   # + набор водителя ndr (правки 220/231) и прохожего npd (dodge, круг 5)
        byname = {c['name']: c for c in allc}
        pre = next((c['name'][:-len('loco_idle_rest')] for c in allc if c['name'].endswith('_loco_idle_rest')), None)
        if not pre: raise SystemExit(f'{char}: no *_loco_idle_rest clip')
        names = clip_list(char, pre); need['n'] = names
        missing = [n for n in names if n not in byname]
        if missing: raise SystemExit(f'missing clips in {char}_a.p3d: {missing}')
        return [byname[n] for n in names]                    # order = CLIPS (homer.c looks clips up by name anyway)
    ce.load_clips = load_needed
    out = Path(a.out)
    ce.export(char, f'art/chars/{skin}.p3d', f'art/chars/{char}_a.p3d', out)
    size = out.stat().st_size
    print(f'{out.name} ({char}, {skin}): {len(need["n"])} clips, {size:,} bytes (budget 1,572,864)')
    if size > 1536 * 1024: raise SystemExit('over budget')
    if a.preview:
        for p in ce.preview(out, Path(a.preview)): print('  preview', p)


if __name__ == '__main__':
    main()
