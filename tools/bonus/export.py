#!/usr/bin/env python3
"""Бонус-игра (правка Роба 420): трассы мини-игры с диска -> данные порта.

  python tools/bonus/export.py --track 1 [--out build/bonus]     одна трасса: b01.shr (SHR7) + b01.bns (правила, точки)
  python tools/bonus/export.py --all                              все 7 трасс меню (b01..b07) + пакет машин msncars-b.shr
  python tools/bonus/export.py --cars                             только пакет машин

Что пишется (папка bonus/ рядом с EBOOT, viewer/bonus.c):
  * b0N.shr — трасса тем же экспортом, что уровни (tools/psp_export.py main(scenes=[b0N], coins=False)): город, коллизия COL1,
    свет "sun" трассы (LGT1), зоны прыжка/DEATH из b0Ndata (tools/jumps_export.py), PVS и обход граней (tools/pvs, tools/wind) —
    те же флаги качества, что tools/build_level.sh. Машина CAR1 — famil_v (ПРАВИЛО ПОРТА: заглушка формата; машины гонки
    рисуются моделями пакета msncars-b.shr).
  * b0N.bns — правила трассы (BNS1, формат — viewer/bonus.h): камера sprint_camShape, финиш и чекпойнты по порядку круга
    (объёмы TriggerVolume), старты car1..car4 (курс из данных), точки пути ИИ по ходу (WPR) и обратно (WPL) в порядке обхода
    (tools/bonus/track.py lap_cycle — правило порта: с какой точки начинать, данные не говорят), радиус точки — объём её триггера.
  * msncars-b.shr — машины гонки (tools/msncar_export.py --names): 43 машины выбора MiniMenu.pag (Vehicle*_Value) — все есть
    на диске; модели читаются по требованию, как машины миссий.
  С 09.10 (часть «подборы, ловушки, огонь, DEATH»): b0N.shr несёт модели подборов — типы SPR1 wrench (art/wrench.p3d) и nitro
  (art/missions/generic/nitro.p3d, грузит ss.mfk) в точках ActionEvent Nitro/Wrench b0Ndata (tools/psp_export.py EXTRA_PICKUPS);
  b0N.bns — версия 2: после пути ИИ подборы (32 Б) и зоны (128 Б: ловушки Event 66, B04G Event 63, свет Event 65), число — в
  pad[0..2] заголовка, объект ловушки (spikes/pistons по строкам SLES_518.97) проверяется. BONUS_PICKUPS_OLD=1 — как до части
  (BNS версии 1, без моделей подборов): красный контроль экспорта."""
import argparse, json, math, os, re, struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'tools/bonus'))
import track as T   # noqa: E402
import roads as RD   # noqa: E402

VEHICLES = ['apu_v', 'bart_v', 'bbman_v', 'bookb_v', 'burns_v', 'carhom_v', 'cArmor', 'cCola', 'cCurator', 'cDonut', 'cDuff',
            'cFire_v', 'cHears', 'cKlimo', 'cletu_v', 'cLimo', 'cNerd', 'comic_v', 'cPolice', 'cSedan', 'cVan', 'elect_v', 'famil_v',
            'fone_v', 'frink_v', 'gramp_v', 'hbike_v', 'homer_v', 'honor_v', 'krust_v', 'lisa_v', 'marge_v', 'moe_v', 'mrplo_v',
            'otto_v', 'plowk_v', 'scorp_v', 'skinn_v', 'smith_v', 'snake_v', 'wiggu_v', 'willi_v', 'zombi_v']   # MiniMenu.pag Vehicle0_Value
RACE_VEHICLES = ["pickupA", "cLimo", "otto_v"]  # measured default b01; menu still 43
# круг 5 (g07b, P17-19): соперники случайно из полного списка 67 машин игры — к 43 машинам выбора добавлены ещё 24 (без повторов с VEHICLES и RACE_VEHICLES). BONUS_AI_ALL_OLD=1 — прежний состав пакета
AI_VEHICLES = ['gramR_v', 'atv_v', 'knigh_v', 'mono_v', 'oblit_v', 'hype_v', 'rocke_v', 'cCellA', 'cCube', 'cBlbart', 'cMilk', 'cNonup', 'cBone', 'compactA', 'minivanA',
               'sedanA', 'sedanB', 'sportsA', 'sportsB', 'wagonA', 'SUVA', 'taxiA', 'coffin', 'hallo']
# Правка Роба 453 (10.10, «текстуры бонус-трасс галимые»): у трасс предел стороны текстуры — родной размер диска (до BONUS_TEX_CAP, по
# умолчанию 512), а не 128 города: мипы считаются из исходника (LANCZOS, до 1 px), палитра по-прежнему точная/k-means 256 по всем
# уровням, T4 у текстур до 16 цветов. Трасса — отдельная сцена без города (стенд 1,7–7,5 МБ SHR7 при 37 МБ у уровня 1).
# BONUS_TEX_OLD=1 — прежнее (128)
# Правка Роба 473 (10.10, «на трассах с камерой за машиной вокруг пустота»): трассе — фон уровня, к которому она привязана (небо и
# горизонт из его *_terra, дальние холмы и облака; только картинка, без коллизии). Привязка трасса -> уровень: BACKDROP (по умолчанию
# b0N -> LN по общим текстурам, привязка Sonnet-Трассы) или BONUS_BACKDROP="1:1,2:2,..."; BONUS_BACKDROP_OLD=1 — без фона, как было
BACKDROP = {n: n for n in range(1, 8)}   # привязка от Sonnet-Трассы (общие текстуры b0N с l<N>*): b0N рисует фон уровня N
if os.environ.get('BONUS_BACKDROP'):
    BACKDROP = {int(a): int(b) for a, b in (kv.split(':') for kv in os.environ['BONUS_BACKDROP'].split(',') if kv)}
if os.environ.get('BONUS_BACKDROP_OLD') == '1': BACKDROP = {}
MIPFADE_OLD = os.environ.get('BONUS_MIPFADE_OLD') == '1'
FADE_EXTRA_GROUND = r'straight[0-9]*$|ditch|boardwalk|pwrplnt_floor|steps_conc'   # дорожное полотно трасс (straight*), канава, настил, пол электростанции, ступени
FADE_EXTRA_WALL = r'wrs_(oldbrick|stucco|wood)_|wrf_grey|old_parapits'   # стены городских кварталов трасс
TEX_CAP = 128 if os.environ.get('BONUS_TEX_OLD') == '1' else int(os.environ.get('BONUS_TEX_CAP', '512'))
# Правка Роба 472 (10.10, «текстуры трасс галимые»): трасса b0N изображает город уровня N (довод по диску: из имён текстур b0N больше всего
# совпадает с текстурами l<N>_terra/l<N>r*/l<N>z* именно уровня N, на всех семи трассах; b01 — парковка Krusty Burger района L1 по кадрам
# PS2). Многие текстуры трассы лежат на диске CLUT4 (16 цветов) или вдвое меньше, а одноимённые в городе — CLUT8/крупнее. Такая
# текстура трассы заменяется городской того же имени (UV те же: нормированные), если город лучше (сторона больше, или тот же размер и
# цветов >= 1,5x при <256 у трассы), рисунок тот же (средняя разница RGB после приведения к размеру трассы <= BONUS_TEX_CITY_DIFF, 15)
# и сохранены признак прозрачности и пропорции. Вывески, финиш, старт и всё, чего нет в городе, остаются своими.
# BONUS_TEX_CITY_OLD=1 — как до правки (текстуры трассы)
TEX_CITY = os.environ.get('BONUS_TEX_CITY_OLD') != '1'
TEX_CITY_DIFF = float(os.environ.get('BONUS_TEX_CITY_DIFF', '15'))


def city_tex_swaps(n):
    """-> (уровень города, {имя текстуры: (url трассы, url города)}) по правилу выше; печатает, что заменено"""
    import numpy as np
    from PIL import Image
    import psp_export as PE, convert as CV
    art = T.GAME / 'art'
    def inv(files):
        c = CV.Converter()
        for f in files:
            try: c.resources(PE.read(art / f'{f}.p3d'))
            except Exception as e: print('skip', f, e)
        return c.textures
    tr = inv([f'b0{n}'])
    def city(k): return inv([f'l{k}_terra'] + sorted(p.stem for p in art.glob(f'l{k}r*.p3d')) + sorted(p.stem for p in art.glob(f'l{k}z*.p3d')))
    levels = {k: city(k) for k in range(1, 8)}
    lv = max(range(1, 8), key=lambda k: (len(set(tr) & set(levels[k])), k == n))
    ct = levels[lv]
    def img(u):
        im = Image.open(PE.ASSETS / u).convert('RGBA')
        return im.transpose(Image.FLIP_TOP_BOTTOM) if u in PE.PNG_SRC else im
    out, rows = {}, []
    for name, tu in tr.items():
        cu = ct.get(name)
        if not cu or cu == tu: continue
        A, B = img(tu), img(cu)
        ca, cb = len(set(A.getdata())), len(set(B.getdata()))
        sa, sb = max(A.size), max(B.size)
        better = sb > sa or (sb == sa and ca < 256 and cb >= 1.5 * ca)
        if not better: continue
        if abs(A.size[0] * B.size[1] - A.size[1] * B.size[0]) > 0.05 * A.size[0] * B.size[1]: continue   # пропорции
        aa, ba = np.asarray(A, float), np.asarray(B.resize(A.size, Image.LANCZOS), float)
        if (aa[..., 3].min() < 255) != (np.asarray(B)[..., 3].min() < 255): continue   # прозрачность
        pm = lambda x: np.concatenate([x[..., :3] * x[..., 3:] / 255., x[..., 3:]], 2)   # разница по предумноженному цвету: RGB прозрачных пикселей не в счёт
        d = float(np.abs(pm(aa) - pm(ba)).mean())
        if d > TEX_CITY_DIFF: continue
        out[name] = (tu, cu); rows.append(f'{name} {A.size[0]}x{A.size[1]}/{ca}цв -> {B.size[0]}x{B.size[1]}/{cb}цв (разн. {d:.1f})')
    print(f'текстуры трассы b0{n} (город уровня {lv}): заменено {len(out)} из {len(tr)}'); [print('  ', r) for r in rows]
    return lv, out


# Правка Роба 472, часть 2 (10.10: «подтянуть из города подходящие текстуры, плотность текселей»): гипотеза — трасса выглядит хуже
# города при тех же 64–128 px, потому что та же картинка растянута на большую площадь. Замер (тексели на метр = sqrt(w*h*площадь UV /
# площадь в мире) по всем поверхностям с повтором, >= 3 повторов на куске): дороги, стены, заборы, деревья у трасс и города совпадают
# (14,4 и 14,3; 14,8 и 15,7; 16,8 и 17,8), но огромные земляные плоскости трасс растянуты втрое-вчетверо (b02 constructsite_b 2,7 против
# 12,8 в городе, b03 и b06 grassa 4,0 и 3,8 против 11,5, b05 grassb 9,8 против 16,7, b02 constructsite_c 5,6 против 14,1). Правило:
# одноимённая (без хвоста цифр после .bmp) непрозрачная текстура города с повтором, у которой на трассе плотность ниже городской
# в BONUS_DENS_MIN (1,3) раза и площадь >= BONUS_DENS_AREA (150 м2), получает UV трассы * k = городская/трассы (не больше
# BONUS_DENS_MAXK, 6): повтор плитки как в городе, рисунок тот же. Куски без повтора (вывески, фасады, старт), вода/небо/облака, прозрачное — не
# трогаются. BONUS_TEX_CITY_OLD=1 — выключает и подмену, и плотность; BONUS_DENS_OLD=1 — только плотность
DENS_OLD = os.environ.get('BONUS_DENS_OLD') == '1'
DENS_MIN = float(os.environ.get('BONUS_DENS_MIN', '1.3')); DENS_AREA = float(os.environ.get('BONUS_DENS_AREA', '150'))
DENS_MAXK = float(os.environ.get('BONUS_DENS_MAXK', '6')); DENS_TILE = 3.0   # кусок «с повтором»: размах UV >= 3 повторов или площадь >= DENS_BIG (огромная плоскость, растянутая на 1–2 повтора)
DENS_BIG = 2000.0
DENS_SKIP = ('water', 'ocean', 'sky', 'cloud', 'sparkle', 'glow', 'smoke')


def _dens_name(t): return re.sub(r'(\.bmp)\d+$', r'\1', t)


def _prim_stats(j, b):
    """JSON+BIN конвертера -> [(текстура, uv_смещение, uv_число, площадь_мира, площадь_UV, размах_UV)] по примитивам с экземплярами"""
    import numpy as np
    by = {}
    for o in j['objects']: by.setdefault(o['mesh'], []).append(np.array(o['matrix'], float).reshape(4, 4))
    out = []
    for mn, prims in j['meshes'].items():
        Ms = by.get(mn) or []
        for pr in prims:
            A = pr['attributes']
            if 'uv' not in A or 'indices' not in A or not Ms: continue
            P = np.frombuffer(b, '<f4', count=A['position'][1], offset=A['position'][0]).reshape(-1, 3).astype(float)
            UV = np.frombuffer(b, '<f4', count=A['uv'][1], offset=A['uv'][0]).reshape(-1, 2).astype(float)
            I = np.frombuffer(b, '<u4', count=A['indices'][1], offset=A['indices'][0]).reshape(-1, 3)
            uvA = 0.5 * np.abs(np.cross(UV[I[:, 1]] - UV[I[:, 0]], UV[I[:, 2]] - UV[I[:, 0]]))
            wa = 0.0
            for M in Ms:
                Q = P @ M[:3, :3] + M[3, :3]
                w_ = 0.5 * np.linalg.norm(np.cross(Q[I[:, 1]] - Q[I[:, 0]], Q[I[:, 2]] - Q[I[:, 0]]), axis=1)
                wa += float(w_[(w_ > 1e-4) & (uvA > 1e-9)].sum())
            ok = uvA > 1e-9
            span = float(max(np.ptp(UV[:, 0]), np.ptp(UV[:, 1]))) if len(UV) else 0.0
            m_ = j['materials'].get(pr['shader'], {})
            out.append((m_.get('texture', ''), A['uv'][0], A['uv'][1], wa, float(uvA[ok].sum()) * len(Ms), span, bool(m_.get('alpha') or m_.get('blend') or m_.get('translucent'))))
    return out


def city_density(lv):
    """-> {имя текстуры города: (площадь, площадь UV)} по кускам с повтором; кэш build/bonus/_citydens_l<lv>.json (диск неизменен)"""
    cache = ROOT / f'build/bonus/_citydens_l{lv}.json'
    if cache.exists():
        try: return {k: tuple(v) for k, v in json.loads(cache.read_text()).items()}
        except Exception: pass
    import psp_export as PE, convert as CV
    art = T.GAME / 'art'
    files = [f'l{lv}_terra'] + sorted(p.stem for p in art.glob(f'l{lv}r*.p3d')) + sorted(p.stem for p in art.glob(f'l{lv}z*.p3d'))
    c = CV.Converter(); acc = {}
    for f in files:
        try: c.resources(PE.read(art / f'{f}.p3d'))
        except Exception as e: print('skip', f, e)
    for f in files:
        out = f'sonnet_dens_{os.getpid()}_{f}'
        try: PE.export_scene(c, art / f'{f}.p3d', out)   # со своим небом/графом сцены, как город (у l2..l7_terra графа нет)
        except Exception as e: print('skip', f, e); continue
        jp, bp = CV.OUT / f'{out}.json', CV.OUT / f'{out}.bin'
        try: st = _prim_stats(json.loads(jp.read_text()), bp.read_bytes())
        finally: jp.unlink(missing_ok=True); bp.unlink(missing_ok=True)
        for t, _o, _n, wa, ua, span, tr_ in st:
            if (span >= DENS_TILE or wa >= DENS_BIG) and wa > 0 and not tr_:
                r = acc.setdefault(t, [0.0, 0.0]); r[0] += wa; r[1] += ua
    cache.parent.mkdir(parents=True, exist_ok=True); cache.write_text(json.dumps(acc))
    return {k: tuple(v) for k, v in acc.items()}


QUALITY = dict(SHR_TEX_CAP256='1', SHR_SMALL_MIPS='1', SHR_LAYERS='1', SHR_T4='1', SHR_SIGN_CAP='1')   # как tools/build_level.sh

BNS_VER = 2                 # 2 (09.10, подборы и ловушки): после пути ИИ — подборы и зоны; число — в pad[0..2] заголовка
PICKUPS_OLD = os.environ.get('BONUS_PICKUPS_OLD') == '1'   # =1 — как до подборов: BNS1 без блока, в SPR1 нет моделей nitro/wrench
PK_FMT = '<4fI12s'          # подбор: центр сферы, радиус, вид (1 nitro, 2 wrench), имя — 32 Б
HZ_FMT = '<4I3f9f3f3f16s16s'   # зона: вид (1 trap, 2 launch, 3 light), arg, rect, 0; центр, оси, полуразмеры, локатор, имя, объект — 120 Б
HZ_LEN = 128
TRAP_OBJECTS = ('spikes', 'pistons')   # объекты ловушек мини-игры по имени — строки SLES_518.97 (0x37e6d0, 0x37e6d8)
CP_FMT = '<3f9f3fII16s'     # центр, оси (3 строки), полуразмеры, rect, номер круга, имя — 84 Б, запись 96 Б
CP_LEN = 96
ST_FMT = '<4f16s'           # x, y, z, курс движка, имя — 32 Б
HDR_FMT = '<4s7I19f'        # 'BNS1', версия, трасса, чекп., стартов, точек по ходу, обратных, флаги; камера 13 f; bbox 6 f
HDR_LEN = 128
PATH_MAX = 256
# правило пути ИИ (выбрано полигоном tools/bonus/sweep.sh по всем трассам и обоим ходам): остановка — центр объёма чекпойнта
# (center) или точка опоры в объёме у прямой между соседями (inside); точки WPR/WPL — промежуточные (1) или нет (0)
STOPS = os.environ.get('BONUS_STOPS', 'center'); VIA = os.environ.get('BONUS_VIA', '1') == '1'   # точек пути ИИ на ход (viewer/bonus.h BNS_MAXPATH)


def track_light(n):
    """свет трассы: группа "sun" art/b0N.p3d (данные), множители к уровню 1 — тем же правилом, что tools/level_light.level_light"""
    import level_light as LL, numpy as np
    g = LL.group_light(T.ART / f'b0{n}.p3d'); ref = LL.level_light(1)
    irr = lambda G: np.array(G['amb'], np.float64) + .25 * np.sum([c for _, c in G['dirs']], 0).astype(np.float64)
    e, er = irr(g), irr(ref)
    S = tuple(float(x / y) if y else 1.0 for x, y in zip(e, er))
    return dict(level=1, group=LL.GROUP, amb=g['amb'], dirs=g['dirs'], ambS=S, dirS=S)


def export_shr(n, out):
    """b0N.p3d -> SHR7 out (+ PVS, обход граней)"""
    for k, v in QUALITY.items(): os.environ.setdefault(k, v)
    os.environ['SHR_NO_REGIONS'] = '1'                  # регионов l<N>r*/z* у трассы нет
    os.environ.setdefault('SHR_TMP', str(ROOT / f'build/bonus/tmp{n}'))
    import psp_export as PE, prp2_export, jumps_export, level_light
    import texq
    texq.CAP = TEX_CAP
    # Сглаживание дали (Роб 10.10: «вдалеке гораздо лучше»): мипы земли и стен затухают к среднему цвету (texq.fade_mips), у земли сдвиг мипа +0,75 (psp_export, тот же код,
    # что у города); у трасс сверх городских имён — своё покрытие дорог и стен (FADE_EXTRA_*). BONUS_MIPFADE_OLD=1 — трассы как до приёма (без затухания и сдвига)
    if MIPFADE_OLD: texq.MIP_FADE = False; os.environ['SHR_GROUND_BIAS_OLD'] = '1'
    else: PE.FADE_EXTRA_GROUND, PE.FADE_EXTRA_WALL = FADE_EXTRA_GROUND, FADE_EXTRA_WALL
    if TEX_CAP > 256: texq.CAPS.update({u: TEX_CAP for u in list(texq.CAPS)})   # неба и вывески города 256/128 — тоже до предела трассы
    # All source labels must use the same resolved retail root in linked worktrees.
    jumps_export.GAME = T.GAME
    prp2_export.GAME = T.GAME
    level_light.GAME = T.GAME
    L = T.locators(n); st = T.starts(L)
    light = track_light(n)
    level_light.level_light = lambda level: light       # свет трассы вместо уровня
    prp2_export.level_files = lambda art, level: [Path(art) / f'b0{n}.p3d']   # стейтпропы/AnimCollision трассы (fx — l01_fx: ss.mfk)
    jumps_export.script_files = lambda level: [T.ART / f'b0{n}data.p3d']      # прыжки, DEATH — из локаторов трассы
    jumps_export.scene_files = lambda level: [T.ART / f'b0{n}.p3d']
    car1 = st[0]
    if not PICKUPS_OLD:   # подборы трассы (ActionEvent Nitro/Wrench b0Ndata): модели — типы SPR1 в той же точке, что сфера подбора
        P = T.pickups(L)
        PE.EXTRA_PICKUPS[:] = [(f, nm, [p['pos'] for p in P if p['kind'] == k]) for f, nm, k in
                               ((T.ART / 'wrench.p3d', 'wrench', 2), (T.ART / 'missions/generic/nitro.p3d', 'nitro', 1))]
        PE.EXTRA_PICKUPS[:] = [e for e in PE.EXTRA_PICKUPS if e[2]]
    PE.level_start = lambda level: ((tuple(car1['pos']), car1['heading']), 'famil_v', f'локатор {car1["name"]} b0{n}data')
    PE.car_model = lambda level, car: T.GAME / 'art/cars/famil_v.p3d'
    # ПРАВИЛО ПОРТА (только трассы): декорация выше ±64 м от своего центра (b07: столб над трассой, y 95…237 м) не влезает в решётку
    # города 2^-9 м — такой кусок кладётся на решётку вдвое крупнее (масштаб куска Rec.sc, движок его учитывает); уровни 1–7 сюда не
    # попадают (у них такой кусок прервал бы экспорт)
    gq0 = PE.grid_quant
    def gq(P, G):
        g = G
        while True:
            try: return gq0(P, g)
            except ValueError:
                if g > G * 64: raise
                g *= 2
    PE.grid_quant = gq
    # сверка g12-data №1109: здесь стоял ПЕРВЫЙ полный прогон PE.main с фоном уровня (backdrop_level), а ниже второй, без фона, затирал результат — фон уровня (небо, горизонт, даль, №473)
    # терялся у всех трасс, кроме b02/b03 с собственным небом. Теперь один прогон, с фоном; BONUSENV1109_OLD=1 — как было (фон не пишется)
    if TEX_CITY:
        _lv, swaps = city_tex_swaps(n)
        Conv0 = PE.Converter
        class SwapConv(Conv0):   # после чтения ресурсов файла трассы имя текстуры указывает на городскую картинку
            def resources(self, root):
                super().resources(root)
                for nm, (tu, cu) in swaps.items():
                    if self.textures.get(nm) == tu: self.textures[nm] = cu
        dens = {} if DENS_OLD else city_density(_lv)
        class DensConv(SwapConv):   # UV земляных плоскостей трассы — к городской плотности текселей (см. выше)
            def export(self, path, outname, *a, **kw):
                r = super().export(path, outname, *a, **kw)
                if dens and Path(path).stem == f'b0{n}' and not a and not kw: self._densify(outname)   # только основной вызов сцены (не стейтпропы)
                return r
            def _densify(self, outname):
                import numpy as np, convert as CV
                from PIL import Image
                jp, bp = CV.OUT / f'{outname}.json', CV.OUT / f'{outname}.bin'
                j = json.loads(jp.read_text()); b = bytearray(bp.read_bytes())
                st = _prim_stats(j, bytes(b)); agg = {}; transp = set(x[0] for x in st if x[6])   # прозрачные (альфа-тест, смешение) не трогаем
                for t, off, cnt, wa, ua, span, tr_ in st:
                    if (span >= DENS_TILE or wa >= DENS_BIG) and wa > 0 and not tr_: r = agg.setdefault(t, [0.0, 0.0]); r[0] += wa; r[1] += ua
                def px(u): return Image.open(PE.ASSETS / u).size, False
                kk, rows = {}, []
                for t, (wa, ua) in agg.items():
                    cu = self.textures.get(t); ck = dens.get(_dens_name(t))
                    if not cu or not ck or t in transp or wa < DENS_AREA or any(x in t for x in DENS_SKIP): continue
                    (w, h), _a = px(cu)
                    cw, ch = px(self.textures.get(_dens_name(t), cu))[0] if _dens_name(t) in self.textures else (w, h)
                    dt = (w * h * ua / wa) ** .5; dc = (cw * ch * ck[1] / ck[0]) ** .5
                    if dc / dt >= DENS_MIN:
                        kk[t] = min(DENS_MAXK, dc / dt); rows.append(f'{t} {wa:.0f} м2: {dt:.1f} -> {dt * kk[t]:.1f} текс/м (город {dc:.1f}, UV x{kk[t]:.2f})')
                for t, off, cnt, wa, ua, span, tr_ in st:
                    if t in kk and (span >= DENS_TILE or wa >= DENS_BIG) and not tr_:
                        a = np.frombuffer(b, '<f4', count=cnt, offset=off); a[:] = (a * kk[t]).astype('<f4')   # b — bytearray: правка на месте
                if kk:
                    bp.write_bytes(bytes(b))
                    print(f'плотность текселей b0{n} (город уровня {_lv}): подтянуто {len(kk)} текстур'); [print('  ', r_) for r_ in rows]
                else: print(f'плотность текселей b0{n}: все в пределах {DENS_MIN}x города')
        PE.Converter = DensConv
    PE.main(1, str(out), scenes=[f'b0{n}'], coins=False, backdrop_level=(None if os.environ.get('BONUSENV1109_OLD') == '1' else BACKDROP.get(n)))
    pvs, wind = ROOT / 'tools/pvs', ROOT / 'tools/wind'
    if not pvs.exists() or (ROOT / 'tools/pvs.c').stat().st_mtime > pvs.stat().st_mtime:
        subprocess.run(['cc', '-O3', '-o', str(pvs), str(ROOT / 'tools/pvs.c'), '-lm'], check=True)
    subprocess.run([str(pvs), str(out), '--samples', '9'], check=True)
    if not wind.exists() or (ROOT / 'tools/wind.c').stat().st_mtime > wind.stat().st_mtime:
        subprocess.run(['cc', '-O3', '-o', str(wind), str(ROOT / 'tools/wind.c'), '-lm', '-lpthread'], check=True)
    subprocess.run([str(wind), str(out)], check=True)
    if FENCE_ROUTE:
        c, tot = route_fence_cut(n, out)
        print(f'заборы трассы: из {tot} звеньев выключено {c} (маршрут проходит сквозь них против нормали; BONUS_FENCE_ROUTE_OLD=1 — не выключать)')


def route_paths(n, shr):
    """маршруты трассы: (L, cps, st, cam, fw, rv, paths) — как их берёт bns_bytes (путь ИИ по опоре трассы, tools/bonus/nav.py)"""
    import nav as NV
    L = T.locators(n); cps = T.checkpoints(L); st = T.starts(L); cam = T.camera(n)
    fw = T.lap_cycle(T.waypoints(L, 'WPR'), cps); rv = T.lap_cycle(T.waypoints(L, 'WPL'), cps, reverse=True)
    N = NV.Nav(NV.col_tris(shr))   # путь ИИ между точками — по опоре трассы (tools/bonus/nav.py, правило порта)
    paths = []
    for rev_, ws in ((0, fw), (1, rv)):
        order = list(range(1, len(cps))) if not rev_ else list(range(len(cps) - 1, 0, -1))
        seq = T.lap_sequence(n, cps, rev_) or [cps[k] for k in order] + [cps[0]]   # круг 7: список оригинала с повторами (b02, b05, b07)
        stops = NV.stop_points(N, seq) if STOPS == 'inside' else [c['pos'] for c in seq]
        via = NV.insert_via(stops, [w['pos'] for w in ws]) if VIA else {}
        pts, tag = NV.loop_path(N, stops, via)
        paths.append((pts[:PATH_MAX], tag[:PATH_MAX]))
    return L, cps, st, cam, fw, rv, paths


# Правка Роба 454 (10.10): заборы трассы (tools/psp_export.py TRACK_FENCES) односторонние — держат с лицевой стороны (нормаль). Если
# маршрут (старт -> путь ИИ вперёд и назад) проходит сквозь забор против его нормали, то это не ограждение гонки (у оригинала там проезжают:
# b04 — линия z = 30 через старт), и звено выключается (верх звена -1e9: движок его не видит); остальные звенья держат. Маршрут — тот же
# путь ИИ, что пишется в .bns. BONUS_FENCE_ROUTE_OLD=1 — не выключать (все заборы как экспортированы)
FENCE_ROUTE = os.environ.get('BONUS_FENCE_ROUTE_OLD') != '1'


def route_fence_cut(n, shr):
    shr = Path(shr); b = bytearray(shr.read_bytes())
    co, = struct.unpack_from('<I', b, 92)
    if bytes(b[co:co + 4]) != b'COL1': return 0, 0
    ntri, nbox, ncyl, nseg = struct.unpack_from('<4I', b, co + 24)
    if not nseg: return 0, 0
    so = co + 48 + 48 * ntri + 64 * nbox + 48 * ncyl
    seg = [list(struct.unpack_from('<8f', b, so + 32 * i)) for i in range(nseg)]
    L, cps, st, cam, fw, rv, paths = route_paths(n, str(shr))
    legs = []
    for pts, _t in paths:
        P = [(p[0], p[2], p[1]) for p in pts]
        legs += [(P[i], P[i + 1]) for i in range(len(P) - 1)]
        for s_ in st:   # старт -> первая точка пути (машина едет с места старта)
            if P: legs.append(((s_['pos'][0], s_['pos'][2], s_['pos'][1]), P[0]))
    def o(a, b_, c): return (b_[0] - a[0]) * (c[1] - a[1]) - (b_[1] - a[1]) * (c[0] - a[0])
    cut = set()
    for (p, q) in legs:
        for i, g in enumerate(seg):
            if i in cut or g[7] < p[2] - 1.0: continue
            a, b_ = (g[0], g[1]), (g[2], g[3])
            if o(p, q, a) * o(p, q, b_) < 0 and o(a, b_, p) * o(a, b_, q) < 0 and (q[0] - p[0]) * g[4] + (q[1] - p[1]) * g[5] < 0:
                cut.add(i)
    for i in cut: struct.pack_into('<f', b, so + 32 * i + 28, -1e9)
    if cut: shr.write_bytes(bytes(b))
    return len(cut), nseg


def bns_bytes(n, bbox, shr):
    L, cps, st, cam, fw, rv, paths = route_paths(n, shr)
    if not cps or cps[0]['name'] != 'finish': raise SystemExit(f'b0{n}: нет финиша (Event 2, номер 1)')
    if len(st) != 4: raise SystemExit(f'b0{n}: стартов {len(st)}, нужно 4')
    pk = [] if PICKUPS_OLD else T.pickups(L)
    hz = [] if PICKUPS_OLD else T.hazards(L)
    tobj = [o for o in T.anim_collision(n) if o in TRAP_OBJECTS] if hz else []
    if any(h['kind'] == T.HAZ_TRAP for h in hz) and len(tobj) != 1: raise SystemExit(f'b0{n}: ловушки без объекта spikes/pistons: {tobj}')
    out = bytearray(struct.pack(HDR_FMT, b'BNS1', 1 if PICKUPS_OLD else BNS_VER, n, len(cps), len(st), len(fw), len(rv), 0,
                                cam['fov'], cam['aspect'], cam['near'], cam['far'], *cam['pos'], *cam['look'], *cam['up'], *bbox))
    out += struct.pack('<2I', len(paths[0][0]), len(paths[1][0]))   # pad[0..1]: точек пути ИИ по ходу / обратно
    if not PICKUPS_OLD: out += struct.pack('<3I', len(pk), len(hz), len(tobj))   # pad[0..2] версии 2: подборов, зон, объектов ловушек
    out += b'\0' * (HDR_LEN - len(out))
    for c in cps:
        t = c['trig'][0]
        r = struct.pack(CP_FMT, *t['c'], *[x for ax in t['axes'] for x in ax], *t['h'], t['rect'], c['data'][1], c['name'].encode()[:15])
        out += r + b'\0' * (CP_LEN - len(r))
    for s in st: out += struct.pack(ST_FMT, *s['pos'], s['heading'], s['name'].encode()[:15])
    for w in fw + rv:
        r = w['trig'][0]['h'][0] if w['trig'] else 5.0   # радиус точки — сфера её триггера (данные), нет триггера — 5 м (правило порта)
        out += struct.pack('<4f', *w['pos'], r)
    for pts, tag in paths:   # путь ИИ: x, y, z, номер точки круга (остановки) у этой точки или -1
        for p, t in zip(pts, tag): out += struct.pack('<4f', *p, float(t))
    for p in pk: out += struct.pack(PK_FMT, *p['pos'], p['r'], p['kind'], p['name'].encode()[:11])
    for h in hz:
        obj = tobj[0] if h['kind'] in (T.HAZ_TRAP, T.HAZ_LAUNCH) and tobj else ''
        r = struct.pack(HZ_FMT, h['kind'], h['arg'], h['rect'], 0, *h['c'], *[x for ax in h['axes'] for x in ax], *h['h'], *h['loc'],
                        h['name'].encode()[:15], obj.encode()[:15])
        out += r + b'\0' * (HZ_LEN - len(r))
    # №1116 (сверка g03-aicars): дороги трассы — хвост файла: блок TRF1 (tools/bonus/roads.py), размер u32, 'BRD1'. Соперники ведутся по ним
    # от точки WPR/WPL к следующей, как оригинал. BONUS_ROADS_OLD=1 — хвоста нет (путь ИИ по опоре, nav.py)
    rd = RD.block(n)
    if rd:
        while len(out) % 16: out += b'\0'
        out += rd + struct.pack('<I4s', len(rd), b'BRD1')
    return bytes(out), dict(path=[len(p[0]) for p in paths],track=n, checkpoints=[c['name'] for c in cps], starts=[s['name'] for s in st],
                            forward=[w['name'] for w in fw], reverse=[w['name'] for w in rv], camera=cam['name'],
                            roads=len(rd), pickups=[p['name'] for p in pk], hazards=[h['name'] for h in hz], trap_objects=tobj)


def shr_bbox(path):
    h = Path(path).read_bytes()[:104]
    assert h[:4] == b'SHR7', path
    return struct.unpack_from('<6f', h, 12)


def cars_pack(out):
    env = dict(os.environ, **QUALITY)
    subprocess.run([sys.executable, str(ROOT / 'tools/msncar_export.py'), '--level', '1', '--names', ','.join(dict.fromkeys(VEHICLES + RACE_VEHICLES + ([] if os.environ.get('BONUS_AI_ALL_OLD') == '1' else AI_VEHICLES))), '--out', str(out)],
                   check=True, env=env, cwd=ROOT)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--track', type=int, action='append', help='номер файла трассы b0N (1..7; 0 — тестовая площадка)')
    ap.add_argument('--all', action='store_true'); ap.add_argument('--cars', action='store_true')
    ap.add_argument('--bns-only', action='store_true', help='только .bns (SHR7 уже есть)')
    ap.add_argument('--out', default=str(ROOT / 'build/bonus'))
    # одиночный файл для правил выпуска (tools/ship_rules.txt: {out}): --track N --shr-file F | --bns-file F; --cars-file F
    ap.add_argument('--shr-file'); ap.add_argument('--bns-file'); ap.add_argument('--cars-file')
    ap.add_argument('--shr', help='трасса SHR7 для .bns (по умолчанию build/bonus/b0N.shr)')
    a = ap.parse_args()
    if a.cars_file: cars_pack(Path(a.cars_file)); return
    if a.shr_file or a.bns_file:
        n = a.track[0]
        if a.shr_file:
            subprocess.run([sys.executable, '-c', f'import sys; sys.path.insert(0, {str(ROOT / "tools/bonus")!r}); import export; '
                            f'export.export_shr({n}, {a.shr_file!r})'], check=True, cwd=ROOT)
        if a.bns_file:
            shr = a.shr or a.shr_file or str(ROOT / f'build/bonus/b{n:02d}.shr')
            b, info = bns_bytes(n, shr_bbox(shr), shr); Path(a.bns_file).write_bytes(b); print(f'{a.bns_file}: {len(b)} Б')
        return
    od = Path(a.out); od.mkdir(parents=True, exist_ok=True)
    tracks = list(T.TRACKS) if a.all else (a.track or [])
    for n in tracks:
        shr = od / f'b{n:02d}.shr'
        if not a.bns_only:
            # каждая трасса — отдельным процессом: psp_export держит состояние модуля (свет, патчи) на весь прогон
            subprocess.run([sys.executable, '-c', f'import sys; sys.path.insert(0, {str(ROOT / "tools/bonus")!r}); import export; '
                            f'export.export_shr({n}, {str(shr)!r})'], check=True, cwd=ROOT)
        b, info = bns_bytes(n, shr_bbox(shr), shr)
        (od / f'b{n:02d}.bns').write_bytes(b)
        print(f'b{n:02d}.bns: {len(b)} Б, ' + json.dumps(info, ensure_ascii=False))
    if a.all or a.cars: cars_pack(od / 'msncars-b.shr')


if __name__ == '__main__':
    main()
