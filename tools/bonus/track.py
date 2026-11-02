"""Бонус-игра (мини-игра «Super Sprint», правка Роба 420): данные трасс с диска — общий модуль для описи
(tools/bonus/inventory.py), экспорта (tools/bonus/export.py) и полигона ботов (tools/bonus/race_host.c читает BNS1).

Что берётся с диска (только чтение, hit-and-run-web/source/game):
  * art/b0N.p3d — геометрия, коллизия, свет "sun" и камера трассы sprint_camShape (имя камеры — строка SLES_518.97);
  * art/b0Ndata.p3d — локаторы (путь art\\B0%ddata.p3d — строка SLES_518.97): старты car1..car4 (CarStart, курс в данных),
    финиш и чекпойнты (Event 2, номер по порядку круга — второе слово данных, объём TriggerVolume), точки пути ИИ
    WPR01.. (по ходу) и WPL01.. (обратный ход; префиксы "WPR"/"WPL" и формат "%s0%d"/"%s%d" — строки SLES_518.97),
    камеры spycam (Event 1), прыжки (Event 48), ловушки (Event 66), DEATH (Event 4), огонь (Event 65);
  * scripts/cars/bonus/bgT_K.con — ходовые машин ИИ ("bonus/bg%d_%d.con", строка SLES_518.97; комментарий файла «// AI»).

Номер трассы меню -> файл: Track0..Track6 страницы MiniMenu.pag (картинки track1..track7) -> b01..b07. Довод — форма:
значок trackN совпадает с планом коллизии b0N (b05 — три кольца, b01 — «C+S»); b00 — круглая площадка без точек пути ИИ и без
чекпойнтов, кроме двух (тестовая, в меню её нет). Вывод по форме, не по коду — вопрос В-строкой в knowledge/questions.tsv.

Координаты как у движка: метры, z отзеркален (как tools/missions/p3dlocs.py), курс движка h = π − угол файла (level_sweep)."""
import math, os, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
import p3d   # noqa: E402

GAME = (ROOT / 'hit-and-run-web/source/game').resolve()  # worktree retail symlink must match psp_export GAME
ART = GAME / 'art'
EXE = ROOT / 'hit-and-run-web/source/disc/SLES_518.97'
TRACKS = (1, 2, 3, 4, 5, 6, 7)   # меню Track0..6 -> b01..b07 (см. шапку)
EV_CHECK, EV_CAM, EV_DEATH, EV_JUMP, EV_FIRE, EV_TRAP = 2, 1, 4, 48, 65, 66


def _mirror(v): return (v[0], v[1], -v[2])


def locators(n):
    """-> [dict(name, type, data (u32...), pos, heading (курс движка, только CarStart), trig [dict(rect, c, h, axes)])]"""
    b = p3d.decompress((ART / f'b0{n}data.p3d').read_bytes())
    out = []
    for c in p3d.chunks(b)[0].children:
        if c.id != 0x3000005: continue
        d = c.data; name, p = p3d.string(d)
        typ, nd = struct.unpack_from('<II', d, p); p += 8
        data = struct.unpack_from(f'<{nd}I', d, p); p += 4 * nd
        pos = _mirror(struct.unpack_from('<3f', d, p))
        L = dict(name=name, type=typ, data=data, pos=pos, trig=[], heading=None)
        if typ == 3 and nd >= 1:   # CarStart: первое слово — угол (float); курс движка = π − угол
            a = struct.unpack('<f', struct.pack('<I', data[0]))[0]
            L['heading'] = math.atan2(math.sin(math.pi - a), math.cos(math.pi - a))
        for t in c.children:
            if t.id != 0x3000006: continue
            _, q = p3d.string(t.data)
            rect, = struct.unpack_from('<I', t.data, q)
            half = struct.unpack_from('<3f', t.data, q + 4); m = struct.unpack_from('<16f', t.data, q + 16)
            axes, h = [], []
            for r in range(3):
                ax = _mirror(m[r * 4:r * 4 + 3]); ln = math.sqrt(sum(a * a for a in ax)) or 1.0
                axes.append(tuple(a / ln for a in ax)); h.append(half[r] * ln)
            if not rect: axes = [(1., 0., 0.), (0., 1., 0.), (0., 0., 1.)]; h = [h[0]] * 3   # сфера: радиус
            L['trig'].append(dict(rect=int(bool(rect)), c=_mirror(m[12:15]), h=tuple(h), axes=tuple(axes)))
        out.append(L)
    return out


def checkpoints(L):
    """чекпойнты круга по порядку данных (Event 2 с номером > 0): финиш — номер 1"""
    cps = [l for l in L if l['type'] == 0 and len(l['data']) >= 2 and l['data'][0] == EV_CHECK and l['data'][1] > 0 and l['trig']]
    return sorted(cps, key=lambda l: l['data'][1])


# Круг 7 (g07b P17-16): порядок прохода чекпойнтов задан в оригинале списками номеров локаторов (в данных у чекпойнта Event 2 — номер; финиш — номер 1, он последний).
# У b02 и b07 номер 3 стоит дважды (трасса проходит перекрёсток два раза), у b05 номера 7 и 3 дважды; остальные идут по возрастанию. Обратный ход — тот же круг наоборот
# (без финиша в начале, финиш снова последним). Те же списки в viewer/bonus.c (brLapList), совпадение проверяет tests/g07b_sverka7/lap_order_check.py.
# BONUS_LAP_OLD=1 — прежний порядок «по возрастанию номеров, без повторов»
LAP_FWD = {1: [2, 3, 4, 5, 6, 7, 8, 1], 2: [2, 3, 4, 5, 6, 3, 8, 9, 1], 3: [2, 3, 4, 5, 6, 7, 1], 4: [2, 3, 4, 5, 6, 7, 1],
           5: [2, 3, 4, 5, 6, 7, 8, 9, 10, 7, 3, 11, 1], 6: [2, 3, 4, 5, 6, 7, 1], 7: [2, 3, 4, 5, 6, 3, 8, 9, 1]}


def lap_numbers(n, reverse=False):
    f = LAP_FWD.get(n)
    if not f: return None
    return list(reversed(f[:-1])) + [f[-1]] if reverse else list(f)


def lap_sequence(n, cps, reverse=False):
    """чекпойнты круга по порядку списка оригинала (с повторами), финиш последним; None — списка нет или номера в данных не те (тогда прежний порядок)"""
    if os.environ.get('BONUS_LAP_OLD') == '1' or os.environ.get('BONUS_SEQ_OLD') == '1': return None   # сведение 1109: оба выключателя окон g07b и g03
    nums = lap_numbers(n, reverse)
    by = {c['data'][1]: c for c in cps}
    if not nums or any(k not in by for k in nums) or set(nums) != set(by): return None
    return [by[k] for k in nums]


def starts(L):
    cs = [l for l in L if l['type'] == 3 and l['name'].startswith('car')]
    return sorted(cs, key=lambda l: l['name'])


def waypoints(L, prefix):
    """точки пути ИИ с префиксом WPR / WPL по номеру (01, 02, …)"""
    w = [l for l in L if l['name'].startswith(prefix) and l['name'][len(prefix):].isdigit()]
    return sorted(w, key=lambda l: int(l['name'][len(prefix):]))


LOC_ACTION = 9                      # ActionEvent-локатор (вид 9): строки данных — объект, сустав, действие; дальше кнопка, флаг
PICK_KIND = {'Nitro': 1, 'Wrench': 2}   # подбираемое трасс (действия ActionEvent b0Ndata; строки "Nitro"/"Wrench" — SLES_518.97)


def _strs(data):
    """слова данных ActionEvent -> строки (объект, сустав, действие)"""
    raw = struct.pack(f'<{len(data)}I', *data)
    return [s.decode('latin1') for s in raw.split(b'\0') if s and all(32 <= ch < 127 for ch in s)]


def pickups(L):
    """Подбираемое трассы (правка 420, часть «подборы»): ActionEvent-локаторы b0Ndata с действием Nitro или Wrench (имена
    nitro*, wrench*, repair* — у repair* действие тоже Wrench). Сфера подбора — TriggerVolume (5 м у трасс, 2 м у b00).
    nitro1 трассы b02 — действие ToggleOnOff объекта crane, не подбор (данные), сюда не входит.
    -> [dict(name, kind (1 nitro, 2 wrench), action, pos (центр объёма), r)] в порядке файла"""
    out = []
    for l in L:
        if l['type'] != LOC_ACTION or not l['trig']: continue
        s = _strs(l['data'])
        act = next((x for x in s if x in PICK_KIND), None)
        if act is None: continue
        t = l['trig'][0]
        out.append(dict(name=l['name'], kind=PICK_KIND[act], action=act, pos=t['c'], r=t['h'][0]))
    return out


EV_LAUNCH = 63   # B04G01..03 трассы b04: сфера на каждом поршне (данные: второе слово 20)
HAZ_TRAP, HAZ_LAUNCH, HAZ_LIGHT = 1, 2, 3


def hazards(L):
    """Ловушки и зоны трассы (часть «ловушки, огонь»): каждый объём TriggerVolume — отдельная запись.
      Event 66 trap* (b04, b07): объём-датчик ловушки, второе слово — номер ловушки (0, 1). Объект ловушки трассы — по имени
              "spikes" (b07) / "pistons" (b04): строки SLES_518.97 рядом с таблицей мини-игры; AnimCollision трассы, клип без цикла.
      Event 63 B04G01..03 (b04): сфера r 3 м ровно на поршне (центры объёмов совпадают с суставами pistons кадра 0 до 1 м),
              второе слово 20 (arg).
      Event 65 fire (b07): зона света — те же данные, что tirefirelight уровней 1/4/7 (0xBBFF9A3F, ARGB: альфа — сила); в данных
              уровней Event 65 — shadows / darker / green / redlight, то есть свет зоны, не урон.
    -> [dict(name, kind, arg, rect, c, axes, h, loc)]"""
    out = []
    for l in L:
        if l['type'] != 0 or len(l['data']) < 1 or not l['trig']: continue
        ev = l['data'][0]
        kind = {EV_TRAP: HAZ_TRAP, EV_LAUNCH: HAZ_LAUNCH, EV_FIRE: HAZ_LIGHT}.get(ev)
        if kind is None: continue
        arg = l['data'][1] if len(l['data']) > 1 else 0
        for t in l['trig']:
            out.append(dict(name=l['name'], kind=kind, arg=arg, rect=t['rect'], c=t['c'], axes=t['axes'], h=t['h'], loc=l['pos']))
    return out


def anim_collision(n):
    """имена объектов AnimCollision (0x3F00008) сцены art/b0N.p3d — ловушки трассы ищутся среди них по имени"""
    b = p3d.decompress((ART / f'b0{n}.p3d').read_bytes())
    return [p3d.string(c.data)[0] for c in p3d.chunks(b)[0].children if c.id == 0x3f00008]


def _d2(a, b): return (a[0] - b[0]) ** 2 + (a[2] - b[2]) ** 2


def lap_cycle(wps, cps, reverse=False):
    """Порядок обхода точек пути на круге (ПРАВИЛО ПОРТА: в данных у точек номера, а с какой начинать — нет). Номера идут по
    кругу (01, 02, …, n, 01). Начало — сдвиг цикла, при котором «прогресс» точек (номер ближнего чекпойнта в порядке круга
    этого хода) возрастает с наименьшим числом откатов; при равенстве — сдвиг с первой точкой ближе к первому чекпойнту хода."""
    if not wps or not cps: return []
    order = cps[1:] + cps[:1] if not reverse else list(reversed(cps[1:])) + cps[:1]   # круг от финиша: чекпойнты хода, финиш последним
    prog = [min(range(len(order)), key=lambda k: _d2(w['pos'], order[k]['pos'])) for w in wps]
    best = None
    for s in range(len(wps)):
        seq = prog[s:] + prog[:s]
        desc = sum(1 for a, b in zip(seq, seq[1:]) if b < a)
        key = (desc, seq[0], _d2(wps[s]['pos'], order[0]['pos']))
        if best is None or key < best[0]: best = (key, s)
    s = best[1]
    return wps[s:] + wps[:s]


def camera(n, name='sprint_camShape'):
    """камера трассы (0x2200): fov (рад, горизонталь кадра 4:3 — knowledge/facts/ps2-camera-fov.yaml), aspect, near, far,
    pos, look, up — в осях движка (z отзеркален)"""
    b = p3d.decompress((ART / f'b0{n}.p3d').read_bytes())
    for c in p3d.walk(p3d.chunks(b)[0]):
        if c.id != 0x2200: continue
        nm, p = p3d.string(c.data)
        if nm != name: continue
        v = struct.unpack_from('<I13f', c.data, p)   # версия, fov, aspect, near, far, pos[3], look[3], up[3]
        return dict(name=nm, fov=v[1], aspect=v[2], near=v[3], far=v[4], pos=_mirror(v[5:8]), look=_mirror(v[8:11]),
                    up=_mirror(v[11:14]))
    return None


def ai_con(track, k):
    """bonus/bgT_K.con — путь, как его строит SLES_518.97 ("bonus/bg%d_%d.con")"""
    return GAME / 'scripts/cars/bonus' / f'bg{track}_{k}.con'


def exe_strings(lo=0x37e398, hi=0x37e9a0):
    """строки исполняемого файла PS2 в окне таблицы мини-игры (только чтение) -> [(смещение, текст)]"""
    b = EXE.read_bytes()[lo:hi]; out, cur, st = [], bytearray(), lo
    for i, ch in enumerate(b):
        if 32 <= ch < 127: cur.append(ch)
        else:
            if len(cur) >= 3: out.append((st, cur.decode()))
            cur = bytearray(); st = lo + i + 1
    return out
