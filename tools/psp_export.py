"""Экспорт уровня SHAR для PSP (формат SHR7): город клетками 40x40 м + небо + карта высот + машина + коллизия COL1.
python tools/psp_export.py            -> весь уровень 1
python tools/psp_export.py 2          -> уровень 2
python tools/psp_export.py --out /tmp/x.shr  -> записать в другой файл (по умолчанию build/level.shr)"""
import sys, json, struct, re, math
from pathlib import Path
import numpy as np
from PIL import Image
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
from convert import Converter, GAME, OUT
import convert as _convert, hashlib as _hashlib
# Правка Роба 226 (03.10: «знаки STOP задом наперёд, везде»): у Pure3D координата v идёт снизу вверх (char_export.py,
# frontend3d_export.py уже переворачивают v). Картинки диска в формате PS2 (P3DI) лежат строками снизу вверх — с прямой v
# конвертера они ложатся верно; картинки, которые на диске лежат PNG (строки сверху вниз), выходили вверх ногами: на уровне 1
# это все вывески — StopSign, elemsign (школа), chruchSign, powerplantsigns, ComCentreSign. Сверка: близнец v226_stop
# (STOP у PS2 читается), кадр порта у школы (TODAY IS DIORAMA-RAMA вверх ногами). Конвертер пишет картинку в
# textures/<sha256(сырые данные)[:20]>.png — запоминаем такие имена у PNG-источников и переворачиваем при загрузке.
# SHR_PNG_FLIP=0 — прежнее (красный контроль tests/tex226)
PNG_SRC = set()
_resources0 = _convert.Converter.resources
def _resources_png(self, root):
    for c in walk(root):
        if c.id != 0x19000: continue
        for im in c.children:
            if im.id != 0x19001: continue
            for d in im.children:
                if d.id == 0x19002 and d.data[4:8] == b'\x89PNG':
                    PNG_SRC.add('textures/' + _hashlib.sha256(d.data[4:]).hexdigest()[:20] + '.png')
    return _resources0(self, root)
_convert.Converter.resources = _resources_png
ASSETS = OUT   # текстуры конвертера (url = 'textures/<hash>.png') — только чтение
from p3d import read, walk, string
from interiors import leaves as col_leaves   # листья CollisionVolume (родители — только габарит для поиска, не твёрдые)
import prp2_export   # стейтпропы и обломки -> подсекция SPR1 секции PRP2 (tools/prp2_export.py, docs/props_spec.md)
import uvq             # 16-битные UV без знака для GE (правка Роба 43)
import jumps_export    # блок JMP1 в COL1: зоны прыжка (Event 48) и подкидывающие точки Гомера (Event 6), tools/jumps_export.py
import animobj_export   # AnimCollision/Anim диска: город, SPR1, живые объёмы COL1, таблица ANM1 (tools/animobj_export.py)
import regions_export   # правка 95 (F11-2): регионы оригинала — что рисовать с места игрока, секция REG1 (tools/regions_export.py)
EXTRA_PICKUPS = []      # правка 420: [(файл p3d, составной рисунок, [точки])] — подборы трасс мини-игры (tools/bonus/export.py); уровни: пусто
from level_sweep import find_spawn, sky_patch   # старт и машина уровня из скриптов/локатора, небо из мировой сферы (tools/level_sweep.py)
import level_light   # свет уровня для освещаемых объектов: группа sun в l<N>_terra, блок LGT1 (tools/level_light.py, рецепт §12)

import os
FAR_PLAIN = os.environ.get('SHR_FAR_PLAIN') == '1'   # правка 508: дальняя версия города без нарезки пишется и помечается флагом F_FARP (1<<25); рисует её движок по файлу far_plain
F_FARP = 1 << 25
TMP_SUFFIX = '' if os.environ.get('SHR_TMP_SHARED') == '1' else f'_{os.getpid()}'
if os.environ.get('SHR_TMP'):   # временные psp_*.json/.bin конвертера — в свою папку (сборка в worktree не пишет в общий
    import convert as _conv     # hit-and-run-web/public/assets и не сталкивается именами с параллельной сборкой)
    OUT = _conv.OUT = Path(os.environ['SHR_TMP']).resolve(); OUT.mkdir(parents=True, exist_ok=True)
# Временные файлы конвертера (OUT/<имя>.json/.bin) — с номером процесса в имени: параллельные экспорты разных сессий (L5 и L6 в один момент) не
# затирают друг другу общий psp_sp_tmp.json (FileNotFoundError, 10.10)
def TN(base): return f'{base}_{os.getpid()}'
CAP, CHUNK, SPLIT, SPLIT_GROUND, CELL, LAYERS = 128, 40.0, 14.0, 7.0, 1.5, 4
SPLIT = float(os.environ.get('SHR_SPLIT_WALL', SPLIT))   # эксперимент шага 7 аудита: нарезка стен у камеры
SPLIT_GROUND = float(os.environ.get('SHR_SPLIT_GROUND', SPLIT_GROUND))   # то же для горизонтальных рёбер (пол, дорога)
# Флаги куска (Rec.flags / PropPiece.flags / SpPiece.flags младший байт):
#   Бит24: authored additive BLMD=2 (не занимает joint/slot SPR1).
#   1 альфа-тест, 2 полупрозрачное, 4 небо, 8 двусторонний (без отсечения задних граней),
#   16 без индексов: вершины подряд по 3 на треугольник (ближняя версия, сразу за ней дальняя), nv = ni + nf,
#   32 вершина 12 Б: u,v int16, цвет GU_COLOR_5650 (u16, альфа = 255), x,y,z int16 (иначе 16 Б: цвет 8888 + pad)
F_DOUBLE, F_NOIDX, F_V12 = 8, 16, 32
F_NEAR = 2048   # правки 224/237/251: кусок города рисуется ближней версией на любой дальности (дальней нет; viewer/main.c drawPiece)
QV12 = np.dtype([('u', '<i2'), ('v', '<i2'), ('c', '<u2'), ('x', '<i2'), ('y', '<i2'), ('z', '<i2')])

def to5650(c):
    """ABGR8888 -> 5650 (r 0-4, g 5-10, b 11-15) с округлением"""
    c = np.asarray(c, np.uint32)
    r, g, b = c & 255, (c >> 8) & 255, (c >> 16) & 255
    return (((r * 31 + 127) // 255) | (((g * 63 + 127) // 255) << 5) | (((b * 31 + 127) // 255) << 11)).astype('<u2')

# Стыки кусков (правка Роба 37, 28.09: «мерцание текстур на стыках» у дома): вершины куска — int16 от его центра в долях
# его размера, у каждого куска свои центр и шаг, и общая вершина двух кусков (трава и тротуар — разные куски по текстуре,
# соседние клетки 40 м) округлялась по-разному: расхождение до ~0.6 мм, по шву — волосяные щели, в них то пиксель фона,
# то нет — шов мерцает при движении камеры. Правило: все куски города на ОДНОЙ решётке шага CITY_GRID (степень двойки),
# центр куска — узел решётки: мир = центр + v·шаг вычисляется в float32 GE без округления (числа — кратные 2^-9 < 2^13),
# и общая вершина даёт один и тот же мировой бит в любом куске. Шаг 2^-9 м (~2 мм, ошибка ≤ 1 мм) покрывает ±64 м —
# самый большой кусок уровня 1 ±37.6 м. Проверка — tools/seams.py.
CITY_GRID = 2.0 ** -9

# Правка Роба 106 (повтор 01.10, «швы, полосы на дороге везде»): GE берёт матрицу модели 24-бит float (sceGuSetMatrix шлёт
# старшие 24 бита float32: мантисса 15 бит, усечение). Центр куска на решётке 2^-9 м в 600–900 м от начала уровня
# в 24 бита не помещается — GE сдвигал каждый кусок по-своему до 14 мм, общий край двух кусков расходился: щель в пиксель
# вдоль стыков кусков (замер tools/seams_raster.py, GE24). Правило: центр куска города — на шаге ORIGIN_STEP 1/16 м: до
# 4096 м от начала он точен в 24 битах, и вершина T + v·2^-9 у соседей совпадает бит в бит и на PSP.
# SHR_ORIGIN24=0 — как до правки (отрицательный контроль сторожа швов).
ORIGIN_STEP = 2.0 ** -4
ORIGIN24 = os.environ.get('SHR_ORIGIN24', '1') != '0'


def f24_exact(x):
    """x точно передаётся в GE 24-бит float (младшие 8 бит float32 нулевые)"""
    return (np.asarray(x, np.float32).view(np.uint32) & np.uint32(0xFF) == 0).all() and np.all(np.float32(x) == np.asarray(x))


def grid_quant(P, G):
    """P (N,3) мир -> (центр — узел решётки G, int16 xyz, Rec.sc = G·32768: движок и GE делят int16 на 32768).
    Центр города — на шаге ORIGIN_STEP (точен в 24-бит float GE, правка 106)"""
    lo_, hi_ = P.min(0), P.max(0)
    step = ORIGIN_STEP if ORIGIN24 else G
    c = np.round((lo_ + hi_) / 2 / step) * step
    if ORIGIN24 and not f24_exact(c): raise ValueError(f'центр куска {c} не точен в 24-бит float GE (дальше 4096 м?)')
    xyz = np.round((P - c) / G)
    if np.abs(xyz).max() > 32767: raise ValueError(f'кусок шире решётки: ±{np.abs(P - c).max():.1f} м при шаге {G}')
    return c, xyz.astype(np.int64), G * 32768

# Правки Роба 224/237/251 (03.10: «опять много швов», «швов стало больше», «швы очень мигают»; слово Роба 03.10: «лучше пусть
# полигоны прыгают, как на первой PlayStation, чем разрывы и просветы»). GE считает вершины во float24 (мантисса 15 бит —
# замер разработчика PPSSPP на PSP, github.com/hrydgard/ppsspp/issues/12058#issuecomment-913225641), PPSSPP — во float32,
# поэтому эмулятор этих трещин не показывает. Город — в 500–900 м от начала координат: ошибка float24 там до ~2 см, у камеры
# это 0.2–0.5 пикселя, и у каждого куска (клетка × текстура) своя матрица — общая вершина соседей округляется по-разному,
# вдоль стыков мигают точки фона (модель GE tools/seams_ge.py, 12 видов × 3 кадра: 902 пикс. щелей при склейке матриц, 570 — по
# шагам; по правилу ниже — 145 и 106).
# Правило порта, все уровни, точность вершин прежняя (решётка 2^-9 м):
#   1) у всех кусков одной клетки 40 м — один центр (центр клетки, шаг ORIGIN_STEP): общая вершина трава/тротуар/дорога внутри
#      клетки — один и тот же int16 при одной и той же матрице, GE считает её одинаково при любой своей точности;
#   2) дальних версий нет — кусок рисуется ближней на любой дальности (стык LOD 50 м и выкинутая мелочь FAR_MIN давали дыры:
#      эмулятор видит их и без float24); флаг куска 2048 (F_NEAR) — движок и инструменты берут ближнюю (viewer/main.c drawPiece);
#   3) Т-стыки ещё раз после юбок (у юбок разной глубины общий вертикальный край: низ короткой — на ребре длинной);
#   4) движок рисует город в осях камеры (viewer/main.c: вид — только поворот, перенос куска = центр − глаз во float32), ошибка
#      float24 — доля расстояния до камеры, а не 600–900 м от начала уровня.
# SHR_SEAMS_OLD=1 — как до правки (свой центр у куска, дальние версии; красный контроль tools/seams_ge.py, tests/seams/ge.sh).
SEAMS_OLD = os.environ.get('SHR_SEAMS_OLD') == '1'
# №511 (швы неба, слово Роба: «PNG плохо наслоен на небо, край слоя виден; местами прямой шов»), правило для неба ВСЕХ уровней и бонус-трасс:
#  1) цвет невидимых пикселей PNG слоя (холмы, облака) берётся у ближайших видимых — кайма на границе альфа-теста и в мипах (tools/texq.py bleed_rgb);
#  2) v слоя неба не переносится на целые повторы и пишется как есть в [0, 1] — движок рисует небо с GU_CLAMP по v (drawSky), поэтому нижняя/верхняя строка
#     слоя не подмешивается билинейным фильтром с противоположной (раньше: REPEAT, у слоя холмов верх непрозрачный, низ прозрачный -> волосяная линия вдоль
#     горизонта на всём круге); u по-прежнему повторяется и сдвигается на целое число повторов.
# SKY_SEAM_OLD=1 (или SHR_SKY_SEAM_OLD=1) — как раньше (красный контроль)
SKY_SEAM_OLD = os.environ.get('SKY_SEAM_OLD') == '1' or os.environ.get('SHR_SKY_SEAM_OLD') == '1'
SKY_TEX = {}   # url текстуры слоя неба -> порог видимости альфы для bleed_rgb (1 — смешиваемое, 65 — альфа-тест, движок GREATER 0x40)


def cell_quant(P, c, G):
    """P (n,3) -> (центр клетки c, int16 xyz, Rec.sc) на решётке G; кусок не влез в ±32767 шагов от центра клетки — свой центр"""
    xyz = np.round((P - c) / G)
    if np.abs(xyz).max() > 32767: return grid_quant(P, G) + (False,)
    return c, xyz.astype(np.int64), G * 32768, True

WELD_EPS = float(os.environ.get('SHR_WELD_EPS', 0.0012))   # м: вершины разных мешей ближе этого — одна точка шва (меньше шага решётки 2^-9 = 1.95 мм)

def weld(groups, eps=WELD_EPS, vis=(0, 2)):
    """Точки шва, которые у художников разошлись на доли миллиметра (трава и тротуар — разные меши), на общей решётке могли
    лечь в соседние узлы — щель в шаг решётки. Правило (правка Роба 37): вершины города ближе eps — одна точка (самая
    «младшая» по координатам). Меняет форму не больше eps; слои-наклейки (≥ 2 мм над подложкой) не трогает."""
    refs = []
    for k, parts in groups.items():
        for pi, part in enumerate(parts):
            for vi in vis: refs.append((k, pi, vi))
    arrs = [groups[k][pi][vi] for k, pi, vi in refs]
    if not arrs: return 0
    P = np.concatenate([np.stack([a['x'], a['y'], a['z']], 1) for a in arrs]).astype(np.float64)
    U, inv = np.unique(P, axis=0, return_inverse=True); inv = inv.ravel()
    cell = np.floor(U / eps).astype(np.int64)
    key = (cell[:, 0] * 2000003 + cell[:, 1]) * 2000029 + cell[:, 2]
    o = np.argsort(key); ks = key[o]
    parent = np.arange(len(U))
    def find(i):
        while parent[i] != i: parent[i] = parent[parent[i]]; i = parent[i]
        return i
    pairs = 0
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                nk = ((cell[:, 0] + dx) * 2000003 + (cell[:, 1] + dy)) * 2000029 + (cell[:, 2] + dz)
                a, b = np.searchsorted(ks, nk, 'left'), np.searchsorted(ks, nk, 'right')
                n = b - a; src = np.repeat(np.arange(len(U)), n)
                if not len(src): continue
                dst = o[np.repeat(a, n) + (np.arange(n.sum()) - np.repeat(np.cumsum(n) - n, n))]
                m = (dst > src) & (np.abs(U[dst] - U[src]).max(1) < eps)
                for i, j in zip(src[m], dst[m]):
                    ri, rj = find(i), find(j)
                    if ri != rj: parent[max(ri, rj)] = min(ri, rj); pairs += 1
    root = np.array([find(i) for i in range(len(U))])
    W = U[root][inv]   # U отсортированы: корень — самая младшая точка кластера
    off = 0
    for (k, pi, vi), a in zip(refs, arrs):
        n = len(a); a['x'], a['y'], a['z'] = W[off:off + n, 0], W[off:off + n, 1], W[off:off + n, 2]; off += n
    return pairs

TJ_EPS = float(os.environ.get('SHR_TJ_EPS', 0.001))      # м: вершина чужого треугольника ближе этого к ребру — Т-стык (как гейт tools/seams.py; точки уже на решётке)
TJ_END = 0.0025     # м: ...и не ближе этого к концам ребра (гейт: 3 мм; ближе — это сама вершина ребра)
# «Иголка»: если после разреза в V конец ребра оказался бы ближе TJ_EPS к новому ребру V–(третья вершина), это не стык,
# а две почти совпавшие вершины у конца ребра (у художников 3–7 мм): разрез рождал бы новую иголку, и разрезы шли бы
# по кругу. Щель там — отрезок в миллиметры у самой вершины. Гейт tools/seams.py считает их отдельно, для сведения.

def tj_needle(a, b, q, c, eps):
    """разрез ребра a–b в точке q с веером от c даёт иголку? (конец ребра ближе eps к прямой q–c) -> маска"""
    d = c - q; l2 = (d * d).sum(1); l2[l2 == 0] = 1e-18
    def dist(p_):
        t = ((p_ - q) * d).sum(1) / l2
        return np.linalg.norm(p_ - (q + t[:, None] * d), axis=1)
    return (dist(a) < eps) | (dist(b) < eps)

def snap_grid(groups, G):
    """вершины кусков -> узлы решётки G в мире (как их потом запишет grid_quant: центр куска — узел решётки, поэтому
    мир = round(P/G)·G ровно). Т-стыки режутся уже по итоговым точкам: иначе щель «в данных» 1–4 мм после решётки
    ложилась на ребро соседа ровно (правка Роба 37)"""
    for parts in groups.values():
        for part in parts:
            for a in (part[0], part[2]):
                for f in ('x', 'y', 'z'): a[f] = np.round(a[f].astype(np.float64) / G) * G

def split_tjunctions(groups, eps=TJ_EPS, passes=12):
    """Т-стыки (правка Роба 37): вершина одного треугольника лежит на ребре другого, а не в его вершине (у художников
    трава кончается посреди ребра тротуара и т. п.). GE растеризует ребро a–b и ломаную a–V–b по-разному (4 бита
    подпикселя), и вдоль шва то пропадает, то появляется пиксель фона — мерцание. Правило: такое ребро режется в точке V
    (новая вершина — точно V, после решётки бит в бит; UV и цвет — по ребру), треугольник — веером от противоположной
    вершины (одно ребро) или от центра (несколько); повтор, пока стыки есть (веер мог лечь на чужую вершину). Только
    ближние версии кусков; предок у детей тот же. Вершины уже на решётке (snap_grid). -> число разрезов"""
    total = 0
    for _ in range(passes):
        refs, tris = [], []
        for k, parts in groups.items():
            for pi, part in enumerate(parts):
                v = part[0]; n = len(v) // 3
                if n: refs.append((k, pi, n)); tris.append(np.stack([v['x'], v['y'], v['z']], 1).astype(np.float64).reshape(-1, 3, 3))
        if not tris: return total
        P = np.concatenate(tris)
        V, vinv = np.unique(P.reshape(-1, 3), axis=0, return_inverse=True); TV = vinv.reshape(-1, 3)   # вершины треугольника — не стык (у «иголки» своя вершина у ребра)
        cell = 2.0
        kv = np.floor(V[:, [0, 2]] / cell).astype(np.int64); vkey = (kv[:, 0] + 100000) * 200000 + (kv[:, 1] + 100000)
        o = np.argsort(vkey, kind='stable'); vkey_s = vkey[o]
        E0 = np.concatenate([P[:, 0], P[:, 1], P[:, 2]]); E1 = np.concatenate([P[:, 1], P[:, 2], P[:, 0]])
        ET = np.tile(np.arange(len(P)), 3); EK = np.repeat(np.arange(3), len(P))
        hits = []
        for c0 in range(0, len(E0), 100000):
            a, b = E0[c0:c0 + 100000], E1[c0:c0 + 100000]; ed = np.arange(c0, c0 + len(a))
            lo = np.floor((np.minimum(a, b)[:, [0, 2]] - eps) / cell).astype(np.int64); hi = np.floor((np.maximum(a, b)[:, [0, 2]] + eps) / cell).astype(np.int64)
            sp = hi - lo + 1; cnt = sp[:, 0] * sp[:, 1]
            rep = np.repeat(np.arange(len(a)), cnt); kk = np.arange(cnt.sum()) - np.repeat(np.cumsum(cnt) - cnt, cnt)
            gx = lo[rep, 0] + kk % sp[rep, 0]; gz = lo[rep, 1] + kk // sp[rep, 0]
            key = (gx + 100000) * 200000 + (gz + 100000)
            s, t = np.searchsorted(vkey_s, key, 'left'), np.searchsorted(vkey_s, key, 'right'); m = t - s
            E = rep[np.repeat(np.arange(len(rep)), m)]; Vi = o[np.repeat(s, m) + (np.arange(m.sum()) - np.repeat(np.cumsum(m) - m, m))]
            d_ = b[E] - a[E]; l2 = (d_ * d_).sum(1); l2[l2 == 0] = 1e-18; tt = ((V[Vi] - a[E]) * d_).sum(1) / l2
            dist = np.linalg.norm(V[Vi] - (a[E] + tt[:, None] * d_), axis=1); L = np.sqrt(l2)
            et = ET[ed[E]]; h = (dist < eps) & (tt * L > TJ_END) & ((1 - tt) * L > TJ_END) & (TV[et, 0] != Vi) & (TV[et, 1] != Vi) & (TV[et, 2] != Vi)
            hh = np.nonzero(h)[0]; h[hh[tj_needle(a[E[hh]], b[E[hh]], V[Vi[hh]], P[et[hh], (EK[ed[E[hh]]] + 2) % 3], eps)]] = False
            hits.append(np.stack([ed[E[h]], Vi[h], tt[h]], 1))
        H = np.concatenate(hits) if hits else np.zeros((0, 3))
        if not len(H): return total
        he, hv, ht = H[:, 0].astype(int), H[:, 1].astype(int), H[:, 2]
        _, u = np.unique(np.stack([he, hv], 1), axis=0, return_index=True); he, hv, ht = he[u], hv[u], ht[u]
        total += len(he)
        if os.environ.get("SHR_TJ_LOG"): print("    проход Т-стыков:", len(he), flush=True)
        htri, hk = ET[he], EK[he]
        by_tri = {}
        for tri, k, vi, t in zip(htri, hk, hv, ht): by_tri.setdefault(int(tri), [[], [], []])[int(k)].append((float(t), int(vi)))
        # треугольники -> части; новые вершины
        base = np.cumsum([0] + [r[2] for r in refs])
        def lerp_c(c0, c1, t):
            return sum(int(round(((int(c0) >> s_) & 0xFF) * (1 - t) + ((int(c1) >> s_) & 0xFF) * t)) << s_ for s_ in (0, 8, 16, 24))
        per_ref = {}
        for tri in by_tri: per_ref.setdefault(int(np.searchsorted(base, tri, 'right') - 1), []).append(tri)
        for ri, mine in per_ref.items():
            k, pi, n = refs[ri]; lo_ = base[ri]
            vn, anc, vf, keep = groups[k][pi]
            drop = np.zeros(n, bool); newv, newa = [], []
            for tri in mine:
                j = tri - lo_; drop[j] = True; cv = vn[3 * j:3 * j + 3]; ed = by_tri[tri]
                def pt(kk, t, vi):
                    r = np.zeros(1, vn.dtype)[0]; a0, a1 = cv[kk], cv[(kk + 1) % 3]
                    r['u'] = a0['u'] * (1 - t) + a1['u'] * t; r['v'] = a0['v'] * (1 - t) + a1['v'] * t
                    r['c'] = lerp_c(a0['c'], a1['c'], t); r['x'], r['y'], r['z'] = V[vi]
                    return r
                chains = [[pt(kk, t, vi) for t, vi in sorted(ed[kk])] for kk in range(3)]
                withp = [kk for kk in range(3) if chains[kk]]
                if len(withp) == 1:
                    kk = withp[0]; seq = [cv[kk]] + chains[kk] + [cv[(kk + 1) % 3]]; apex = cv[(kk + 2) % 3]
                    tl = [(seq[i], seq[i + 1], apex) for i in range(len(seq) - 1)]
                else:
                    ctr = np.zeros(1, vn.dtype)[0]
                    for f_ in ('u', 'v', 'x', 'y', 'z'): ctr[f_] = (cv[0][f_] + cv[1][f_] + cv[2][f_]) / 3
                    ctr['c'] = sum(int(round(sum((int(cv[i]['c']) >> s_) & 0xFF for i in range(3)) / 3)) << s_ for s_ in (0, 8, 16, 24))
                    ring = []
                    for kk in range(3): ring += [cv[kk]] + chains[kk]
                    tl = [(ring[i], ring[(i + 1) % len(ring)], ctr) for i in range(len(ring))]
                for tr in tl: newv.extend(tr); newa.append(anc[j])
            keepm = np.repeat(~drop, 3)
            vn2 = np.concatenate([vn[keepm], np.array(newv, vn.dtype)])
            anc2 = np.concatenate([anc[~drop], np.array(newa, anc.dtype)])
            groups[k][pi] = (vn2, anc2, vf, keep)
    return total

def ni_bytes(q, v12):
    """16-байтные вершины (QV) -> байты вершинного потока без индексов (16 Б или 12 Б с цветом 5650)"""
    if not v12: return q.tobytes()
    o = np.zeros(len(q), QV12)
    for k in ('u', 'v', 'x', 'y', 'z'): o[k] = q[k]
    o['c'] = to5650(q['c'])
    return o.tobytes()
SKY_S = 1 / 60   # небо ужимаем и рисуем вокруг камеры
SKY_NAMES = ('horizon', 'sky')
FAR_MIN = float(os.environ.get('SHR_FAR_MIN', 1.2))   # в дальней версии куска выкидываем исходные треугольники мельче этого (м²)
# SHR_FAR_MIN — только для замера (правка 106, 30.09: 0 = все исходные треугольники, эталон щелей tools/seams_far.py --vs)
FAR_RULE = os.environ.get('SHR_FAR_RULE', 'seam')   # правка 106: 'area' — как до правки (только площадь, отрицательный контроль)
FAR_GROW = float(os.environ.get('SHR_FAR_GROW', 0.15))   # правка 106: швы добавляют в дальние версии не больше 15 % треугольников
FAR_SPLIT = 28.0  # дальняя версия = исходные треугольники, лишь рёбра длиннее 28 м делим пополам. PSP выбрасывает
                  # треугольник целиком, если хоть одна вершина вне гвард-бэнда 4096 px (~83.7° от оси взгляда при
                  # вьюпорте 480x272, fovy 62°). От угла кадра до этой границы >= 33.7°, дальний кусок весь >= 50 м
                  # от глаза -> видимый треугольник с такой вершиной имеет ребро >= 2*50*sin(33.7°/2) = 29.0 м
PIECE_VERTS = 60000   # «суп» вершин (ближняя + дальняя версия) на кусок; уникальных всегда <= 65535
SKY_SECTORS = 8   # небо — 8 секторов по 45° (азимут центра треугольника вокруг камеры), движок отсекает их пирамидой
SKY_CAP = 30.0    # ...и «шапка» над головой (центр треугольника ближе 30 ужатых м к оси): иначе ящик каждого сектора
                  # дотягивается до оси, накрывает глаз и не отсекается никогда
GRAD_TOL = 0.5    # текстура неба = вертикальный градиент, если в каждой строке пиксели отличаются не больше чем на 0.5
BAKE_MAX = 8.0    # ...и вмешиваем её в цвет вершин, только если плавная заливка отличается от текстуры не больше 8/255
# Старт уровня — из данных (docs/РЕЦЕПТ_УРОВНЯ.md §1.2 п. 2): InitLevelPlayerVehicle(машина, локатор) в leveli.mfk -> локатор
# level<N>_carstart в art/missions/level0N/level.p3d (tools/level_sweep.find_spawn, курс движка = pi - угол из файла).
# SPAWN — ПРАВИЛО ПОРТА поверх данных, только уровень 1, до слова Роба (РЕЦЕПТ §7 вопрос 1): старт — точка телепорта
# «Simpsons' House» (220, 3.5, 172) курсом pi/2, как на PSP с этапа 1; локатор уровня 1 — в гараже в 5.2 м от неё.
SPAWN = {1: ((220, 3.5, 172), np.pi / 2)}


def level_start(level):
    """-> ((x, y, z), курс), машина игрока, откуда старт. Машина — первый аргумент InitLevelPlayerVehicle уровня."""
    sp = find_spawn(level)
    if 'car' not in sp: raise SystemExit(f'уровень {level}: нет InitLevelPlayerVehicle в leveli.mfk ({sp.get("error")})')
    if level in SPAWN: return SPAWN[level], sp['car'], 'правило порта SPAWN'
    if 'pos' not in sp: raise SystemExit(f'уровень {level}: локатор старта {sp.get("locator")} не найден ({sp.get("error")})')
    return (tuple(sp['pos']), sp['heading']), sp['car'], f"локатор {sp['locator']} ({sp['source']})"


def car_model(level, car):
    """Модель машины игрока: LoadDisposableCar(путь, car, ...) в level.mfk уровня, иначе art/cars/<car>.p3d."""
    txt = re.sub(r'//[^\n]*', '', (GAME / f'scripts/missions/level0{level}/level.mfk').read_text(errors='replace'))
    m = re.search(r'LoadDisposableCar\s*\(\s*"([^"]+)"\s*,\s*"' + re.escape(car) + r'"', txt)
    p = GAME / m.group(1).replace('\\', '/') if m else GAME / 'art' / 'cars' / f'{car}.p3d'
    if not p.exists(): raise SystemExit(f'уровень {level}: нет модели машины {car}: {p}')
    return p


def export_scene(conv, path, outname):
    """Сцена города с небом из данных: у l2..l7_terra нет графа сцены 0x120100, небо только в мировой сфере 0x3f0000b —
    level_sweep.sky_patch дописывает граф с её составным рисунком (обычно horizon1). У сцен с графом (l1_terra) файл не меняется."""
    import convert as cv
    orig = cv.read
    cv.read = lambda p_: sky_patch(orig(p_))
    try: return conv.export(path, outname)
    finally: cv.read = orig
VT = [('u', '<f4'), ('v', '<f4'), ('c', '<u4'), ('x', '<f4'), ('y', '<f4'), ('z', '<f4')]

def swizzle_level(data, w, h):
    """CLUT8 w x h -> swizzle GE: блоки 16 байт x 8 строк, высота добивается до кратной 8 последней строкой"""
    a = np.frombuffer(data, np.uint8).reshape(h, w); hp = (h + 7) & ~7
    if hp != h: a = np.concatenate([a, np.repeat(a[-1:], hp - h, 0)])
    return a.reshape(hp // 8, 8, w // 16, 16).transpose(0, 2, 1, 3).tobytes()

def load_tex(url, cache, blob, blend=False, sky=False, flat=False):   # flat — №679 мультик: средний цвет вместо рисунка (альфа сохраняется); blend — только для texq (копия вырезки без масштаба покрытия); sky — слой неба (№511)
    if not url: return -1
    skyb = sky and url in SKY_TEX and not SKY_SEAM_OLD
    ck = url + '#sky' if skyb else url
    if flat: ck += '#toon'
    if ck in cache: return cache[ck]
    img = Image.open(ASSETS / url).convert('RGBA')
    if url in PNG_SRC and os.environ.get('SHR_PNG_FLIP', '1') != '0': img = img.transpose(Image.FLIP_TOP_BOTTOM)   # правка 226
    if skyb:
        import texq as _tq; img = _tq.bleed_rgb(img, SKY_TEX[url])   # №511
    if flat:   # №679: однотонная заливка средним цветом по альфе; у вырезок (кусты, заборы) форма альфы остаётся, у сплошных текстур размер минимальный 16x8
        a_ = np.asarray(img, np.float64).reshape(-1, 4); wt_ = a_[:, 3:4] / 255.0 + 1e-6
        avg_ = ((a_[:, :3] * wt_).sum(0) / wt_.sum()).round().astype(np.uint8)
        al_ = np.asarray(img.getchannel('A'))
        if al_.min() >= 250:
            img = Image.new('RGBA', (16, 8), tuple(int(x) for x in avg_) + (255,))
        else:
            arr_ = np.zeros((img.size[1], img.size[0], 4), np.uint8); arr_[..., :3] = avg_; arr_[..., 3] = al_
            img = Image.fromarray(arr_, 'RGBA')
    w, h = img.size; s = min(1.0, CAP / max(w, h))
    w, h = max(16, int(w * s)), max(8, int(h * s))
    if img.size != (w, h): img = img.resize((w, h), Image.LANCZOS)
    q = img.quantize(256, method=Image.Quantize.FASTOCTREE)
    pal = np.zeros((256, 4), np.uint8)
    p = np.array(q.getpalette(rawmode='RGBA'), np.uint8).reshape(-1, 4)[:256]; pal[:len(p)] = p
    levels = []; lw, lh, cur = w, h, img
    while lw >= 16 and len(levels) < 8:
        if levels:
            a = np.asarray(cur, np.int32).reshape(-1, 4)
            idx = ((a[:, None, :] - pal[None].astype(np.int32)) ** 2).sum(2).argmin(1).astype(np.uint8)
        else: idx = np.asarray(q, np.uint8).reshape(-1)
        levels.append(idx.tobytes())
        lw, lh = lw // 2, max(1, lh // 2)
        if lw >= 16: cur = img.resize((lw, lh), Image.LANCZOS)
    # уровни сразу в swizzle (блоки 16 Б x 8 строк, высота добита до кратной 8 повтором последней строки — как
    # swizzleLevel в main.c): байт 7 заголовка = 1, движку не нужна вторая (линейная) копия — -3 МБ ОЗУ (шаг 4.2 аудита)
    out = struct.pack('<HHHBB8x', w, h, len(levels), 0, 1) + pal.tobytes()
    lw, lh = w, h
    for lv in levels:
        sw = swizzle_level(lv, lw, lh); out += sw + b'\0' * (-len(sw) % 16)
        lw, lh = lw // 2, max(1, lh // 2)
    blob.append(out); cache[ck] = len(blob) - 1
    return cache[ck]

# Ф2 текстуры (docs/АУДИТ_ОПТИМИЗАЦИИ 10.3): точные палитры tools/texq.py — T8, предел 128, уровни сразу в
# swizzle (байт 7 = 1, вторая копия в ОЗУ не нужна). Размеры и число мипов те же, что у load_tex выше.
#   SHR_TEXQ=0          -> старый путь (FASTOCTREE, load_tex выше)
#   SHR_TEX_CAP256=1    -> родной 256x128 для неба и фона (TEX_CAP256_SKY), остальное 128
#   SHR_SMALL_MIPS=1    -> мипы и ниже 16 px (до 1 px; строка 16 Б, байт 8 = 1; BOX + покрытие альфа-теста)
# Правка Роба 129 (01.10, «вывески слишком чёткие — усреднить под PSP, золотая середина; ни вылетов, ни долгих загрузок»):
#   SHR_SIGN_CAP=1 (по умолчанию) -> вывески и щиты с надписью (TEX_CAP256_SIGN) — не больше SIGN_CAP по длинной стороне,
#                                   как остальной город (до 01.10 им возвращали родные 256x128). 0 — как было (256)
#   SHR_T4=1 (по умолчанию)       -> текстуры, у которых на уровне 0 не больше 16 цветов (на диске это CLUT4, 0x19001 bpp=4),
#                                   пишутся 4 битами (байт 6 = 4): уровень 0 побайтно тот же, памяти вдвое меньше. Мипы — в те
#                                   же 16 цветов; если средняя ошибка мипов больше texq.T4_MIP_ERR — остаётся T8 (золотая
#                                   середина: вдали без ступенек). 0 — как было (всё T8)
# T4 — только в level.shr (движок: viewer/main.c texRowBytes); пакеты трафика, комнат, машин миссий и сборов импортируют этот
# модуль и остаются T8 (их загрузчики T4 не ждут) — поэтому CLUT4 включается только при запуске экспортёра уровня.
TEX_CAP256_SKY = ('textures/a7a9b8ac41248b5e9546.png',    # l1_cloud_2 (l1_terra.p3d)
                  'textures/68f3567d2aa80fb31ee6.png',    # l1_valleybackground
                  'textures/a44b975c13f87db7f865.png')    # l1_cloud_1
TEX_CAP256_SIGN = ('textures/613596a3df666840a734.png',   # lard_lad (l1_terra.p3d)
                   'textures/2f9eba2961ee2932bcf9.png',   # billboard (l1r2.p3d)
                   'textures/020948d017434d037b00.png',   # areacode_base (l1r3.p3d)
                   'textures/0c3ef8f9ed6afa467bf9.png',   # tomacco (l1r6.p3d)
                   'textures/e3a9ff751e5b9e72872f.png')   # tirefire (l1r6.p3d)
TEX_CAP256 = TEX_CAP256_SKY + TEX_CAP256_SIGN
SIGN_CAP = 128
FADE_EXTRA_GROUND = FADE_EXTRA_WALL = ''   # доп. имена (регулярка в нижнем регистре) для затухания мипов и сдвига мипа: земля/дороги и стены; задаёт tools/bonus/export.py для трасс
if os.environ.get('SHR_TEXQ', '1') != '0':
    import texq
    texq.CAP, texq.CLUT4, texq.PRESWIZZLE = CAP, os.environ.get('SHR_T4', '0') == '1' and __name__ == '__main__', True   # Роб 01.10 15:1x: «жёлтый след на асфальте на скорости» — мипы T4 в 16 цветов; выкл., как было
    texq.SMALL_MIPS = os.environ.get('SHR_SMALL_MIPS') == '1'
    if os.environ.get('SHR_TEX_CAP256') == '1':
        sign = os.environ.get('SHR_SIGN_CAP', '0') == '1'   # Роб 01.10 14:4x: «некоторые билборды очень сильно испортил» — 5 вывесок снова 256×128 (128 экономил лишь ~120 КБ)
        texq.CAPS.update({u: 256 for u in TEX_CAP256_SKY})
        texq.CAPS.update({u: SIGN_CAP if sign else 256 for u in TEX_CAP256_SIGN})
    texq.BLEED_URLS = SKY_TEX   # №511: тот же словарь, пополняется pack_prims
    load_tex = texq.load_tex
    if os.environ.get('SHR_PNG_FLIP', '1') != '0': texq.FLIP_V = PNG_SRC   # правка 226: тот же набор, пополняется конвертером

def subdivide_lod(P, UV, C, split=SPLIT, ground_split=SPLIT_GROUND, far_split=None):
    """Режем длинные треугольники: PSP не обрезает их у камеры, и дорога у машины пропадала бы.
    Согласованная нарезка (27.09, «стыки на полу»): решение резать принимается ПО РЕБРУ, по его собственной длине
    (почти горизонтальное ребро — порог ground_split, иначе split), и режется всегда в середине. Оба соседа общего
    ребра решают одинаково -> общие вершины совпадают, нет Т-стыков и щелей (раньше резали самое длинное ребро
    треугольника, и сосед мог это же ребро оставить целым).
    far_split: заодно собираем дальнюю версию — те же исходные треугольники, порезанные только до far_split
    («дальние предки»). Каждый мелкий треугольник — потомок ровно одного предка, края совпадают.
    -> P, UV, C                                     (far_split=None)
    -> P, UV, C, anc, (fP, fUV, fC, forig)          anc: номер предка у каждого мелкого треугольника,
                                                    forig: номер исходного треугольника у каждого предка"""
    P, UV, C = P.reshape(-1, 3, 3), UV.reshape(-1, 3, 2), C.reshape(-1, 3)
    orig, anc = np.arange(len(P)), np.full(len(P), -1)
    done, far, nfar = [], [], 0
    def adopt(new):   # треугольники new становятся дальними предками
        nonlocal nfar
        if not new.any(): return
        anc[new] = nfar + np.arange(int(new.sum())); nfar += int(new.sum())
        far.append((P[new], UV[new], C[new], orig[new]))
    def cmid(c0, c1):   # цвет ABGR в середине ребра (побайтно, с альфой — стекло полупрозрачное)
        return sum(((((c0 >> s) & 0xFF) + ((c1 >> s) & 0xFF)) // 2) << s for s in (0, 8, 16, 24)).astype(np.uint32)
    for _ in range(12):
        d = np.stack([P[:, (i + 1) % 3] - P[:, i] for i in range(3)], 1)          # рёбра i: v_i -> v_{i+1}
        e = np.linalg.norm(d, axis=2)
        horiz = np.abs(d[..., 1]) < 0.3 * (e + 1e-9)
        long_ = e > np.where(horiz, ground_split, split)
        if far_split is not None: adopt((anc < 0) & (e.max(1) <= far_split))
        big = long_.any(1)
        if far_split is not None: adopt((anc < 0) & ~big)   # страховка: готовый треугольник без предка сам себе предок
        done.append((P[~big], UV[~big], C[~big], anc[~big]))
        if not big.any(): P = P[:0]; break
        P, UV, C, L, orig, anc = P[big], UV[big], C[big], long_[big], orig[big], anc[big]
        cnt = L.sum(1)
        # поворот вершин: одно длинное ребро -> оно ребро 0; два -> короткое ребро 0; три -> как есть
        k = np.where(cnt == 1, L.argmax(1), np.where(cnt == 2, (~L).argmax(1), 0))
        n = np.arange(len(P)); idx = [(k + j) % 3 for j in range(3)]
        v = [P[n, i] for i in idx]; t = [UV[n, i] for i in idx]; c = [C[n, i] for i in idx]
        m = [(v[j] + v[(j + 1) % 3]) / 2 for j in range(3)]; mt = [(t[j] + t[(j + 1) % 3]) / 2 for j in range(3)]
        mc = [cmid(c[j], c[(j + 1) % 3]) for j in range(3)]
        outP, outT, outC, outO, outA = [], [], [], [], []
        def emit(sel, tris):
            for (p0, t0, c0), (p1, t1, c1), (p2, t2, c2) in tris:
                outP.append(np.stack([p0[sel], p1[sel], p2[sel]], 1)); outT.append(np.stack([t0[sel], t1[sel], t2[sel]], 1))
                outC.append(np.stack([c0[sel], c1[sel], c2[sel]], 1)); outO.append(orig[sel]); outA.append(anc[sel])
        V = [(v[j], t[j], c[j]) for j in range(3)]; M = [(m[j], mt[j], mc[j]) for j in range(3)]
        emit(cnt == 1, [(V[0], M[0], V[2]), (M[0], V[1], V[2])])                         # режем ребро 0
        emit(cnt == 2, [(V[0], V[1], M[1]), (V[0], M[1], M[2]), (M[1], V[2], M[2])])     # рёбра 1 и 2
        emit(cnt == 3, [(V[0], M[0], M[2]), (M[0], V[1], M[1]), (M[2], M[1], V[2]), (M[0], M[1], M[2])])
        P, UV, C = np.concatenate(outP), np.concatenate(outT), np.concatenate(outC)
        orig, anc = np.concatenate(outO), np.concatenate(outA)
    if len(P):
        if far_split is not None: adopt(anc < 0)
        done.append((P, UV, C, anc))
    out = (np.concatenate([d[0] for d in done]).reshape(-1, 3), np.concatenate([d[1] for d in done]).reshape(-1, 2),
           np.concatenate([d[2] for d in done]).reshape(-1))
    if far_split is None: return out
    if not far:   # пустой примитив (0 треугольников): np.concatenate([]) упал бы
        far = [(np.zeros((0, 3, 3), np.float32), np.zeros((0, 3, 2), np.float32), np.zeros((0, 3), np.uint32), np.zeros(0, int))]
    fP, fUV, fC, forig = (np.concatenate([f[i] for f in far]) for i in range(4))
    return out + (np.concatenate([d[3] for d in done]), (fP.reshape(-1, 3), fUV.reshape(-1, 2), fC.reshape(-1), forig))

def subdivide(P, UV, C, split=SPLIT, ground_split=SPLIT_GROUND):
    return subdivide_lod(P, UV, C, split, ground_split)

def tri_area(P):
    T = np.asarray(P, np.float64).reshape(-1, 3, 3)
    return np.linalg.norm(np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]), axis=1) / 2

def far_keep(prims):
    """Правка Роба 106 («стыки рябят, проступают швы»): какие исходные треугольники города остаются в дальней версии куска
    (дальше LOD_DIST 50 м, viewer/main.c). До правки — только по площади (>= FAR_MIN): узкие полосы стыков (бордюр, кромка
    крыши, край тротуара) малы по площади и выпадали, у оставшихся соседей открывался шов (уровень 1: 23.6 тыс. рёбер,
    73 км, tools/seams_far.py --vs). Правило по соседству, одно для всех уровней: мелкие треугольники, связанные общими
    рёбрами (вершины до 1 мм, между мешами тоже), — «компонент». Компонент без общего ребра с крупным треугольником —
    отдельная мелочь (ящик на крыше, табличка): выкидываем, шва он не откроет. Компонент, вшитый в крупные поверхности,
    закрывает шов: оставляем, начиная с самых «швовых» (длина общих с крупными рёбер на треугольник: узкие длинные полосы
    первыми), пока рост дальней версии не превысит FAR_GROW.   prims: [(t, fl, P, UV, C)] -> (маски по prims, сводка)"""
    T = [np.asarray(p[2], np.float64).reshape(-1, 3, 3) for p in prims]
    cuts = np.cumsum([0] + [len(t_) for t_ in T])
    P = np.concatenate(T) if T else np.zeros((0, 3, 3)); n = len(P)
    big = tri_area(P) >= FAR_MIN
    split = lambda m: [m[cuts[i]:cuts[i + 1]] for i in range(len(T))]
    if FAR_RULE == 'area' or n == 0 or big.all():
        return split(big), f'дальние версии: правило площади, {int(big.sum()):,} из {n:,} исходных треугольников'
    q = np.round(P.reshape(-1, 3) * 1000).astype(np.int64)   # вершины до 1 мм: общие вершины соседних мешей диска совпадают
    _, vid = np.unique(q, axis=0, return_inverse=True); vid = vid.reshape(-1, 3); nv = int(vid.max()) + 1
    E = np.sort(np.stack([vid[:, [0, 1]], vid[:, [1, 2]], vid[:, [2, 0]]], 1), axis=2)
    key = (E[..., 0] * nv + E[..., 1]).ravel()
    L = np.linalg.norm(P[:, [1, 2, 0]] - P, axis=2).ravel()   # длина ребра v_i -> v_{i+1}
    tri = np.repeat(np.arange(n), 3)
    o = np.argsort(key, kind='stable'); key, tri, L = key[o], tri[o], L[o]
    s_ = key[1:] == key[:-1]
    a, b, el = tri[:-1][s_], tri[1:][s_], L[:-1][s_]          # пары треугольников с общим ребром (на ребре > 2 — цепочкой)
    sm = ~big[a] & ~big[b]; ua, ub = a[sm], b[sm]
    lab = np.arange(n)                                         # компоненты мелких: метка = наименьший номер, со скачками
    while True:
        m_ = np.minimum(lab[ua], lab[ub]); new = lab.copy()
        np.minimum.at(new, ua, m_); np.minimum.at(new, ub, m_); new = new[new]
        if (new == lab).all(): break
        lab = new
    t1, t2 = big[a] & ~big[b], big[b] & ~big[a]                # шов: мелкий треугольник с общим ребром с крупным
    st, sl = np.concatenate([b[t1], a[t2]]), np.concatenate([el[t1], el[t2]])
    seam = np.bincount(lab[st], weights=sl, minlength=n)       # длина шва компонента (по корню)
    small = np.nonzero(~big)[0]; cnt = np.bincount(lab[small], minlength=n)
    cand = np.nonzero(seam > 0)[0]
    order = cand[np.argsort(-(seam[cand] / cnt[cand]), kind='stable')]
    take = order[np.cumsum(cnt[order]) <= int(FAR_GROW * big.sum())]
    chosen = np.zeros(n, bool); chosen[take] = True
    keep = big | (chosen[lab] & ~big)
    msg = (f'дальние версии (правка 106): крупных {int(big.sum()):,}, во швах {int(cnt[cand].sum()):,} мелких '
           f'({len(cand):,} комп., шов {seam[cand].sum():,.0f} м), взято {int(cnt[take].sum()):,} '
           f'(шов {seam[take].sum():,.0f} м, {100 * seam[take].sum() / max(seam[cand].sum(), 1e-9):.1f}%), '
           f'отдельной мелочи выкинуто {int((~big).sum() - cnt[cand].sum()):,}; итог {int(keep.sum()):,} из {n:,} '
           f'(+{100 * (keep.sum() / big.sum() - 1):.1f}% к правилу площади, предел +{100 * FAR_GROW:.0f}%)')
    return split(keep), msg

def gradient_rows(url):
    """Текстура — чистый вертикальный градиент (каждая строка одного цвета)? -> цвета строк (H, 4) или None"""
    a = np.asarray(Image.open(ASSETS / url).convert('RGBA'), np.float64)
    return a.mean(1) if np.abs(a - a.mean(1, keepdims=True)).max() <= GRAD_TOL else None

def sample_rows(rows, v):
    """Цвет градиента в точке v так, как его берёт PSP: билинейно по v, повтор текстуры (GU_REPEAT)."""
    H = len(rows); y = np.asarray(v, np.float64) * H - .5; i0 = np.floor(y).astype(int); f = (y - i0)[..., None]
    return rows[i0 % H] * (1 - f) + rows[(i0 + 1) % H] * f

def abgr_split(C): return np.stack([(C >> s) & 0xFF for s in (0, 8, 16, 24)], -1).astype(np.float64)   # -> r, g, b, a

def bake_gradient(UV, C, rows, n=12):
    """Цвет градиентной текстуры в каждой вершине вмешиваем в цвет вершины (как GU_TFX_MODULATE).
    -> новые цвета и макс. ошибка (0..255) внутри треугольников против текстуры (выборка n*(n+1)/2 точек)."""
    vq = np.round(UV[:, 1] * 256) / 256                                # v так, как оно ляжет в файл
    col = abgr_split(C); tex = sample_rows(rows, vq)
    baked = np.clip(np.round(tex * col / 255), 0, 255).astype(np.uint32)
    newC = (baked[:, 0] | baked[:, 1] << 8 | baked[:, 2] << 16 | baked[:, 3] << 24).astype(np.uint32)
    i, j = np.meshgrid(np.arange(n + 1), np.arange(n + 1)); m = i + j <= n
    w = np.stack([i[m], j[m], n - i[m] - j[m]], 1) / n                 # барицентрические точки
    V, CO, BK = vq.reshape(-1, 3), col.reshape(-1, 3, 4), baked.reshape(-1, 3, 4).astype(np.float64)
    truth = sample_rows(rows, V @ w.T) * np.einsum('pk,tkc->tpc', w, CO) / 255   # текстура * цвет вершины
    gour = np.einsum('pk,tkc->tpc', w, BK)                                        # плавная заливка вершин
    return newC, float(np.abs(truth - gour).max(initial=0.0))

def is_sky(obj): return obj['name'].lower().startswith(SKY_NAMES)
BACKDROP_RE = re.compile(r'horizon|sky|backmount|backfade|valley|cloud|topfade', re.I)   # фон уровня для трасс (правка 473)
JUNK_MESHES = ('Plane1Shape',)   # служебная плоскость с палитрой в небе

# Семейство «вырезка со смешиванием» (правка Роба 28.09: у лестницы пожарной машины «в некоторых местах пропадают
# текстуры»): материал с альфа-тестом (ATST) и смешиванием (BLMD 1), у которого альфа текстуры уровня 0 двоичная — дыры
# и сплошное, промежуточных значений < CUT_MID (на уровне 1: лестница пожарки, перила, рельсы серого дерева, антенна,
# стол для пикника, край леса — 6 из 23 материалов ATST+BLMD). Вблизи смешивание двоичной альфы = альфа-тест, а на
# мелких мипах (PSP выбирает уровень по наклону, лестница-трамплин видна вдоль) альфа усредняется (у лестницы 62%)
# и предмет становится полупрозрачным призраком, а без записи глубины ещё и перекрывается соседним прозрачным.
# Правило: такой материал рисуется вырезкой — флаг 1 (альфа-тест, запись глубины, в общем проходе), мипы с сохранением
# покрытия (texq, как у листвы и заборов с одним альфа-тестом). Мягкие края (листва, заборы: промежуточной альфы 3–34%)
# не трогаем. Правило порта (в данных — только ATST/BLMD шейдера и картинка), сверить с эталоном PS2.
CUT_MID = 0.02
_cut = {}
def binary_alpha(url):
    if not url: return False
    if url not in _cut:
        a = np.asarray(Image.open(ASSETS / url).convert('RGBA'))[..., 3]
        _cut[url] = bool((a <= 8).any()) and float(((a > 8) & (a < 247)).mean()) < CUT_MID
    return _cut[url]

# Освещаемые предметы мира (29.09, рецепт §12): меш с шейдером LIT 1 и без цвета вершин (конвертер даёт белый) в оригинале
# светит группа света уровня, как машину и персонажей (на диске: забор пристани l3z5/l6z5, пикап l5r4, дверь-шеврон гонок
# L6/L7; на уровне 1 таких нет). WORLD_LIGHT — свет уровня (tools/level_light.level_light), его ставит main(); None — как раньше
WORLD_LIGHT = None
LIT_STATS = [0]   # треугольников, освещённых светом уровня (сводка экспорта)

# №679 (Роб 17.10, режим «мультик»): вариант данных окружения. SHR_TOON=1: текстура материала заменяется однотонной заливкой её среднего цвета (альфа
# вырезок остаётся), кроме материалов, где рисунок несёт смысл (TOON_KEEP_RE: вывески, окна, стекло, двери, логотипы, надписи, плакаты, разметка дорог),
# полупрозрачных/аддитивных (свет, частицы, небо); цвет вершин (запечённый свет) — две ступени: свет и одна тень. Без переменной данные побайтно прежние.
TOON = os.environ.get('SHR_TOON') == '1'
TOON_KEEP_RE = re.compile(r'sign|win|glass|door|logo|letter|poster|billboard|flag|stop|crosswalk|stripe|marking|plate|neon|clock|screen|menu|ad_|kwik|krust|burger|lard|face|head|eye|char_|paint|frame|sky|glow|glare|light|lamp|coin|spark|smoke|cloud|water|river|ocean|wave|shadow|decal|skid|arrow|hud|icon|map|sdewlk_corner|rd[0-9]*_(crosswalk|striped|parking)', re.I)
TOON_FLAT_RE = re.compile(r'grass|asphalt|sidewlk|sdewlk|concrete|siding|stucco|dirt|path|field|bark|tree|oak|cypress|shrub|hedge|bush|roof|shingle|cobble|driveway|gravel|sand|rock|gry_|ditch|hay|plank|wood|deska|metal', re.I)
TOON_STD_MAX = 26.0   # рисунок с малым разбросом цвета — заливка; с большим (окна и двери в текстуре стены, надписи) — остаётся
_TOON_STD = {}
def toon_std(url):
    """разброс цвета текстуры (по альфе, 32x32, 0..255): мало — однотонная, много — на ней нарисовано что-то осмысленное"""
    if url in _TOON_STD: return _TOON_STD[url]
    try:
        a = np.asarray(Image.open(ASSETS / url).convert('RGBA').resize((32, 32)), np.float64); al = a[..., 3] / 255.0
        m = (a[..., :3] * (al[..., None] + 1e-6)).sum((0, 1)) / (al + 1e-6).sum()
        v = float(np.sqrt((((a[..., :3] - m) ** 2).sum(2) * al).sum() / max(al.sum(), 1e-6) / 3.0))
    except Exception: v = 99.0
    _TOON_STD[url] = v; return v
# №679: контур — выпуклые рёбра (> TOON_EDGE_DEG) и нижние/верхние границы стен КРУПНЫХ предметов (здания и т.п.), не кустов, заборов, земли и бордюров;
# линия вынесена наружу по нормали грани на TOON_EDGE_OUT м (не лежит на самой грани); запись TLN2 по клеткам TOON_CELL м, вершины int16 в сантиметрах от угла клетки
TOON_EDGE_DEG, TOON_EDGE_MIN, TOON_EDGE_OUT, TOON_CELL = 60.0, 1.0, 0.025, 64.0
TOON_NOT_BIG_RE = re.compile(r'shrub|hedge|bush|tree|leaf|grass|fence|rail|wire|pole|lamp|light|sign|sidew|sdewlk|curb|asphalt|road|path|field|dirt|water|river|ocean|sky|horizon|cloud|glow|shadow|decal|wheel|car|char', re.I)
TOON_LINES = []   # (x1,y1,z1,x2,y2,z2) мир
# №897 (Роб 23.10: «L1 J3-2: четыре тонкие вертикальные линии уходят в небо, как тросы»): контур собирался с КАЖДОГО вызова pack_prims, в том числе у мешей, которые в город
# не идут: стейтпропы и анимированные объекты (SPR1: у L1 ветряная мельница windmilShape0, башня 19 м), типы пропов (PRP1), монета. Их меши лежат в осях объекта/типа,
# экземпляр ставит движок — линии башни повисали в точке (159, −486), где башни нет (рёбра пирамиды 4,5 м у основания, 19 м вверх). Правило: контур — только с мешей города
# (вызов в цикле сцен main). SHR_TOON_ALL_OLD=1 — как было (красный контроль, tests/toon/ghost_lines.py)
TOON_ALL_OLD = os.environ.get('SHR_TOON_ALL_OLD') == '1'
_TOON_CITY = [False]
def toon_collect_edges(Pw, obj, prim, mat):
    T = np.asarray(Pw, np.float64).reshape(-1, 3, 3)
    if len(T) < 8 or TOON_NOT_BIG_RE.search(prim['shader'] + ' ' + obj['name'] + ' ' + obj['mesh']): return
    lo = T.reshape(-1, 3).min(0); hi = T.reshape(-1, 3).max(0); ext = hi - lo
    if ext[1] < 3.0 or max(ext[0], ext[2]) < 4.0: return   # не здание и не крупный предмет
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); ln = np.linalg.norm(n, axis=1); ok = ln > 1e-9; n[ok] /= ln[ok, None]
    if (np.abs(n[ok, 1]) < 0.35).mean() < 0.25: return   # мало стен (земля, крыша-плоскость)
    key = lambda p_: tuple(np.round(p_ * 50).astype(np.int64))
    edges = {}
    for i, t in enumerate(T):
        if not ok[i]: continue
        for a, b in ((0, 1), (1, 2), (2, 0)):
            ka, kb = key(t[a]), key(t[b])
            if ka == kb: continue
            edges.setdefault((ka, kb) if ka < kb else (kb, ka), []).append((i, a, b))
    cs = np.cos(np.radians(TOON_EDGE_DEG))
    for e, tr in edges.items():
        pa, pb = np.array(e[0]) / 50.0, np.array(e[1]) / 50.0
        if np.linalg.norm(pa - pb) < TOON_EDGE_MIN: continue
        if len(tr) >= 2:
            i1, i2 = tr[0][0], tr[1][0]
            if float(np.dot(n[i1], n[i2])) >= cs: continue
            # выпуклость: третья вершина второй грани за плоскостью первой
            v3 = T[i2][[k for k in range(3) if k not in (tr[1][1], tr[1][2])][0]]
            if float(np.dot(n[i1], v3 - pa)) >= -1e-4: continue
            off = n[i1] + n[i2]; off /= max(np.linalg.norm(off), 1e-9)
        elif len(tr) == 1:   # граница: только низ и верх меша у стены (не швы материалов)
            i1 = tr[0][0]
            if abs(n[i1][1]) > 0.35: continue
            ym = (pa[1] + pb[1]) / 2
            if abs(pa[1] - pb[1]) > 0.3 or not (ym <= lo[1] + 0.4 or ym >= hi[1] - 0.4): continue
            off = n[i1]
        else: continue
        TOON_LINES.append(np.r_[pa + off * TOON_EDGE_OUT, pb + off * TOON_EDGE_OUT])

def write_toon_lines(path):
    """TOON_LINES -> TLN2: 'TLN2', u32 ncell, u32 nline, ячейки (i16 cx, i16 cz, u32 первая, u32 число), затем линии 6 x i16 (см от угла клетки (cx*CELL, 0, cz*CELL))"""
    L = np.array(TOON_LINES, np.float64).reshape(-1, 6) if TOON_LINES else np.zeros((0, 6))
    mid = (L[:, :3] + L[:, 3:]) / 2
    cx = np.floor(mid[:, 0] / TOON_CELL).astype(int); cz = np.floor(mid[:, 2] / TOON_CELL).astype(int)
    order = np.lexsort((cz, cx)); L, cx, cz = L[order], cx[order], cz[order]
    cells = []; i = 0
    while i < len(L):
        j = i
        while j < len(L) and cx[j] == cx[i] and cz[j] == cz[i]: j += 1
        cells.append((int(cx[i]), int(cz[i]), i, j - i)); i = j
    q = np.zeros((len(L), 6), np.int16)
    for (a, b, f, c_) in cells:
        o = np.array([a * TOON_CELL, 0.0, b * TOON_CELL, a * TOON_CELL, 0.0, b * TOON_CELL])
        q[f:f + c_] = np.clip(np.round((L[f:f + c_] - o) * 100), -32767, 32767).astype(np.int16)
    out = b'TLN2' + struct.pack('<II', len(cells), len(L))
    for a, b, f, c_ in cells: out += struct.pack('<hhII', a, b, f, c_)
    Path(path).write_bytes(out + q.tobytes())
    print(f'контур мультика: {len(L)} линий в {len(cells)} клетках {TOON_CELL:g} м, {len(out) + q.nbytes:,} Б'.replace(',', ' '), flush=True)

TOON_LO, TOON_THR = 0.70, 0.80
TOON_SUN = (0.40, 0.80, 0.45)   # на солнце (viewer/skin.h SKIN_SUN_DIR_*): грань смотрит к солнцу при n·sun >= TOON_SUN_MIN
TOON_SUN_MIN, TOON_SHADOW_L, TOON_LIT_RGB = 0.10, 0.60, 235.0
def toon_shade(C, P):
    """цвет вершин (ABGR u32) и мировые вершины P (N*3, 3) -> одна ступень тени (№679, ресёрч 4.2): по треугольнику, свет = грань к солнцу И запечённая яркость не ниже
    TOON_SHADOW_L от «освещено» (падающая тень остаётся); цвет — оттенок запечки, яркость TOON_LIT_RGB или TOON_LO от неё"""
    c = np.asarray(C, np.uint32).reshape(-1, 3); T = np.asarray(P, np.float64).reshape(-1, 3, 3)
    r = (c & 255).astype(np.float64); g = ((c >> 8) & 255).astype(np.float64); b = ((c >> 16) & 255).astype(np.float64)
    m = np.stack([r.mean(1), g.mean(1), b.mean(1)], 1); l = m.mean(1)
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); n /= np.maximum(np.linalg.norm(n, axis=1), 1e-12)[:, None]
    lit = ((n @ np.array(TOON_SUN)) >= TOON_SUN_MIN) & (l >= TOON_SHADOW_L * TOON_LIT_RGB)
    tint = m / np.maximum(l, 4.0)[:, None]; k = np.where(lit, 1.0, TOON_LO) * TOON_LIT_RGB
    rgb = np.clip(tint * k[:, None], 0, 255).astype(np.uint32)
    out = np.empty(c.shape, np.uint32)
    for v in range(3): out[:, v] = (c[:, v] & 0xFF000000) | (rgb[:, 2] << 16) | (rgb[:, 1] << 8) | rgb[:, 0]
    return out.reshape(-1)

def pack_prims(meta, raw, conv_cache, blob, want=lambda o: True):
    """-> список (tex, flags, P, UV, C) в координатах объекта*матрица"""
    out = []
    for obj in meta['objects']:
        if not want(obj) or obj['mesh'] in JUNK_MESHES: continue
        M = np.array(obj['matrix'], np.float64).reshape(4, 4)
        for prim in meta['meshes'].get(obj['mesh'], []):
            at = prim['attributes']
            g = lambda k, w, t='<f4': np.frombuffer(raw, t, at[k][1], at[k][0]).reshape(-1, w) if w > 1 else np.frombuffer(raw, t, at[k][1], at[k][0])
            pos, uv, col, idx = g('position', 3), g('uv', 2), g('color', 3), g('indices', 1, '<u4')
            world = (np.c_[pos, np.ones(len(pos))] @ M)[:, :3]
            mat = meta['materials'].get(prim['shader'], {})
            flags = (1 if mat.get('alpha') else 0) | (2 if mat.get('translucent') or mat.get('blend') else 0)
            if flags == 3 and os.environ.get('SHR_CUT_BLEND', '1') != '0' and binary_alpha(mat.get('textureUrl', '')) and \
                    ('alpha' not in at or g('alpha', 1).min() >= 250 / 255):
                flags = 1   # вырезка со смешиванием -> вырезка (см. binary_alpha); SHR_CUT_BLEND=0 — как раньше (сравнение)
            # №447: BLMD=2 is additive, not ordinary transparency. Bit 24 survives SPR1 joint/slot packing.
            if mat.get('blend') == 2: flags = (flags | 2 | (1 << 24))
            # полупрозрачному материалу — мипы без масштаба покрытия альфа-теста (texq.load_tex blend)
            if obj['name'].lower().startswith(SKY_NAMES) and mat.get('textureUrl') and not SKY_SEAM_OLD:   # №511: слой неба — цвет под прозрачным
                SKY_TEX.setdefault(mat['textureUrl'], 1 if flags & 2 else 65)
            toon_ = TOON and mat.get('textureUrl') and not (flags & 2) and not obj['name'].lower().startswith(SKY_NAMES) and not TOON_KEEP_RE.search(prim['shader'] + ' ' + obj['name']) and (TOON_FLAT_RE.search(prim['shader'] + ' ' + obj['name']) or toon_std(mat['textureUrl']) < TOON_STD_MAX)
            t = load_tex(mat.get('textureUrl', ''), conv_cache, blob, blend=bool(flags & 2), sky=obj['name'].lower().startswith(SKY_NAMES), flat=bool(toon_))
            c = np.clip(col * 255, 0, 255).astype(np.uint32)
            # альфа вершин (PS2) — только у полупрозрачных материалов: стекло Krusty/Buzz Cola — 40%, как в оригинале
            A = np.clip(g('alpha', 1) * 255 + .5, 0, 255).astype(np.uint32) if (flags & 2) and 'alpha' in at else 0xFF
            abgr = (A << 24) | (c[:, 2] << 16) | (c[:, 1] << 8) | c[:, 0]
            if WORLD_LIGHT is not None and mat.get('lit') and bool((col >= 1.0).all()):   # LIT без цвета вершин — свет уровня
                Ai = (np.asarray(A, np.uint32)[idx] if np.ndim(A) else np.full(len(idx), A, np.uint32))
                Cv = (Ai << 24) | (level_light.world_lit_colours(world[idx], WORLD_LIGHT) & 0xFFFFFF)
                LIT_STATS[0] += len(idx) // 3
            else: Cv = abgr[idx]
            if toon_: Cv = toon_shade(Cv, world[idx])   # №679: одна ступень тени (грань к солнцу + запечённая тень)
            if TOON and (TOON_ALL_OLD or _TOON_CITY[0]) and not (flags & 2) and not obj['name'].lower().startswith(SKY_NAMES): toon_collect_edges(world[idx], obj, prim, mat)   # №679: контур зданий и крупных предметов
            out.append((t, flags, world[idx].astype(np.float32), uv[idx].astype(np.float32), Cv.astype(np.uint32)))
    return out


# №674 (диагональные вспышки, 17.10): веер юбки шва (skirts.skirt_soup) тянется из нижнего начала цепочки ко всем её вершинам — треугольники до 47 м и тоньше 0,5 м.
# На низкой ближней плоскости вершина такого треугольника уходит за ±2048 px, GE выбрасывает его целиком или рисует клин на пол-экрана (ближние версии: на L1 271 такой треугольник).
# SHR_SPLIT_LONG=1 — ближние юбки режутся до ребра SHR_SPLIT_LONG_M (12 м) тем же срединным правилом, что и город (subdivide_lod; общее ребро режется одинаково у обоих соседей), площадь/UV/цвет сохраняются.
SPLIT_LONG = os.environ.get('SHR_SPLIT_LONG') == '1'; SPLIT_LONG_M = float(os.environ.get('SHR_SPLIT_LONG_M', 12.0))
def split_soup(v, maxe=None):
    if not len(v): return v
    maxe = SPLIT_LONG_M if maxe is None else maxe
    P = np.stack([v['x'], v['y'], v['z']], 1).astype(np.float64); UV = np.stack([v['u'], v['v']], 1).astype(np.float64)
    P2, UV2, C2 = subdivide_lod(P.reshape(-1, 3), UV.reshape(-1, 2), v['c'].astype(np.uint32), maxe, maxe)
    out = np.zeros(len(P2), dtype=v.dtype)
    out['u'], out['v'], out['c'] = UV2[:, 0], UV2[:, 1], C2
    out['x'], out['y'], out['z'] = P2[:, 0], P2[:, 1], P2[:, 2]
    return out

def verts(P, UV, C):
    v = np.zeros(len(P), dtype=VT)
    v['u'], v['v'], v['c'] = UV[:, 0], UV[:, 1], C
    v['x'], v['y'], v['z'] = P[:, 0], P[:, 1], P[:, 2]
    return v

def heightmap(tris):
    lo = tris.reshape(-1, 3).min(0); hi = tris.reshape(-1, 3).max(0)
    W, H = int((hi[0] - lo[0]) / CELL) + 2, int((hi[2] - lo[2]) / CELL) + 2
    hm = np.full((H, W, LAYERS), -32768, np.int16)
    n = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    ln = np.linalg.norm(n, axis=1); ok = ln > 1e-6
    tris, n = tris[ok], n[ok] / ln[ok, None]
    walk = np.abs(n[:, 1]) > 0.55
    for t in tris[walk]:
        x0, x1 = int((t[:, 0].min() - lo[0]) / CELL), int((t[:, 0].max() - lo[0]) / CELL)
        z0, z1 = int((t[:, 2].min() - lo[2]) / CELL), int((t[:, 2].max() - lo[2]) / CELL)
        xs, zs = np.meshgrid(np.arange(x0, x1 + 1), np.arange(z0, z1 + 1))
        px, pz = lo[0] + (xs + .5) * CELL, lo[2] + (zs + .5) * CELL
        (ax, ay, az), (bx, by, bz), (cx, cy, cz) = t
        d = (bz - cz) * (ax - cx) + (cx - bx) * (az - cz)
        if abs(d) < 1e-9: continue
        w1 = ((bz - cz) * (px - cx) + (cx - bx) * (pz - cz)) / d
        w2 = ((cz - az) * (px - cx) + (ax - cx) * (pz - cz)) / d
        w3 = 1 - w1 - w2
        inside = (w1 >= -.05) & (w2 >= -.05) & (w3 >= -.05)
        if not inside.any(): continue
        hy = (w1 * ay + w2 * by + w3 * cy)[inside]
        for zz, xx, yy in zip(zs[inside], xs[inside], hy):
            cm = int(np.clip(yy * 100, -32000, 32000)); cell = hm[zz, xx]
            if (np.abs(cell.astype(np.int32) - cm) < 40).any(): continue
            free = np.where(cell == -32768)[0]
            if len(free): cell[free[0]] = cm
            else:
                j = cell.argmin()
                if cm > cell[j]: cell[j] = cm
    # стены: крутые треугольники коллизии -> по клетке диапазон высот, который нельзя проехать
    wall = np.zeros((H, W, 2), np.int16); wall[..., 0] = 32767; wall[..., 1] = -32767
    steep = tris[~walk]
    for t in steep:
        e = max(np.linalg.norm(t[1] - t[0]), np.linalg.norm(t[2] - t[1]), np.linalg.norm(t[0] - t[2]))
        n = int(min(80, max(1, e / (CELL * .5))))
        i, j = np.meshgrid(np.arange(n + 1), np.arange(n + 1)); m = i + j <= n
        a, b = i[m] / n, j[m] / n; pts = t[0] + a[:, None] * (t[1] - t[0]) + b[:, None] * (t[2] - t[0])
        gx = ((pts[:, 0] - lo[0]) / CELL).astype(int); gz = ((pts[:, 2] - lo[2]) / CELL).astype(int)
        ok2 = (gx >= 0) & (gz >= 0) & (gx < W) & (gz < H)
        gx, gz, yy = gx[ok2], gz[ok2], np.clip(pts[ok2, 1] * 100, -32000, 32000).astype(np.int16)
        np.minimum.at(wall[..., 0], (gz, gx), yy); np.maximum.at(wall[..., 1], (gz, gx), yy)
    return lo, W, H, hm, wall

# ---------------------------------------------------------------------------------------------------------------
# Коллизия: секция COL1 (после блобов клеток, до PVS; смещение в заголовке SHR7 @92). Всё little endian, float32.
#   ColHdr  48 B: "COL1", ox, oz, cell, W, H, ntri, nbox, ncyl, nseg, nref, flags
#           flags (с 29.09) = смещение блока JMP1 от начала секции (зоны прыжка, подкидывающие точки; tools/jumps_export.py), 0 — нет
#   ColTri  48 B: a[3] b[3] c[3] n[3]      земля (Intersect 0x3f00003); n = unit(cross(b-a, c-a)), n.y >= 0
#   ColBox  64 B: c[3] ax[3] ay[3] az[3] h[3] pad   ориентированный ящик (оси единичные, правая тройка)
#   ColCyl  48 B: c[3] axis[3] r halfLen flat pad[3]  flat=1 цилиндр, 0 капсула; сфера = капсула с halfLen 0
#   ColBox.pad / ColCyl.pad[0]: float material, 0 hard, 1 finite foliage (same family policy on every level), 2 phantom (ColCyl only, правка 414).
#   ColSeg  32 B: a[2] b[2] n[2] y0 y1     забор (x,z), n — в сторону проезжей части, y0/y1 = -1e6/+1e6
#   u32 cellStart[W*H+1]; u32 refs[nref] = (type << 29) | index, type 0 tri 1 box 2 cyl 3 seg
# Массивы идут подряд в этом порядке без промежутков; конец секции выровнен на 16.
# Сетка 4 м от (ox, oz), клетка gz*W+gx. Примитив записан во все клетки, которые задевает его xz-габарит
# (у забора — все клетки в пределах 0.5 м от отрезка).
COL_CELL = 4.0
COL_SEG_MARGIN = 0.5
COL_FENCE_Y = 1e6
COL_MIN_2A, COL_MIN_H = 1e-4, 1e-3   # вырожденный треугольник: удвоенная площадь < 1e-4 м² или высота < 1 мм
COL_VERT_NY = 1e-6                   # |n.y| меньше — стена строго вертикальна: сторону берём из нормали диска
MIRROR = np.diag([1.0, 1.0, -1.0, 1.0])
# StaticPhys — здания/стены/заборы-объекты в мировых координатах; InstaStatPhys — большие деревья, живые изгороди,
# колонны, столбы и т.п.: объём задан в своих координатах и ставится матрицей каждого экземпляра (как их меш).
# Одиночный CollisionObject верхнего уровня (l1r6_rotating_sign, флаг static=1) — тоже в мировых.
COL_STATIC = {0x3f00001: 'StaticPhys', 0x3f0000a: 'InstaStatPhys', 0x7010000: 'CollisionObject'}
# сбиваемое и подвижное — не в COL1 (этап 5): фонари, мелкие деревья, гидранты, почтовые ящики, знаки (DynaPhys),
# будки, ящики, торговые автоматы, кусты-шары (AnimDynaPhys), платформы/вентиляторы/двери/люки (AnimCollision)
COL_DYNAMIC = {0x3f00002: 'DynaPhys', 0x3f0000e: 'AnimDynaPhys', 0x3f00008: 'AnimCollision'}

def col_instances(entity):
    """Матрицы (уже отзеркаленные по z, как у меша в convert.py) всех экземпляров сущности из её InstanceList."""
    out = []
    def rec(c, m):
        if c.id == 0x120103:
            _, p = string(c.data); m = np.frombuffer(c.data, '<f4', 16, p + 4).reshape(4, 4).astype(np.float64) @ m
        if c.id == 0x120107: out.append(MIRROR @ m @ MIRROR)
        for s in c.children: rec(s, m)
    for c in entity.children:
        if c.id == 0x3000008: rec(c, np.eye(4))
    return out

def col_place(shape, M):
    """Лист CollisionVolume (dict из interiors.leaves, уже отзеркален) -> ('box'|'cyl', 16 или 12 float) в мире."""
    R = M[:3, :3]
    c = np.append(np.asarray(shape['center'], np.float64), 1.0) @ M
    if shape['kind'] == 'box':
        A = np.asarray(shape['axes'], np.float64) @ R; s = np.linalg.norm(A, axis=1)
        h = np.asarray(shape['halfExtents'], np.float64) * s; A = A / s[:, None]
        ax = A[0]; ay = A[1] - ax * (ax @ A[1]); ay /= np.linalg.norm(ay); az = np.cross(ax, ay)   # правая тройка
        return 'box', [*c[:3], *ax, *ay, *az, *h, 0.0], float(np.abs(np.abs(A @ A.T) - np.eye(3)).max())
    if shape['kind'] == 'cylinder':
        a = np.asarray(shape['axis'], np.float64) @ R; s = np.linalg.norm(a); a /= s
        u = np.cross(a, [1.0, 0, 0] if abs(a[0]) < .9 else [0, 1.0, 0]); u /= np.linalg.norm(u)
        rs = max(np.linalg.norm(u @ R), np.linalg.norm(np.cross(a, u) @ R))
        if a[1] < 0: a = -a   # направление оси не важно; вверх — удобнее смотреть
        return 'cyl', [*c[:3], *a, shape['radius'] * rs, shape['length'] * s, float(shape['flatEnds']), 0, 0, 0], 0.0
    rs = max(np.linalg.norm(R, axis=0))
    return 'cyl', [*c[:3], 0.0, 1.0, 0.0, shape['radius'] * rs, 0.0, 0.0, 0, 0, 0], 0.0

# ---------------------------------------------------------------------------------------------------------------
# Пропы (этап 3б): секция PRP1 — сбиваемые DynaPhys (0x3f00002): гидранты, ящики, знаки, мелкие деревья, фонари.
# Атрибут 0x3000600 на сущности = (cls, physProp, звук); cls==2 — статика (данные сами говорят «не трогать»,
# остаётся в городских кусках как раньше). Масса — из ATC art/atc/atc.p3d (0x3000602, 63 строки: 3 строки
# (звук, частицы, обломок) + 3 числа; второе число — масса, сверено с docs/gameplay.md).
def atc_mass_table():
    root = read(GAME / 'art' / 'atc' / 'atc.p3d')
    c = next(x for x in walk(root) if x.id == 0x3000602)
    d = c.data; n, = struct.unpack_from('<I', d, 0); p = 4; mass = []
    for _ in range(n):
        _, p = string(d, p); _, p = string(d, p); _, p = string(d, p)
        f = struct.unpack_from('<3f', d, p); p += 12
        mass.append(f[1])
    return mass

# №314 (перепись миссий 16.10): AddCollectibleStateProp(имя, локатор, n) — предмет миссии модели art/missions/levelNN/<имя>.p3d (L7: bombbarrel, бочки
# m3/m5/m6/m7 на локаторах своих миссий, у шлюза АЭС). Модель — стейтпроп со скелетом и эффектами: тип SPR1 (как ключ/нитро, prp2_export.add_pickup),
# экземпляр в точке каждого локатора, СКРЫТЫЙ (состояние 0); движок (viewer/msn_view.c state_prop + main_loop.h) включает его на этапе миссии и
# прячет при подборе. SHR_MSNSPROP_OLD=1 — как раньше (нет в SPR1, предмет невидим). -> [(файл p3d, имя, [позиции порта])]
def mission_stateprops(level):
    if os.environ.get('SHR_MSNSPROP_OLD') == '1': return []
    import sys as _s
    _s.path.insert(0, str(Path(__file__).resolve().parent / 'missions'))
    from p3dlocs import read_locators
    want = {}
    for f in sorted((GAME / f'scripts/missions/level{level:02d}').glob('*.mfk')):
        if f.stem.startswith(('e3', 'demo', 'level-d')) or f.stem.endswith('-d'): continue   # демо и сборки E3 уровень не грузит
        for m in re.finditer(r'AddCollectibleStateProp\(\s*"([^"]+)"\s*,\s*"([^"]+)"', f.read_text(errors='ignore')):
            want.setdefault(m.group(1).lower(), [])
            if m.group(2).lower() not in want[m.group(1).lower()]: want[m.group(1).lower()].append(m.group(2).lower())
    if not want: return []
    locs = {}
    for p in sorted((GAME / 'art').glob(f'l{level}*.p3d')) + sorted((GAME / f'art/missions/level{level:02d}').glob('*.p3d')):
        for l in read_locators(p, p.name)[0]: locs.setdefault(l.name.lower(), l.pos)
    out = []
    for name, ln in want.items():
        f = GAME / f'art/missions/level{level:02d}/{name}.p3d'
        pts = [tuple(locs[x]) for x in ln if x in locs]
        if f.exists() and pts: out.append((f, name, pts))
    return out

def prop_entities(scenes, art):
    """Сканирует сцены уровня (сырые чанки, без Converter) -> {имя сущности: dict(scene, mat, mats=[4x4 мировых])}."""
    atc = atc_mass_table(); out = {}
    for scene in scenes:
        root = read(art / f'{scene}.p3d')
        for ent in root.children:
            if ent.id != 0x3f00002: continue
            name = string(ent.data)[0]
            if 'powerbox' in name.lower(): continue
            eff = next((c for c in ent.children if c.id == 0x3000600), None)
            cls, mat = struct.unpack_from('<II', eff.data) if eff else (2, 0)
            if cls == 2: continue   # данные говорят: статика — не трогаем
            mats = col_instances(ent)
            if not mats: continue
            row = out.setdefault(name, dict(scene=scene, mat=mat, mass=atc[mat] if mat < len(atc) else 100.0, mats=[]))
            row['mats'].extend(mats)
    return out

def quat_from_R(m):
    """3x3 (мир = m @ локальный столбец) -> кватернион (w, x, y, z), стандартный алгоритм Шеппарда."""
    tr = m[0, 0] + m[1, 1] + m[2, 2]
    if tr > 0:
        s = np.sqrt(tr + 1.0) * 2
        w, x, y, z = 0.25 * s, (m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        s = np.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2]) * 2
        w, x, y, z = (m[2, 1] - m[1, 2]) / s, 0.25 * s, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s
    elif m[1, 1] > m[2, 2]:
        s = np.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2]) * 2
        w, x, y, z = (m[0, 2] - m[2, 0]) / s, (m[0, 1] + m[1, 0]) / s, 0.25 * s, (m[1, 2] + m[2, 1]) / s
    else:
        s = np.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1]) * 2
        w, x, y, z = (m[1, 0] - m[0, 1]) / s, (m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, 0.25 * s
    n = np.sqrt(w * w + x * x + y * y + z * z) or 1.0
    return w / n, x / n, y / n, z / n

def prop_section(prop_types, prop_pieces_raw, tmap, coin_type=-1, coin_pos=()):
    """-> bytes секции PRP1: общие вершины/индексы (формат кусков города) + таблицы кусков/типов/экземпляров."""
    QV = np.dtype([('u', '<i2'), ('v', '<i2'), ('c', '<u4'), ('x', '<i2'), ('y', '<i2'), ('z', '<i2'), ('pad', '<i2')])
    vblob, iblob, precs, first_of = bytearray(), bytearray(), [], []
    cur = -1
    for ti, t, fl, P, UV, C in prop_pieces_raw:
        if ti != cur:
            first_of.append(len(precs)); cur = ti
        lo_, hi_ = P.min(0), P.max(0); c = (lo_ + hi_) / 2; S = max(float((hi_ - lo_).max()) / 2, 1e-3)
        q = np.zeros(len(P), QV)
        q['x'], q['y'], q['z'] = [np.clip(np.round((P[:, j] - c[j]) / S * 32767), -32767, 32767) for j in range(3)]
        q['u'], q['v'] = uvq.tri_i16(UV[:, 0], UV[:, 1])   # GE читает u16: целый сдвиг на треугольник (tools/uvq.py, правка Роба 43)
        q['c'] = C
        # без индексов (шаг 3 аудита): вершины подряд, nv = ni, iOff = 0; флаг 16 — без индексов, 8 — двусторонний
        vOff = len(vblob); vblob += q.tobytes(); vblob += b'\0' * (-len(vblob) % 16)
        precs.append((c[0], c[1], c[2], S * 32768 / 32767, tmap.get(t, -1), fl | F_DOUBLE | F_NOIDX, len(q), len(q), vOff, 0))
    first_of.append(len(precs))
    trecs = bytearray()
    for i, ty in enumerate(prop_types):
        first, n = first_of[i], first_of[i + 1] - first_of[i]
        trecs += struct.pack('<3f2I', ty['radius'], ty['halfH'], ty['mass'], first, n)
    irecs = bytearray()
    for ty in prop_types:
        for M in ty['mats']:
            x, y, z = M[3, 0], M[3, 1], M[3, 2]
            w, qx, qy, qz = quat_from_R(M[:3, :3].T)
            irecs += struct.pack('<3f4fI', x, y, z, w, qx, qy, qz, ty['index'])
    prec_bytes = bytearray()
    for ox, oy, oz, sc, tex, fl, nv, ni, vOff, iOff in precs:
        prec_bytes += struct.pack('<4f2i4I', ox, oy, oz, sc, tex, fl, nv, ni, vOff, iOff)
    head = b'PRP1' + struct.pack('<5I', len(precs), len(prop_types), sum(len(t['mats']) for t in prop_types), len(vblob), len(iblob))
    head += struct.pack('<iI', coin_type, len(coin_pos))   # @24 тип-меш монеты (-1 нет), @28 число монет
    crecs = b''.join(struct.pack('<3f', *p_) for p_ in coin_pos)   # монеты: x,y,z после экземпляров
    return bytes(head) + bytes(vblob) + bytes(iblob) + bytes(prec_bytes) + bytes(trecs) + bytes(irecs) + crecs

def atc_rows():
    """Строки ATC art/atc/atc.p3d (0x3000602): [(звук, частицы, обломок, (f0, f1, f2))] по номеру physProp."""
    root = read(GAME / 'art' / 'atc' / 'atc.p3d')
    c = next(x for x in walk(root) if x.id == 0x3000602)
    d = c.data; n, = struct.unpack_from('<I', d, 0); p = 4; rows = []
    for _ in range(n):
        a, p = string(d, p); b, p = string(d, p); e, p = string(d, p)
        rows.append((a, b, e, struct.unpack_from('<3f', d, p))); p += 12
    return rows

def col_material(ent, atc):
    """Материал объёма по данным (правка Роба 71, 01.10): куст FOLIAGE — у сущности CollisionEffect 0x3000600, чья строка
    ATC (physProp) даёт частицы eShrub (строка 10: smash / eShrub, звук hit_shrub — изгороди l*_hedgeunit, l2/l5_shrubsml,
    кусты-шары l*_shrubround). Имя сущности не решает: «bushesShape*», «BushsHouse*» на L1 — обычная статика мира (cls 7,
    physProp 0, nosound), как стены домов."""
    eff = next((c for c in walk(ent) if c.id == 0x3000600), None)
    if eff is None or len(eff.data) < 8: return 0.0
    _, phys = struct.unpack_from('<II', eff.data)
    return 1.0 if phys < len(atc) and atc[phys][1] == 'eShrub' else 0.0

def col_skeleton_frame(root, name, shapes):
    """Правка Роба 357 (L3 H7-7: «на дороге прозрачный блок»). Одиночный CollisionObject верхнего уровня (0x7010000) со скелетом того
    же имени (вращающиеся вывески, l3r5/l6r5_beefboard, wiggum, firefly, DB_*, прожекторы L7): листья — у владельца-сустава 0. Данные
    авторов двух видов: у большинства листья уже в мировых (wiggum: лист в 9 м от корня скелета), у beefboard — в осях сустава 0
    (лист (4,6; 8,9; 0) при корне (135,5; −2,1; 86,6); рисунок табло висит на суставах того же скелета). Кадр выбирается по данным:
    тот, в котором центр листьев ближе к корню своего скелета (якорь рисунка). Нет скелета — мировые, как было.
    SHR_ANIMCOL_FRAME_OLD=1 — прежнее (всегда мировые). -> матрица (уже в осях порта, как у col_instances)"""
    from car_dump import skeleton
    sk = next((c for c in root.children if c.id == 0x4500 and string(c.data)[0] == name), None)
    if sk is None or not shapes: return np.eye(4)
    J = skeleton(sk)
    if not J: return np.eye(4)
    M = MIRROR @ J[0][3] @ MIRROR
    t = M[3, :3]
    c = np.mean([np.asarray(s_['center'], np.float64) for s_ in shapes], 0)
    cw = (np.append(c, 1.0) @ M)[:3]
    return M if np.linalg.norm(cw - t) < np.linalg.norm(c - t) else np.eye(4)

def col_collect(roots):
    """roots: [(имя сцены, корень p3d)] -> tris (N,4,3), boxes (N,16), cyls (N,12), segs (N,8), отчёт"""
    tris, nrm, boxes, cyls, segs = [], [], [], [], []
    atc = atc_rows()
    rep = dict(static={}, skipped={}, errors=[], ortho=0.0, flat_cyl=0, spheres=0, zero_box=0, foliage=[], boxnames=[], cylnames=[])
    for scene, root in roots:
        for c in walk(root):
            if c.id == 0x3f00003:   # земля: индексы, вершины, нормали по треугольникам (+ список типов поверхности)
                ni, = struct.unpack_from('<I', c.data); idx = np.frombuffer(c.data, '<u4', ni, 4); p = 4 + 4 * ni
                nv, = struct.unpack_from('<I', c.data, p); pos = np.frombuffer(c.data, '<f4', 3 * nv, p + 4).reshape(-1, 3); p += 4 + 12 * nv
                nn, = struct.unpack_from('<I', c.data, p)
                fn = np.frombuffer(c.data, '<f4', 3 * nn, p + 4).reshape(-1, 3) if nn * 3 == ni else np.zeros((ni // 3, 3), np.float32)
                tris.append(pos[idx[:ni // 3 * 3]].reshape(-1, 3, 3)); nrm.append(fn[:ni // 3])
            elif c.id == 0x3000000 and (scene.endswith('_terra') or (TRACK_FENCES and re.fullmatch(r'b0\d', scene))):   # забор: начало, конец, нормаль
                segs.append(np.frombuffer(c.data, '<f4', 9).reshape(3, 3))
        for ent in root.children:
            if ent.id not in COL_STATIC and ent.id not in COL_DYNAMIC: continue
            name = string(ent.data)[0]
            objs = [ent] if ent.id == 0x7010000 else [o for o in walk(ent) if o.id == 0x7010000]
            shapes = []
            try:
                for o in objs:
                    for v in o.children:
                        if v.id == 0x7010001: shapes.extend(col_leaves(v, name))
            except ValueError as e:
                rep['errors'].append(f'{scene}/{name}: {e}'); continue
            if not shapes: continue
            if ent.id in COL_DYNAMIC:   # считаем расставленные экземпляры (AnimCollision стоит сам, без списка)
                d = rep['skipped'].setdefault(COL_DYNAMIC[ent.id], {})
                k = re.sub(r'\d+', '#', name); d[k] = d.get(k, 0) + (len(col_instances(ent)) if ent.id != 0x3f00008 else 1)
                continue
            mats = col_instances(ent) if ent.id == 0x3f0000a else [np.eye(4)]
            if ent.id == 0x7010000 and os.environ.get('SHR_ANIMCOL_FRAME_OLD') != '1':
                mats = [col_skeleton_frame(root, name, shapes)]   # правка 357: объём одиночного CollisionObject — в кадре его скелета
            kind = COL_STATIC[ent.id]; st = rep['static'].setdefault(kind, [0, 0, 0])
            st[0] += len(mats)
            mat = col_material(ent, atc)
            for M in mats:
                for s in shapes:
                    k, rec, err = col_place(s, M); rep['ortho'] = max(rep['ortho'], err)
                    # куст — по physProp из данных (col_material), не по имени; заборы мира остаются твёрдыми
                    if mat == 1.0:
                        rec[15 if k == 'box' else 9] = 1.0  # COL1 reserved float: material FOLIAGE
                        rep['foliage'].append(dict(scene=scene, name=name, kind=k,
                                                   index=len(boxes) if k == 'box' else len(cyls)))
                    if k == 'box':
                        boxes.append(rec); st[1] += 1; rep['zero_box'] += min(rec[12:15]) <= 0; rep['boxnames'].append(f'{scene}/{name}')
                    else:
                        cyls.append(rec); st[2] += 1; rep['flat_cyl'] += rec[8] == 1; rep['spheres'] += s['kind'] == 'sphere'; rep['cylnames'].append(f'{scene}/{name}')
    T = np.concatenate(tris).astype(np.float64); T[..., 2] *= -1
    FN = np.concatenate(nrm).astype(np.float64); FN[:, 2] *= -1
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); a2 = np.linalg.norm(n, axis=1)
    emax = np.max([np.linalg.norm(T[:, (i + 1) % 3] - T[:, i], axis=1) for i in range(3)], 0)
    ok = (a2 >= COL_MIN_2A) & (a2 / np.maximum(emax, 1e-12) >= COL_MIN_H)
    rep['tri_src'], rep['tri_degenerate'] = len(T), int((~ok).sum())
    T, FN, n = T[ok], FN[ok], n[ok] / a2[ok, None]
    # одинаковые треугольники из соседних зон — один раз
    q = np.round(T * 1000).astype(np.int64)                                   # мм; вершины по порядку внутри треугольника
    order = np.lexsort((q[..., 2], q[..., 1], q[..., 0]), axis=-1)
    key = np.take_along_axis(q, order[..., None], axis=1).reshape(-1, 9)
    _, first = np.unique(key, axis=0, return_index=True); first.sort()
    rep['tri_duplicate'] = len(T) - len(first); T, FN, n = T[first], FN[first], n[first]
    # ориентация: n.y >= 0; строго вертикальные — в сторону нормали из файла (после зеркала)
    flip = np.where(np.abs(n[:, 1]) > COL_VERT_NY, n[:, 1] < 0, (n * FN).sum(1) < 0)
    T[flip] = T[flip][:, [0, 2, 1]]; n[flip] *= -1   # (зеркало z разворачивает обход почти у всех — это нормально)
    rep['tri_flipped'] = int(((n * FN).sum(1) < 0).sum())   # смотрят против нормали диска: нависающие (n.y<0 в игре)
    rep['tri_vertical'] = int((np.abs(n[:, 1]) <= COL_VERT_NY).sum())
    TR = np.concatenate([T, n[:, None]], 1)
    S = np.array(segs, np.float64).reshape(-1, 3, 3); S[..., 2] *= -1
    # нормали заборов: в файле всегда слева от направления (замкнутые петли вокруг непроезжих областей) и смотрят
    # из петли наружу — на дорогу. Оставляем как есть; проверка заливкой от спавна — tools/col_check.py
    SG = np.c_[S[:, 0][:, [0, 2]], S[:, 1][:, [0, 2]], S[:, 2][:, [0, 2]], np.full(len(S), -COL_FENCE_Y), np.full(len(S), COL_FENCE_Y)]
    return TR, np.array(boxes, np.float64).reshape(-1, 16), np.array(cyls, np.float64).reshape(-1, 12), SG, rep

# Правка Роба 454 (10.10, бонус-трассы b01–b07: «заборы проезжаются насквозь, и много прозрачных стен»): Wall 0x3000000 брались только из
# *_terra, у трасс (сцена b0N) 49–100 заборов на трассу не попадали в COL1 вовсе (SG: 0 звеньев у всех семи) — ограждения рисовались,
# а проезжались. Теперь заборы трассы идут тем же путём, что города (граница — за ней пустота, бесконечная; внутренний — до верха
# видимого, не выше опоры + FENCE_CAP), а внутренний забор трассы, над которым нигде нет нарисованного (невидимая стена посреди
# трассы), отбрасывается (TRACK_FENCE_KEEP_INVISIBLE). SHR_TRACK_FENCES_OLD=1 — прежнее (заборов трассы нет)
TRACK_FENCES = os.environ.get('SHR_TRACK_FENCES_OLD') != '1'
TRACK_FENCE_KEEP_INVISIBLE = os.environ.get('SHR_TRACK_FENCE_KEEP_INVISIBLE') == '1'
TRACK_FENCE_VIS = 0.3   # м: нарисованное выше опоры меньше этого — «ничего нет»
# Заборы Wall 0x3000000 (правка Роба 47 «часть домов — стена до неба»). В данных у забора нет высоты: все концы y = 0
# (двумерная линия, l1_terra: 1236 из 1236), до 29.09 порт ставил каждому -1e6..1e6. Семейство «стена выше крыши»
# (tools/cartest/tallwalls.py): линия участка проходит через дом/газон, машина с трамплина (зона прыжка, JMP1) летит над
# крышей и газоном — и упирается в пустоту. Правило порта (сверить с эталоном PS2, вопрос Робу), закон 3 очереди соблюдён:
#   граница карты — за забором (2 м по -n) нет опоры коллизии (пустота: край мира, кварталы без коллизии) — бесконечная, как было;
#   внутренний забор — за ним опора есть: забор режется на куски не длиннее FENCE_PIECE, верх куска = max(видимое в полосе
#   ±FENCE_BAND + FENCE_MARGIN, опора в полосе + FENCE_MIN). По земле и подскоком он держит как раньше; над всем
#   видимым вдоль него — не держит (машина в полёте над крышей/газоном проходит).
# FENCE_MIN 2.5 м над опорой: кузов 1.6 м + подскок; машина по земле и Гомер одним прыжком (1.9 м) не проходят
FENCE_PIECE, FENCE_BAND, FENCE_MIN, FENCE_MARGIN, FENCE_BEHIND = 2.0, 1.5, 2.5, 1.0, 2.0
# №905 (Роб 23.10, L1 H4-6: «прыжок с трамплина — машина врезалась в невидимое препятствие, слева только коричневая гора»): «граница карты» узнавалась только по пустоте опоры
# коллизии за забором (2 м по −n). У подножия обрыва горы опоры за забором нет (склон — не земля), и одно звено объявлялось краем мира: кусок с верхом +1e6 (L1: сегмент 1688,
# (76.6, −345.9)–(72.9, −346.3); оба соседних звена той же линии — внутренние, верх 3,0…3,5 м). Машина в полёте над ямой упиралась в стену до неба. Правило: «граница» между
# внутренними звеньями той же линии (концы сходятся в точках в 10 см, с обоих концов соседи только внутренние) — внутренняя; до FENCE_BRIDGE_RUN звеньев подряд. Настоящая
# граница карты (линия целиком пустая) прежняя. SHR_FENCE_BRIDGE_OLD=1 — как было (красный контроль, tools/cartest/airwall905.c)
FENCE_BRIDGE_OLD = os.environ.get('SHR_FENCE_BRIDGE_OLD') == '1'
FENCE_BRIDGE_RUN = 3
# Слово дирижёра 15.10 (класс «ударяюсь о прозрачное», №581; перепись tools/fence_vis_census.c: ~20 % длины заборов на каждом уровне без рисунка): внутренние заборы города, у которых
# подряд FENCE_TRIM_RUN кусков (по 2 м) без нарисованного над землёй и с ровной землёй с обеих сторон, коллизии не получают (как уже делается для трасс). Включение: SHR_FENCE_TRIM=1
# (по умолчанию выключено до подтверждения оригиналом: Sonnet-PS2 проверяет, держит ли оригинал в таких местах); SHR_FENCE_TRIM_RUN — длина прогона в кусках (по умолчанию 2)
FENCE_TRIM = os.environ.get('SHR_FENCE_TRIM') == '1'
FENCE_TRIM_RUN = int(os.environ.get('SHR_FENCE_TRIM_RUN', '2'))
# правка Роба 93 (30.09): верх забора брался по всему видимому в полосе ±1.5 м — у пожарной машины это конец лестницы и крыша
# Krusty (до 12 м), и в воздухе висела невидимая стена. Забор не выше земли + FENCE_CAP (живая изгородь и забор оригинала ниже 3 м)
FENCE_CAP = 3.0
FENCE_ROOF = os.environ.get('SHR_FENCE_ROOF_OLD') != '1'   # №705: факт PS2 есть (knowledge/facts/ps2-h82-fire-escape.yaml: 25 м/с садится на крышу, забора над краем нет) — по умолчанию ВКЛ; SHR_FENCE_ROOF_OLD=1 — прежнее
FENCE_LEDGE_MIN, FENCE_ROOF_MARGIN = 2.0, 0.3   # №705: перепад земли лицо→изнанка и запас над крышей
# Правка Роба 173 (01.10, L1 J9-4, гонка: «перестань делать из кустов стены, хочу проехать через кусты — доработай их везде»):
# внутренний забор, у которого над землёй в полосе ±FENCE_BAND стоит только листва (текстуры FOL_TEX_RE: живые изгороди
# дворов, кусты; не деревья — у них ствол), помечается «изгородь»: y0 = -COL_FENCE_FOL (движок: машина проезжает насквозь,
# viewer/col.c colPassFoliage). Граница карты и забор у стены/забора/дома (нелиства выше земли + FOL_SOLID) — как были.
# SHR_HEDGE_PASS=0 — без пометки (как до правки 173)
FOL_TEX_RE = re.compile(r'shrub|hedge|bush')
COL_FENCE_FOL = 2e6
FOL_SOLID, FOL_MIN = 0.5, 0.4   # м: нелиства выше земли больше FOL_SOLID — твёрдое рядом; листва выше земли меньше FOL_MIN — не изгородь
FENCE_MERGE = 0.5   # м: соседние куски с таким разбросом верха сливаются в одно звено (верх — по максимуму): меньше звеньев и контактов

def vis_top_grid(tris, lo, W, H, cell):
    """верх рисуемой геометрии по клеткам (барицентрическая выборка не реже cell/2), -1e9 — пусто"""
    G = np.full((H, W), -1e9, np.float32)
    P = np.asarray(tris, np.float32).reshape(-1, 3, 3)
    ext = np.max([np.linalg.norm(P[:, (i + 1) % 3] - P[:, i], axis=1) for i in range(3)], 0)
    ns = np.clip(np.ceil(ext / (cell * .5)).astype(int), 1, 300)
    for n in np.unique(ns):
        S = P[ns == n]; us = np.arange(n + 1) / n
        uu, vv = np.meshgrid(us, us); m = uu + vv <= 1 + 1e-6; uu, vv = uu[m], vv[m]
        for k0 in range(0, len(uu), 32):
            u = uu[k0:k0 + 32][None, :, None]; v = vv[k0:k0 + 32][None, :, None]
            Q = (S[:, None, 0] * (1 - u - v) + S[:, None, 1] * u + S[:, None, 2] * v).reshape(-1, 3)
            ix = ((Q[:, 0] - lo[0]) / cell).astype(np.int64); iz = ((Q[:, 2] - lo[2]) / cell).astype(np.int64)
            ok = (ix >= 0) & (ix < W) & (iz >= 0) & (iz < H)
            np.maximum.at(G, (iz[ok], ix[ok]), Q[ok, 1])
    return G

VG_NY, VG_NY_ALPHA, VG_BELOW, VG_ABOVE, VG_MIN_2A, VG_ROOF = 0.6, 0.85, 0.8, 0.4, 0.02, 3.0
# правка Роба 95 (30.09, F11-2: «холмы, которых быть не должно; еду сквозь текстуры»): склоны 0.3 делали твёрдыми стены ямы
# электростанции (l1z6) и овраг — туда можно было заехать, в оригинале нельзя. Склон снова от 0.6; не опора — всё над зонами
# DEATH оригинала (овраг R3D01..10: падение = возврат к дороге, правка 86) и всё ниже земли города больше VG_UNDER (интерьеры)
VG_DEATH_PAD, VG_UNDER = 6.0, 1.5
TEX_PIX_K = 2 * math.tan(math.radians(62 / 2)) / 272   # м на пиксель на 1 м глубины (viewer/main.c FOVY 62, 272 строки)


def col_visible_ground(TR, vis, vfl, lo, W, H, hm, deaths=(), vreg=None, regs=None, foliage=None):
    """Правило порта 11.3 (решение Роба 30.09: «если где-то проваливается — так не должно быть»): ночные боты нашли 86 мест
    провала сквозь видимую землю — у канав на диске нет Intersect. Видимый непрозрачный треугольник, на который можно встать
    (n.y >= VG_NY: и склоны оврагов; ровная трава с альфой — тоже), без земли коллизии под ним (слои карты высот Intersect в [y - VG_BELOW, y + VG_ABOVE] у центра) — становится
    землёй коллизии. Прозрачное (листва, вода — флаги 1/2) опорой не становится: вода остаётся провалом в зону DEATH.
    -> (TR с добавленными строками a, b, c, n; сколько добавлено)"""
    P = np.asarray(vis, np.float64).reshape(-1, 3, 3); fl = np.asarray(vfl)
    if len(fl) != len(P): return TR, 0
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]); a2 = np.linalg.norm(n, axis=1)
    ny = np.abs(n[:, 1]) / np.maximum(a2, 1e-12)
    sel = ((fl & 2) == 0) & (ny >= VG_NY) & (a2 >= VG_MIN_2A) & (((fl & 1) == 0) | (ny >= VG_NY_ALPHA))   # вода (2) — никогда; альфа (1) — только ровная трава
    # №445: drawable foliage is not a second, HARD support above its soft COL volume.
    # Classification comes from the owning entity's ATC eShrub, never the texture name.
    if foliage is not None and os.environ.get('FOLIAGE_GROUND_OLD', '0') != '1':
        assert len(foliage) == len(P)
        sel &= ~np.asarray(foliage, bool)
    c = P.mean(1)
    gx = np.floor((c[:, 0] - lo[0]) / CELL).astype(np.int64); gz = np.floor((c[:, 2] - lo[2]) / CELL).astype(np.int64)
    inside = (gx >= 0) & (gx < W) & (gz >= 0) & (gz < H)
    sup = np.zeros(len(P), bool)
    ix = np.nonzero(sel & inside)[0]
    L = hm[gz[ix], gx[ix]].astype(np.float64) / 100.; L[hm[gz[ix], gx[ix]] == -32768] = np.nan
    y = c[ix, 1][:, None]
    with np.errstate(invalid='ignore'):
        sup[ix] = np.any((L >= y - VG_BELOW) & (L <= y + VG_ABOVE), axis=1)
        top = np.nanmax(np.where(np.isnan(L), -1e9, L), axis=1); top[top < -1e8] = np.nan
        sup[ix] |= ~np.isnan(top) & (c[ix, 1] > top + VG_ROOF)   # выше земли клетки больше VG_ROOF — крыши/навесы (у них свои коробки)
        bot = np.nanmin(np.where(np.isnan(L), 1e9, L), axis=1); bot[bot > 1e8] = np.nan
        sup[ix] |= ~np.isnan(bot) & (c[ix, 1] < bot - VG_UNDER)  # правка 95: под землёй города (пол интерьера, яма) — не опора
    if regs is not None and vreg is not None and len(vreg) == len(P):   # правка 95 (F11-2): декорация издали (регион, которого
        sup |= ~regs.allowed(c[:, 0], c[:, 2], vreg)                     # оригинал с этого места не держит) — не опора
    for d in deaths:                                             # правка 95: над зоной DEATH (овраг) — не опора, как в оригинале
        cc = np.asarray(d['c'], np.float64); ax = np.asarray(d.get('axes', np.eye(3)), np.float64).reshape(3, 3); hh = np.asarray(d['h'], np.float64)
        q = c - cc; inx = np.abs(q @ ax[0]) <= hh[0] + VG_DEATH_PAD; inz = np.abs(q @ ax[2]) <= hh[2] + VG_DEATH_PAD
        sup |= inx & inz
    add = sel & inside & ~sup
    T = P[add]; nn = n[add] / a2[add, None]
    down = nn[:, 1] < 0; T[down] = T[down][:, [0, 2, 1]]; nn[down] *= -1
    if not len(T): return TR, 0
    return np.concatenate([TR, np.concatenate([T, nn[:, None]], 1)], 0), int(len(T))


# Правка Роба 414 (09.10, L5 K8-2 «фантом»; решение Роба 10.10: «в оригинале на фонтан заезжаешь со всех сторон»). Общее правило
# «невидимый столб посреди проезжей опоры»: стоячая капсула COL1 (r 0,2–0,8 м), которая поднимается над землёй машины минимум на PH_RISE,
# вокруг неё (кольцо PH_RING_R) та же опора, а на высоте кузова (опора + PH_BAND0 … + PH_BAND1) рядом с осью нет НИ ОДНОГО рисуемого
# крутого треугольника (стены, стволы, столбы, знаки; пол и склоны n.y >= VG_NY не в счёт) — держать там нечего: отмечается
# ColCyl.pad[0] = 2 (PHANTOM), машина проезжает насквозь (viewer/col.c cylPhantom, только шаг машины). Ошибка правила всегда в твёрдую
# сторону: габарит треугольника берётся по ящику, любой рисуемый предмет рядом — столб остаётся твёрдым. Предохранитель PH_MAX на уровень:
# если отмечено больше — рисуемая геометрия не та (например экземпляры не попали в vis), правило отменяется целиком.
# SHR_PHANTOM_OLD=1 — без правила (как до правки). Данные оригинала признака не дают (разбор 240ebe0): правило порта, решение Роба
PH_RMIN, PH_RMAX, PH_AXY, PH_HL, PH_RISE = 0.2, 0.8, 0.95, 0.3, 1.2
PH_BAND0, PH_BAND1, PH_PAD, PH_RING_R, PH_RING_TOL, PH_RING_MIN, PH_MAX = 0.3, 1.5, 0.35, 1.5, 0.6, 6, 40

def col_phantom_pillars(CY, vis, vfl, lo, W, H, hm):
    """CY (N,12) -> (CY с pad[0] = 2 у невидимых столбов, [(x, z, низ, верх)])"""
    if os.environ.get('SHR_PHANTOM_OLD') == '1' or not len(CY): return CY, []
    P = np.asarray(vis, np.float64).reshape(-1, 3, 3); fl = np.asarray(vfl)
    if len(fl) != len(P): return CY, []
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]); a2 = np.linalg.norm(n, axis=1)
    occ = ((fl & 2) == 0) & (a2 >= VG_MIN_2A) & (np.abs(n[:, 1]) / np.maximum(a2, 1e-12) < VG_NY)   # крутое рисуемое: стена, ствол, столб
    fo = fl[occ]
    mn, mx = P[occ].min(1), P[occ].max(1)
    def layers(x, z):
        gx = int(math.floor((x - lo[0]) / CELL)); gz = int(math.floor((z - lo[2]) / CELL))
        if not (0 <= gx < W and 0 <= gz < H): return np.empty(0)
        v = hm[gz, gx]; return v[v != -32768].astype(np.float64) / 100.
    ang = np.arange(8) * (math.pi / 4); found = []
    cand = np.nonzero((CY[:, 9] == 0) & (CY[:, 8] == 0) & (np.abs(CY[:, 4]) >= PH_AXY) & (CY[:, 7] >= PH_HL) &
                      (CY[:, 6] >= PH_RMIN) & (CY[:, 6] <= PH_RMAX))[0]
    dbg = os.environ.get('SHR_PHANTOM_DEBUG')   # «x,z,радиус»: разбор, какое условие отсекло кандидата рядом с точкой (отладка правки 414)
    dbg = [float(v) for v in dbg.split(',')] if dbg else None
    for i in cand:
        x, y, z, r, hl = CY[i, 0], CY[i, 1], CY[i, 2], CY[i, 6], CY[i, 7]
        bot, top = y - abs(CY[i, 4]) * hl - r, y + abs(CY[i, 4]) * hl + r
        near = dbg is not None and (x - dbg[0]) ** 2 + (z - dbg[1]) ** 2 <= dbg[2] ** 2
        lay = layers(x, z)
        if near: print(f'  414dbg cyl {i} c=({x:.2f},{y:.2f},{z:.2f}) r={r:.2f} hl={hl:.2f} span=({bot:.2f},{top:.2f}) layers={np.round(lay, 2).tolist()}')
        for g in lay:
            if not (bot + PH_BAND0 < g < top - PH_RISE):
                if near: print(f'    g={g:.2f}: вне диапазона опоры ({bot + PH_BAND0:.2f}..{top - PH_RISE:.2f})')
                continue
            ring = sum(np.any(np.abs(layers(x + PH_RING_R * math.cos(a), z + PH_RING_R * math.sin(a)) - g) <= PH_RING_TOL) for a in ang)
            if ring < PH_RING_MIN:
                if near: print(f'    g={g:.2f}: кольцо опоры {ring}/8 < {PH_RING_MIN}')
                continue
            R = r + PH_PAD
            hit = ((mx[:, 0] >= x - R) & (mn[:, 0] <= x + R) & (mx[:, 2] >= z - R) & (mn[:, 2] <= z + R) &
                   (mx[:, 1] >= g + PH_BAND0) & (mn[:, 1] <= g + PH_BAND1))
            if near: print(f'    g={g:.2f}: кольцо {ring}/8 ок; рисуемых крутых треугольников в полосе {int(hit.sum())}' +
                           ''.join(f'\n      tri fl={int(fo[hit][q])} y {mn[hit][q, 1]:.2f}..{mx[hit][q, 1]:.2f} x {mn[hit][q, 0]:.2f}..{mx[hit][q, 0]:.2f} z {mn[hit][q, 2]:.2f}..{mx[hit][q, 2]:.2f}' for q in range(min(int(hit.sum()), 8))))
            if not hit.any(): found.append((i, x, z, bot, top)); break
    if len(found) > PH_MAX:
        print(f'  невидимые столбы (414): найдено {len(found)} > {PH_MAX} — правило отменено (рисуемая геометрия неполна?)')
        return CY, []
    CY = CY.copy()
    for i, *_ in found: CY[i, 9] = 2.0
    return CY, [(round(x, 1), round(z, 1), round(b, 1), round(t, 1)) for _, x, z, b, t in found]

# Правка Роба 475 (10.10, находки ботов: «прозрачные стены», перепись L1–L7): общее правило «невидимый объём — не стена для машины».
# Коробка/капсула COL1 (статика мира: StaticPhys, InstaStatPhys, одиночный CollisionObject) режется на куски PV_CHUNK по длине. Кусок
# ПУСТ, если в PV_STEEP_PAD вокруг него (поле удара оригинала: капсулы *_gens стоят в 1–2 м от рисунка) нет ни одного крутого рисуемого
# треугольника (стена, ствол, столб, забор, борт), нет рисуемого верха (плоское в верхних PV_TOP_BAND — крыша, платформа, стол),
# нет пропов/стейтпропов/живых объектов (их рисунок не в vis) и кольцо 1.5 м вокруг куска стоит на опоре не меньше чем на PV_RING_MIN
# (иначе это край: край карты, причал над водой, обрыв — остаётся твёрдым). Пол и склон, проходящие сквозь высокий объём, его не оправдывают.
# Пустой кусок отмечается ColBox.pad / ColCyl.pad[0] = 2 (PHANTOM): машину не держит ни боком, ни верхом, ни краем под кузовом
# (viewer/col.c boxPhantom/cylPhantom, только шаг машины); Гомер и пробы мира держат как было. Смешанная коробка заменяется своими кусками
# (первый — на месте, остальные в конец списка: индексы прежних не сдвигаются), цельная пустая — отмечается целиком. Горизонтальная
# капсула режется на капсулы по своей оси (объединение то же). Ошибка правила всегда в твёрдую сторону. Предохранитель PV_MAX_FRAC:
# если пусто больше этой доли — рисуемая геометрия неполна (экземпляры не попали в vis), правило отменяется целиком.
# SHR_PHANTOM_VOL_OLD=1 — без правила. Данные оригинала признака не дают: правило порта, слово Роба 09–10.10 («пустота останавливает машину»)
PV_BOTTOM, PV_PAD, PV_BIG, PV_MAX_FRAC = 0.25, 0.35, 24.0, 0.5
PV_TOP_BAND = 0.5
PV_STEEP_PAD = 2.0   # м: крутое рисуемое ближе этого к куску — кусок не пуст
PV_CHUNK, PV_CHUNK_MAX, PV_CHUNK_BUDGET, PV_CAP_AXY = 3.0, 48, 256, 0.7   # м длины куска, предел кусков по оси, капсула с |axis.y| < этого режется по оси
PV_RING_OFF, PV_RING_STEP, PV_RING_MAX, PV_RING_MIN, PV_RING_DY = 1.5, 2.0, 24, 0.75, (-1.5, 2.5)

class PhantomProbe:
    """Запросы «пусто ли вокруг куска» и «стоит ли он на краю опоры» по рисуемому городу (vis, vfl), точкам пропов (extra) и карте высот"""
    def __init__(self, vis, vfl, extra_pts=(), ground=None):
        P = self.P = np.asarray(vis, np.float64).reshape(-1, 3, 3); fl = np.asarray(vfl)
        mn, mx = P.min(1), P.max(1); ext = (mx - mn).max(1)
        nrm = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]); a2 = np.linalg.norm(nrm, axis=1)
        steep_ = np.abs(nrm[:, 1]) / np.maximum(a2, 1e-12) < VG_NY
        real_ = ((fl & 2) == 0) & (a2 >= VG_MIN_2A)   # не вода, не вырожденное
        self.steep = steep_ & real_; self.solidflat = ~steep_ & real_
        small = np.nonzero(ext < PV_BIG)[0]; self.big = np.nonzero(ext >= PV_BIG)[0]
        self.G = 16.0
        cs = np.floor((mn[small][:, [0, 2]] + mx[small][:, [0, 2]]) / 2 / self.G).astype(np.int64)
        grid = {}
        for k, key in zip(small, map(tuple, cs)): grid.setdefault(key, []).append(k)
        self.grid = {k: np.array(v) for k, v in grid.items()}
        self.ex = np.asarray(extra_pts, np.float64).reshape(-1, 3); self.ground = ground
    def cand(self, c, r):   # треугольники, чей габарит может задеть шар радиуса r вокруг c
        r2 = r + PV_BIG / 2 + self.G
        i0 = np.floor((c[[0, 2]] - r2) / self.G).astype(int); i1 = np.floor((c[[0, 2]] + r2) / self.G).astype(int)
        ids = [self.grid[(a, b)] for a in range(i0[0], i1[0] + 1) for b in range(i0[1], i1[1] + 1) if (a, b) in self.grid]
        return np.concatenate(ids + [self.big]) if ids else self.big
    def empty(self, c, A, h, k_up):   # A (3,3) оси куска, h (3,) полуразмеры, k_up — ось, ближайшая к вертикали
        r = float(np.linalg.norm(h)) + PV_STEEP_PAD
        ids = self.cand(c, r)
        up = A[k_up][1] >= 0
        if len(ids):
            L = np.einsum('nvj,aj->nva', self.P[ids] - c, A)   # вершины в осях куска
            lo_, hi_ = L.min(1), L.max(1)
            sh = h + PV_STEEP_PAD
            sl, sk = -sh.copy(), sh.copy()   # крутое: запас PV_STEEP_PAD по сторонам, подошва обрезана на PV_BOTTOM
            if up: sl[k_up] = -h[k_up] + PV_BOTTOM
            else: sk[k_up] = h[k_up] - PV_BOTTOM
            if np.any(np.all((hi_ >= sl) & (lo_ <= sk), axis=1) & self.steep[ids]): return False
            tl, tk = -(h + PV_PAD), h + PV_PAD   # рисуемый верх куска: плоское в верхних PV_TOP_BAND
            if up: tl[k_up], tk[k_up] = h[k_up] - PV_TOP_BAND, h[k_up] + PV_PAD
            else: tl[k_up], tk[k_up] = -h[k_up] - PV_PAD, -h[k_up] + PV_TOP_BAND
            if np.any(np.all((hi_ >= tl) & (lo_ <= tk), axis=1) & self.solidflat[ids]): return False
        if len(self.ex) and np.any(np.all(np.abs((self.ex - c) @ A.T) <= h + PV_PAD + 2.0, axis=1)): return False
        return True
    def ring(self, c, A, h, k_up):   # доля точек кольца вокруг контура куска, под которыми есть опора коллизии у его подошвы
        if self.ground is None: return 1.0
        lo, W, H, hm = self.ground
        bot = c[1] - abs(A[k_up][1]) * h[k_up]
        ka, kb = [k for k in range(3) if k != k_up]
        ha, hb = h[ka] + PV_RING_OFF, h[kb] + PV_RING_OFF
        pts = []
        for sgn in (-1, 1):
            for t in np.linspace(-hb, hb, max(2, min(PV_RING_MAX // 4, int(2 * hb / PV_RING_STEP) + 1))): pts.append(c + A[ka] * sgn * ha + A[kb] * t)
            for t in np.linspace(-ha, ha, max(2, min(PV_RING_MAX // 4, int(2 * ha / PV_RING_STEP) + 1))): pts.append(c + A[kb] * sgn * hb + A[ka] * t)
        ok = 0
        for q in pts:
            gx = int(math.floor((q[0] - lo[0]) / CELL)); gz = int(math.floor((q[2] - lo[2]) / CELL))
            if not (0 <= gx < W and 0 <= gz < H): continue
            v = hm[gz, gx]; v = v[v != -32768].astype(np.float64) / 100.
            ok += bool(np.any((v >= bot + PV_RING_DY[0]) & (v <= bot + PV_RING_DY[1])))
        return ok / len(pts)
    def phantom(self, c, A, h, k_up):
        e = self.empty(c, A, h, k_up)
        return e, (self.ring(c, A, h, k_up) if e else 1.0)

def _runs(state, n_a, n_b):
    """state[b][a] -> [(a0, a1, b, значение)] подряд идущие одинаковые куски по оси a в ряду b"""
    out = []
    for b in range(n_b):
        a0 = 0
        for a in range(1, n_a + 1):
            if a == n_a or state[b][a] != state[b][a0]: out.append((a0, a, b, state[b][a0])); a0 = a
    return out

# Правка Роба 587 (общее правило, 15.10, Sonnet-Стены; вместо латки по координатам viewer/col.c): кусты и изгороди, чья коллизия по данным диска
# твёрдая (CollisionEffect physProp 0, не eShrub: common_gens51Shape L1 J9-4 и подобные), а ВИДИМАЯ геометрия внутри объёма — листва (текстуры
# FOL_TEX_RE, как у заборов, правка 173): объём помечается листвой (pad 1) — машина проезжает, как через куст. Критерий по объёму: треугольников
# листвы не меньше FV_MIN_FOL, не-листвы (кроме земли под низом) не больше FV_MAX_NON доли. Объёмы без видимой геометрии (невидимые) не трогаются.
# SHR_FOL_VOL_OLD=1 — прежнее (ничего не помечать).
# Уточнение 16.10 (решение Дирижёра, закон 2: у PS2 все такие объёмы твёрдые, cls 7 physProp 0, мягкий только l1_hedgeunit eShrub; мягкость кустов
# J9-4/J9-5 — отступление по слову Роба №524/№587): листвой (pad 1) остаётся ТОЛЬКО куст без основания. Группа соседних объёмов одной сущности
# мягкая, если (а) у неё нет каменного основания: нет боковых треугольников текстур FV_BASE_RE (foundation/кирпич/камень/блок и деревянный ящик deska/plank/wood/board, текстуры конвертера
# по именам; бетон тротуара и дерево сюда не входят, они стоят у кустов J9-4 рядом) вдоль оси объёма (до FV_BASE_PERP м от оси) на
# длине не меньше FV_BASE_SPAN его длины, и (б) листва не выше FV_MAX_H над землёй (тротуар/газон под объёмом). Остальное твёрдое, как у PS2
# (изгородь на цоколе участка common_gens5/10/28/31/36 L1); (в) группа не вплотную к стене: вертикальной не-листвы выше FV_WALL_H в пределах
# FV_WALL_DIST м нет. SHR_FOL_BASE_OLD=1 — прежнее решение по одной листве.
FV_BASE_RE = re.compile(r'found|brick|stone|cement|block|rock|cobble|deska|plank|wood|board')
FV_BASE_SPAN, FV_BASE_PERP, FV_MAX_H = 0.5, 1.3, 3.5
FV_BASE_MAXH = 4.0   # основание: треугольники не выше 4 м (цоколь изгороди 2–3,3 м вместе с подземной частью); фундамент здания (L7 z3_gensShape101: 9,5 м × 40 м) основанием куста не считается
FV_WALL_DIST, FV_WALL_H, FV_WALL_FRAC = 1.5, 1.0, 0.6   # куст вплотную к стене (вертикальная не-листва выше FV_WALL_H в пределах FV_WALL_DIST м): мягкий куст у стены даёт «влез и врезался», у PS2 твёрдый
FV_MARGIN, FV_MIN_FOL, FV_MAX_NON, FV_GRID = 0.4, 6, 0.5, 16.0
def col_foliage_volumes(BX, CY, vis, vfl, vtx, ufol, bnames=(), cnames=(), ubase=None):
    """-> BX', CY', отчёт [(вид, индекс, центр, листвы, не листвы)]"""
    BX = np.array(BX, np.float64).reshape(-1, 16); CY = np.array(CY, np.float64).reshape(-1, 12)
    if os.environ.get('SHR_FOL_VOL_OLD') == '1' or not (len(BX) or len(CY)) or not len(ufol): return BX, CY, []
    T = np.asarray(vis, np.float64).reshape(-1, 3, 3)
    if len(T) != len(vtx): return BX, CY, []
    cen = T.mean(1); nrm = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); nl = np.linalg.norm(nrm, axis=1); nrm = nrm / np.maximum(nl, 1e-12)[:, None]
    isf = np.isin(np.asarray(vtx), np.asarray(list(ufol)))
    cell = {}
    key = np.floor(cen[:, [0, 2]] / FV_GRID).astype(np.int64)
    for k, i in zip(map(tuple, key), range(len(T))): cell.setdefault(k, []).append(i)
    def near(c, rad):
        k0 = np.floor((np.array([c[0], c[2]]) - rad) / FV_GRID).astype(int); k1 = np.floor((np.array([c[0], c[2]]) + rad) / FV_GRID).astype(int)
        out = []
        for a in range(k0[0], k1[0] + 1):
            for b in range(k0[1], k1[1] + 1): out += cell.get((a, b), [])
        return np.array(out, np.int64)
    rep = []; stat = {}
    ymax_t = T[:, :, 1].max(1); isb = np.isin(np.asarray(vtx), np.asarray(list(ubase))) if ubase else np.zeros(len(T), bool)
    BX0_, CY0_ = BX.copy(), CY.copy()
    def vol_mask(k, i, M):
        if k == 'box':
            B = BX0_[i]; c = B[:3]; A = B[3:12].reshape(3, 3); h = B[12:15]
            return np.all(np.abs((cen - c) @ A.T) <= h + M, axis=1)
        C = CY0_[i]; c = C[:3]; ax = C[3:6]; r_, hl = C[6], C[7]; d = cen - c; t = d @ ax
        return (np.abs(t) <= hl + r_ + M) & (np.linalg.norm(d - np.outer(t, ax), axis=1) <= r_ + M)
    def vol_axis(k, i):
        if k == 'cyl': C = CY0_[i]; return C[:3], C[3:6], float(C[7] + C[6])
        B = BX0_[i]; A = B[3:12].reshape(3, 3); h = B[12:15]; ka = [j for j in range(3) if abs(A[j, 1]) < 0.7]
        ku = max(ka, key=lambda j: h[j]) if ka else int(np.argmax(h)); return B[:3], A[ku], float(h[ku])
    def base_span(k, i):
        """доля длины объёма вдоль его оси, закрытая боковыми треугольниками основания (FV_BASE_RE) не дальше FV_BASE_PERP м от оси"""
        c, ax, half = vol_axis(k, i); ax = ax / max(np.linalg.norm(ax), 1e-9)
        sel = np.nonzero(vol_mask(k, i, 1.5) & isb & ~isf & (np.abs(nrm[:, 1]) < 0.7) & (ysp_t <= FV_BASE_MAXH))[0]; iv = []
        for t in sel:
            P = T[t] - c; al = P @ ax; pp = np.linalg.norm(P - np.outer(al, ax), axis=1)
            if pp.min() <= FV_BASE_PERP: iv.append((float(al.min()), float(al.max())))
        if not iv: return 0.0
        iv.sort(); tot = 0.0; a0, b0 = iv[0]
        for a, b in iv[1:]:
            if a > b0: tot += b0 - a0; a0, b0 = a, b
            else: b0 = max(b0, b)
        return min((tot + b0 - a0) / max(2 * half, 1e-9), 2.0)
    def vol_box(k, i):
        if k == 'box':
            B = BX0_[i]; A = B[3:12].reshape(3, 3); h = B[12:15]; e = np.abs(A.T) @ h; return B[:3] - e, B[:3] + e
        C = CY0_[i]; e = np.abs(C[3:6]) * C[7] + C[6]; return C[:3] - e, C[:3] + e
    def vol_groups(lst):
        """объёмы одной сущности -> группы соседних (пересечение ящиков-границ +0,5 м): один куст/одна изгородь = одно решение"""
        bb = [vol_box(k, i) for k, i, _ in lst]; par = list(range(len(lst)))
        def f_(a):
            while par[a] != a: par[a] = par[par[a]]; a = par[a]
            return a
        for a in range(len(lst)):
            for b in range(a + 1, len(lst)):
                if np.all(bb[a][0] - 0.5 <= bb[b][1]) and np.all(bb[b][0] - 0.5 <= bb[a][1]): par[f_(a)] = f_(b)
        out = {}
        for a in range(len(lst)): out.setdefault(f_(a), []).append(lst[a])
        return list(out.values())
    ysp_t = T[:, :, 1].max(1) - T[:, :, 1].min(1)
    def wall_frac(g_):
        """доля листвы группы (треугольники в её объёмах), у которой в пределах FV_WALL_DIST м по плану стоит вертикальная не-листва выше FV_WALL_H
        (стена дома: вершины и центр её треугольника); куст, лишь касающийся стены концом ряда, мягким остаётся"""
        fi = np.zeros(len(T), bool); wi = np.zeros(len(T), bool)
        for k, i, _ in g_: fi |= vol_mask(k, i, 0.3) & isf; wi |= vol_mask(k, i, FV_WALL_DIST + 1.0)
        f = np.nonzero(fi)[0]
        if not len(f): return 0.0
        w = np.nonzero(wi & ~isf & (np.abs(nrm[:, 1]) < 0.5) & (ysp_t > FV_WALL_H))[0]
        if not len(w): return 0.0
        wp = np.concatenate([T[w].reshape(-1, 3), cen[w]])[:, [0, 2]]; fx = cen[f][:, [0, 2]]
        near_ = sum(1 for q in fx if np.linalg.norm(wp - q, axis=1).min() <= FV_WALL_DIST)
        return near_ / len(f)
    def fol_height(k, i):
        f = vol_mask(k, i, 0.3) & isf
        if not f.any(): return 0.0
        top = float(ymax_t[f].max()); g = vol_mask(k, i, 2.5) & ~isf & (nrm[:, 1] > 0.7) & (cen[:, 1] < top)
        return top - float(np.percentile(cen[g][:, 1], 20)) if g.any() else 0.0
    def decide(idx, inside_mask, low_mask):
        sel = idx[inside_mask]
        if not len(sel): return None
        fol = int(isf[sel].sum())
        non = int(((~isf[sel]) & ~(low_mask[inside_mask] & (nrm[sel, 1] > 0.7))).sum())
        return fol, non
    for i in range(len(BX)):
        B = BX[i]
        if B[15] != 0 or not np.all(B[12:15] > 0): continue
        c = B[:3]; A = B[3:12].reshape(3, 3); h = B[12:15]
        idx = near(c, float(np.linalg.norm(h)) + 1)
        if not len(idx): continue
        d = cen[idx] - c; loc = d @ A.T
        inside = np.all(np.abs(loc) <= h + FV_MARGIN, axis=1)
        ku = int(np.argmax(np.abs(A[:, 1])))
        low = (loc[:, ku] * np.sign(A[ku, 1] if A[ku, 1] else 1) <= -h[ku] + 0.5)
        r = decide(idx, inside, low)
        if r: stat[('box', i)] = r
    for i in range(len(CY)):
        C = CY[i]
        if C[9] != 0 or C[6] <= 0: continue
        c = C[:3]; ax = C[3:6]; r_, hl = C[6], C[7]
        idx = near(c, hl + r_ + 1)
        if not len(idx): continue
        d = cen[idx] - c; t = d @ ax; rad = np.linalg.norm(d - np.outer(t, ax), axis=1)
        inside = (np.abs(t) <= hl + r_ + FV_MARGIN) & (rad <= r_ + FV_MARGIN)
        low = (d[:, 1] <= -r_ + 0.5)
        rr = decide(idx, inside, low)
        if rr: stat[('cyl', i)] = rr
    # решение по СУЩНОСТИ (сцена/имя объекта диска): все её твёрдые объёмы вместе — изгородь складывается из ящика-основания и капсул, по
    # отдельности ящик может быть наполовину в земле/клумбе; листва по сумме должна преобладать, и хотя бы один объём — настоящая листва
    ent = {}
    for (k, i), r in stat.items():
        nm = (bnames if k == 'box' else cnames)[i] if i < len(bnames if k == 'box' else cnames) else f'{k}{i}'
        ent.setdefault(nm, []).append((k, i, r))
    for nm, lst in ent.items():
        fol = sum(r[0] for _, _, r in lst); non = sum(r[1] for _, _, r in lst)
        anyfol = any(r[0] >= FV_MIN_FOL and r[1] <= FV_MAX_NON * (r[0] + r[1]) for _, _, r in lst)
        if fol < FV_MIN_FOL or non > FV_MAX_NON * (fol + non) or not anyfol: continue
        if ubase and os.environ.get('SHR_FOL_BASE_OLD') != '1':
            lst = [e_ for g_ in vol_groups(lst) if not (any(base_span(k, i) >= FV_BASE_SPAN or fol_height(k, i) > FV_MAX_H for k, i, _ in g_) or wall_frac(g_) >= FV_WALL_FRAC) for e_ in g_]   # группа с основанием или высокая: твёрдая, как у PS2
            if not lst: continue
        for k, i, r in lst:
            if k == 'box': BX[i, 15] = 1.0
            else: CY[i, 9] = 1.0
            rep.append((k, i, (BX[i, :3] if k == 'box' else CY[i, :3]).round(1).tolist(), r[0], r[1]))
    return BX, CY, rep


def col_phantom_volumes(BX, CY, vis, vfl, extra_pts=(), ground=None):
    """BX (N,16), CY (M,12) -> (BX', CY' с pad = 2 у невидимых кусков, список (вид, индекс, центр, размеры, доля опоры, индекс исходного объёма) для отчёта).
    Смешанные объёмы режутся (первый кусок на месте, остальные в конец BX/CY)"""
    BX = np.array(BX, np.float64).reshape(-1, 16); CY = np.array(CY, np.float64).reshape(-1, 12)
    if os.environ.get('SHR_PHANTOM_VOL_OLD') == '1' or not (len(BX) or len(CY)): return BX, CY, []
    if len(vfl) != len(np.asarray(vis).reshape(-1, 3, 3)): return BX, CY, []
    BX0, CY0 = BX.copy(), CY.copy()
    pr = PhantomProbe(vis, vfl, extra_pts, ground)
    rep, addB, addC = [], [], []
    def chunks(n_tot, hh):
        n = int(min(PV_CHUNK_MAX, max(1, math.ceil(2 * hh / PV_CHUNK)))); return n, hh / n
    nB0, nC0 = len(BX), len(CY)
    for i in range(nB0):
        B = BX[i]
        if B[15] != 0 or not np.all(B[12:15] > 0): continue
        A = B[3:12].reshape(3, 3); h = B[12:15]; c = B[:3]; ku = int(np.argmax(np.abs(A[:, 1])))
        ka, kb = [k for k in range(3) if k != ku]
        na, ha = chunks(0, h[ka]); nb, hb = chunks(0, h[kb])
        while na * nb > PV_CHUNK_BUDGET:   # огромные ящики (плиты 60×60 м): куски крупнее, но не больше бюджета запросов
            if na >= nb: na = max(1, na - max(1, na // 8)); ha = h[ka] / na
            else: nb = max(1, nb - max(1, nb // 8)); hb = h[kb] / nb
        st = [[None] * na for _ in range(nb)]
        for b in range(nb):
            for a in range(na):
                cc = c + A[ka] * (-h[ka] + ha * (2 * a + 1)) + A[kb] * (-h[kb] + hb * (2 * b + 1))
                hh = h.copy(); hh[ka] = ha; hh[kb] = hb
                st[b][a] = pr.phantom(cc, A, hh, ku)
                st[b][a] = (st[b][a][0] and st[b][a][1] >= PV_RING_MIN, round(st[b][a][1], 2))
        flags = [[x[0] for x in row] for row in st]
        if not any(any(r) for r in flags): continue
        if all(all(r) for r in flags):   # цельная пустая
            BX[i, 15] = 2.0; rep.append(('box', i, c.round(2).tolist(), h.round(2).tolist(), min(x[1] for r_ in st for x in r_), i)); continue
        first = True   # смешанная: ряды одинаковых кусков по оси a
        for a0, a1, b, fv in _runs(flags, na, nb):
            cc = c + A[ka] * (-h[ka] + ha * (a0 + a1)) + A[kb] * (-h[kb] + hb * (2 * b + 1))
            hh = h.copy(); hh[ka] = ha * (a1 - a0); hh[kb] = hb
            R = np.concatenate([cc, A.reshape(-1), hh, [2.0 if fv else 0.0]])
            if first: BX[i] = R; first = False; idx = i
            else: addB.append(R); idx = nB0 + len(addB) - 1
            if fv: rep.append(('box', idx, cc.round(2).tolist(), hh.round(2).tolist(), min(st[b][a][1] for a in range(a0, a1)), i))
    for i in range(nC0):
        C = CY[i]
        if C[9] != 0 or C[6] <= 0: continue
        ax = C[3:6]; r, hl = C[6], C[7]
        u = np.cross(ax, [1.0, 0, 0] if abs(ax[0]) < .9 else [0, 1.0, 0]); u /= np.linalg.norm(u); w_ = np.cross(ax, u)
        A = np.array([u, ax, w_])
        split = C[8] == 0 and abs(ax[1]) < PV_CAP_AXY and hl > PV_CHUNK / 2
        n = int(min(PV_CHUNK_MAX, max(1, math.ceil(2 * hl / PV_CHUNK)))) if split else 1
        seg = hl / n
        res = []
        for k in range(n):
            cc = C[:3] + ax * (-hl + seg * (2 * k + 1))
            h = np.array([r, seg + (0 if C[8] else r), r])
            e, rf = pr.phantom(cc, A, h, 1)
            res.append((cc, e and rf >= PV_RING_MIN, rf))
        if not any(x[1] for x in res): continue
        if all(x[1] for x in res): CY[i, 9] = 2.0; rep.append(('cyl', i, C[:3].round(2).tolist(), [round(r, 2), round(hl, 2)], min(x[2] for x in res), i)); continue
        k0 = 0; first = True
        for k in range(1, n + 1):   # подряд одинаковые куски — одна капсула
            if k == n or res[k][1] != res[k0][1]:
                half = seg * (k - k0); cc = C[:3] + ax * (-hl + seg * (k0 + k))
                R = C.copy(); R[:3] = cc; R[7] = half; R[9] = 2.0 if res[k0][1] else 0.0
                if first: CY[i] = R; first = False; idx = i
                else: addC.append(R); idx = nC0 + len(addC) - 1
                if res[k0][1]: rep.append(('cyl', idx, cc.round(2).tolist(), [round(r, 2), round(half, 2)], min(x[2] for x in res[k0:k]), i))
                k0 = k
    n_ = len(rep)
    if n_ > PV_MAX_FRAC * (nB0 + nC0):
        print(f'  невидимые объёмы (475): пустых кусков {n_} из {nB0 + nC0} объёмов > {PV_MAX_FRAC:.0%} — правило отменено (рисуемая геометрия неполна?)')
        return BX0, CY0, []
    if addB: BX = np.concatenate([BX, np.array(addB)])
    if addC: CY = np.concatenate([CY, np.array(addC)])
    return BX, CY, rep

# Правка Роба 489 (10.10, фонтан L5 K8-2: «на чашу заезжаешь между рыбами; столбики под рыбами не сбиваются — это памятник, твёрдые»): капсула COL1
# статического (не сбиваемого) объёма, стоящая на опоре, не шире своего ВИДИМОГО столба + PH_FIT_PAD: невидимого утолщения вокруг нарисованного столба
# (у фонтана r 0,54 против столбика 0,7×0,7 м) нет — машина встаёт у самого столба, а не за 0,2 м до него. Видимый столб — крутые рисуемые
# непрозрачные треугольники в полосе кузова (опора + 0,3 … + 1,2 м) в пределах r + PH_PAD от оси; ничего рисуемого рядом — капсула не трогается (ошибка
# правила — в твёрдую сторону: радиус только уменьшается и не ниже PH_FIT_MIN). Листва (pad[0] = 1) и лежащие капсулы не затрагиваются.
# SHR_CAPSULE_FIT_OLD=1 — как было
PH_FIT_PAD, PH_FIT_MIN, PH_BAND_CAR = 0.05, 0.25, 1.2

def col_fit_capsules(CY, vis, vfl, lo, W, H, hm):
    """CY (N,12) -> (CY с уменьшенными радиусами, [(x, z, r_до, r_после)])"""
    if os.environ.get('SHR_CAPSULE_FIT_OLD') == '1' or not len(CY): return CY, []
    P = np.asarray(vis, np.float64).reshape(-1, 3, 3); fl = np.asarray(vfl)
    if len(fl) != len(P): return CY, []
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]); a2 = np.linalg.norm(n, axis=1)
    occ = ((fl & 3) == 0) & (a2 >= VG_MIN_2A) & (np.abs(n[:, 1]) / np.maximum(a2, 1e-12) < VG_NY)   # крутое, рисуемое, непрозрачное, не вода
    mn, mx = P[occ].min(1), P[occ].max(1)
    def layers(x, z):
        gx = int(math.floor((x - lo[0]) / CELL)); gz = int(math.floor((z - lo[2]) / CELL))
        if not (0 <= gx < W and 0 <= gz < H): return np.empty(0)
        v = hm[gz, gx]; return v[v != -32768].astype(np.float64) / 100.
    cand = np.nonzero((CY[:, 9] == 0) & (CY[:, 8] == 0) & (np.abs(CY[:, 4]) >= PH_AXY) & (CY[:, 7] >= PH_HL) &
                      (CY[:, 6] >= PH_RMIN) & (CY[:, 6] <= PH_RMAX))[0]
    CY = CY.copy(); out = []
    for i in cand:
        x, y, z, r, hl = CY[i, 0], CY[i, 1], CY[i, 2], CY[i, 6], CY[i, 7]
        bot, top = y - abs(CY[i, 4]) * hl - r, y + abs(CY[i, 4]) * hl + r
        for g in layers(x, z):
            if not (bot + PH_BAND0 < g < top - PH_RISE): continue
            R = r + PH_PAD
            sel = ((mx[:, 0] >= x - R) & (mn[:, 0] <= x + R) & (mx[:, 2] >= z - R) & (mn[:, 2] <= z + R) &
                   (mx[:, 1] >= g + PH_BAND0) & (mn[:, 1] <= g + PH_BAND_CAR))
            if not sel.any(): break
            ext = max(np.abs(mn[sel][:, [0, 2]] - [x, z]).max(), np.abs(mx[sel][:, [0, 2]] - [x, z]).max())
            nr = max(PH_FIT_MIN, ext + PH_FIT_PAD)
            if nr < r - 1e-3:
                out.append((round(x, 2), round(z, 2), round(r, 2), round(nr, 2))); CY[i, 6] = nr
            break
    return CY, out

# Правка (11.10, слово Роба через дирижёра: «у оригинала видимая стена, к которой подъезжает машина, держит»): рисуемая вертикальная непрозрачная стена
# (выше 1 м, от 0,8 м² на треугольник), до которой машина доезжает и по линии которой коллизии нет — подтверждено пробой кандидатов (tools/autoexplore/walls,
# заезд на 12 и 20 м/с с обеих сторон без упора; список точек tools/walls_confirmed/L<N>.tsv). Вокруг каждой точки (радиус DW_R по плану, ±DW_DY по высоте)
# по каждому такому треугольнику ставится тонкая коробка вдоль его плоскости. Только для подтверждённых точек, не для всех рисуемых стен. DRAWN_WALL_COL_OLD=1 — без правила
DW_R, DW_DY, DW_MINH, DW_MINA, DW_T, DW_ROW = 4.0, 3.0, 1.0, 0.8, 0.15, 0.75

def col_drawn_wall_boxes(BX, level, vis, vfl):
    """BX (N,16) -> (BX + тонкие коробки, число добавленных)"""
    f = Path(__file__).resolve().parent / 'walls_confirmed' / f'L{level}.tsv'
    if os.environ.get('DRAWN_WALL_COL_OLD') == '1' or not f.exists(): return BX, 0
    pts = np.array([[float(x) for x in ln.split()[:3]] for ln in f.read_text().splitlines() if ln.strip() and not ln.startswith('#')], np.float64).reshape(-1, 3)
    P = np.asarray(vis, np.float64).reshape(-1, 3, 3); fl = np.asarray(vfl)
    if not len(pts) or len(fl) != len(P): return BX, 0
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]); a2 = np.linalg.norm(n, axis=1)
    wall = ((fl & 3) == 0) & (a2 >= 2 * DW_MINA) & (np.abs(n[:, 1]) / np.maximum(a2, 1e-12) < 0.35) & (np.hypot(n[:, 0], n[:, 2]) / np.maximum(a2, 1e-12) > 0.5)
    c = P.mean(1); new = []; used = set()
    for p in pts:
        # расстояние по плану от точки до треугольника: меньшее из центроида, вершин и середин рёбер (длинная стена не должна выпадать из-за далёкого центроида)
        dd = np.hypot(c[:, 0] - p[0], c[:, 2] - p[2])
        for k_ in range(3): dd = np.minimum(dd, np.hypot(P[:, k_, 0] - p[0], P[:, k_, 2] - p[2])); dd = np.minimum(dd, np.hypot((P[:, k_, 0] + P[:, (k_ + 1) % 3, 0]) / 2 - p[0], (P[:, k_, 2] + P[:, (k_ + 1) % 3, 2]) / 2 - p[2]))
        near = np.nonzero(wall & (dd <= DW_R) & (P[:, :, 1].min(1) - DW_DY <= p[1]) & (P[:, :, 1].max(1) + DW_DY >= p[1]))[0]   # по высоте: точка в диапазоне высот треугольника ± DW_DY (высокая стена имеет центроид далеко от точки)
        for i in near:
            if i in used: continue
            T = P[i]; nh = np.array([n[i, 0], 0, n[i, 2]]) / math.hypot(n[i, 0], n[i, 2]); ay = np.array([0.0, 1, 0]); ax = np.cross(ay, nh)
            u = (T - T[0]) @ ax; v = (T - T[0]) @ ay
            if v.max() - v.min() < DW_MINH: continue
            used.add(i); d = float(np.mean((T - T[0]) @ nh)); tilt = abs(n[i, 1]) / math.hypot(n[i, 0], n[i, 2])
            # треугольник режется на горизонтальные полосы DW_ROW по высоте: коробка на полосу покрывает только то, что занято треугольником (описанный прямоугольник накрывал бы воздух)
            nrow = max(1, int(math.ceil((v.max() - v.min()) / DW_ROW))); rh = (v.max() - v.min()) / nrow
            for r in range(nrow):
                vm = v.min() + (r + .5) * rh; xs = []
                for a_, b_ in ((0, 1), (1, 2), (2, 0)):
                    if (v[a_] - vm) * (v[b_] - vm) <= 0 and abs(v[a_] - v[b_]) > 1e-9: xs.append(u[a_] + (u[b_] - u[a_]) * (vm - v[a_]) / (v[b_] - v[a_]))
                if len(xs) < 2 or max(xs) - min(xs) < 0.2: continue
                u0, u1 = min(xs), max(xs); th = DW_T + min(0.45, 0.5 * rh * tilt)
                ctr = T[0] + ax * (u0 + u1) / 2 + ay * vm + nh * d
                new.append([*ctr, *ax, *ay, *nh, (u1 - u0) / 2, rh / 2, th, 0.0])
    if not new: return BX, 0
    return np.concatenate([np.asarray(BX, np.float64).reshape(-1, 16), np.array(new)]), len(new)


# Правка Роба 164б/187 (F12-6 правые перила: «ударился об невидимый столб»; 17.10, решение дирижёра): у перил/парапетов ВИДИМАЯ полка (крышка) шире ножки,
# а коллизия отрезков одной линии разная: ящики 72/73 только ножка (0,98 м), соседние 69/70 полной ширины (1,40 м) — на стыке невидимый уступ 0,2–0,34 м,
# о торец которого бьётся колесо (v 16 → 3,7 м/с за кадр; у PS2 машина проезжает 11 м до следующего шва на 14,5 м/с). Правило (общее для всех линий перил):
# длинный узкий ящик-ножка (полуширина >= RC_MINHW), у которого в линии (оси вдоль параллельны, центры в RC_LAT поперёк, зазор вдоль <= RC_GAP) есть
# сосед шире на RC_DW0…RC_DW1, и у верха которого рисуются вертикальные грани шире ножки на тех же RC_DW0…RC_DW1 (площадь >= RC_AREA) — к ножке
# добавляется ящик полки по ширине видимой грани, толщиной от низа этих граней до верха ножки. RAIL_CAP_COL_OLD=1 — без правила
RC_MINHW, RC_DW0, RC_DW1, RC_LAT, RC_GAP, RC_AREA = 0.29, 0.05, 0.45, 0.25, 0.3, 1.0

def col_rail_caps(BX, vis, vfl):
    """BX (N,16) -> (BX + ящики полок, список (ящик, ширина полки))"""
    if os.environ.get('RAIL_CAP_COL_OLD') == '1': return BX, []
    BX = np.asarray(BX, np.float64).reshape(-1, 16)
    if not len(BX): return BX, []
    P = np.asarray(vis, np.float64).reshape(-1, 3, 3)
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]); a2 = np.linalg.norm(n, axis=1); nn = n / np.maximum(a2, 1e-12)[:, None]
    vm = (np.abs(nn[:, 1]) < 0.3) & (a2 > 0.02)
    V = P[vm]; VN = nn[vm]; VA = 0.5 * a2[vm]; VC = V.mean(axis=1)
    G = {}
    for i, c in enumerate(VC): G.setdefault((int(c[0] // 4), int(c[2] // 4)), []).append(i)
    def frame(B):
        ax = [B[3:6], B[6:9], B[9:12]]; hh = B[12:15]
        vi = int(np.argmax([abs(a[1]) for a in ax])); rest = [k for k in range(3) if k != vi]
        wi, li = (rest[0], rest[1]) if hh[rest[0]] < hh[rest[1]] else (rest[1], rest[0])
        up = np.asarray(ax[vi]) if ax[vi][1] > 0 else -np.asarray(ax[vi])
        wv = np.array([ax[wi][0], 0, ax[wi][2]]); lv = np.array([ax[li][0], 0, ax[li][2]])
        nw, nl = np.linalg.norm(wv), np.linalg.norm(lv)
        return vi, wi, li, up, (wv / nw if nw > 0.9 else None), (lv / nl if nl > 0.5 else None)
    FR = [frame(B) for B in BX]; new = []; rep = []
    for ia, A in enumerate(BX):
        vi, wi, li, up, wh, lh = FR[ia]; hh = A[12:15]
        if A[15] != 0 or hh[li] < 1.5 or hh[wi] < RC_MINHW or hh[wi] > 0.8 or wh is None or lh is None or abs(A[3 + 3 * li + 1]) > 0.5: continue
        ctr = A[0:3]; topc = ctr + up * hh[vi]; ok = False
        for ib, B in enumerate(BX):
            if ib == ia or B[15] != 0: continue
            vb, wb, lb, upb, wbh, lbh = FR[ib]; hb = B[12:15]
            if wbh is None or lbh is None or not (RC_DW0 <= hb[wb] - hh[wi] <= RC_DW1) or abs(np.dot(wbh, wh)) < 0.97 or abs(np.dot(lbh, lh)) < 0.94: continue
            if abs(np.dot(B[0:3] - ctr, wh)) > RC_LAT or abs(np.dot(B[0:3] - ctr, lh)) - hh[li] - hb[lb] > RC_GAP: continue
            if abs((B[0:3] + upb * hb[vb])[1] - topc[1]) > 1.5: continue
            ok = True; break
        if not ok: continue
        cells = set((int((ctr[0] + dx) // 4), int((ctr[2] + dz) // 4)) for dx in (-6, 0, 6) for dz in (-6, 0, 6))
        offs = []
        for c_ in cells:
            for i in G.get(c_, ()):
                if abs(np.dot(VN[i], wh)) < 0.95: continue
                c = VC[i]
                if c[1] < topc[1] - 1.0 or c[1] > topc[1] + 0.6 or abs(np.dot(c - ctr, lh)) > hh[li] + 0.2: continue
                off = abs(np.dot(c - ctr, wh))
                if hh[wi] + RC_DW0 <= off <= hh[wi] + RC_DW1: offs.append((off, VA[i], V[i][:, 1].min()))
        if not offs: continue
        ar = np.array(offs)
        if ar[:, 1].sum() < RC_AREA: continue
        wcap = float(np.average(ar[:, 0], weights=ar[:, 1])); capT = float(min(0.8, max(0.2, topc[1] - ar[:, 2].min() + 0.06)))
        C = A.copy(); C[0:3] = ctr + up * (hh[vi] - capT / 2); C[12 + wi] = wcap; C[12 + vi] = capT / 2; C[15] = 0.0
        new.append(C); rep.append((ia, round(wcap, 2), round(capT, 2)))
    if not new: return BX, []
    return np.concatenate([BX, np.array(new)]), rep

# Правка Роба 488 (10.10, слово Роба: «тонкие столбы, фонари, знаки сбиваются нормально, как в оригинале; машина не должна упираться в воздух
# в 1–2 м от них»). Капсулы/коробки *_gens (r ≈ 1,1–1,9 м) стоят вокруг СБИВАЕМОГО предмета (DynaPhys PRP1, AnimDynaPhys SPR1 с реакцией,
# cls != 2): это его поле удара из данных диска, не стена. Малая статическая капсула/коробка COL1 (поперёк до PO_SPAN, капсула r <= PO_RMAX),
# к оси/контуру которой сбиваемый предмет ближе PO_NEAR по горизонтали (и по высоте внутри объёма), принадлежит предмету: pad = 2, машину не
# держит (viewer/col.c boxPhantom/cylPhantom), предмет бьётся своей коллизией и реакцией (props.c/stateprops.c). Большие объёмы (стены, дома,
# памятники), листва (pad 1) и уже отмеченные не трогаются. SHR_PROP_OWNED_OLD=1 — без правила. Решение Роба 488
PO_NEAR, PO_SPAN, PO_RMAX, PO_DY = 1.5, 4.0, 2.0, 1.0

def col_prop_owned(BX, CY, brk_pts):
    """BX (N,16), CY (M,12), brk_pts (K,3) -> (BX, CY с pad = 2, [(вид, индекс, имя-индекс)]) для объёмов, принадлежащих сбиваемому предмету"""
    BX = np.array(BX, np.float64).reshape(-1, 16); CY = np.array(CY, np.float64).reshape(-1, 12)
    pts = np.asarray(brk_pts, np.float64).reshape(-1, 3)
    if os.environ.get('SHR_PROP_OWNED_OLD') == '1' or not len(pts): return BX, CY, []
    found = []
    for i, C in enumerate(CY):
        if C[9] != 0 or C[6] <= 0 or C[6] > PO_RMAX: continue
        ax = C[3:6]; hl, r = C[7], C[6]
        axz = math.hypot(ax[0], ax[2])
        if 2 * (hl * axz + r) > PO_SPAN and C[8] == 0 and axz > 0.3: continue   # длинная горизонтальная — забор/балка, не поле предмета
        a = C[:3] - ax * hl; b = C[:3] + ax * hl; d = b - a
        t = np.clip(((pts - a) @ d) / max(float(d @ d), 1e-9), 0.0, 1.0)
        q = a + t[:, None] * d
        dh = np.hypot(pts[:, 0] - q[:, 0], pts[:, 2] - q[:, 2])
        lo_y, hi_y = min(a[1], b[1]) - r - PO_DY, max(a[1], b[1]) + r + PO_DY
        if np.any((dh <= PO_NEAR) & (pts[:, 1] >= lo_y) & (pts[:, 1] <= hi_y)): found.append(('cyl', i))
    for i, B in enumerate(BX):
        if B[15] != 0 or not np.all(B[12:15] > 0): continue
        A = B[3:12].reshape(3, 3); h = B[12:15]
        ku = int(np.argmax(np.abs(A[:, 1]))); ka, kb = [k for k in range(3) if k != ku]
        if 2 * h[ka] > PO_SPAN or 2 * h[kb] > PO_SPAN: continue
        q = (pts - B[:3]) @ A.T
        ex_ = np.maximum(np.abs(q[:, ka]) - h[ka], 0); ez_ = np.maximum(np.abs(q[:, kb]) - h[kb], 0)
        dh = np.hypot(ex_, ez_)
        if np.any((dh <= PO_NEAR) & (np.abs(q[:, ku]) <= h[ku] + PO_DY)): found.append(('box', i))
    for kind, i in found:
        if kind == 'box': BX[i, 15] = 2.0
        else: CY[i, 9] = 2.0
    return BX, CY, found

def col_fence_tops(SG, hmap, vis_tris, folmask=None, drop_invisible=False):
    """SG (N,8) -> SG' (M,8): внутренние заборы порезаны на куски с конечным верхом (y1), границы — как были.
    folmask (по треугольникам vis_tris) — листва: куски, где над землёй только она, — «изгородь» (y0 = -COL_FENCE_FOL, правка 173)
    hmap = (lo, W, H, hm) — карта высот коллизии (heightmap: земля по треугольникам Intersect, до LAYERS слоёв, клетка CELL)"""
    lo, W, H, hm = hmap
    top = hm.max(-1).astype(np.float32) / 100.; top[hm.max(-1) == -32768] = np.nan
    VG = vis_top_grid(vis_tris, lo, W, H, CELL)
    VT = np.asarray(vis_tris, np.float32).reshape(-1, 9)
    VGf = vis_top_grid(VT[folmask].reshape(-1, 3), lo, W, H, CELL) if folmask is not None and folmask.any() else None
    VGn = vis_top_grid(VT[~folmask].reshape(-1, 3), lo, W, H, CELL) if VGf is not None else None
    # №524 (11.10, L1 J9-5: «кусты должны проезжаться»): изгородь решалась по сетке 4 м, и любой столб, дом или вывеска в соседней клетке делали звено
    # твёрдым — линия кустов из мягких и твёрдых кусков (z=141: 935 мягкий, 934/936/937 твёрдые). Теперь листва/не-листва по сетке 1 м и полосе ±0.75 м.
    # HEDGE_FINE_OLD=1 — как до правки
    HF = 4 if os.environ.get('HEDGE_FINE_OLD', '0') != '1' else 1
    if HF > 1 and VGf is not None:
        VGf1 = vis_top_grid(VT[folmask].reshape(-1, 3), lo, W * HF, H * HF, CELL / HF); VGn1 = vis_top_grid(VT[~folmask].reshape(-1, 3), lo, W * HF, H * HF, CELL / HF)
    else: VGf1 = VGn1 = None
    def cells(x, z):
        gx = np.floor((x - lo[0]) / CELL).astype(np.int64); gz = np.floor((z - lo[2]) / CELL).astype(np.int64)
        ok = (gx >= 0) & (gx < W) & (gz >= 0) & (gz < H); return gx.clip(0, W - 1), gz.clip(0, H - 1), ok
    out, st = [], dict(bound=0, inner=0, pieces=0, h=[], hedge=0)
    def is_bound(s_):   # за забором (2 м по -n) пустота — граница карты
        a_, b_, n_ = s_[0:2], s_[2:4], s_[4:6]; u_ = np.linspace(0, 1, max(2, int(np.ceil(float(np.hypot(*(b_ - a_))))) + 1)); P_ = a_ + (b_ - a_) * u_[:, None]
        gx_, gz_, ok_ = cells(P_[:, 0] - n_[0] * FENCE_BEHIND, P_[:, 1] - n_[1] * FENCE_BEHIND)
        return not (ok_ & ~np.isnan(top[gz_, gx_])).mean() > .5
    isb = [is_bound(s_) for s_ in SG]
    if not FENCE_BRIDGE_OLD:   # №905: одиночные «границы» посреди линии внутренних звеньев (подножие обрыва) — внутренние
        ends = {}; key_ = lambda p_: (int(round(float(p_[0]) * 10)), int(round(float(p_[1]) * 10)))
        for i_, s_ in enumerate(SG): ends.setdefault(key_(s_[0:2]), []).append(i_); ends.setdefault(key_(s_[2:4]), []).append(i_)
        nbridge = 0
        for _pass in range(FENCE_BRIDGE_RUN):
            flip = []
            for i_, s_ in enumerate(SG):
                if not isb[i_]: continue
                good = 0
                for p_ in (s_[0:2], s_[2:4]):
                    nb_ = [j_ for j_ in ends.get(key_(p_), []) if j_ != i_]
                    if nb_ and all(not isb[j_] for j_ in nb_): good += 1
                if good == 2: flip.append(i_)
            for i_ in flip: isb[i_] = False
            nbridge += len(flip)
            if not flip: break
        st['bridged'] = nbridge
    for si_, s in enumerate(SG):
        a, b, n = s[0:2], s[2:4], s[4:6]; L = float(np.hypot(*(b - a)))
        k = max(1, int(np.ceil(L / FENCE_PIECE)))
        u = np.linspace(0, 1, max(2, int(np.ceil(L)) + 1))
        P = a + (b - a) * u[:, None]
        gx, gz, ok = cells(P[:, 0] - n[0] * FENCE_BEHIND, P[:, 1] - n[1] * FENCE_BEHIND)
        if isb[si_]:   # граница карты: за забором пустота — бесконечный, одним куском, как было (№905: кроме одиночных звеньев посреди внутренней линии)
            out.append(s); st['bound'] += 1; continue
        st['inner'] += 1
        pieces = []; trim_flags = []
        for j in range(k):
            pa, pb = a + (b - a) * (j / k), a + (b - a) * ((j + 1) / k)
            m = int(np.ceil(np.hypot(*(pb - pa)))) + 1
            t = np.linspace(0, 1, m); Q = pa + (pb - pa) * t[:, None]
            off = np.linspace(-FENCE_BAND, FENCE_BAND, 5)
            X = (Q[:, None, 0] + n[0] * off[None]).ravel(); Z = (Q[:, None, 1] + n[1] * off[None]).ravel()
            cx, cz, ok2 = cells(X, Z)
            vt = VG[cz[ok2], cx[ok2]]; vt = float(vt.max()) if len(vt) else -1e9
            gt = top[cz[ok2], cx[ok2]]; gt = float(np.nanmax(gt)) if len(gt) and not np.isnan(gt).all() else -1e9
            if drop_invisible and gt > -1e8 and vt < gt + TRACK_FENCE_VIS:   # трасса: над землёй у звена нет нарисованного — невидимая стена
                pieces.append((pa, pb, None, gt, False)); st['invisible'] = st.get('invisible', 0) + 1; continue
            y1 = max(vt + FENCE_MARGIN, gt + FENCE_MIN)
            if gt > -1e8: y1 = min(y1, gt + FENCE_CAP)
            # Правка №705 (L5 H8-2, пожарная лестница-трамплин ведёт на крышу; 17.10): забор по краю крыши/уступа, где с лица земля НИЖЕ земли за ним на LEDGE_MIN и более (стена
            # обрыва уже держит снизу), не растёт ещё на 2,5…3 м над крышей: верх = земля за забором + ROOF_MARGIN. Машина, перелетевшая край, садится на крышу, а не бьётся
            # о невидимый «парапет» (seg 645: 22 → 0,4 м/с). SHR_FENCE_ROOF_OLD=1 — прежнее (факт PS2: knowledge/facts/ps2-h82-fire-escape.yaml)
            if FENCE_ROOF and gt > -1e8:
                fx2, fz2, fok2 = cells(Q[:, 0] + n[0] * FENCE_BEHIND, Q[:, 1] + n[1] * FENCE_BEHIND)
                bx2, bz2, bok2 = cells(Q[:, 0] - n[0] * FENCE_BEHIND, Q[:, 1] - n[1] * FENCE_BEHIND)
                gf = top[fz2[fok2], fx2[fok2]] if fok2.any() else np.array([np.nan]); gb = top[bz2[bok2], bx2[bok2]] if bok2.any() else np.array([np.nan])
                if fok2.all() and bok2.all() and not np.isnan(gf).any() and not np.isnan(gb).any() and float(gb.min()) - float(gf.max()) >= FENCE_LEDGE_MIN:
                    y1 = min(y1, float(gb.max()) + FENCE_ROOF_MARGIN); st['roof'] = st.get('roof', 0) + 1
            if FENCE_TRIM and not drop_invisible and gt > -1e8 and vt < gt + TRACK_FENCE_VIS:   # №581-класс: над землёй у куска ничего не нарисовано; считаем, ровная ли земля и с лица
                fx, fz, fok = cells(Q[:, 0] + n[0] * FENCE_BEHIND, Q[:, 1] + n[1] * FENCE_BEHIND)
                ft_ = top[fz[fok], fx[fok]] if fok.any() else np.array([np.nan])
                if not np.isnan(ft_).any() and len(ft_) and fok.all(): trimmable = True   # и с лица, и с изнанки земля (не обрыв к воде)
                else: trimmable = False
            else: trimmable = False
            if y1 < -1e8: y1 = COL_FENCE_Y
            hedge = False
            if VGf is not None and gt > -1e8:
                if VGf1 is not None:
                    off1 = np.linspace(-.75, .75, 4); X1 = (Q[:, None, 0] + n[0] * off1[None]).ravel(); Z1 = (Q[:, None, 1] + n[1] * off1[None]).ravel()
                    gx1 = np.floor((X1 - lo[0]) / (CELL / HF)).astype(np.int64); gz1 = np.floor((Z1 - lo[2]) / (CELL / HF)).astype(np.int64)
                    k1 = (gx1 >= 0) & (gx1 < W * HF) & (gz1 >= 0) & (gz1 < H * HF); ft = VGf1[gz1[k1], gx1[k1]]; nt = VGn1[gz1[k1], gx1[k1]]
                else: ft = VGf[cz[ok2], cx[ok2]]; nt = VGn[cz[ok2], cx[ok2]]
                ft = float(ft.max()) if len(ft) else -1e9; nt = float(nt.max()) if len(nt) else -1e9
                hedge = ft >= gt + FOL_MIN and nt <= gt + FOL_SOLID   # над землёй — листва, твёрдого выше FOL_SOLID нет
            pieces.append((pa, pb, y1, gt, hedge)); trim_flags.append(trimmable)
        # соседние куски с близким верхом (разброс до FENCE_MERGE) — снова один забор с верхом по максимуму: длинная линия
        # вдоль ровной живой изгороди остаётся одним звеном, как в данных (стыки звеньев и число контактов не меняются)
        if FENCE_TRIM and not drop_invisible and len(trim_flags) == len(pieces):   # прогоны подряд ≥ FENCE_TRIM_RUN кусков по 2 м без рисунка над ровной землёй с обеих сторон — не держим
            j0 = 0; keep = [True] * len(pieces)
            while j0 < len(pieces):
                if not trim_flags[j0]: j0 += 1; continue
                j1 = j0
                while j1 < len(pieces) and trim_flags[j1]: j1 += 1
                if j1 - j0 >= FENCE_TRIM_RUN:
                    for q in range(j0, j1): keep[q] = False
                    st['trimmed'] = st.get('trimmed', 0) + (j1 - j0); st['trim_m'] = st.get('trim_m', 0.0) + sum(float(np.hypot(*(pieces[q][1] - pieces[q][0]))) for q in range(j0, j1))
                j0 = j1
            pieces = [p_ for p_, k_ in zip(pieces, keep) if k_]
        pieces = [p_ for p_ in pieces if p_[2] is not None]   # отброшенные невидимые звенья (трасса)
        r0 = 0
        while r0 < len(pieces):
            r1, lo_, hi_ = r0 + 1, pieces[r0][2], pieces[r0][2]
            while r1 < len(pieces) and pieces[r1][4] == pieces[r0][4] and max(hi_, pieces[r1][2]) - min(lo_, pieces[r1][2]) <= FENCE_MERGE:
                lo_, hi_ = min(lo_, pieces[r1][2]), max(hi_, pieces[r1][2]); r1 += 1
            hd = pieces[r0][4]; st['hedge'] += int(hd)
            out.append([*pieces[r0][0], *pieces[r1 - 1][1], *n, -COL_FENCE_FOL if hd else -COL_FENCE_Y, hi_]); st['pieces'] += 1
            gts = [p_[3] for p_ in pieces[r0:r1] if p_[3] > -1e8]
            if gts: st['h'].append(hi_ - max(gts))
            r0 = r1
    return np.array(out, np.float64).reshape(-1, 8), st

def col_xz_bounds(TR, BX, CY):
    """xz-габариты примитивов (N, 4: x0 z0 x1 z1) — ровно так же считает tools/col_check.py"""
    t = np.c_[TR[:, :3, 0].min(1), TR[:, :3, 2].min(1), TR[:, :3, 0].max(1), TR[:, :3, 2].max(1)]
    e = np.abs(BX[:, 3:12].reshape(-1, 3, 3)) * BX[:, 12:15, None]          # |ось| * полуразмер
    ex, ez = e[:, :, 0].sum(1), e[:, :, 2].sum(1)
    b = np.c_[BX[:, 0] - ex, BX[:, 2] - ez, BX[:, 0] + ex, BX[:, 2] + ez]
    r, hl = CY[:, 6], CY[:, 7]
    cx, cz = np.abs(CY[:, 3]) * hl + r, np.abs(CY[:, 5]) * hl + r            # капсула/цилиндр: с запасом
    c = np.c_[CY[:, 0] - cx, CY[:, 2] - cz, CY[:, 0] + cx, CY[:, 2] + cz]
    return t, b, c

def col_seg_cells(SG, ox, oz, cell, W, H, margin=COL_SEG_MARGIN):
    """-> (клетки, номера заборов): клетки, чей квадрат, раздутый на margin, пересекает отрезок"""
    cells, ids = [], []
    for i, (ax, az, bx, bz) in enumerate(SG[:, :4]):
        gx0 = int(np.floor((min(ax, bx) - margin - ox) / cell)); gx1 = int(np.floor((max(ax, bx) + margin - ox) / cell))
        gz0 = int(np.floor((min(az, bz) - margin - oz) / cell)); gz1 = int(np.floor((max(az, bz) + margin - oz) / cell))
        gx, gz = np.meshgrid(np.arange(max(gx0, 0), min(gx1, W - 1) + 1), np.arange(max(gz0, 0), min(gz1, H - 1) + 1))
        gx, gz = gx.ravel(), gz.ravel()
        lo_x, hi_x = ox + gx * cell - margin, ox + (gx + 1) * cell + margin
        lo_z, hi_z = oz + gz * cell - margin, oz + (gz + 1) * cell + margin
        dx, dz = bx - ax, bz - az; t0, t1 = np.zeros(len(gx)), np.ones(len(gx))
        for d, a, lo_, hi_ in ((dx, ax, lo_x, hi_x), (dz, az, lo_z, hi_z)):   # отсечение отрезка по прямоугольнику
            if abs(d) < 1e-12:
                bad = (a < lo_) | (a > hi_); t0 = np.where(bad, 2.0, t0)
            else:
                u0, u1 = (lo_ - a) / d, (hi_ - a) / d
                t0, t1 = np.maximum(t0, np.minimum(u0, u1)), np.minimum(t1, np.maximum(u0, u1))
        hit = t0 <= t1
        cells.append(gz[hit] * W + gx[hit]); ids.append(np.full(int(hit.sum()), i))
    return (np.concatenate(cells), np.concatenate(ids)) if cells else (np.zeros(0, int), np.zeros(0, int))

def col_section(TR, BX, CY, SG, cell=COL_CELL, sweep=None, jmp=b''):
    """-> bytes секции COL1 и статистика сетки. sweep {('box'|'cyl', номер): (x0, z0, x1, z1)} — живые объёмы
    (tools/animobj_export.py): в сетку идут все клетки, которые объём заметает за клип, не только поза кадра 0.
    jmp — блок JMP1 (tools/jumps_export.py: зоны прыжка машин и подкидывающие точки Гомера) после ссылок, выровнен
    на 16; его смещение от начала секции — в последнем поле заголовка (flags; 0 — блока нет)"""
    t, b, c = col_xz_bounds(TR, BX, CY)
    for (k, i), (x0, z0, x1, z1) in (sweep or {}).items():
        a = b if k == 'box' else c
        a[i] = [min(a[i, 0], x0), min(a[i, 1], z0), max(a[i, 2], x1), max(a[i, 3], z1)]
    s = np.c_[np.minimum(SG[:, 0], SG[:, 2]) - COL_SEG_MARGIN, np.minimum(SG[:, 1], SG[:, 3]) - COL_SEG_MARGIN,
              np.maximum(SG[:, 0], SG[:, 2]) + COL_SEG_MARGIN, np.maximum(SG[:, 1], SG[:, 3]) + COL_SEG_MARGIN]
    allb = np.concatenate([t, b, c, s])
    ox, oz = np.floor(allb[:, 0].min() / cell) * cell, np.floor(allb[:, 1].min() / cell) * cell
    W = int(np.floor((allb[:, 2].max() - ox) / cell)) + 1; H = int(np.floor((allb[:, 3].max() - oz) / cell)) + 1
    pc, pr = [], []
    for typ, bb in ((0, t), (1, b), (2, c)):
        if not len(bb): continue
        gx0 = np.floor((bb[:, 0] - ox) / cell).astype(int); gz0 = np.floor((bb[:, 1] - oz) / cell).astype(int)
        gx1 = np.floor((bb[:, 2] - ox) / cell).astype(int); gz1 = np.floor((bb[:, 3] - oz) / cell).astype(int)
        nx, nz = gx1 - gx0 + 1, gz1 - gz0 + 1; cnt = nx * nz
        idx = np.repeat(np.arange(len(bb)), cnt); k = np.arange(cnt.sum()) - np.repeat(np.cumsum(cnt) - cnt, cnt)
        gx = gx0[idx] + k % nx[idx]; gz = gz0[idx] + k // nx[idx]
        pc.append(gz * W + gx); pr.append((typ << 29) | idx)
    sc, si = col_seg_cells(SG, ox, oz, cell, W, H)
    pc.append(sc); pr.append((3 << 29) | si)
    pc, pr = np.concatenate(pc).astype(np.int64), np.concatenate(pr).astype(np.int64)
    o = np.lexsort((pr, pc)); pc, pr = pc[o], pr[o]
    start = np.zeros(W * H + 1, np.int64); np.cumsum(np.bincount(pc, minlength=W * H), out=start[1:])
    head = b'COL1' + struct.pack('<3f8I', ox, oz, cell, W, H, len(TR), len(BX), len(CY), len(SG), len(pr), 0)
    cy = np.zeros(len(CY), [('f', '<f4', 8), ('flat', '<u4'), ('pad', '<f4', 3)])   # flat — uint32
    cy['f'] = CY[:, :8]; cy['flat'] = CY[:, 8].astype(np.uint32)
    cy['pad'] = CY[:, 9:12]
    body = [TR.astype('<f4').tobytes(), BX.astype('<f4').tobytes(), cy.tobytes(),
            SG.astype('<f4').tobytes(), start.astype('<u4').tobytes(), pr.astype('<u4').tobytes()]
    sec = bytearray(head + b''.join(body)); sec += b'\0' * (-len(sec) % 16)
    if jmp:
        struct.pack_into('<I', sec, 44, len(sec)); sec += jmp; sec += b'\0' * (-len(sec) % 16)
    per = np.diff(start); ne = per[per > 0]
    stats = dict(ox=ox, oz=oz, W=W, H=H, nref=len(pr), bytes=len(sec), cells_used=len(ne), mean=float(ne.mean()),
                 max=int(per.max()), p99=float(np.percentile(ne, 99)), max_tri=int(np.bincount(pc[pr >> 29 == 0], minlength=W * H).max()))
    return bytes(sec), stats

def col_report(rep, st, TR, BX, CY, SG):
    print(f'коллизия COL1: {len(TR):,} треуг. земли (из {rep["tri_src"]:,}: вырожденных {rep["tri_degenerate"]}, '
          f'повторов {rep["tri_duplicate"]}, нависающих перевёрнуто к n.y>=0 {rep["tri_flipped"]}, '
          f'вертикальных {rep["tri_vertical"]}), ящиков {len(BX)}, '
          f'цилиндров/капсул/сфер {len(CY)} (плоских торцов {rep["flat_cyl"]}, сфер {rep["spheres"]}), заборов {len(SG)}')
    for k, (ni, nb, nc) in rep['static'].items(): print(f'  в COL1 {k}: {ni} экз., ящиков {nb}, цилиндров {nc}')
    for k, d in rep['skipped'].items(): print(f'  НЕ в COL1 (этап 5) {k}: {sum(d.values())} шт. — ' + ', '.join(f'{n} {c}' for n, c in sorted(d.items(), key=lambda x: -x[1])))
    if rep['errors']: print('  ошибки чтения объёмов:', *rep['errors'], sep='\n    ')
    print(f'  сетка {st["W"]}x{st["H"]} по {COL_CELL:g} м от ({st["ox"]:g}, {st["oz"]:g}), ссылок {st["nref"]:,}, '
          f'в непустой клетке ср. {st["mean"]:.1f}, 99% {st["p99"]:.0f}, макс {st["max"]} (треуг. макс {st["max_tri"]}), '
          f'непустых клеток {st["cells_used"]:,}; секция {st["bytes"] / 2**20:.2f} MB; неортогональность осей ящиков {rep["ortho"]:.1e}')

# ---------------------------------------------------------------------------------------------------------------
# Машина игрока (docs/car_spec.md): куски по суставам + секция CAR1 (смещение в заголовке @100).
# Куски машины (owner 1): Rec.pad[0] = сустав (0 кузов … 26), Rec.pad[1] = вариант (0 машина, 1 остов huska).
# Вершины варианта 0 — в осях своего сустава (без позиции покоя: её движок берёт из таблицы суставов CAR1),
# остов — в позе покоя целиком (сустав 0). Цвет вершин = запечённая светотень по нормалям (шейдеры машины LIT 1).
#   'CAR1' u32 version, njoint, nhinge, ndam, nflare, pad[2]                                   32 Б
#   float lift, huskLift, wheelRad, maxSteer, shadowAdj[8]                                    48 Б
#   joint[njoint] : char name[12]; float x, y, z (SHR, позиция покоя в модели); u32 parent    28 Б
#   hinge[nhinge] : u8 joint, axis(0 X, 1 Y), i8 sign, u8 zone; float max, stiff, cx, cy, cz  24 Б
#   dam[ndam]     : u16 texNorm, texDam; u8 zone(0 перед, 1 зад, 2 левый, 3 правый борт), pad[3]  8 Б
#   flare[nflare] : u8 joint, kind(0 стоп, 1 задний ход), mode(0 YAX, 1 LYAX, 2 AAX), pad; u16 tex, pad;
#                   u32 abgr; float w, h, dist, ox, oy, oz                                     36 Б
# Версия 2 (29.09, машина уровня в движке): pad[0] заголовка = смещение блока машины от начала секции, pad[1] = его длина.
#   info (CAR_INFO_FMT, 128 Б): char car[16] (имя .con: famil_v, honor_v, …); кузов halfW, halfL, bodyOfs, wheelX, wheelF,
#   wheelR, wheelRad, bodyLo, bodyHi, bandLo, bandHi, step (поля CarParams viewer/car.h); сиденье водителя seat x y z (сустав dl);
#   якорь посадки seatAx, seatAz (конец клипа посадки ставит Гомера на сиденье) и точка у двери doorX, doorZ; камера за машиной
#   FollowCameraData (0x3000100, ближний вариант): rot, elev, mag, target x y z, far mag; u32 flags (1 SetHasDoors)
# Правило кузова (одно для всех машин, правило порта — точных чисел оригинала нет, сверка с эталоном PS2): мерка по модели
# машины (меш сустава 0 «кузов» в позе покоя, суставы колёс w0..w3, меш колеса, сустав dl) плюс поправка, снятая с эталона
# CAR_REF: у famil_v движок с этапа 2 держит числа car.c, принятые Робом на PSP; поправка = эти числа минус мерка famil_v,
# поэтому уровень 1 побайтно прежний, а у остальных машин размеры свои (tools/psp_export.car_measure/car_info)
CAR_REF = 'famil_v'
CAR_REF_GEOM = dict(halfW=1.10, halfL=2.675, bodyOfs=-0.32, wheelX=0.91, wheelF=1.47, wheelR=1.82, wheelRad=0.37,
                    bodyLo=0.28, bodyHi=1.65)                    # viewer/car.c carParamsFor до 29.09 (famil_v)
CAR_REF_BAND = (0.35, 1.6)       # слой стен кузова famil_v: ниже — бордюры (колёса заезжают), выше крыши — нет
CAR_STEP = 0.35                  # ступенька под колесом — свойство мира (бордюры), одно на все машины
CAR_REF_DOOR = (-1.475, 0.57)    # viewer/homer.c HOMER_DOOR_X/Z до 29.09: якорь посадки famil_v (x, z модели)
CAR_INFO_FMT = '<16s12f3f4f7fI'
CAR_ROOF_BAND = 0.6              # крыша — верх кузова ближе к оси, чем 0.6 полуширины (зеркала и ручки по бокам — не крыша)
CAR_SUN =np.array([0.35, 0.85, -0.40]); CAR_SUN = CAR_SUN / np.linalg.norm(CAR_SUN)   # сверху, спереди-справа (модель смотрит в -Z)
CAR_AMB, CAR_DIF = 0.50, 0.55   # светотень: ambient + диффуз (подобрано на глаз, AMBI шейдеров не берём)
CAR_HINGES = {'HoodRot': (0, 1, 0), 'TrunkRot': (0, -1, 1), 'DoorDRot': (1, -1, 2), 'DoorPRot': (1, 1, 3)}   # ось, знак, зона
CAR_ZONES = {'Hood': 0, 'Back': 1, 'DoorD': 2, 'DoorP': 3}

def car_physics_object(root):
    """Body physics, not its simplified BV collision proxy; chunk order varies by car."""
    objects = [c for c in root.children if c.id == 0x7011000]
    return next((c for c in objects if not string(c.data)[0].lower().endswith('bv')), objects[0] if objects else None)

# Правка Роба 506 (щели моделей, «багажник — только пример»): у машин детали (кузов, крышки, двери, бамперы) — отдельные меши; на 88 моделях диска
# 63 имеют несваренные границы между деталями, 38 — зазоры 1–30 мм между границами деталей (замер tools: build/ scratch cargaps; docs/handoff/
# ОПТИМИЗАЦИЯ_PSP_РЕСЁРЧ). На экране 480x272 вместе со сдвигом проекции на ¼ px такая щель то видна, то нет. Правило: граничные вершины разных деталей
# (не колёса, не полупрозрачное стекло) ближе CARGAP_MM (15 мм) стягиваются в общий центр кластера в позе покоя — щель закрыта, детали по-прежнему
# двигаются отдельными суставами. SHR_CARGAP_OLD=1 — прежние вершины
CARGAP_OLD = os.environ.get('SHR_CARGAP_OLD') == '1'
CARGAP_MM = float(os.environ.get('SHR_CARGAP_MM', '15'))


def car_close_gaps(entries, bake_matrix, eps_mm=None):
    """entries: [(имя, tex, flags, P(3n x 3, float32), UV, C, M(4x4))]; P в осях сустава (bake_matrix=False) или в позе покоя.
    -> те же записи без M, где границы разных деталей ближе CARGAP_MM сведены в одну точку"""
    eps = (CARGAP_MM if eps_mm is None else eps_mm) / 1000.0
    model = []
    for n_, t, fl, P, UV, C, M in entries:
        Pm = P.astype(np.float64) if bake_matrix else (np.c_[P.astype(np.float64), np.ones(len(P))] @ M)[:, :3]
        model.append(Pm)
    movable = [not (re.fullmatch(r'w[0-3]', e[0]) or (e[2] & 2)) for e in entries]   # колёса и полупрозрачное не трогаем
    key = lambda A: np.round(A * 10000).astype(np.int64)   # 0,1 мм
    # граничные рёбра деталей (по объекту: у объекта может быть несколько примитивов)
    edges = {}
    for i, e in enumerate(entries):
        if not movable[i]: continue
        K = key(model[i]).reshape(-1, 3, 3)
        d = edges.setdefault(e[0], {})
        for a, b in ((0, 1), (1, 2), (2, 0)):
            for ka, kb in zip(map(tuple, K[:, a]), map(tuple, K[:, b])):
                ee = (ka, kb) if ka < kb else (kb, ka)
                if ee[0] != ee[1]: d[ee] = d.get(ee, 0) + 1
    pts = []   # (объект, ключ, позиция)
    for obj, d in edges.items():
        bv = set()
        for (ka, kb), c in d.items():
            if c == 1: bv.add(ka); bv.add(kb)
        for k_ in bv: pts.append((obj, k_, np.array(k_, np.float64) / 10000.0))
    if len(pts) < 2: return [e[:6] for e in entries], 0
    cell = {}
    for j, (_, _, v) in enumerate(pts): cell.setdefault(tuple(np.floor(v / eps).astype(int)), []).append(j)
    par = list(range(len(pts)))
    def find(x):
        while par[x] != x: par[x] = par[par[x]]; x = par[x]
        return x
    for j, (o, _, v) in enumerate(pts):
        c = np.floor(v / eps).astype(int)
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for dz in (-1, 0, 1):
                    for m in cell.get((c[0] + dx, c[1] + dy, c[2] + dz), []):
                        if m > j and pts[m][0] != o and np.linalg.norm(pts[m][2] - v) <= eps: par[find(m)] = find(j)
    clusters = {}
    for j in range(len(pts)): clusters.setdefault(find(j), []).append(j)
    target = {}   # (объект, ключ) -> новая позиция
    moved = 0
    for js in clusters.values():
        if len({pts[j][0] for j in js}) < 2: continue
        c = np.mean([pts[j][2] for j in {pts[j][1]: j for j in js}.values()], axis=0)   # центр по различным точкам
        for j in js:
            if np.linalg.norm(pts[j][2] - c) > 1e-6: target[(pts[j][0], pts[j][1])] = c; moved += 1
    # второй проход: граничная вершина, не попавшая в кластер, прижимается к ближайшей вершине ДРУГОЙ детали (любой, не только граничной) в пределах eps
    claimed = {pk for pk in target}
    allv = {}   # ячейка -> [(объект, позиция)] по всем вершинам подвижных деталей
    for i, e in enumerate(entries):
        if not movable[i]: continue
        for v in np.unique(key(model[i]), axis=0):
            pos = v / 10000.0; allv.setdefault(tuple(np.floor(pos / eps).astype(int)), []).append((e[0], pos))
    own = {}
    for i, e in enumerate(entries):
        if movable[i]: own.setdefault(e[0], set()).update(map(tuple, np.unique(key(model[i]), axis=0)))
    for obj, k_, v in pts:
        if (obj, k_) in target: continue
        c = np.floor(v / eps).astype(int); best = None; bd = eps + 1e-9
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for dz in (-1, 0, 1):
                    for o2, u in allv.get((c[0] + dx, c[1] + dy, c[2] + dz), []):
                        if o2 == obj: continue
                        d_ = float(np.linalg.norm(u - v))
                        if 1e-6 < d_ < bd: bd = d_; best = u
        if best is not None:
            kb = tuple(key(best[None])[0])
            if kb in own.get(obj, ()): continue   # на месте уже стоит своя вершина этой детали: слияние схлопнуло бы треугольники
            target[(obj, k_)] = best; moved += 1
    out = []
    for i, e in enumerate(entries):
        P = e[3]
        if movable[i] and target:
            K = key(model[i]); newm = model[i].copy(); hit = False
            for r, k_ in enumerate(map(tuple, K)):
                tv = target.get((e[0], k_))
                if tv is not None: newm[r] = tv; hit = True
            if hit:
                P = newm.astype(np.float32) if bake_matrix else (np.c_[newm, np.ones(len(newm))] @ np.linalg.inv(e[6]))[:, :3].astype(np.float32)
        out.append((e[0], e[1], e[2], P, e[4], e[5]))
    return out, moved


def car_gap_metric(entries, bake_matrix):
    """(граничных вершин, зазоров 1–30 мм): сколько граничных вершин деталей имеют ближайшую вершину ДРУГОЙ детали на 1–30 мм (щель)"""
    key = lambda A: np.round(A * 10000).astype(np.int64)
    parts = {}
    for e in entries:
        if re.fullmatch(r'w[0-3]', e[0]): continue   # стекло считаем: стягивание кузова не должно открывать щель к стеклу
        P = e[3].astype(np.float64) if bake_matrix else (np.c_[e[3].astype(np.float64), np.ones(len(e[3]))] @ e[6])[:, :3]
        parts.setdefault(e[0], []).append(P)
    V = {n: np.vstack(a) for n, a in parts.items()}
    cell = {}; bnd = []
    for n, P in V.items():
        for v in np.unique(key(P), axis=0): cell.setdefault(tuple(np.floor(v / 300.0).astype(int)), []).append((n, v / 10000.0))   # ячейки 30 мм
        K = key(P).reshape(-1, 3, 3); cnt = {}
        for a, b in ((0, 1), (1, 2), (2, 0)):
            for ka, kb in zip(map(tuple, K[:, a]), map(tuple, K[:, b])):
                ee = (ka, kb) if ka < kb else (kb, ka)
                if ee[0] != ee[1]: cnt[ee] = cnt.get(ee, 0) + 1
        bv = set()
        for ee, c in cnt.items():
            if c == 1: bv.add(ee[0]); bv.add(ee[1])
        bnd += [(n, np.array(v, np.float64) / 10000.0) for v in bv]
    near = 0
    for n, v in bnd:
        c = np.floor(v / .03).astype(int); m = 9.0
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for dz in (-1, 0, 1):
                    for o2, u in cell.get((c[0] + dx, c[1] + dy, c[2] + dz), []):
                        if o2 != n: m = min(m, float(np.linalg.norm(u - v)))
        near += 0.001 < m <= 0.03
    return len(bnd), int(near)


# №1061 (Роб 27.10, TEST74: «покрышки чёрные издалека видны, крутятся как будто кто-то въехал в краску; на такси они жёлтые; пусть будут чёрные и у всех одна»):
# колесо машины ложится на кусок ОБЩЕГО атласа кузова (такси: угол 32x64 из 128x128 рядом с жёлтыми панелями), а развёртка протектора идёт по кромке квадрата колеса
# или по самой картинке диска: фильтр и мипы (в том числе затухание мипов к среднему цвету ВСЕГО атласа — у такси жёлтому) подмешивают чужие тексели, протектор крутится
# и мажет колесо жёлтым или серым. Правило: колесо получает СВОЮ текстуру — вырезку квадрата колеса из атласа с картинкой диска (боковины как были), тексели вокруг
# боковин заменены ближайшими использованными (кромки не подмешивают соседей), остальное поле — одна резина TIRE_RUBBER; ВСЕ треугольники протектора (поперёк оси
# колеса, как viewer/wheelcap.h) смотрят в центр самого большого квадрата резины (не меньше 8x8 — выдерживает мип 3), у всех машин один чёрный тон. Мипы колеса —
# без затухания к среднему. Одинаковые вырезки у разных машин — одна запись. Колесо, у которого есть только протектор, — общая текстура резины 16x8. TIRE1061_OLD=1 — как до правки
TIRE_OLD = os.environ.get('TIRE1061_OLD') == '1'
TIRE_RUBBER = (32, 37, 40)   # тон резины PS2: кольцо покрышки famil_v на кадре PS2 (build/ps2ref/c3.png, 5120x2880), медиана (32, 37, 40)
TIRE_BLOCK = 8
TIRE_STAT = {'wheels': 0, 'fixed': 0, 'skipped': 0, 'new': 0, 'grown': 0}
TIRE_WHY = []   # причины пропусков (перепись)


def _tri_mask(T, w, h):
    """T (n,3,2) — треугольники в пикселях вырезки -> булева карта (h, w): пиксели, которых треугольник касается (сетка 4x4 внутри пикселя + рёбра и вершины — вырожденные треугольники)"""
    m = np.zeros((h, w), bool)
    g = (np.arange(4) + .5) / 4
    for t in T:
        lo = np.floor(t.min(0)).astype(int).clip(0, [w - 1, h - 1]); hi = np.ceil(t.max(0)).astype(int).clip(0, [w, h])
        for e in range(3):   # рёбра и вершины
            a, b = t[e], t[(e + 1) % 3]
            for f in np.linspace(0, 1, max(2, int(np.hypot(*(b - a)) * 4) + 1)):
                q = a + (b - a) * f; x, y = int(np.floor(q[0])), int(np.floor(q[1]))
                if 0 <= x < w and 0 <= y < h: m[y, x] = True
        (ax, ay), (bx, by), (cx, cy) = t; d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(d) < 1e-9: continue
        ys, xs = np.meshgrid(np.arange(lo[1], hi[1]), np.arange(lo[0], hi[0]), indexing='ij')
        px = xs[..., None, None] + g[None, None, :, None] + 0 * g[None, None, None, :]; py = ys[..., None, None] + g[None, None, None, :] + 0 * g[None, None, :, None]
        l1 = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / d; l2 = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / d; l3 = 1 - l1 - l2
        ins = ((l1 >= -1e-9) & (l2 >= -1e-9) & (l3 >= -1e-9)).any((2, 3))
        m[ys[ins], xs[ins]] = True
    return m


def _dilate(m, n=1):
    for _ in range(n):
        p = np.pad(m, 1); m = m | p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:] | p[:-2, :-2] | p[2:, 2:] | p[:-2, 2:] | p[2:, :-2]
    return m


def _big_square(free):
    """булева карта свободных пикселей -> (сторона, x, y) самого большого квадрата из свободных, целиком внутри карты без выхода на кромку (запас 1 пиксель)"""
    h, w = free.shape; f = free.copy(); f[0, :] = f[-1, :] = False; f[:, 0] = f[:, -1] = False
    dp = np.zeros((h, w), int); best = (0, 0, 0)
    for y in range(h):
        for x in range(w):
            if f[y, x]:
                dp[y, x] = 1 + (min(dp[y - 1, x], dp[y, x - 1], dp[y - 1, x - 1]) if y and x else 0)
                if dp[y, x] > best[0]: best = (int(dp[y, x]), x - dp[y, x] + 1, y - dp[y, x] + 1)
    return best


def tire_prepare(a, U, side):
    """a — картинка колеса (H, W, 4 uint8), U (n, 3, 2) — развёртка треугольников в долях картинки, side (n,) — боковины (диск); -> (вырезка RGBA uint8, uv по вершинам (3n, 2) float32, ключ) или None"""
    H, W = a.shape[:2]
    rub = np.array(TIRE_RUBBER, np.uint8)
    S = U[side].reshape(-1, 2)
    if ((S.max(0) - S.min(0)) > 1.1).any(): TIRE_STAT['skipped'] += 1; TIRE_WHY.append('повтор развёртки'); return None
    base = np.floor(S.min(0) + 1e-3); k = np.array([W, H], np.float64)
    Pp = (U - base) * k                                                    # пиксели исходной картинки (развёртка всех треугольников)
    Ps = Pp[side]
    lo = np.floor(Ps.reshape(-1, 2).min(0) + 1e-3).astype(int); hi = np.ceil(Ps.reshape(-1, 2).max(0) - 1e-3).astype(int)
    w2 = max(16, 1 << int(np.ceil(np.log2(max(hi[0] - lo[0], 1))))); h2 = max(8, 1 << int(np.ceil(np.log2(max(hi[1] - lo[1], 1)))))
    lo = lo - np.array([(w2 - (hi[0] - lo[0])) // 2, (h2 - (hi[1] - lo[1])) // 2])   # запас до степени двойки — поровну по сторонам
    if w2 > W * 2 or h2 > H * 2: TIRE_STAT['skipped'] += 1; TIRE_WHY.append('вырезка больше атласа'); return None
    has_tread = bool((~side).any())
    for _ in range(3):   # не хватило места под квадрат резины 8x8 вне боковин — вырезка вдвое выше (потом шире), до трёх раз
        ys = (lo[1] + np.arange(h2)) % H; xs = (lo[0] + np.arange(w2)) % W
        crop = a[ys][:, xs].copy(); Pc = Ps - lo.astype(np.float64)
        used = _tri_mask(Pc, w2, h2)
        near = _dilate(used, 2); sq = _big_square(~near) if has_tread else (0, 0, 0)
        if not has_tread or sq[0] >= TIRE_BLOCK: break
        TIRE_STAT['grown'] += 1
        if h2 <= w2: lo = lo - np.array([0, 0]); h2 *= 2
        else: w2 *= 2
    else: TIRE_STAT['skipped'] += 1; TIRE_WHY.append('нет места под резину'); return None
    # 1) пиксели вокруг боковин: ближайшие использованные (волной); 2) всё дальше двух пикселей — резина
    filled = used.copy(); rgb = crop[..., :3].astype(np.float64)
    for _ in range(3):
        acc = np.zeros_like(rgb); cnt = np.zeros((h2, w2))
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            sh = np.roll(filled, (dy, dx), (0, 1)); sr = np.roll(rgb, (dy, dx), (0, 1))
            if dy: sh[0 if dy > 0 else -1, :] = False
            if dx: sh[:, 0 if dx > 0 else -1] = False
            acc += sr * sh[..., None]; cnt += sh
        new = ~filled & (cnt > 0); rgb[new] = acc[new] / cnt[new][:, None]; filled |= new
    crop[..., :3] = np.round(rgb).astype(np.uint8); crop[..., 3] = 255
    crop[~near, :3] = rub
    key = 'tire1061:%dx%d:%s' % (w2, h2, _hashlib.sha1(crop.tobytes() + bytes(str(sq), 'ascii')).hexdigest()[:16])
    uvi = ((Pp - lo.astype(np.float64)) / np.array([w2, h2], np.float64)).reshape(-1, 2)   # (3*ntri, 2): боковины сдвинуты в вырезку
    if has_tread:
        cx, cy = (sq[1] + sq[0] / 2) / w2, (sq[2] + sq[0] / 2) / h2
        uvi[np.repeat(~side, 3)] = (cx, cy)
    return crop, uvi.astype(np.float32), key


def tire_tex(url, pos, uv, idx, cache, blob):
    """колесо (вершины pos/uv куска w0..w3, индексы idx) -> (номер своей текстуры, uv по индексам (len(idx), 2)) или None (правило не применяется: развёртка на повторы, прозрачность)"""
    if TIRE_OLD or not url or 'texq' not in globals(): return None
    TIRE_STAT['wheels'] += 1
    img = Image.open(ASSETS / url).convert('RGBA')
    if url in PNG_SRC and os.environ.get('SHR_PNG_FLIP', '1') != '0': img = img.transpose(Image.FLIP_TOP_BOTTOM)
    U = uv[idx].reshape(-1, 3, 2).astype(np.float64); Tp = pos[idx].reshape(-1, 3, 3).astype(np.float64)
    n = np.cross(Tp[:, 1] - Tp[:, 0], Tp[:, 2] - Tp[:, 0]); side = n[:, 0] ** 2 >= .49 * ((n * n).sum(1) + 1e-12)
    if not side.any():   # только протектор: общая текстура резины
        im = Image.new('RGBA', (16, 8), TIRE_RUBBER + (255,)); t = texq.load_img('tire1061:rubber', im, cache, blob); TIRE_STAT['fixed'] += 1
        return t, np.tile(np.array([[.5, .5]], np.float32), (len(idx), 1))
    r = tire_prepare(np.asarray(img, np.uint8), U, side)
    if r is None: return None
    crop, uvi, key = r
    if key not in cache: TIRE_STAT['new'] += 1
    t = texq.load_img(key, Image.fromarray(crop, 'RGBA'), cache, blob); TIRE_STAT['fixed'] += 1
    return t, uvi


def car_prims(meta, raw, conv_cache, blob, bake_matrix, light=None, tire=True):
    """-> [(имя объекта (сустав), tex, flags, P, UV, C)]; P в осях сустава (bake_matrix=False) или в позе покоя.
    light — свет уровня (tools/level_light.level_light): светотень CAR_AMB × ambS + CAR_DIF × dirS по каналам R, G, B
    (29.09, рецепт §12: машины освещаются на ходу группой sun уровня, форма — прежняя, принятая на уровне 1; множители —
    средняя освещённость уровня к уровню 1). Нет света или уровень 1 — множители 1, байты как до 29.09."""
    aS = np.array(light['ambS'] if light else (1.0, 1.0, 1.0), np.float64)[None, :]
    dS = np.array(light['dirS'] if light else (1.0, 1.0, 1.0), np.float64)[None, :]
    out = []
    for obj in meta['objects']:
        M = np.array(obj['matrix'], np.float64).reshape(4, 4) if bake_matrix else np.eye(4)
        for prim in meta['meshes'].get(obj['mesh'], []):
            at = prim['attributes']
            g = lambda k, w: np.frombuffer(raw, '<f4', at[k][1], at[k][0]).reshape(-1, w)
            pos, uv = g('position', 3), g('uv', 2)
            idx = np.frombuffer(raw, '<u4', at['indices'][1], at['indices'][0])
            P = (np.c_[pos, np.ones(len(pos))] @ M)[:, :3]
            if 'normal' in at: N = g('normal', 3) @ M[:3, :3]
            else:   # нормали нет — по граням, усреднённые в вершинах
                N = np.zeros_like(P); T = P[idx].reshape(-1, 3, 3); fn = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
                for k in range(3): np.add.at(N, idx.reshape(-1, 3)[:, k], fn)
            N = N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
            k = np.clip(CAR_AMB * aS + (CAR_DIF * dS) * np.maximum(N @ CAR_SUN, 0)[:, None] + (0.08 * dS) * N[:, 1:2], 0, 1)
            c = np.round(k * 255).astype(np.uint32)   # (n, 3): R, G, B
            abgr = (0xFF << 24) | (c[:, 2] << 16) | (c[:, 1] << 8) | c[:, 0]
            mat = meta['materials'].get(prim['shader'], {})
            flags = (1 if mat.get('alpha') else 0) | (2 if mat.get('translucent') or mat.get('blend') else 0)
            t = load_tex(mat.get('textureUrl', ''), conv_cache, blob, blend=bool(flags & 2))
            tf = None
            if tire and re.fullmatch(r'w[0-3]', obj['name']) and not flags & 3:   # №1061: своя текстура колеса, резина чёрная (tire=False — дальняя версия трафика: суп как был, поверх тёмные колёса №866)
                tf = tire_tex(mat.get('textureUrl', ''), pos, uv, idx, conv_cache, blob)
                if tf: t, uvx = tf
            out.append((obj['name'], t, flags, P[idx].astype(np.float32), (uvx if tf else uv[idx]).astype(np.float32), abgr[idx].astype(np.uint32),
                        np.array(obj['matrix'], np.float64).reshape(4, 4)))
    if CARGAP_OLD: return [e[:6] for e in out]
    # проверка после стягивания: правило не должно ухудшать модель — пробуем 15, 8 и 4 мм, берём лучший по числу зазоров, при росте зазоров или граничных
    # вершин (схлопывание) деталь остаётся как была (otto_v: 48 -> 52 при 15 мм)
    b0 = car_gap_metric(out, bake_matrix); best = None
    for mm in (CARGAP_MM, CARGAP_MM / 2, CARGAP_MM / 4):
        cand, nm = car_close_gaps(out, bake_matrix, mm)
        if not nm: continue
        ce = [c + (e[6],) for c, e in zip(cand, out)]
        b1 = car_gap_metric(ce, bake_matrix)
        if b1[1] <= b0[1] and b1[0] <= b0[0] and (best is None or b1[1] < best[1][1]): best = (cand, b1, nm, mm)
    if best and best[1][1] < b0[1]:
        if not bake_matrix: print(f'  щели деталей (506): {best[3]:g} мм, стянуто граничных вершин {best[2]}, зазоров {b0[1]} -> {best[1][1]}', flush=True)
        return best[0]
    return [e[:6] for e in out]

def car_export(conv, cache, blob, carp, car, light=None):
    """машина игрока уровня (carp — модель, car — имя скриптов cars/<car>.con) -> car_groups, lift, car1 (данные для секции CAR1
    без номеров текстур level.shr), доп. текстуры. light — свет уровня (tools/level_light): светотень кузова и остова"""
    import car_dump as cd
    huskp = GAME / 'art' / 'cars' / 'huska.p3d'
    groups, extra = {}, set()
    if not carp.exists(): return groups, 0.0, None, extra
    root = read(carp); conv.resources(root)
    comp = next(c for c in root.children if c.id == 0x4512)
    _, p = string(comp.data); skname, _ = string(comp.data, p)
    J = cd.skeleton(next(c for c in root.children if c.id == 0x4500 and string(c.data)[0] == skname))
    jn = {j[0]: i for i, j in enumerate(J)}
    jpos = np.array([[j[3][3, 0], j[3][3, 1], -j[3][3, 2]] for j in J])      # SHR: z зеркалим
    conv.export(carp, TN('psp_car'), vehicle=True)
    meta = json.loads((OUT / (TN('psp_car') + '.json')).read_text()); raw = (OUT / (TN('psp_car') + '.bin')).read_bytes()
    lo_y, wr = 1e9, 0.0
    for name, t, fl, P, UV, C in car_prims(meta, raw, cache, blob, False, light):
        j = jn[name]; lo_y = min(lo_y, float((P[:, 1] + jpos[j, 1]).min()))
        if name.startswith('w') and name[1:].isdigit(): wr = max(wr, float(np.abs(P[:, 1]).max()))
        v = verts(P, UV, C)
        groups.setdefault(((j, 0), t, fl), []).append((v, np.arange(len(v) // 3), v, np.ones(len(v) // 3, bool)))
    for ext in ('json', 'bin'): (OUT / f'{TN("psp_car")}.{ext}').unlink()
    lift = -lo_y
    husk_lift = 0.0
    if huskp.exists():
        conv.export(huskp, TN('psp_husk'), vehicle=True)
        meta = json.loads((OUT / (TN('psp_husk') + '.json')).read_text()); raw = (OUT / (TN('psp_husk') + '.bin')).read_bytes()
        hp = car_prims(meta, raw, cache, blob, True, light)
        husk_lift = -min(float(P[:, 1].min()) for _, _, _, P, _, _ in hp)
        for name, t, fl, P, UV, C in hp:
            v = verts(P, UV, C)
            groups.setdefault(((0, 1), t, fl), []).append((v, np.arange(len(v) // 3), v, np.ones(len(v) // 3, bool)))
        for ext in ('json', 'bin'): (OUT / f'{TN("psp_husk")}.{ext}').unlink()
    # шарниры: родной PhysicsObject кузова; у 24 моделей BV стоит первым
    hinges = []
    po = car_physics_object(root)
    if po is None: raise ValueError(f'{carp.name}: no PhysicsObject')
    for jc in (c for c in po.children if c.id == 0x7011020):
        idx, vol, stiff, amax, amin, dof = struct.unpack_from('<I4fI', jc.data)
        if dof != 1 or J[idx][0] not in CAR_HINGES: continue
        vec = next((struct.unpack_from('<3f', v.data) for v in jc.children if v.id == 0x7011002), (0, 0, 0))
        ax, sg, zone = CAR_HINGES[J[idx][0]]
        hinges.append((idx, ax, sg, zone, amax, stiff, vec[0], vec[1], -vec[2]))
    # битые текстуры: не в шейдерах -> грузим сами
    texnames = [string(c.data)[0] for c in root.children if c.id == 0x19000]
    dams = []
    for d in texnames:
        if 'Dam' not in d: continue
        norm = d.replace('Dam', 'Norm'); zone = next((z for k, z in CAR_ZONES.items() if k + 'Dam' in d), -1)
        un, ud = conv.textures.get(norm.lower()), conv.textures.get(d.lower())
        if zone < 0 or not un or not ud: continue
        tn, td = load_tex(un, cache, blob), load_tex(ud, cache, blob); extra |= {tn, td}
        dams.append((tn, td, zone))
    # блики стоп-сигналов / заднего хода (BillboardQuadGroup на суставах brake1..4 / rev1..4)
    sh = cd.shaders(root); flares = []
    for g, s, q, mode, t, col, w, h, dist, zt, zw in cd.billboards(root):
        jname = g[:-5] if g.endswith('Shape') else g
        if jname not in jn: continue
        kind = 0 if jname.startswith('brake') else 1 if jname.startswith('rev') else -1
        if kind < 0: continue
        url = conv.textures.get(sh.get(s, ('', {}))[1].get('TEX', [''])[0].lower())
        ti = load_tex(url, cache, blob, blend=True) if url else -1   # блик рисуется полупрозрачным
        if ti >= 0: extra.add(ti)
        a, r, gg, b = (col >> 24) & 255, (col >> 16) & 255, (col >> 8) & 255, col & 255
        flares.append((jn[jname], kind, {'YAX': 0, 'LYAX': 1, 'AAX': 2}.get(mode, 0), ti, (a << 24) | (b << 16) | (gg << 8) | r,
                       w, h, dist, t[0], t[1], -t[2]))
    con = (GAME / 'scripts' / 'cars' / f'{car}.con').read_text(errors='replace')
    m = re.search(r'SetShadowAdjustments\s*\(([^)]*)\)', con)
    shadow = [float(x) for x in m.group(1).split(',')] if m else [0.0] * 8
    m = re.search(r'SetMaxWheelTurnAngle\s*\(([^)]*)\)', con)
    max_steer = np.radians(float(m.group(1))) if m else np.radians(30)
    car1 = dict(joints=[(J[i][0], *jpos[i], J[i][1]) for i in range(len(J))], hinges=hinges, dams=dams, flares=flares,
                lift=lift, husk_lift=husk_lift, wheel_rad=wr or 0.38, max_steer=float(max_steer), shadow=(shadow + [0.0] * 8)[:8],
                info=car_info(car, carp))
    inf = car1['info']
    print(f'машина: {len(J)} суставов, {len(groups)} групп кусков (с остовом), lift {lift:.3f}, остов {husk_lift:.3f}, '
          f'колесо R {wr:.3f}, шарниров {len(hinges)}, битых текстур {len(dams)}, бликов {len(flares)}')
    print(f'  машина {car}: кузов {2 * inf["halfW"]:.2f} x {2 * inf["halfL"]:.2f} м (сдвиг {inf["bodyOfs"]:+.2f}), низ {inf["bodyLo"]:.2f} '
          f'крыша {inf["bodyHi"]:.2f}, колёса ±{inf["wheelX"]:.2f} / +{inf["wheelF"]:.2f} −{inf["wheelR"]:.2f} R {inf["wheelRad"]:.2f}; '
          f'сиденье ({inf["seat"][0]:.2f}, {inf["seat"][2]:.2f}), дверь ({inf["door"][0]:.3f}, {inf["door"][1]:.3f}), '
          f'якорь ({inf["seatA"][0]:.3f}, {inf["seatA"][1]:.3f}); камера {inf["cam"][1]:.2f} рад {inf["cam"][2]:.2f} м')
    return groups, lift, car1, extra


def car_measure(carp):
    """Мерка машины по модели (для правила кузова): свой Converter — чужие шейдеры/текстуры в экспорт уровня не попадают.
    -> dict: lift, halfW, halfL, bodyOfs (меш сустава 0 в позе покоя, SHR), wheelX/F/R (суставы w0..w3), wheelRad (меш колеса),
    lo (низ меша кузова над землёй), roof (верх кузова у оси), dl (сиденье водителя, SHR), cams [(index, 6 чисел)], doors"""
    import car_dump as cd
    root = read(carp)
    comp = next(c for c in root.children if c.id == 0x4512)
    _, p = string(comp.data); skname, _ = string(comp.data, p)
    J = cd.skeleton(next(c for c in root.children if c.id == 0x4500 and string(c.data)[0] == skname))
    jn = {j[0]: i for i, j in enumerate(J)}
    jpos = np.array([[j[3][3, 0], j[3][3, 1], -j[3][3, 2]] for j in J])
    cv = Converter(); cv.export(carp, TN('psp_carm'), vehicle=True)
    meta = json.loads((OUT / (TN('psp_carm') + '.json')).read_text()); raw = (OUT / (TN('psp_carm') + '.bin')).read_bytes()
    for ext in ('json', 'bin'): (OUT / f'{TN("psp_carm")}.{ext}').unlink()
    body, ends, ys, wr = [], [], [], 0.0
    for obj in meta['objects']:
        j = jn[obj['name']]
        for prim in meta['meshes'].get(obj['mesh'], []):
            at = prim['attributes']
            pos = np.frombuffer(raw, '<f4', at['position'][1], at['position'][0]).reshape(-1, 3).astype(np.float64)
            ys.append(float((pos[:, 1] + jpos[j, 1]).min()))     # как car_export: lift по всем кускам в позе покоя
            if j == 0: body.append(pos + jpos[0])
            if re.fullmatch(r'w\d', obj['name']): wr = max(wr, float(np.abs(pos[:, 1]).max()))
            else: ends.append((pos + jpos[j])[:, 2])              # длина — все куски, кроме колёс (капот и багажник — концы машины)
    lift = -min(ys); B = np.concatenate(body); Z = np.concatenate(ends)
    Wp = np.array([jpos[jn[f'w{k}']] for k in range(4)])
    half = float(np.abs(B[:, 0]).max()); axis = np.abs(B[:, 0]) <= CAR_ROOF_BAND * half   # ширина — кузов (двери с зеркалами — нет)
    cams = sorted(struct.unpack_from('<I6f', c.data) for c in root.children if c.id == 0x3000100)
    return dict(lift=lift, halfW=half, halfL=float(Z.max() - Z.min()) / 2, bodyOfs=-float(Z.max() + Z.min()) / 2,
                wheelX=float(np.abs(Wp[:, 0]).mean()), wheelF=-float(Wp[:, 2].min()), wheelR=float(Wp[:, 2].max()), wheelRad=wr,
                lo=float(B[:, 1].min()) + lift, roof=float(B[axis, 1].max()) + lift, dl=jpos[jn['dl']].tolist() if 'dl' in jn else None,
                cams=cams)


_car_ref = {}
def car_info(car, carp):
    """Блок машины CAR1 v2 по правилу выше (одно для всех машин). Эталон CAR_REF меряется тем же car_measure."""
    if CAR_REF not in _car_ref: _car_ref[CAR_REF] = car_measure(GAME / 'art' / 'cars' / f'{CAR_REF}.p3d')
    R, m = _car_ref[CAR_REF], car_measure(carp)
    g = {k: m[k] + (CAR_REF_GEOM[k] - R[k]) for k in ('halfW', 'halfL', 'bodyOfs', 'wheelX', 'wheelF', 'wheelR', 'wheelRad')}
    g['bodyLo'] = max(0.15, m['lo'] + (CAR_REF_GEOM['bodyLo'] - R['lo']))      # 0.15 — как у машин трафика (traffic_export)
    g['bodyHi'] = m['roof'] + (CAR_REF_GEOM['bodyHi'] - R['roof'])
    g['bandLo'] = min(CAR_REF_BAND[0], g['bodyLo'] + (CAR_REF_BAND[0] - CAR_REF_GEOM['bodyLo']))
    g['bandHi'] = g['bodyHi'] + (CAR_REF_BAND[1] - CAR_REF_GEOM['bodyHi'])
    g['step'] = CAR_STEP
    # сиденье — сустав dl; якорь посадки сдвинут от сиденья так же, как у эталона (конец клипа посадки Гомера ставит таз на
    # сиденье); точка у двери — не ближе к борту, чем у эталона (Гомер твёрдый о кузов: изнутри коробки кузова он к ней не дойдёт)
    dl = m['dl'] or R['dl']
    sa = (dl[0] + (CAR_REF_DOOR[0] - R['dl'][0]), dl[2] + (CAR_REF_DOOR[1] - R['dl'][2]))
    clear = -CAR_REF_DOOR[0] - CAR_REF_GEOM['halfW']
    door = (min(sa[0], -(g['halfW'] + clear)), sa[1])
    near = next((c for c in m['cams'] if c[0] < 256), None) or next((c for c in R['cams'] if c[0] < 256))
    far = next((c for c in m['cams'] if c[0] >= 256), None)
    con = (GAME / 'scripts' / 'cars' / f'{car}.con').read_text(errors='replace')
    hd = re.search(r'SetHasDoors\s*\(\s*(\d+)', con)
    return dict(car=car, **g, seat=tuple(dl), seatA=sa, door=door, cam=tuple(near[1:7]), camFar=far[3] if far else 0.0,
                flags=1 if (not hd or hd.group(1) != '0') else 0)


def car_info_bytes(inf):
    g = [inf[k] for k in ('halfW', 'halfL', 'bodyOfs', 'wheelX', 'wheelF', 'wheelR', 'wheelRad', 'bodyLo', 'bodyHi', 'bandLo', 'bandHi', 'step')]
    b = struct.pack(CAR_INFO_FMT, inf['car'].encode()[:15], *g, *inf['seat'], *inf['seatA'], *inf['door'], *inf['cam'], inf['camFar'], inf['flags'])
    return b + b'\0' * (-len(b) % 16)


def car_section(car1, tmap):
    """Версия 3 (29.09, свет уровня): сразу за блоком машины — блок LGT1 (128 Б, viewer/levlight.h, tools/level_light.block):
    группа света sun уровня и множители к уровню 1 для персонажей (skin.h). Версия 2 — без него."""
    if not car1: return b''
    info = car_info_bytes(car1['info']) if car1.get('info') else b''
    lgt = level_light.block(car1['light']) if info and car1.get('light') else b''
    out = bytearray(b'CAR1' + struct.pack('<7I', (3 if lgt else 2) if info else 1, len(car1['joints']), len(car1['hinges']), len(car1['dams']), len(car1['flares']), 0, 0))
    out += struct.pack('<4f8f', car1['lift'], car1['husk_lift'], car1['wheel_rad'], car1['max_steer'], *car1['shadow'])
    for name, x, y, z, par in car1['joints']:
        out += struct.pack('<12s3fI', name.encode()[:11], x, y, z, par)
    for j, ax, sg, zone, amax, stiff, cx, cy, cz in car1['hinges']:
        out += struct.pack('<BBbB5f', j, ax, sg, zone, amax, stiff, cx, cy, cz)
    for tn, td, zone in car1['dams']:
        out += struct.pack('<HHB3x', tmap.get(tn, 0xFFFF), tmap.get(td, 0xFFFF), zone)
    for j, kind, mode, ti, abgr, w, h, dist, ox, oy, oz in car1['flares']:
        out += struct.pack('<BBBxHxxI6f', j, kind, mode, tmap.get(ti, 0xFFFF), abgr, w, h, dist, ox, oy, oz)
    if info:
        out += b'\0' * (-len(out) % 16); struct.pack_into('<2I', out, 24, len(out), len(info)); out += info
        out += lgt   # LGT1 — смещение блока машины + его длина (оба кратны 16)
    out += b'\0' * (-len(out) % 16)
    return bytes(out)

# Слои-наклейки (Ф2, правки Роба 37 и 43). Две поверхности, лежащие параллельно с малым зазором (разметка на дороге, коврик
# на крыльце, вывеска и наличник на стене, тень дерева на траве), на PSP получают почти одну 16-битную глубину и мигают:
# окно GE — 65535·f/(f−n)·(1 − n/d) (DepthRange(65535, 0), GEQUAL, viewer/main.c), ближняя плоскость NEAR_Z 1.5 м, дальняя
# 260 м -> два слоя с зазором g на дальности d различаются на g·K/d² единиц, K ≈ 98 900; меньше единицы — с d ≈ √(g·K):
# 3 мм — 17 м, 3 см — 54 м, 10 см — 99 м. Ближние версии кусков рисуются до ~100 м (LOD_DIST 50 м + клетка 40 м), поэтому
# слоем считается всё до LAYER_GAP = 11 см (D²/K = 10.1 см и запас; до 29.09 — только до 3 мм: слои 3–30 мм у дома
# Симпсонов мигали вдали). Проверка — tools/seams.py (раздел «слои»).
# Правило (общее для всех уровней и комнат; данных о порядке слоёв на диске нет — правило порта из устройства глубины):
#   * слой A — треугольник над подложкой B, параллельно, центр одного над другим, зазор < LAYER_GAP; слой — полупрозрачное
#     над непрозрачным, иначе меньшее по исходному треугольнику (дальнему предку). Движок рисует слой со сдвигом глубины
#     к камере LAYER_BIAS (флаг 64), наклейку на наклейке — с двойным и т. д. до 4 этажей (флаги 256 = +1, 1024 = +2), иначе
#     второй этаж спорил бы с первым;
#   * подложка по другую сторону от слоя сама становится слоем над ним (со своей стороны она ближе к зрителю);
#   * если все подложки A по одну сторону от неё (зазор ≥ SIDE_EPS) — обход A разворачивается лицом от них и кусок рисуется
#     с отсечением изнанки (флаг 512). Сдвиг глубины без этого «протаскивал» бы слой сквозь подложку, если смотреть с её
#     стороны: низ ящика под асфальтом, наличник с изнанки забора, тень под травой вдали вылезали поверх. В оригинале
#     (24-бит глубина PS2) спорить нечему — там видно ближнее. Цена: свес A за край подложки с её изнанки не виден (сводка);
#   * наклейка в самой плоскости подложки (зазор < SIDE_EPS, шаг решётки) и без других подложек стороны не имеет — сдвиг
#     без отсечения;
#   * подложки с обеих сторон («бутерброд»: полка между полками) — A закрыта с любой стороны, сдвиг её только вытащил бы
#     наружу: не слой.
# Решение — по каждому ближнему треугольнику, уже сваренному, на решётке и с разрезанными Т-стыками (как в файле); дальний
# предок кладётся в каждый кусок, где есть его потомки (копия в куске-слое рисуется со сдвигом поверх той же картинки),
# иначе на границе LOD 50 м между кусками была бы дыра.
F_LAYER = 64       # слой: сдвиг глубины к камере LAYER_BIAS (viewer/main.c setFlags)
F_LAYER2 = 256     # этаж слоёв: +1 к этажу (сдвиг = LAYER_BIAS × этаж)
F_LSIDE = 512      # слой со стороной: обход лицом от подложки, рисуется с отсечением изнанки
LAYER_GAP = 0.11   # м (см. выше: зазор, который 16-бит глубина не различает на дальности ближних версий, с запасом)
SIDE_EPS = 0.002   # м: ближе — наклейка в плоскости подложки, стороны нет (точки уже на решётке 2^-9 ≈ 2 мм: мельче — шум решётки)
COVER_PULL = 0.05  # вершины A для сводки «свес за край подложки» — на 5% к центру (вершина на общем краю — не «мимо»)
F_LAYER3 = 1024    # этаж слоёв: +2 (стопка из 4 — полки, бумаги в комнатах)
F_SKYV = 4096      # слой неба с v в [0, 1]: рисуется с GU_CLAMP по v (№511; viewer/main_render.h drawSky), иначе верх/низ слоя смешиваются билинейно
LAYER_LEVELS = 4

def mark_layers(groups, never=None):
    """Ф2 + правки 37/43: tools/coplanar.py по ближним версиям всех кусков. Слои уходят в группы с флагами
    64 (этаж: +256 = +1, +1024 = +2; +512 — сторона и обход лицом от подложки) — см. правило выше.
    never — матрица регионов «никогда не грузятся вместе» (правка 95, tools/regions_export.py): между ними слоёв нет —
    оригинал их вместе не рисует (декорация издали и настоящий район на одном месте)"""
    import coplanar
    gap = LAYER_GAP
    keys = list(groups); P, T, FL, GI, REF, AA, RG = [], [], [], [], [], [], []
    for gi, k in enumerate(keys):
        for pj, part in enumerate(groups[k]):
            v = part[0]; n = len(v) // 3
            if not n: continue
            P.append(np.stack([v['x'], v['y'], v['z']], 1).astype(np.float64).reshape(-1, 3, 3))
            T.append(np.full(n, k[1])); FL.append(np.full(n, k[2])); GI.append(np.full(n, gi)); REF.append((gi, pj, n))
            RG.append(np.full(n, k[3] if len(k) > 3 else -1))
            AA.append(tri_area(np.stack([part[2]['x'], part[2]['y'], part[2]['z']], 1))[part[1]])   # площадь исходного (предка)
    if not P: return
    P, T, FL, GI, AA, RG = np.concatenate(P), np.concatenate(T), np.concatenate(FL), np.concatenate(GI), np.concatenate(AA) * 2, np.concatenate(RG)
    R, N, C, A = coplanar.analyze(P, T, FL, GI, maxgap=gap, same_eps=SIDE_EPS)
    if never is not None and len(R):
        ra, rb = RG[R[:, 0].astype(int)], RG[R[:, 1].astype(int)]
        cross = (ra >= 0) & (rb >= 0) & never[np.maximum(ra, 0), np.maximum(rb, 0)]
        if cross.any(): print(f'  слои: пар из регионов, которые вместе не грузятся, — {int(cross.sum())} (не слои, правка 95)'); R = R[~cross]
    Ach = A   # площадь ребёнка нарезки (как в файле): по ней «бутерброд» — то же, что видит гейт tools/seams.py
    ai, bj, g = R[:, 0].astype(int), R[:, 1].astype(int), R[:, 2]
    n = len(P)
    # «бутерброд» — по всем закрывающим A (пишут глубину, не меньше A): с обеих сторон — A не видна ниоткуда
    cov = ((FL[bj] & 2) == 0) & ((Ach[ai] <= Ach[bj] * 1.001) | ((FL[ai] & 2) != 0)) & (np.abs(g) >= SIDE_EPS)
    ca, cs = ai[cov], np.sign(g[cov])[:, None] * N[bj[cov]]
    cref = np.zeros((n, 3))
    if len(ca):
        o = np.lexsort((np.abs(g[cov]), ca)); f0 = np.r_[True, ca[o][1:] != ca[o][:-1]]; cref[ca[o][f0]] = cs[o][f0]
    sand = np.zeros(n, bool); sand[ca[np.einsum('ij,ij->i', cs, cref[ca]) < 0]] = True
    # кто наклейка, решает исходный треугольник (дальний предок): мелкие дети нарезки подложки бывают меньше детей наклейки;
    # при равных — ребёнок, потом номер группы и треугольника (иначе равные дубли в одной группе остались бы без порядка)
    eqAA = np.abs(AA[ai] - AA[bj]) <= AA[bj] * .001; eqC = np.abs(Ach[ai] - Ach[bj]) <= Ach[bj] * .001
    smaller = (AA[ai] < AA[bj] * .999) | (eqAA & (Ach[ai] < Ach[bj] * .999)) | (eqAA & eqC & ((GI[ai] > GI[bj]) | ((GI[ai] == GI[bj]) & (ai > bj))))
    # кто слой: полупрозрачное над непрозрачным; иначе меньшее по исходнику. Подложкой бывает и полупрозрачное (ковёр-тень
    # под ножкой мебели): со сдвигом оно тоже проходит проверку глубины, поэтому то, что над ним, обязано быть выше этажом.
    # Две полупрозрачные глубину не пишут — друг с другом не спорят
    blA, blB = (FL[ai] & 2) != 0, (FL[bj] & 2) != 0
    ok = ~(blA & blB) & ((blA & ~blB) | ((blA == blB) & smaller))
    # центр A над B, но слой — B: пара переворачивается, зазор — центра B от плоскости A (плоскости параллельны)
    okr = ~(blA & blB) & ~ok
    gr = -g * np.sign(np.einsum('ij,ij->i', N[ai], N[bj]))
    ai, bj, g = np.r_[ai[ok], bj[okr]], np.r_[bj[ok], ai[okr]], np.r_[g[ok], gr[okr]]
    def sides(ai, bj, g):   # опорная сторона слоя — от ближайшей подложки со стороной
        far_ = np.abs(g) >= SIDE_EPS
        svp = np.where(far_, np.sign(g), 0)[:, None] * N[bj]
        ref = np.zeros((n, 3)); hasref = np.zeros(n, bool)
        fi = np.nonzero(far_)[0]
        if len(fi):
            o = fi[np.lexsort((np.abs(g[fi]), ai[fi]))]; first = np.r_[True, ai[o][1:] != ai[o][:-1]]
            ref[ai[o][first]] = svp[o][first]; hasref[ai[o][first]] = True
        return far_, svp, ref, hasref
    # подложка с другой стороны от слоя A (не закрывающая его целиком — иначе «бутерброд» выше): слоем становится она над A
    # (со своей стороны она ближе к зрителю), иначе сдвиг A протащил бы его сквозь неё
    far_, svp, ref, hasref = sides(ai, bj, g)
    rv = far_ & (np.einsum('ij,ij->i', svp, ref[ai]) < 0) & ~sand[ai]
    gv = -g * np.sign(np.einsum('ij,ij->i', N[ai], N[bj]))
    ai, bj, g = np.r_[ai[~rv], bj[rv]], np.r_[bj[~rv], ai[rv]], np.r_[g[~rv], gv[rv]]
    far_, svp, ref, hasref = sides(ai, bj, g)
    lay = np.zeros(n, bool); lay[ai] = True; lay &= ~sand
    pm = ~sand[ai]; pa, pb = ai[pm], bj[pm]
    lev = lay.astype(int)   # этаж: выше каждой своей подложки (пары без циклов: роль по исходнику и стороне)
    for _ in range(LAYER_LEVELS):
        nl = lay.astype(int); np.maximum.at(nl, pa, lev[pb] + 1); nl = np.minimum(nl, LAYER_LEVELS)
        if (nl == lev).all(): break
        lev = nl
    # сторона — если все подложки слоя с одной стороны (разворот выше мог оставить спор у подложки — тогда без стороны)
    conflict = np.zeros(n, bool); conflict[ai[far_ & (np.einsum('ij,ij->i', svp, ref[ai]) < 0)]] = True
    sided = lay & hasref & ~conflict
    # сводка: свес за край подложки (вершина A, чуть к центру, не над подложкой с той же стороны)
    over = np.zeros(n, bool); ci = np.nonzero(sided)[0]
    if len(ci):
        Q = (P[ci] + (C[ci][:, None, :] - P[ci]) * COVER_PULL).reshape(-1, 3); own = np.repeat(ci, 3)
        R2 = coplanar.analyze_pts(P, T, GI, own, Q, maxgap=gap, same_eps=SIDE_EPS)
        qi, b2, g2 = R2[:, 0].astype(int), R2[:, 1].astype(int), R2[:, 2]
        same = np.einsum('ij,ij->i', np.sign(g2)[:, None] * N[b2], ref[own[qi]]) > 0
        good = ((FL[b2] & 2) == 0) & (same | (np.abs(g2) < SIDE_EPS))
        cov = np.zeros(len(Q), bool); cov[qi[good]] = True
        over[ci] = ~cov.reshape(-1, 3).all(1)
    st = dict(tris=0, sided=0, lev2=0, over=int(over.sum()), sand=int(sand.sum()), dup=0)
    off = 0
    def swap(arr, idx):
        if len(idx): arr[3 * idx + 1], arr[3 * idx + 2] = arr[3 * idx + 2].copy(), arr[3 * idx + 1].copy()
    def normals(arr):
        X = np.stack([arr['x'], arr['y'], arr['z']], 1).astype(np.float64).reshape(-1, 3, 3)
        return np.cross(X[:, 1] - X[:, 0], X[:, 2] - X[:, 0])
    for gi, pj, cnt in REF:
        m, lv, sd, rf = lay[off:off + cnt], lev[off:off + cnt], sided[off:off + cnt], ref[off:off + cnt]; off += cnt
        if not m.any(): continue
        k = keys[gi]; vn, anc, vf, keep = groups[k][pj]
        nf = len(keep)
        code = np.where(m, F_LAYER | np.where((lv - 1) & 1, F_LAYER2, 0) | np.where((lv - 1) & 2, F_LAYER3, 0) | np.where(sd, F_LSIDE, 0), 0)
        vn = vn.copy(); swap(vn, np.nonzero(sd & (np.einsum('ij,ij->i', normals(vn), rf) < 0))[0])   # лицом от подложки
        subs, used = {}, np.zeros(nf, int)
        for c in np.unique(code):
            tm = code == c
            am = np.zeros(nf, bool); am[anc[tm]] = True; used += am
            loc = np.cumsum(am) - 1
            far = vf[np.repeat(am, 3)].copy()
            if c & F_LSIDE:   # копия предка — лицом туда же, куда её потомки в этом куске
                fs = np.zeros((nf, 3)); np.add.at(fs, anc[tm], rf[tm])
                swap(far, np.nonzero(np.einsum('ij,ij->i', normals(far), fs[am]) < 0)[0])
            subs[int(c)] = (vn[np.repeat(tm, 3)], loc[anc[tm]], far, keep[am])
        st['dup'] += int(((used > 1) & keep).sum())
        groups[k][pj] = subs.pop(0, (vn[:0], anc[:0], vf[:0], keep[:0]))
        for c, part in subs.items():
            groups.setdefault((k[0], k[1], k[2] | c) + tuple(k[3:]), []).append(part)
            t_ = len(part[0]) // 3; st['tris'] += t_
            if c & F_LSIDE: st['sided'] += t_
            if c & (F_LAYER2 | F_LAYER3): st['lev2'] += t_
    for k in [k for k in groups if not any(len(p[0]) for p in groups[k])]: del groups[k]
    print(f'слои (флаг {F_LAYER}, зазор < {gap * 1000:g} мм): {int(lay.sum())} ближних треуг.; со стороной (обход от подложки, '
          f'отсечение, флаг {F_LSIDE}) {st["sided"]} (свес за край подложки {st["over"]}), этаж 2–4 (флаги {F_LAYER2}/{F_LAYER3}) '
          f'{st["lev2"]}; «бутерброд» (не слой) {st["sand"]}; дальних предков в двух кусках {st["dup"]}')

# Правка Роба 496 (10.10, «световые пятна от фонарей L5 сделать заново»). Диск: level.mfk уровня SetStatepropShadow("l5_streetlamp", "l5_streetlamp_lightpoolShape")
# — у каждого фонаря из списка в основании стоит меш пятна света (art/l05_fx.p3d: квадрат 11,7 м на 0,083 м над основанием, шейдер
# mkr_glow_add_m_for_final, BLMD 2 = сложение, текстура glow_add.bmp, цвет вершин (0.73 0.71 0.52), альфа 0,6). Прежняя попытка
# (codex3/content-lights-20261009) рисовала их своим отдельным проходом (lightpool.c): рисунок поверх города вне сортировки
# прозрачного, свой выбор тумана/глубины, текстура и матрица шли мимо слоёв — отсюда пересвет, чёрные края и мерцание кадра
# (GPU FAIL 3 pairs: плоскость в 8 см от земли борется с ней в 16-бит глубине). Теперь это ОБЫЧНЫЕ полупрозрачные аддитивные куски
# города (флаг 2 + бит 24 №447), каждый квадрат — в матрице своего фонаря: порядок прозрачного, туман сложения (glowFogBegin),
# отсутствие записи глубины и сдвиг слоя над землёй (mark_layers) дают штатные пути; никакого нового рендерера и проходов.
# Пятно привязано к месту фонаря на диске (DynaPhys/AnimDynaPhys), сбитый фонарь оставляет пятно — состояние не отслеживается.
# SHR_LIGHTPOOL_OLD=1 — без пятен (как до правки)
LIGHTPOOL_RE = re.compile(r'SetStatepropShadow\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\)')

def lightpool_prims(level, scenes, art, conv, cache, blob, add_city, reg_of):
    """-> (число пятен, число треугольников); кладёт куски в город через add_city(prims, reg)"""
    if os.environ.get('SHR_LIGHTPOOL_OLD') == '1': return 0, 0
    scr = GAME / f'scripts/missions/level0{level}/level.mfk'
    fx = art / f'l0{level}_fx.p3d'
    if not scr.exists() or not fx.exists(): return 0, 0
    binds = LIGHTPOOL_RE.findall(re.sub(r'//[^\n]*', '', scr.read_text(errors='replace')))
    if not binds: return 0, 0
    shapes = {sh for _, sh in binds}
    if len(shapes) != 1: raise SystemExit(f'пятна света: {len(shapes)} разных мешей {shapes}, ожидался один')
    conv.resources(read(fx))
    tmp = f'psp_lp_tmp{TMP_SUFFIX}'
    conv.export(fx, tmp, mesh_name=next(iter(shapes)))
    meta = json.loads((OUT / f'{tmp}.json').read_text()); raw = (OUT / f'{tmp}.bin').read_bytes()
    for ext in ('json', 'bin'): (OUT / f'{tmp}.{ext}').unlink()
    obj0 = meta['objects'][0]
    wanted = {lamp.lower() for lamp, _ in binds}
    seen, n_pool, n_tri = set(), 0, 0
    for scene in scenes:
        for ent in read(art / f'{scene}.p3d').children:
            if ent.id not in (0x3f00002, 0x3f0000e): continue
            name = string(ent.data)[0].lower(); base = name[:-6] if name.endswith('_shape') else name
            if base not in wanted: continue
            for M in col_instances(ent):
                key = tuple(np.round(M[3, :3], 2))
                if key in seen: continue
                seen.add(key)
                prims = pack_prims(dict(meta, objects=[dict(obj0, matrix=M.flatten().tolist())]), raw, cache, blob)
                prims = [(t, fl | F_DOUBLE, P, UV, C) for t, fl, P, UV, C in prims]
                add_city(prims, reg_of(scene)); n_pool += 1; n_tri += sum(len(P) // 3 for _, _, P, _, _ in prims)
    return n_pool, n_tri

# Правка Роба 509 (10.10, «пусть за щелью не будет контрастного света: под тёмным тёмное, под светлым светлое»): подложка ПО КЛЕТКАМ вместо одного цвета
# на весь уровень (№437). Для клетки 40 м: средний цвет её земли (текстура диска × цвет вершин с запечённым светом, по площади треугольников, только
# непрозрачная земля |n.y| > 0.5 без слоёв) и глубина плоскости под клеткой: ниже самой низкой земли квадрата 40 м (карта высот COL) и ящика клетки,
# с запасом UNDERLAY_MARGIN. Пишется в Cell.pad (раньше только регион+1 в младшем байте): байт 0 регион, байт 1 глубина плоскости под низом ящика клетки
# в шагах 0,25 м (0..63,75 м), байты 2–3 цвет RGB565 (0 — подложки нет). Движок (viewer/main_render.h cellUnderlayDraw) рисует под каждой видимой
# клеткой квадрат этим цветом: через щель виден цвет соседней земли, а не небо/единый тон. SHR_CELL_UNDERLAY_OLD=1 — без подложки (pad = регион, как раньше)
UNDERLAY_MARGIN, UNDERLAY_STEP, UNDERLAY_GROUND_DY, UNDERLAY_NEIGHBOUR, UNDERLAY_NOHM = 0.3, 0.25, 0.8, 4.0, 3.0   # м: запас под землёй; шаг глубины; земля — не выше низа опоры + DY; соседи — полоса вокруг квадрата
_TEX_AVG = {}

def tex_avg_rgb(blob, t):
    """средний цвет текстуры по записи blob[t] (последний мип, по альфе) -> (r, g, b) 0..1; нет текстуры — белый"""
    if t is None or t < 0 or t >= len(blob): return (1.0, 1.0, 1.0)
    if t in _TEX_AVG: return _TEX_AVG[t]
    import texq
    try:
        lv = texq.decode(blob[t]); a = np.asarray(lv[-1], np.float64).reshape(-1, 4)
        w = a[:, 3:4] / 255.0 + 1e-6; rgb = tuple(float(x) for x in ((a[:, :3] * w).sum(0) / w.sum()) / 255.0)
    except Exception: rgb = (1.0, 1.0, 1.0)
    _TEX_AVG[t] = rgb; return rgb

def cell_underlay(keys, city_chunks, blob, hmap, ck, clo_y, hmin=None, fol=frozenset()):
    """-> Cell.pad без региона: (глубина << 8) | (RGB565 << 16), 0 — нет земли; keys — группы клетки, ck — (cx, cz) клетки 40 м, clo_y — низ ящика клетки.
    Цвет — только с горизонтальной ЗЕМЛИ и дороги: без листвы (текстуры кустов/изгородей fol), воды и полупрозрачного (флаги), без крыш и всего, что выше
    самой низкой опоры карты высот в этой точке больше UNDERLAY_GROUND_DY (hmin — самая низкая опора клетки карты высот, см)""" 
    if os.environ.get('SHR_CELL_UNDERLAY_OLD') == '1': return 0
    sum_, wt = np.zeros(3), 0.0
    for key in keys:
        if key[2] & (1 | 2 | 4 | 64): continue   # альфа-тест, полупрозрачное, небо, наклейка — не земля
        if key[1] in fol: continue                 # листва: кусты, изгороди (текстуры FOL_TEX_RE)
        ta = tex_avg_rgb(blob, key[1])
        for vn, _ in city_chunks[key][0]:
            n3 = len(vn) // 3 * 3
            if not n3: continue
            X = np.stack([vn['x'], vn['y'], vn['z']], 1)[:n3].reshape(-1, 3, 3).astype(np.float64)
            nn = np.cross(X[:, 1] - X[:, 0], X[:, 2] - X[:, 0]); ar = np.linalg.norm(nn, axis=1)
            g = (np.abs(nn[:, 1]) > .5 * np.maximum(ar, 1e-12)) & (ar > 1e-6)
            if hmin is not None and g.any():   # крыши, навесы, мосты над землёй: центр треугольника не выше самой низкой опоры клетки карты высот + DY
                ctr = X.mean(1); gx_ = np.floor((ctr[:, 0] - hmap[0][0]) / CELL).astype(np.int64); gz_ = np.floor((ctr[:, 2] - hmap[0][2]) / CELL).astype(np.int64)
                inb = (gx_ >= 0) & (gx_ < hmap[1]) & (gz_ >= 0) & (gz_ < hmap[2])
                low = np.full(len(ctr), np.inf); low[inb] = hmin[gz_[inb], gx_[inb]]
                g &= ~(ctr[:, 1] > low + UNDERLAY_GROUND_DY)
            if not g.any(): continue
            c = vn['c'][:n3].reshape(-1, 3).astype(np.uint32)
            col = np.stack([(c & 0xFF), (c >> 8 & 0xFF), (c >> 16 & 0xFF)], 2).astype(np.float64).mean(1) / 255.0   # (n,3) средний цвет вершин ABGR -> RGB
            w = (ar[g] / 2)[:, None]
            sum_ += (w * col[g] * np.array(ta)).sum(0); wt += float(w.sum())
    if wt <= 0: return 0
    rgb = np.clip(sum_ / wt, 0, 1)
    v = (int(rgb[0] * 31 + .5) << 11) | (int(rgb[1] * 63 + .5) << 5) | int(rgb[2] * 31 + .5)
    v = max(v, 1)
    lo, W, H, hm = hmap; cx, cz = ck
    nb = UNDERLAY_NEIGHBOUR   # правка 509: и полоса соседей вокруг квадрата — плоскость не выше земли соседней клетки у шва (не торчит в щель)
    gx0 = int(math.floor((cx * CHUNK - nb - lo[0]) / CELL)); gx1 = int(math.floor(((cx + 1) * CHUNK + nb - lo[0]) / CELL))
    gz0 = int(math.floor((cz * CHUNK - nb - lo[2]) / CELL)); gz1 = int(math.floor(((cz + 1) * CHUNK + nb - lo[2]) / CELL))
    ymin = clo_y
    cx0, cx1, cz0, cz1 = max(0, gx0), min(W - 1, gx1), max(0, gz0), min(H - 1, gz1)
    if cx0 <= cx1 and cz0 <= cz1:
        blk = hm[cz0:cz1 + 1, cx0:cx1 + 1].ravel(); blk = blk[blk != -32768]
        if len(blk): ymin = min(ymin, float(blk.min()) / 100.0)
    if gx0 < 0 or gz0 < 0 or gx1 > W - 1 or gz1 > H - 1 or cx0 > cx1 or cz0 > cz1:
        ymin = min(ymin, clo_y - UNDERLAY_NOHM)   # часть квадрата вне карты высот (нет опоры коллизии): запас глубже — под выпирающими кусками соседей
    d = int(min(255, max(0, math.ceil((clo_y - (ymin - UNDERLAY_MARGIN)) / UNDERLAY_STEP))))
    return (d << 8) | (v << 16)

def main(level, out_path=None, scenes=None, coins=True, backdrop_level=None):
    # правка 420 (бонус-игра): scenes — свой список сцен (трасса мини-игры b0N, tools/bonus/export.py), их же ресурсы;
    # coins=False — без меша монеты missions/level0N/level.p3d (сценарий мини-игры ss.mfk его не грузит). По умолчанию — как было
    global WORLD_LIGHT
    WORLD_LIGHT = level_light.level_light(level)   # свет уровня: машина (CAR1 v3 + LGT1) и освещаемые предметы мира (pack_prims)
    art = GAME / 'art'
    own_scenes = scenes is not None
    if not own_scenes:
        scenes = [f'l{level}_terra'] + sorted(p.stem for p in art.glob(f'l{level}r*.p3d')) + sorted(p.stem for p in art.glob(f'l{level}z*.p3d'))
    conv = Converter(); blob = []; cache = {}
    # правка Роба 473 (бонус-трассы): фон уровня, к которому привязана трасса (небо, горизонт, дальние холмы, облака) — только картинка,
    # без коллизии и города; рисуется как небо уровня (вокруг камеры, ужатое). tools/bonus/export.py BACKDROP
    bd_path = art / f'l{backdrop_level}_terra.p3d' if backdrop_level else None
    for path in ([art / f'{s_}.p3d' for s_ in scenes] + ([bd_path] if bd_path else []) if own_scenes else sorted(art.glob(f'l{level}*.p3d'))):
        try: conv.resources(read(path))
        except Exception as e: print('skip', path.name, e)
    # правка Роба 500/416, приёмы 1 и 2 ресёрча (слово Роба 10.10 «сглаживание пикселей, рябь вдалеке, давай сильно»): мип-затухание дальних уровней — у текстур земли
    # (асфальт, разметка дорог, тротуар, бетон, дорожки, трава) и стен (кирпич, сайдинг, штукатурка, фундамент; без окон, дверей, вывесок);
    # плюс положительный сдвиг выбора мипа (наклон x 2^SHR_GROUND_BIAS, по умолчанию 0,75 уровня) только у земли — дальше уходит на размытый мип.
    # SHR_MIPFADE_OLD=1 — без затухания, SHR_GROUND_BIAS_OLD=1 — без сдвига (tools/texq.py fade_mips; GROUND_URLS используется ниже при записи наклона)
    # №504 (бордюр рябит везде, 11.10): sdewlk_corner (угол тротуара с водостоком, в имени опечатка sdewlk, 48 тонких длинных кусков на L1) раньше в класс не входил
    GROUND_RE = r'(asphalt|rd[0-9]*_|sidewlk|sdewlk|cobble|concrete|driveway|path|dirt_strip|grass|field|com_sand|dockwood|opt_grass_path)'
    WALL_RE = r'(?!.*(sign|door|window|glass|logo|letter|flag))(.*brick|.*siding|.*stucco|.*wall|.*_found|gry_found|.*plank|.*panel)'
    if FADE_EXTRA_GROUND: GROUND_RE = GROUND_RE[:-1] + '|' + FADE_EXTRA_GROUND + ')'
    if FADE_EXTRA_WALL: WALL_RE = WALL_RE[:-1] + '|' + FADE_EXTRA_WALL + ')'
    GROUND_URLS = {u for nm_, u in conv.textures.items() if re.match(GROUND_RE, nm_)}
    if 'texq' in globals(): texq.FADE_URLS = GROUND_URLS | {u for nm_, u in conv.textures.items() if re.match(WALL_RE, nm_)}
    print('пропы: сбиваемые объекты…', flush=True)
    prop_ent = prop_entities(scenes, art); prop_type_names = set(prop_ent)
    print(f'  {len(prop_ent)} типов, {sum(len(v["mats"]) for v in prop_ent.values())} экземпляров (класс статики — в городе, как раньше)')
    # стейтпропы (AnimDynaPhys: ящики, автоматы, будки, колонки, кусты-шары…) и обломки BreakableObject — PRP2/SPR1;
    # их меши убираем из города (раньше там стояли все части составного рисунка сразу, без коллизии)
    print('стейтпропы…', flush=True)
    def sp_pack(f, mesh):
        conv.export(f, TN('psp_sp_tmp'), mesh_name=mesh)
        meta_ = json.loads((OUT / (TN('psp_sp_tmp') + '.json')).read_text()); raw_ = (OUT / (TN('psp_sp_tmp') + '.bin')).read_bytes()
        for ext in ('json', 'bin'): (OUT / f'{TN("psp_sp_tmp")}.{ext}').unlink()
        return pack_prims(meta_, raw_, cache, blob)
    def sp_tex(f, sh):
        s_ = conv.shaders.get(sh)
        # только билборды SPR1 — всегда полупрозрачные (viewer/main.c): мипы без масштаба покрытия
        return (load_tex(conv.textures.get(s_['texture'], ''), cache, blob, blend=True), int(s_['blend'])) if s_ else -1
    sprops = prp2_export.StateProps(level, art, sp_pack, sp_tex)
    sprops.report()
    aobj = None   # AnimCollision 0x3F00008 / Anim 0x3F0000C: раньше их составной рисунок никто не ставил (грузовик Duff и др.)
    if os.environ.get('SHR_NO_ANIMOBJ') != '1':   # =1 — как до пакета (сравнение до/после, отрицательный контроль гейта)
        print('анимированные объекты (AnimCollision/Anim)…', flush=True)
        aobj = animobj_export.AnimObjects(level, art, sprops, conv)
        print('  ' + aobj.summary())
    wrench_pts = jumps_export.wrenches(level)   # гаечные ключи (пункт 4): модель — тип SPR1 в конце списка (индексы прежних не сдвигаются)
    if wrench_pts:
        conv.resources(read(art / 'wrench.p3d'))   # шейдеры/текстуры файла ключа
        Tw = sprops.add_wrenches([w_['pos'] for w_ in wrench_pts])
        print(f'гаечные ключи: {len(wrench_pts)} экземпляров wrench, {len(Tw["pieces"])} кусков, {len(Tw["bills"])} билб.')
    for pf, pn, pts in mission_stateprops(level):   # №314: бочки L7 (AddCollectibleStateProp bombbarrel): тип SPR1 скрытым, на локаторах миссий
        conv.resources(read(pf))
        Tm = sprops.add_pickup(pf, pn, pts, hidden=True)
        print(f'предмет миссии {pn}: {len(pts)} экземпляров, {len(Tm["pieces"])} кусков, {len(Tm["bills"])} билб.')
    for pf, pn, pts in EXTRA_PICKUPS:   # правка 420: подборы трасс мини-игры (nitro) — tools/bonus/export.py; у уровней пусто
        conv.resources(read(pf))
        Tp = sprops.add_pickup(pf, pn, pts)
        print(f'подбор {pn}: {len(pts)} экземпляров, {len(Tp["pieces"])} кусков, {len(Tp["bills"])} билб.')
    city_skip = prop_type_names | sprops.city_skip
    groups, coll, sky, roots = {}, [], [], []
    vis_foliage = []
    vis_tris, vis_fl, vis_tx = [], [], []   # рисуемый город (мир) и флаги его треугольников: верх заборов (правка 47), опора по земле (11.3)
    city_src = []   # правка 106: меши города копятся, дальние версии решает far_keep по всему городу (швы между мешами)
    # правка 95 (F11-2): регион каждого меша города (файл l<N>r*/l<N>z*, −1 — без региона: объекты AnimCollision) — кусок
    # не смешивает регионы (ключ группы), движок и PVS рисуют только регионы, которые оригинал держит с этого места (REG1)
    reg_names = regions_export.level_regions(level) if os.environ.get('SHR_NO_REGIONS') != '1' else []
    reg_of = lambda scene: reg_names.index(scene.lower()) if scene.lower() in reg_names else -1
    city_reg, vis_reg, col_by, vis_by = [], [], {}, {}
    def add_city(prims, reg=-1, foliage=False):
        for t, fl, P, UV, C in prims:
            vis_tris.append(np.asarray(P, np.float32)); vis_fl.append(np.full(len(P) // 3, fl, np.int32)); vis_tx.append(np.full(len(P) // 3, t, np.int32))
            vis_foliage.append(np.full(len(P) // 3, foliage, bool))
            vis_reg.append(np.full(len(P) // 3, reg, np.int32))
            city_src.append((t, fl, P, UV, C)); city_reg.append(reg)
            if reg >= 0: vis_by.setdefault(reg_names[reg], []).append(np.asarray(P, np.float32).reshape(-1, 3, 3))
    def group_city():   # меши города -> группы (клетка 40 м, текстура, флаги, регион) с ближней/дальней версией
        if SEAMS_OLD: masks, msg = far_keep(city_src)
        else: masks, msg = [(np.ones if FAR_PLAIN else np.zeros)(len(np.asarray(p[2]).reshape(-1, 9)), bool) for p in city_src], \
            'дальние версии: нет (правки 224/237/251: кусок рисуется ближней версией на любой дальности)'
        print(msg, flush=True)
        n_src = n_cut = 0
        for (t, fl, P, UV, C), okeep, rg in zip(city_src, masks, city_reg):
            n_src += len(np.asarray(P).reshape(-1, 9))
            P, UV, C, anc, (fP, fUV, fC, forig) = subdivide_lod(P, UV, C, far_split=FAR_SPLIT)
            n_cut += len(P) // 3
            # клетку выбирает дальний предок (по своему центру), мелкие потомки идут за ним в тот же кусок:
            # иначе ближняя и дальняя версии одной поверхности разъехались бы по разным кускам (дыры/двойники)
            keys = np.floor(fP.reshape(-1, 3, 3).mean(1)[:, [0, 2]] / CHUNK).astype(int)
            keep = okeep[forig]
            for key in {tuple(k) for k in keys}:
                fm = (keys == key).all(1); nm = fm[anc]
                local = np.cumsum(fm) - 1
                groups.setdefault((key, t, fl, rg), []).append(
                    (verts(P[np.repeat(nm, 3)], UV[np.repeat(nm, 3)], C[np.repeat(nm, 3)]), local[anc[nm]],
                     verts(fP[np.repeat(fm, 3)], fUV[np.repeat(fm, 3)], fC[np.repeat(fm, 3)]), keep[fm]))
        if os.environ.get('SHR_TEXSTATS'):   # правка 498: по текстуре — треугольники, диапазон UV (повторяющаяся ли), число клеток
            st_ = {}
            for t_, fl_, P_, UV_, C_ in city_src:
                a_ = np.asarray(UV_, np.float64).reshape(-1, 2)
                if not len(a_): continue
                r_ = st_.setdefault(int(t_), dict(tris=0, umin=1e9, vmin=1e9, umax=-1e9, vmax=-1e9, flags=set()))
                r_['tris'] += len(a_) // 3; r_['umin'] = min(r_['umin'], float(a_[:, 0].min())); r_['umax'] = max(r_['umax'], float(a_[:, 0].max()))
                r_['vmin'] = min(r_['vmin'], float(a_[:, 1].min())); r_['vmax'] = max(r_['vmax'], float(a_[:, 1].max())); r_['flags'].add(int(fl_) & 15)
            inv_ = {}
            for k_, v_ in cache.items():
                if isinstance(v_, int): inv_.setdefault(v_, []).append(k_)
            Path(os.environ['SHR_TEXSTATS']).write_text(json.dumps({str(k): dict(v, flags=sorted(v['flags']), urls=inv_.get(k, [])) for k, v in st_.items()}))
        print(f'нарезка города (стены > {SPLIT:g} м, пол > {SPLIT_GROUND:g} м): {n_src:,} исходных треугольников -> {n_cut:,} ({n_cut / max(n_src, 1):.2f}x)', flush=True)
    foliage_atc = atc_rows()
    for scene in scenes:
        roots.append((scene, export_scene(conv, art / f'{scene}.p3d', TN(f'psp_{scene}'))))
        meta = json.loads((OUT / (TN(f'psp_{scene}') + '.json')).read_text()); raw = (OUT / (TN(f'psp_{scene}') + '.bin')).read_bytes()
        for t, fl, P, UV, C in pack_prims(meta, raw, cache, blob, is_sky):
            sky.append((t, fl, P, UV, C))
        # Keep original drawable order; carry physical ownership only into COL synthesis.
        fol_meshes = {string(m.data)[0] for ent in roots[-1][1].children
                      if ent.id in COL_STATIC and col_material(ent, foliage_atc) == 1.0
                      for m in walk(ent) if m.id == 0x10000}
        for obj in meta['objects']:
            one = dict(meta, objects=[obj])
            _TOON_CITY[0] = True   # №897: контур — только у мешей города
            try: prims_ = pack_prims(one, raw, cache, blob, lambda o: not is_sky(o) and o['mesh'] not in city_skip)
            finally: _TOON_CITY[0] = False
            add_city(prims_, reg_of(scene), obj['mesh'] in fol_meshes)
        if meta.get('collision'):
            o, n = meta['collision']; coll.append(np.frombuffer(raw, '<f4', n, o).reshape(-1, 3, 3))
            if reg_of(scene) >= 0: col_by[scene.lower()] = coll[-1]
        for ext in ('json', 'bin'): (OUT / f'{TN(f"psp_{scene}")}.{ext}').unlink()
    if bd_path:
        roots_bd = export_scene(conv, bd_path, TN('psp_backdrop'))
        meta = json.loads((OUT / (TN('psp_backdrop') + '.json')).read_text()); raw = (OUT / (TN('psp_backdrop') + '.bin')).read_bytes()
        nb = len(sky)
        for t, fl, P, UV, C in pack_prims(meta, raw, cache, blob, lambda o: bool(BACKDROP_RE.search(o['name']) or BACKDROP_RE.search(o['mesh']))):
            sky.append((t, fl, P, UV, C))
        print(f'фон уровня {backdrop_level} для трассы: {len(sky) - nb} примитивов неба/горизонта/дали (без коллизии)', flush=True)
        for ext in ('json', 'bin'): (OUT / f'{TN("psp_backdrop")}.{ext}').unlink()
    if aobj: add_city(aobj.city_prims())   # неподвижные части AnimCollision/Anim — обычные меши города (кадр 0)
    lp_n, lp_t = lightpool_prims(level, scenes, art, conv, cache, blob, add_city, reg_of)   # правка 496: пятна света фонарей (SetStatepropShadow)
    if lp_n: print(f'пятна света фонарей (496): {lp_n} фонарей, {lp_t} треугольников (до нарезки), аддитивные куски города')
    group_city()
    print('карта высот…', flush=True)
    tris = np.concatenate(coll); lo, W, H, hm, wall = heightmap(tris)
    regs = None
    if reg_names:   # правка 95: маска регионов по клеткам 20 м (сетка PVS) — из ZoneEvent/телепортов/миссий диска и земли регионов
        regs = regions_export.Regions(level, col_by, {k: np.concatenate(v) for k, v in vis_by.items()}, lo, W, H, CELL)
        for ln in regs.report(): print(ln, flush=True)
        if not regs.ok: print(f'  регионов {len(reg_names)} > {regions_export.MAX_REG}: маска не пишется, рисуется всё'); regs = None

    # машина игрока уровня (InitLevelPlayerVehicle, модель — LoadDisposableCar): куски по суставам + остов, данные CAR1
    (start_pos, start_h), car, start_src = level_start(level)
    carp = car_model(level, car)
    print(f'старт: {start_src}, {tuple(round(float(v), 3) for v in start_pos)} курс {float(start_h):.4f}; машина {car} ({carp.relative_to(GAME)})', flush=True)
    light = WORLD_LIGHT
    print(f'свет уровня (группа {light["group"]} l{level}_terra): фон {light["amb"]}, направленных {len(light["dirs"])}, '
          f'множители к уровню 1: фон {tuple(round(x, 3) for x in light["ambS"])}, направленный {tuple(round(x, 3) for x in light["dirS"])}', flush=True)
    car_groups, lift, car1, car_tex = car_export(conv, cache, blob, carp, car, light)
    if car1: car1['light'] = light

    # небо: вокруг центра, ужатое, режем крупно (в ужатых метрах); 8 секторов по азимуту, чтобы движок
    # не рисовал то, что за кадром. Градиентные текстуры (l1_backfade*) вмешиваем в цвет вершин, без текстуры.
    sky_groups = {}
    if sky:
        urls = {i: u.split('#')[0] for u, i in cache.items()}; grads, baked = {}, []   # '#blend' — копия текстуры (texq)
        allp = np.concatenate([P for _, _, P, _, _ in sky]); sc = (allp.min(0) + allp.max(0)) / 2; sc[1] = 0
        for t, fl, P, UV, C in sky:
            P, UV, C = subdivide((P - sc) * SKY_S, UV, C, SPLIT, SPLIT)
            if t >= 0 and t not in grads: grads[t] = gradient_rows(urls[t])
            if t >= 0 and grads[t] is not None:
                C2, err = bake_gradient(UV, C, grads[t]); baked.append((urls[t], len(P) // 3, err))
                if err <= BAKE_MAX:   # без текстуры; UV обнуляем, чтобы вершины лучше склеились
                    C, t, UV = C2, -1, np.zeros_like(UV)
            cen = P.reshape(-1, 3, 3).mean(1)
            sec = np.floor((np.arctan2(cen[:, 0], cen[:, 2]) + np.pi) / (2 * np.pi / SKY_SECTORS)).astype(int) % SKY_SECTORS
            sec[np.hypot(cen[:, 0], cen[:, 2]) < SKY_CAP] = SKY_SECTORS                  # шапка
            for s_ in np.unique(sec):
                sel = np.repeat(sec == s_, 3); v = verts(P[sel], UV[sel], C[sel]); n = len(v) // 3
                sky_groups.setdefault(((int(s_), 0), t, fl | 4), []).append((v, np.arange(n), v, np.ones(n, bool)))
        print(f'небо: {sum(len(p[0]) for v in sky_groups.values() for p in v) // 3} треугольников, '
              f'{len(sky_groups)} кусков ({SKY_SECTORS} секторов + шапка)')
        for u, n, err in baked:
            print(f'  градиент {u} ({n} треуг.): макс. ошибка заливки {err:.2f}/255 -> '
                  + ('цвет вершин, без текстуры' if err <= BAKE_MAX else 'оставляем текстуру'))

    print('пропы: геометрия…', flush=True)
    prop_types, prop_pieces_raw = [], []
    for name, info in prop_ent.items():
        outname = TN(f'psp_prop_{len(prop_types)}')
        conv.export(art / f"{info['scene']}.p3d", outname, mesh_name=name)
        meta = json.loads((OUT / f'{outname}.json').read_text()); raw = (OUT / f'{outname}.bin').read_bytes()
        parts = pack_prims(meta, raw, cache, blob)
        for ext in ('json', 'bin'): (OUT / f'{outname}.{ext}').unlink()
        if not parts: continue
        allp = np.concatenate([P for _, _, P, _, _ in parts])
        lo_, hi_ = allp.min(0), allp.max(0)
        radius = float(np.hypot((hi_[0] - lo_[0]) / 2, (hi_[2] - lo_[2]) / 2))
        ti = len(prop_types)
        for t, fl, P, UV, C in parts: prop_pieces_raw.append((ti, t, fl, P, UV, C))
        prop_types.append(dict(name=name, index=ti, mass=info['mass'], radius=radius, halfH=float((hi_[1] - lo_[1]) / 2), mats=info['mats'], mat=info['mat']))
    # монеты (этап 3б): один меш coinShape_000 (как лишний тип пропа без экземпляров) + точки kind 14 из локаторов
    coin_pos, seen = [], set()
    for sc_ in scenes:
        for c in walk(read(art / f'{sc_}.p3d')):
            if c.id != 0x3000005: continue
            nm, pp_ = string(c.data); kind, ln = struct.unpack_from('<II', c.data, pp_)
            if kind != 14: continue
            x_, y_, z_ = struct.unpack_from('<3f', c.data, pp_ + 8 + ln * 4); key = (round(x_, 2), round(y_, 2), round(-z_, 2))
            if key not in seen: seen.add(key); coin_pos.append((x_, y_, -z_))
    coin_type = -1
    parts = []
    if coins:
        conv.export(art / f'missions/level0{level}/level.p3d', TN('psp_coin'), mesh_name='coinShape_000')
        meta = json.loads((OUT / (TN('psp_coin') + '.json')).read_text()); raw = (OUT / (TN('psp_coin') + '.bin')).read_bytes()
        parts = pack_prims(meta, raw, cache, blob)
        for ext in ('json', 'bin'): (OUT / f'{TN("psp_coin")}.{ext}').unlink()
    if parts:
        coin_type = len(prop_types)
        for t, fl, P, UV, C in parts: prop_pieces_raw.append((coin_type, t, fl, P, UV, C))
        prop_types.append(dict(name='coinShape_000', index=coin_type, mass=0.0, radius=0.35, halfH=0.35, mats=[]))
    print(f'  монеты: {len(coin_pos)} в мире, меш {"есть" if parts else "НЕ НАЙДЕН"}')
    print(f'  {len(prop_types)} типов пропов, {len(prop_pieces_raw)} кусков геометрии, '
          f'{sum(len(t["mats"]) for t in prop_types)} экземпляров')

    # швы и слои (правка Роба 37): сварка -> точки на общую решётку -> Т-стыки по итоговым точкам -> слои по тем самым
    # треугольникам, что лягут в файл (проверка tools/seams.py судит тот же файл тем же правилом глубины)
    if FAR_PLAIN:   # правка 508: ближние версии свариваются без дальних (результат как без SHR_FAR_PLAIN), дальние — отдельно
        print(f'швы: сварено точек ближе {WELD_EPS * 1000:.1f} мм — {weld(groups, vis=(0,))} (правка Роба 37), дальние версии {weld(groups, vis=(2,))}', flush=True)
    else: print(f'швы: сварено точек ближе {WELD_EPS * 1000:.1f} мм — {weld(groups)} (правка Роба 37)', flush=True)
    snap_grid(groups, CITY_GRID)
    if os.environ.get('SHR_TJ', '1') != '0':   # =0 — как до 29.09 (было/стало, отрицательный контроль tools/seams.py)
        print(f'Т-стыки: рёбер разрезано в точках чужих вершин — {split_tjunctions(groups)} (правка Роба 37)', flush=True)
    if os.environ.get('SHR_LAYERS') == '1': mark_layers(groups, regs.never() if regs else None)   # слои-«наклейки»: флаги 64/256/512 (правки Роба 37, 43)
    allv = np.concatenate([p[0] for v in groups.values() for p in v])
    bb = [allv['x'].min(), allv['y'].min(), allv['z'].min(), allv['x'].max(), allv['y'].max(), allv['z'].max()]
    (sx, sy, sz), sh = start_pos, start_h
    # клетки: всё, что в клетке, лежит одним блоком и грузится за одно чтение
    # правка 181 (слово Роба 02.10): клетка — (клетка 40 м, регион оригинала l<N>r*/l<N>z*): движок держит в памяти регионы набором
    # ворот, как оригинал (viewer/zonegate.h), поэтому клетка не смешивает регионы; Cell.pad = регион + 1 (0 — без региона, всегда).
    # SHR_CELL_MIXED=1 — клетки как до правки (регионы вперемешку, pad 0)
    rkey = lambda k: k[3] if len(k) > 3 else -1
    if os.environ.get('SHR_CELL_MIXED') == '1': cellkeys = [(ck, None) for ck in sorted({k[0] for k in groups})]
    else: cellkeys = sorted({(k[0], rkey(k)) for k in groups})
    owners = [(0, [k for k in groups if k[0] == ck and (rg is None or rkey(k) == rg)]) for ck, rg in cellkeys]
    owners += [(1, list(car_groups)), (2, list(sky_groups))]
    src = {0: groups, 1: car_groups, 2: sky_groups}
    QV = np.dtype([('u', '<i2'), ('v', '<i2'), ('c', '<u4'), ('x', '<i2'), ('y', '<i2'), ('z', '<i2'), ('pad', '<i2')])
    # текстуры, которые никто не использует (градиенты неба ушли в цвет вершин), не кладём в файл
    # правка Роба 77/88 (30.09, «стыки на асфальте», «битый пончик»): GE выбирает мип по треугольнику (GU_TEXTURE_AUTO) — у
    # соседних треугольников асфальта разные уровни, видна ступенька. Движок (viewer/main.c bindTex) берёт мип по глубине
    # пикселя (GU_TEXTURE_SLOPE): наклон = размер пикселя на 1 м глубины × плотность текстуры (тексели на метр города).
    # Плотность — по треугольникам города этой текстуры (площадь UV × w × h / площадь в мире), байты 12..15 заголовка (float)
    dens = {}
    for key, parts in groups.items():
        if key[1] < 0: continue
        for part in parts:
            vv_ = part[0]; n3 = len(vv_) // 3 * 3
            if not n3: continue
            X_ = np.stack([vv_['x'], vv_['y'], vv_['z']], 1)[:n3].reshape(-1, 3, 3).astype(np.float64)
            U_ = np.stack([vv_['u'], vv_['v']], 1)[:n3].reshape(-1, 3, 2).astype(np.float64)
            wa = np.linalg.norm(np.cross(X_[:, 1] - X_[:, 0], X_[:, 2] - X_[:, 0]), axis=1) / 2
            ua = np.abs((U_[:, 1, 0] - U_[:, 0, 0]) * (U_[:, 2, 1] - U_[:, 0, 1]) - (U_[:, 2, 0] - U_[:, 0, 0]) * (U_[:, 1, 1] - U_[:, 0, 1])) / 2
            a0, b0 = dens.get(key[1], (0., 0.)); dens[key[1]] = (a0 + float(wa.sum()), b0 + float(ua.sum()))
    GROUND_BIAS = 0.0 if os.environ.get('SHR_GROUND_BIAS_OLD') == '1' else float(os.environ.get('SHR_GROUND_BIAS', '0.75'))
    ground_idx = {cache[k_] for u in GROUND_URLS for k_ in (u, u + '#toon') if k_ in cache} if GROUND_BIAS else set()   # №679: и плоский ключ мультика
    nsl = 0
    for t_, (wa, ua) in dens.items():
        tw_, th_ = struct.unpack_from('<HH', blob[t_], 0)
        if wa > 1e-6 and ua > 1e-9:
            sl = TEX_PIX_K * math.sqrt(ua * tw_ * th_ / wa)
            if t_ in ground_idx: sl *= 2.0 ** GROUND_BIAS   # приём 2: земля и дороги уходят на размытый мип раньше
            blob[t_] = blob[t_][:12] + struct.pack('<f', sl) + blob[t_][16:]; nsl += 1
            # метка «земля» (байт 9 заголовка, был в добивке): движок (main_render.h texSlope) умножает наклон этих текстур на множитель из файла groundbias рядом с EBOOT без пересборки данных.
            # SHR_GROUND_TAG_OLD=1 — без метки (множитель в движке ничего не делает)
            if t_ in ground_idx and os.environ.get('SHR_GROUND_TAG_OLD') != '1' and blob[t_][9] == 0: blob[t_] = blob[t_][:9] + b'\x01' + blob[t_][10:]
    print(f'мип по глубине (77/88): наклон записан у {nsl} текстур города; земля со сдвигом мипа +{GROUND_BIAS}: {len(ground_idx)} текстур; метка земли (байт 9): {sum(1 for t_ in dens if t_ in ground_idx and blob[t_][9] == 1)}')
    used = sorted({k[1] for d in src.values() for k in d if k[1] >= 0} | {t for _, t, _, _, _, _ in prop_pieces_raw if t >= 0} | car_tex | sprops.textures())
    tmap = {t: i for i, t in enumerate(used)}; blob = [blob[t] for t in used]
    def piece_chunks(parts):
        """группа -> куски по ~PIECE_VERTS вершин супа (ближняя + дальняя версии); предок и его потомки не разлучаются
        -> [(ближние вершины, дальние вершины)], число ближних треуг., число дальних"""
        nv = np.concatenate([p[0] for p in parts]); fv = np.concatenate([p[2] for p in parts])
        base = np.cumsum([0] + [len(p[3]) for p in parts])[:-1]
        anc = np.concatenate([p[1] + b for p, b in zip(parts, base)]); keep = np.concatenate([p[3] for p in parts])
        w = np.bincount(anc, minlength=len(keep)) * 3 + keep * 3
        chunk = (np.cumsum(w) - w) // PIECE_VERTS
        out = []
        for ch in range(int(chunk.max()) + 1):
            nm, fm = chunk[anc] == ch, (chunk == ch) & keep
            vn, vf = nv[np.repeat(nm, 3)], fv[np.repeat(fm, 3)]
            if len(vn): out.append((vn, vf))
        return out, len(nv) // 3, int(keep.sum())
    # правка Роба 106 (повтор 01.10, «наслоение, чтобы не было стыков»): юбки на открытых краях земли обеих версий кусков
    # города (tools/skirts.py) — по всему городу сразу: край открыт, если ребра-близнеца нет ни у одного соседа.
    # ПО УМОЛЧАНИЮ ВЫКЛ (SHR_SKIRTS=1 — включить): замер 01.10 (tools/seams_raster.py, 22 вида × 3 кадра, GE24) после точных
    # центров кусков (ORIGIN_STEP) — швов на земле 578 и с юбками 578, полос глубины +41, а треугольников +1.6 % ближних и
    # +7 % дальних (кадр GE). Щели стыков на PSP давал 24-бит сдвиг кусков, не нехватка наслоения. Решение за Робом
    city_chunks = {key: piece_chunks(groups[key]) for key in groups}
    if os.environ.get('SHR_SKIRTS', '1') == '1':   # Роб 01.10 15:1x: «швы всё равно видны — пусть полигоны заходят друг на друга» — юбки ВКЛ
        import skirts
        flat = [(key, ci) for key in sorted(groups, key=lambda k: (k[0], k[2], k[1])) for ci in range(len(city_chunks[key][0]))]
        tri3 = lambda v: np.stack([v['x'], v['y'], v['z']], 1).astype(np.float64).reshape(-1, 3, 3)
        cid_ = {}   # правка 507: клетка 40 м куска (у клеток разные матрицы GE) — общее ребро кусков разных клеток получает юбку ТОЛЬКО по SHR_XCELL_SKIRT=1
        # (замер L1: щели 145→89 (cat) / 106→88 (sep), но +14 % треугольников при кромке ≥ 3 м; по умолчанию выключено; SHR_XCELL_MINLEN, SHR_XCELL_SKIRT_ANYTEX=1).
        # Правка 508: при SHR_FAR_PLAIN=1 такие юбки получает только ДАЛЬНЯЯ версия (кольцо шва ближний/дальний лежит на границе клеток), любой текстуры
        xc_all_ = os.environ.get('SHR_XCELL_SKIRT') == '1'; xc_far_ = FAR_PLAIN and not xc_all_
        sp_, msg = skirts.plan([(key[2], tri3(city_chunks[key][0][ci][0]), tri3(city_chunks[key][0][ci][1])) for key, ci in flat],
                               [cid_.setdefault(key[0], len(cid_)) for key, ci in flat] if (xc_all_ or xc_far_) else None,
                               [int(key[1]) for key, ci in flat] if (xc_all_ and os.environ.get('SHR_XCELL_SKIRT_ANYTEX') != '1') else None,
                               (1,) if xc_far_ else (0, 1))
        print(msg, flush=True)
        for i, (key, ci) in enumerate(flat):
            if i not in sp_: continue
            vn, vf = city_chunks[key][0][ci]
            sn_ = skirts.skirt_soup(vn, *sp_[i][0])
            if SPLIT_LONG: sn_ = split_soup(sn_)   # №674: ближние юбки без треугольников длиннее 12 м
            city_chunks[key][0][ci] = (np.concatenate([vn, sn_]),
                                       np.concatenate([vf, skirts.skirt_soup(vf, *sp_[i][1])]))
    cell_c, cell_own = {}, [0, 0]
    if not SEAMS_OLD:   # правки 224/237/251: точки юбок — на решётку; Т-стыки всех кусков вместе с юбками (у юбок разной глубины
        tj = {}         # общий вертикальный край: низ короткой — на ребре длинной); предок не нужен — дальних версий нет
        far_keep_ = {}  # правка 508: SHR_FAR_PLAIN=1 — дальняя версия куска (исходные треугольники, рёбра > FAR_SPLIT режутся) пишется в файл как данные,
                        # движок её НЕ рисует (флаг куска 2048 остаётся): замер выигрыша tools/far_plain_est.py. По умолчанию не пишется
        for key in city_chunks:
            for ci, (vn, vf) in enumerate(city_chunks[key][0]):
                for f in ('x', 'y', 'z'): vn[f] = np.round(vn[f].astype(np.float64) / CITY_GRID) * CITY_GRID
                if FAR_PLAIN:
                    vf = vf.copy()
                    for f in ('x', 'y', 'z'): vf[f] = np.round(vf[f].astype(np.float64) / CITY_GRID) * CITY_GRID
                    far_keep_[(key, ci)] = vf
                tj[(key, ci)] = [(vn, np.arange(len(vn) // 3), vf[:0], np.zeros(0, bool))]
        print(f'швы (правки 224/237/251): Т-стыки вместе с юбками — {split_tjunctions(tj)}', flush=True)
        for (key, ci), parts in tj.items(): city_chunks[key][0][ci] = (parts[0][0], far_keep_.get((key, ci), parts[0][2]))
        ylo, yhi = {}, {}   # центр клетки 40 м: x, z — середина клетки, y — середина её точек; шаг ORIGIN_STEP (точен во float24)
        for key, (chs, _, _) in city_chunks.items():
            for vn, _ in chs:
                if len(vn): ylo[key[0]] = min(ylo.get(key[0], 1e9), float(vn['y'].min())); yhi[key[0]] = max(yhi.get(key[0], -1e9), float(vn['y'].max()))
        for k0 in ylo:
            c = np.array([(k0[0] + .5) * CHUNK, (ylo[k0] + yhi[k0]) / 2, (k0[1] + .5) * CHUNK])
            cell_c[k0] = np.round(c / ORIGIN_STEP) * ORIGIN_STEP
    uhmin = np.where(hm == -32768, np.inf, hm.astype(np.float64) / 100.0).min(2)   # самая низкая опора клетки карты высот, м (inf — нет); правка 509
    ufol = frozenset(cache[k_] for nm, u in conv.textures.items() if FOL_TEX_RE.search(nm) for k_ in (u, u + '#toon') if k_ in cache and isinstance(cache[k_], int))   # №679: плоская (мультик) текстура лежит под ключом u#toon — коллизия не должна от неё меняться
    cells, pieces, tris_total, far_total = [], [], 0, 0
    for owner, keys in owners:
        if not keys: continue
        first, cblob, clo, chi = len(pieces), bytearray(), np.full(3, 1e9), np.full(3, -1e9)
        for key in sorted(keys, key=lambda k: (k[2], k[1]) + tuple(k[3:4])):
            ch_list, n_near, n_far = city_chunks[key] if owner == 0 else piece_chunks(src[owner][key])
            if owner == 0: tris_total += n_near; far_total += n_far
            for vn, vf in ch_list:
                vv = np.concatenate([vn, vf])   # обе версии — один массив вершин (почти все вершины общие)
                P = np.stack([vv['x'], vv['y'], vv['z']], 1).astype(np.float64)
                lo_, hi_ = P.min(0), P.max(0); c = (lo_ + hi_) / 2; S = max(float((hi_ - lo_).max()) / 2, 1e-3)
                clo, chi = np.minimum(clo, lo_), np.maximum(chi, hi_)
                q = np.zeros(len(vv), QV)
                if owner == 0:   # город — общая решётка (правка Роба 37): стыки кусков совпадают до бита
                    if SEAMS_OLD: c, xyz, sc_rec = grid_quant(P, CITY_GRID)
                    else: c, xyz, sc_rec, ok_ = cell_quant(P, cell_c[key[0]], CITY_GRID); cell_own[ok_] += 1   # правки 224/237/251
                    q['x'], q['y'], q['z'] = xyz[:, 0], xyz[:, 1], xyz[:, 2]
                else:
                    q['x'], q['y'], q['z'] = [np.clip(np.round((P[:, j] - c[j]) / S * 32767), -32767, 32767) for j in range(3)]
                    sc_rec = S * 32768 / 32767
                skyv = owner == 2 and not SKY_SEAM_OLD and key[1] >= 0 and len(vv) and float(vv['v'].min()) >= -.02 and float(vv['v'].max()) <= 1.02
                if skyv: q['u'], q['v'] = uvq.tri_i16_clampv(vv['u'], vv['v'])   # №511: слой неба с v в [0, 1] — без переноса на повторы, флаг F_SKYV (GU_CLAMP по v)
                else: q['u'], q['v'] = uvq.tri_i16(vv['u'], vv['v'])   # GE читает u16: без отрицательных (tools/uvq.py, правка Роба 43)
                q['c'] = vv['c']
                # без индексов (шаг 3 аудита, gebench на PSP: 1.83x быстрее индексов): ближняя версия — вершины
                # подряд по 3 на треугольник, сразу за ней дальняя (у машины и неба дальняя не нужна: nf = 0).
                # Вершина 12 Б (цвет 5650), если альфа всех вершин 255 и это не небо (градиенты неба — 8888).
                if owner != 0: q = q[:len(vn)]
                v12 = owner != 2 and bool((q['c'] >> 24 == 0xFF).all())
                fl = key[2] | F_NOIDX | (F_V12 if v12 else 0) | (F_DOUBLE if owner == 1 else 0) | (F_SKYV if skyv else 0)
                if owner == 0 and not SEAMS_OLD: fl |= F_NEAR   # правки 224/237/251: ближняя версия на любой дальности (дальней нет)
                nn = len(vn); nf_ = len(q) - nn
                if owner == 0 and FAR_PLAIN and nf_ > 0: fl |= F_FARP   # правка 508: у куска есть дальняя версия без нарезки (движок берёт её по файлу far_plain)
                off = len(cblob)
                cblob += ni_bytes(q, v12); cblob += b'\0' * (-len(cblob) % 16)
                pieces.append((lo_, hi_, tmap.get(key[1], -1), fl, len(q), nn, owner, c, sc_rec, off, nf_, len(cells),
                               key[0] if owner == 1 else ((key[3] + 1 if owner == 0 and len(key) > 3 and key[3] >= 0 else 0), 0)))
                # машина: (сустав, вариант) -> Rec.pad[0..1]; город: pad[0] = регион + 1 (правка 95, REG1), 0 — без региона
        regs_ = {rkey(k) for k in keys} if owner == 0 else {-1}
        creg = (next(iter(regs_)) + 1) if len(regs_) == 1 and next(iter(regs_)) >= 0 else 0   # правка 181: Cell.pad = регион + 1
        if owner == 0 and keys:   # правка 509: квадрат 40 м — тот же, что выберет движок (центр ящика клетки), а не ключ клетки (кусок может выпирать за квадрат)
            creg |= cell_underlay(keys, city_chunks, blob, (lo, W, H, hm), (int(math.floor((clo[0] + chi[0]) / 2 / CHUNK)), int(math.floor((clo[2] + chi[2]) / 2 / CHUNK))), float(clo[1]), uhmin, ufol)
        cells.append([clo, chi, first, len(pieces) - first, owner, bytes(cblob), creg])
    if os.environ.get('SHR_CELL_UNDERLAY_OLD') != '1':
        cu_ = [c_[6] for c_ in cells if c_[4] == 0 and c_[6] >> 16]
        if cu_:
            rgb_ = np.array([[(v >> 27 & 31) * 255 // 31, (v >> 21 & 63) * 255 // 63, (v >> 16 & 31) * 255 // 31] for v in cu_])
            print(f'подложка по клеткам (509): {len(cu_)} клеток из {sum(1 for c_ in cells if c_[4] == 0)}; цвет земли: от {rgb_.min(0).tolist()} до {rgb_.max(0).tolist()}, '
                  f'средний {rgb_.mean(0).round().astype(int).tolist()}; глубина под ящиком: медиана {np.median([(v >> 8 & 255) * UNDERLAY_STEP for v in cu_]):.2f} м, '
                  f'макс {max((v >> 8 & 255) * UNDERLAY_STEP for v in cu_):.1f} м', flush=True)
    head = bytearray(b'SHR7' + struct.pack('<II', len(blob), len(pieces)))
    head += struct.pack('<6f4f f', *bb, sx, sy, sz, sh, lift)
    head += struct.pack('<3fII', lo[0], lo[2], CELL, W, H)
    head += b'\0' * 4                                   # 76: смещение таблицы кусков
    head += struct.pack('<II', len(cells), 0) + b'\0' * 16  # 80: клеток, 84: смещение таблицы клеток, 88: PVS1, 92: COL1, 96: PRP1, 100: CAR1
    head += hm.tobytes(); head += b'\0' * (-len(head) % 16)
    head += wall.tobytes(); head += b'\0' * (-len(head) % 16)
    for t in blob: head += t; head += b'\0' * (-len(head) % 16)
    struct.pack_into('<I', head, 76, len(head))
    recs = bytearray()
    for lo_, hi_, t, fl, nv, ni, owner, c, S, off, nf, ci, jv in pieces:
        recs += struct.pack('<6fiIIII3ffIII2I', *lo_, *hi_, t, fl, nv, ni, owner, *c, S, off, nf, ci, *jv)
    struct.pack_into('<I', head, 84, len(head) + len(recs))
    off = len(head) + len(recs) + 48 * len(cells); off += -off % 16; crecs = bytearray()
    for clo, chi, first, n, owner, cb, creg in cells:
        crecs += struct.pack('<6fIIIIII', *clo, *chi, first, n, owner, off, len(cb), creg); off += len(cb)
    out = head + recs + crecs; out += b'\0' * (-len(out) % 16); out += b''.join(c[5] for c in cells)
    # коллизия COL1 — после блобов клеток (движок их читает по смещениям, резидентная часть не меняется),
    # PVS (tools/pvs) допишется уже после неё
    print('коллизия…', flush=True)
    TR, BX, CY, SG, rep = col_collect(roots)
    ubase = frozenset(cache[k_] for nm, u in conv.textures.items() if FV_BASE_RE.search(nm) for k_ in (u, u + '#toon') if k_ in cache and isinstance(cache[k_], int))   # текстуры каменного основания (FV_BASE_RE)
    BX, CY, fvr = col_foliage_volumes(BX, CY, np.concatenate(vis_tris), np.concatenate(vis_fl), np.concatenate(vis_tx), ufol, rep['boxnames'], rep['cylnames'], ubase)   # №587 общее правило: твёрдый по данным объём с листвой внутри — куст
    print(f'листва по рисунку внутри твёрдых объёмов (587): {len(fvr)} помечено мягкими; по именам: ' + ', '.join(sorted({re.sub(r"\d+", "#", (rep["boxnames"] if k_ == "box" else rep["cylnames"])[i_].split("/", 1)[-1]) for k_, i_, *_r in fvr})[:12]), flush=True)
    TR, vg = col_visible_ground(TR, np.concatenate(vis_tris), np.concatenate(vis_fl), lo, W, H, hm, jumps_export.deaths(level),
                                np.concatenate(vis_reg), regs, np.concatenate(vis_foliage))   # 11.3: опора по видимой земле (без оврагов DEATH и декораций издали, правка 95)
    print(f'опора по видимой земле (11.3): +{vg} треугольников (видимая непрозрачная земля без коллизии оригинала под ней)')
    CY, phs = col_phantom_pillars(CY, np.concatenate(vis_tris), np.concatenate(vis_fl), lo, W, H, hm)   # правка 414: невидимые столбы — насквозь для машины
    print(f'невидимые столбы посреди опоры (414): {len(phs)} {phs[:8]}')
    # правка 475: невидимые коробки/капсулы мира. Рисунок пропов (PRP1) и стейтпропов (SPR1) в vis не входит — их точки идут отдельно
    ex_pts = [M[3, :3] for t_ in prop_types for M in t_['mats']] + [M[3, :3] for T_ in sprops.types for M, _ in T_['inst']]
    if os.environ.get('SHR_PHANTOM_VOL_INPUTS'): np.savez(os.environ['SHR_PHANTOM_VOL_INPUTS'], BX=BX, CY=CY, vis=np.concatenate(vis_tris), vfl=np.concatenate(vis_fl), ex=np.array(ex_pts).reshape(-1, 3), lo=np.array(lo), WH=np.array([W, H]), hm=hm, TR=TR, bn=np.array(rep['boxnames']), cn=np.array(rep['cylnames']), vtx=np.concatenate(vis_tx), ufol=np.array(sorted(ufol), np.int64))
    BX, CY, pvs = col_phantom_volumes(BX, CY, np.concatenate(vis_tris), np.concatenate(vis_fl), ex_pts, (lo, W, H, hm))
    brk_pts = [M[3, :3] for t_ in prop_types for M in t_['mats']] + [M[3, :3] for T_ in sprops.types if T_['kind'] == 0 and T_['cls'] != 2 for M, _ in T_['inst']]
    if os.environ.get('SHR_PROP_OWNED_INPUTS'): np.save(os.environ['SHR_PROP_OWNED_INPUTS'], np.array(brk_pts).reshape(-1, 3))
    BX, CY, pown = col_prop_owned(BX, CY, brk_pts)
    if os.environ.get('SHR_PROP_OWNED_OLD') != '1':
        nm2_ = lambda kind, i: (rep['boxnames'] if kind == 'box' else rep['cylnames'])[i] if i < len(rep['boxnames' if kind == 'box' else 'cylnames']) else '?'
        k2_ = {}
        for kind, i in pown:
            k_ = re.sub(r'\d+', '#', nm2_(kind, i).split('/', 1)[-1]); k2_[k_] = k2_.get(k_, 0) + 1
        print(f'объёмы сбиваемых предметов (488): {len(pown)} (сбиваемых точек {len(brk_pts)}) не держат машину; по именам: ' +
              ', '.join(f'{k} {v}' for k, v in sorted(k2_.items(), key=lambda kv: -kv[1])[:12]))
        if os.environ.get('SHR_PROP_OWNED_DUMP'):
            Path(os.environ['SHR_PROP_OWNED_DUMP']).write_text(json.dumps([dict(kind=k, index=i, name=nm2_(k, i), c=(BX[i, :3] if k == 'box' else CY[i, :3]).round(2).tolist()) for k, i in pown], ensure_ascii=False))
    if os.environ.get('SHR_PHANTOM_VOL_OLD') != '1':
        nm_ = lambda kind, i: (rep['boxnames'] if kind == 'box' else rep['cylnames'])[i] if i < len(rep['boxnames' if kind == 'box' else 'cylnames']) else '?'
        kinds_ = {}
        for kind, _i, c_, sz_, rf_, i in pvs:
            k_ = re.sub(r'\d+', '#', nm_(kind, i).split('/', 1)[-1]); kinds_[k_] = kinds_.get(k_, 0) + 1
        print(f'невидимые объёмы мира (475): {len(pvs)} кусков машину не держат; коробок {len(BX)}, капсул {len(CY)} (после нарезки); по именам: ' +
              ', '.join(f'{k} {v}' for k, v in sorted(kinds_.items(), key=lambda kv: -kv[1])[:12]))
        if os.environ.get('SHR_PHANTOM_VOL_DUMP'):
            Path(os.environ['SHR_PHANTOM_VOL_DUMP']).write_text(json.dumps([dict(kind=k, index=i, name=nm_(k, src_), c=c_, size=sz_, ring=rf_) for k, i, c_, sz_, rf_, src_ in pvs], ensure_ascii=False))
    CY, fits = col_fit_capsules(CY, np.concatenate(vis_tris), np.concatenate(vis_fl), lo, W, H, hm)   # правка 489: капсула не шире видимого столба
    print(f'капсулы по видимому столбу (489): сужено {len(fits)} {fits[:12]}')
    BX, ndw = col_drawn_wall_boxes(BX, level, np.concatenate(vis_tris), np.concatenate(vis_fl))
    if ndw: print(f'рисуемые стены без коллизии, подтверждённые пробой: +{ndw} тонких коробок (tools/walls_confirmed/L{level}.tsv)')
    BX, rcaps = col_rail_caps(BX, np.concatenate(vis_tris), np.concatenate(vis_fl))
    if rcaps: print(f'полки перил (164б/187): +{len(rcaps)} ящиков по видимой ширине: ' + ', '.join(f'{i}→{w} м' for i, w, _ in rcaps[:12]))
    fol_t = sorted(set(cache[k_] for nm, u in conv.textures.items() if FOL_TEX_RE.search(nm) for k_ in (u, u + '#toon') if k_ in cache))   # №679: и плоский ключ мультика   # правка 173: листва
    folmask = np.isin(np.concatenate(vis_tx), fol_t) if os.environ.get('SHR_HEDGE_PASS', '1') == '1' and fol_t else None
    if os.environ.get('SHR_FOLTEX_DUMP'):   # №524: индексы текстур листвы (FOL_TEX_RE) с именами — для офлайн-детекторов tools/autoexplore/walls (только запись файла)
        Path(os.environ['SHR_FOLTEX_DUMP']).write_text(json.dumps({str(cache[u]): nm for nm, u in conv.textures.items() if FOL_TEX_RE.search(nm) and u in cache}, ensure_ascii=False))
    SG, fst = col_fence_tops(SG, (lo, W, H, hm), np.concatenate(vis_tris), folmask, drop_invisible=TRACK_FENCES and own_scenes and not TRACK_FENCE_KEEP_INVISIBLE and all(re.fullmatch(r'b0\d', x_) for x_ in scenes))   # правка 47: внутренние заборы — до верха видимого
    print(f'заборы: границ (за ними пустота, бесконечные) {fst["bound"]}, внутренних {fst["inner"]} -> кусков {fst["pieces"]} (изгородей {fst["hedge"]}), ' +
          (f'№905: одиночных «границ» между внутренними звеньями -> внутренние {fst["bridged"]}, ' if fst.get("bridged") else '') + (f'невидимых звеньев отброшено {fst.get("invisible", 0)}, ' if fst.get("invisible") else '') + (f'FENCE_TRIM: убрано кусков {fst["trimmed"]} ({fst["trim_m"]:.0f} м), ' if fst.get("trimmed") else '') + (f'верх над опорой: медиана {np.median(fst["h"]):.1f} м, 90% {np.percentile(fst["h"], 90):.1f} м' if len(fst["h"]) else 'верх над опорой: заборов нет'))
    if fst.get('roof'): print(f'заборы по краю крыши/уступа (№705): верх = земля за забором + {FENCE_ROOF_MARGIN} м у {fst["roof"]} кусков')
    sweep = None
    if aobj: BX, CY, sweep = aobj.col_extend(BX, CY)   # объёмы AnimCollision: статичные + живые (заметание клипа — в сетку)
    jmp, jz, jp = jumps_export.jmp1(level)   # зоны прыжка машин и подкидывающие точки Гомера из данных (правки 46, 47)
    col, cst = col_section(TR, BX, CY, SG, sweep=sweep, jmp=jmp)
    out += b'\0' * (-len(out) % 16); struct.pack_into('<I', out, 92, len(out)); out += col
    if aobj: rep['skipped'].pop('AnimCollision', None)   # объёмы AnimCollision теперь в COL1 (статичные и живые, ANM1)
    col_report(rep, cst, TR, BX, CY, SG); jumps_export.report(jz, jp)
    # пропы PRP1 — сразу после COL1 (заголовок SHR7 теперь 100 Б, @96); PVS (tools/pvs) допишется в самый конец
    # PRP2 = PRP1 (заголовок 64 Б) + подсекция SPR1 стейтпропов в конце (prp2_export.wrap_prp2)
    spr = sprops.section(tmap, [t['name'] for t in prop_types])
    out += b'\0' * (-len(out) % 16); struct.pack_into('<I', out, 96, len(out))
    # PRP2 @44/@48: по типу PRP1 частицы ATC + обломок (удар машиной: вода гидранта, листья, мусор, почта, искры фонарей)
    type_fx = prp2_export.type_fx_table(prop_types, sprops.atc)
    anm = aobj.anm1() if aobj else b''   # ANM1: объект -> экземпляры SPR1, живые объёмы COL1, действие и триггер
    out += prp2_export.wrap_prp2(prop_section(prop_types, prop_pieces_raw, tmap, coin_type, coin_pos), spr, type_fx, anm)
    if aobj: print(f'  ANM1: {len(aobj.objs)} объектов, {sum(len(o["vols"]) for o in aobj.objs)} живых объёмов, {len(anm)} Б')
    print('  частицы типов PRP1: ' + ', '.join(f"{t['name']}={type_fx[2 * i]}/{type_fx[2 * i + 1]}" for i, t in enumerate(prop_types)
                                                  if type_fx[2 * i] or type_fx[2 * i + 1] != 0xFF))
    print(f'  имён систем частиц SPR1: {sprops.fx_names}')
    print(f'стейтпропы SPR1: {len(sprops.types)} типов, {sum(len(T["inst"]) for T in sprops.types)} экземпляров, {len(spr) / 1024:.0f} КБ')
    # машина CAR1 — сразу после PRP1 (заголовок SHR7 теперь 104 Б, @100); PVS (tools/pvs) — в самый конец
    if car1:
        out += b'\0' * (-len(out) % 16); struct.pack_into('<I', out, 100, len(out)); out += car_section(car1, tmap)
    if regs is not None:   # правка 95: REG1 — последней перед PVS1 (tools/pvs допишет PVS1 сразу за хвостом REGF)
        out += b'\0' * (-len(out) % 16); out += regs.section(len(out))
        nreg = sum(1 for p_ in pieces if p_[6] == 0 and p_[12][0])
        print(f'  REG1: {len(regs.names)} регионов, сетка {regs.W}x{regs.H}; кусков города с регионом {nreg} из {sum(1 for p_ in pieces if p_[6] == 0)}')
    print(f'пропы PRP1: {len(prop_types)} типов, {sum(len(t["mats"]) for t in prop_types)} экземпляров')
    dst = Path(out_path) if out_path else ROOT / 'build' / 'level.shr'
    dst.parent.mkdir(parents=True, exist_ok=True); dst.write_bytes(out)
    # правка 129: источник каждой записи текстуры (гейт tools/tex_gate.py сверяет уровень 0 с PNG диска побайтно); key#blend — та же картинка
    texsrc = [None] * len(blob)   # cache: ключ -> номер в blob до отбора used; tmap: тот номер -> номер записи в файле
    for key, i in cache.items():
        j = tmap.get(i, -1) if isinstance(i, int) else -1
        if 0 <= j < len(blob) and texsrc[j] is None: texsrc[j] = dict(url=key.split('#')[0], blend=key.endswith('#blend'))
    if TOON:   # №679: линии контура — острые рёбра видимого города (угол между гранями > TOON_EDGE_DEG, длина >= TOON_EDGE_MIN) отдельным файлом <level>.toonlines (TLN1)
        write_toon_lines(str(dst) + '.toonlines')
    Path(str(dst) + '.texsrc.json').write_text(json.dumps(dict(cap=CAP, caps={u: c for u, c in getattr(texq, 'CAPS', {}).items()} if 'texq' in globals() else {},
                                                          t4=bool(getattr(texq, "CLUT4", False)) if "texq" in globals() else False, tex=texsrc)))
    mb = len(out) / 2**20
    print(f'{dst.name}: {mb:.1f} MB, {tris_total:,} треугольников (дальних {far_total:,}), {len(blob)} текстур, {len(cells)} клеток, {len(pieces)} кусков, карта {W}x{H}')
    if not SEAMS_OLD: print(f'швы (правки 224/237/251): кусков города с центром клетки {cell_own[1]:,}, со своим центром (не влезли в ±64 м) {cell_own[0]:,}')
    print(f'свет уровня: освещаемых предметов мира без цвета вершин (LIT) — {LIT_STATS[0]} треуг.; CAR1 с LGT1 — {bool(car1)}')

if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser(description='SHAR -> PSP level export (SHR7)')
    ap.add_argument('level', nargs='?', type=int, default=1)
    ap.add_argument('--out', default=str(ROOT / 'build' / 'level.shr'), help='куда писать (по умолчанию build/level.shr)')
    a = ap.parse_args()
    main(a.level, a.out)
