#!/usr/bin/env python3
"""№912 «Мультик без текстур»: список заливки уровня на Маке (flat912.bin рядом с level.shr). Движок не разбирает текстуры при загрузке (на PSP это ≈ 1,8 с на уровень),
а читает готовое решение по каждой текстуре: заливка или рисунок, преобладающий тон, доля тона, плотность границ.

  python3 tools/flat_list.py level.shr [notoon.txt] [выход flat912.bin] [--report]

Правила те же, что в viewer/toonflat.h (flatScan0): тон — среднее записей палитры рядом (RGB ≤ 40) с самой весомой; рисунок (остаётся с текстурой): прозрачность (>2 % точек),
список notoon (вывески), второй цвет (вне 2R от первого) заметной доли и контраста (у земли — от 0,4 % точек: разметка), доля тона < 40 %, плотность резких границ
(соседние точки, разница яркости > 35) выше порога (стены 3,5 %, земля 2,5 %). Формат FLT1: 'FLT1', ntex u32, подпись u32 (FNV-1a по номерам текстур кусков, как у notoon.txt), затем ntex × 8 байт:
  kind(0 рисунок, 1 заливка), причина(0 заливка, 1 прозрачность, 2 notoon, 3 второй цвет, 4 много цветов, 5 границы), R, G, B, доля 0..255, границы 0..255, 0
Причина (reason) нужна для отчёта: «список исключений числом»."""
import os, struct, sys
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))   # shr7read рядом (python -I не добавляет папку скрипта)

R2, R2_FAR = 1600, 6400
EDGE_D = 35
EDGE_WALL, EDGE_GROUND = .035, .025
JOINT_MAX = 0.10   # швы плит, трещины: тёмный второй цвет земли до этой доли точек
LOGO_D, LOGO_CHROMA, LOGO_MIN, LOGO_MAX = 90.0, 70, 0.004, 0.35   # знак/логотип: насыщенные точки далеко от основного тона, доля текстуры
THIN_D, THIN_CHROMA, THIN_LIGHT, THIN_MIN, THIN_MAX = 45.0, 40, 60, 0.003, 0.025   # №918: тонкая разметка на земле (жёлтая линия на сером асфальте, светлые штрихи): точки далеко от тона и насыщеннее него (+40) или светлее (+60), доля 0,3–2,5 % — земля остаётся родной текстурой (вид 0, причина 7)


WALL929_OLD = os.environ.get('WALL929_OLD') == '1'   # красный контроль: прежнее правило стен
ASPH929_OLD = os.environ.get('ASPH929_OLD') == '1'   # красный контроль: прежнее правило земли
WALL_FAR_D, WALL_FAR_MAX, WALL_E8_D, WALL_E8_MAX = 45.0, 0.02, 8, 0.03   # №929: стена уходит в заливку, только если однотонная: точек дальше 45 от тона не больше 2 %, соседних пар с разницей яркости > 8 не больше 3 % (кирпич, вагонка, окна, двери, черепица остаются текстурой, причина 10)
ASPH_R_GROUP, ASPH_R, ASPH_LUM = 24.0, 14.0, 140   # №929: асфальт проезжей части — родной текстурой (причина 8): тон земли без рисунка в радиусе 14 от тона дорог уровня (текстуры с разметкой, ≥ 2 в радиусе 24; тёмнее яркости 140)
WALL952_OLD = os.environ.get('WALL952_OLD') == '1'   # красный контроль: правило стен №929 без №952
FLAT952_E1 = 4.0   # №952: стена, у которой средний разброс цвета вокруг среднего (по трём каналам, из 255) не больше 4, — однотонная (слабый рельеф кирпича и досок не виден после цветокоррекции мультика и света): заливка
ASPH_EXTRA = ((108, 91, 103),)                       # тон асфальта загородной дороги (rd2_country / asphalt_country): в L3 и L6 разметочной текстуры такого тона нет, а плоская asphalt_country есть


def fnv(vals):
    h = 2166136261
    for v in vals: h = ((h ^ v) * 16777619) & 0xffffffff
    return h


def load_level(path):
    f = Path(path).read_bytes()
    assert f[:4] == b'SHR7'
    ntex, npc = struct.unpack_from('<II', f, 4)
    hw, hh = struct.unpack_from('<ii', f, 12 + 14 * 4)
    tableOff = struct.unpack_from('<I', f, 76)[0]
    p = 104 + hw * hh * 4 * 2; p = (p + 15) & ~15; p += hw * hh * 2 * 2; p = (p + 15) & ~15
    tex = []
    for i in range(ntex):
        w, h, lv = struct.unpack_from('<HHH', f, p); t4 = f[p + 6] == 4; pre = f[p + 7] == 1; ground = f[p + 9] == 1
        pal = np.frombuffer(f, np.uint8, 1024, p + 16).reshape(256, 4); q = p + 16 + 1024
        rb0 = None; lw, lh = w, h; l0 = None
        for l in range(lv):
            rb = max(16, (lw + 1) >> 1) if t4 else (16 if (pre and lw < 16) else lw); rows = ((lh + 7) & ~7) if pre else lh
            if l == 0: l0 = (q, rb, rows)
            q += rb * rows; q = (q + 15) & ~15; lw >>= 1; lh = lh > 1 and lh >> 1 or 1
        tex.append(dict(w=w, h=h, t4=t4, pre=pre, ground=ground, pal=pal, l0=l0)); p = q
    sig = fnv(struct.unpack_from('<I', f, tableOff + 80 * i + 24)[0] for i in range(npc))
    return f, tex, sig


def indices(f, t):
    q, rb, rows = t['l0']; w, h = t['w'], t['h']
    raw = np.frombuffer(f, np.uint8, rb * rows, q)
    if t['pre']:
        bpr = rb // 16
        raw = raw.reshape(rows // 8, bpr, 8, 16).transpose(0, 2, 1, 3).reshape(rows, rb)
    else:
        raw = raw.reshape(rows, rb)
    if t['t4']:
        lo, hi = raw & 15, raw >> 4
        px = np.stack([lo, hi], axis=2).reshape(rows, rb * 2)
    else:
        px = raw
    return px[:h, :w]


def cluster(pal, cnt, skipc=None):
    """-> (вес, цвет RGB float) самой весомой группы записей (R ≤ 40), без записей ближе 2R к skipc; None если пусто"""
    used = [j for j in range(256) if cnt[j] and pal[j, 3] >= 16]
    if skipc is not None:
        used = [j for j in used if ((pal[j, :3].astype(int) - skipc) ** 2).sum() > R2_FAR]
    if not used: return None
    cand = sorted(used, key=lambda j: (-cnt[j], j))[:12]
    best = None
    rgb = pal[used, :3].astype(int); w = np.array([cnt[j] for j in used])
    for c in cand:
        m = ((rgb - pal[c, :3].astype(int)) ** 2).sum(axis=1) <= R2
        wt = int(w[m].sum())
        if best is None or wt > best[0]: best = (wt, m)
    wt, m = best
    col = (rgb[m] * w[m, None]).sum(axis=0) / w[m].sum()
    return wt, col


def classify(f, t, notoon):
    pal = t['pal']; px = indices(f, t); cnt = np.bincount(px.ravel(), minlength=256)
    alpha = pal[:, 3]
    all_ = int(cnt.sum()); tot = int(cnt[alpha >= 16].sum()); semi = int(cnt[(alpha >= 16) & (alpha < 240)].sum())
    if not all_ or not tot or (all_ - tot) * 50 > all_ or semi * 50 > all_: return 0, 1, (0, 0, 0), 0, 0
    if notoon: return 0, 2, (0, 0, 0), 0, 0
    c1 = cluster(pal, cnt)
    if c1 is None: return 0, 1, (0, 0, 0), 0, 0
    w1, col1 = c1
    col1i = np.rint(col1).astype(int); ground = t['ground']
    lum = lambda c: .299 * c[0] + .587 * c[1] + .114 * c[2]
    joints = False
    c2 = cluster(pal, cnt, col1i)
    if c2 is not None:
        w2, col2 = c2; col2i = np.rint(col2).astype(int); c2i = col2i
        contrast = abs(lum(col2i) - lum(col1i)); c2d = int(((col2i - col1i) ** 2).sum() ** .5)
        share = w2 / tot; lim = .004 if ground else .15; clim = 40. if ground else 55.
        # швы плит, трещины, стыки: второй цвет у земли ТЕМНЕЕ основного и его немного — для мультика это зерно, не рисунок (плитка остаётся ровной с шумом)
        joints = ground and lum(c2i) < lum(col1i) - 10. and share <= JOINT_MAX
        if not joints and ((share >= lim and (contrast >= clim or c2d >= 150)) or (not ground and share >= .25 and contrast >= 38.)):
            return 0, 3, (0, 0, 0), 0, 0
    else: joints = False
    if not ground:   # знак, логотип, эмблема: небольшое насыщенное пятно на тёмном или ровном фоне (№912, слово Роба 23.10) — остаётся текстурой
        rgbp = pal[:, :3].astype(int); dd = np.sqrt(((rgbp - col1) ** 2).sum(axis=1)); chroma = rgbp.max(axis=1) - rgbp.min(axis=1)
        sat_far = (dd > LOGO_D) & (chroma >= LOGO_CHROMA) & (alpha >= 16)
        sh = float(cnt[sat_far].sum()) / tot
        if LOGO_MIN <= sh <= LOGO_MAX: return 0, 6, tuple(int(x) for x in col1i), 0, 0
    if ground and not joints:   # №918: тонкая разметка (kind 3 — крупная — уже вернулся родной текстурой); земля с редкими яркими или насыщенными штрихами не уходит в заливку
        rgbp = pal[:, :3].astype(int); dd = np.sqrt(((rgbp - col1) ** 2).sum(axis=1)); chroma = rgbp.max(axis=1) - rgbp.min(axis=1); base_ch = col1.max() - col1.min()
        base_l = .299 * col1[0] + .587 * col1[1] + .114 * col1[2]; lm = .299 * rgbp[:, 0] + .587 * rgbp[:, 1] + .114 * rgbp[:, 2]
        thin = (dd > THIN_D) & ((chroma >= base_ch + THIN_CHROMA) | (lm > base_l + THIN_LIGHT)) & (alpha >= 16)
        sh = float(cnt[thin].sum()) / tot
        if THIN_MIN <= sh <= THIN_MAX: return 0, 7, tuple(int(x) for x in col1i), 0, 0
    lum = (.299 * pal[:, 0] + .587 * pal[:, 1] + .114 * pal[:, 2]).astype(np.int32)
    L = lum[px]; e = 0; n = 0
    if px.shape[0] > 1 and px.shape[1] > 1:
        dh = np.abs(L[:, 1:] - L[:, :-1]) > EDGE_D; dv = np.abs(L[1:, :] - L[:-1, :]) > EDGE_D
        e = int(dh.sum() + dv.sum()); n = dh.size + dv.size
    ef = e / n if n else 0.
    ed = int(min(255, ef * 255))
    if ef > (EDGE_GROUND if ground else EDGE_WALL) and not joints: return 0, 5, tuple(int(x) for x in col1i), 0, ed
    if w1 / tot < .40: return 0, 4, tuple(int(x) for x in col1i), 0, ed
    if not ground and not WALL952_OLD:   # №952: «что однотонное или имитируется градиентом — в заливку»; тон заливки движок берёт со сдвигом к среднему (flatTintBuild), поэтому плавный градиент не темнеет и не светлеет
        rgbp = pal[:, :3].astype(float); a_ = alpha >= 16; mean = (rgbp[a_] * cnt[a_, None]).sum(axis=0) / tot
        e1 = float((np.abs(rgbp[a_] - mean).mean(axis=1) * cnt[a_]).sum()) / tot
        if e1 <= FLAT952_E1: return 1, 0, tuple(int(x) for x in col1i), int(255 * w1 / tot), ed
    if not ground and not WALL929_OLD:   # №929: «где однотонная — заливка, иначе текстура»
        rgbp = pal[:, :3].astype(float); far = float(cnt[(np.sqrt(((rgbp - col1) ** 2).sum(axis=1)) > WALL_FAR_D) & (alpha >= 16)].sum()) / tot
        e8 = 0.
        if px.shape[0] > 1 and px.shape[1] > 1:
            h8 = np.abs(L[:, 1:] - L[:, :-1]) > WALL_E8_D; v8 = np.abs(L[1:, :] - L[:-1, :]) > WALL_E8_D; e8 = (int(h8.sum()) + int(v8.sum())) / n
        if far > WALL_FAR_MAX or e8 > WALL_E8_MAX: return 0, 10, tuple(int(x) for x in col1i), 0, ed
    return 1, 0, tuple(int(x) for x in col1i), int(255 * w1 / tot), ed


def parse_notoon(path, ntex, sig):
    s = {}
    p = Path(path)
    if not p.exists(): return set()
    txt = p.read_text(errors='replace'); head, _, rest = txt.partition('\n')
    import re
    m = re.match(r'ntex (\d+) sig ([0-9a-fA-F]+)', head)
    if not m or int(m.group(1)) != ntex or int(m.group(2), 16) != sig: return set()
    out = set(); bit = 1
    for tok in re.finditer(r'[A-Za-z]+|\d+', rest):
        t = tok.group(0)
        if t[0].isalpha():
            if t[0] == 'f': bit = 2
        elif bit == 1 and int(t) < ntex: out.add(int(t))
    return out


def road_tones(f, tex, res):
    """№929: тона асфальта уровня — основной тон земли с рисунком (разметка: причины 3 и 7), если рядом есть ещё одна такая текстура (≥ 2 в радиусе ASPH_R_GROUP), и он тёмнее ASPH_LUM"""
    tone = {}
    for i, t in enumerate(tex):
        if not t['ground'] or res[i][1] not in (3, 7): continue
        c = cluster(t['pal'], np.bincount(indices(f, t).ravel(), minlength=256))
        if c is not None and .299 * c[1][0] + .587 * c[1][1] + .114 * c[1][2] <= ASPH_LUM: tone[i] = c[1]
    grp = [tone[i] for i in tone if sum(1 for j in tone if np.sqrt(((tone[i] - tone[j]) ** 2).sum()) <= ASPH_R_GROUP) >= 2]
    return grp + [np.array(c, float) for c in ASPH_EXTRA]


# №974: бордюр и угловая деталь с люком/стоком — по содержимому текстуры (одни и те же картинки на всех уровнях диска): 1 — заливка бордюра (серый бетон 64×64), 2 — рисунок углового люка.
# Ключ — первые 12 знаков sha1 от индексов точек уровня 0 и размера; палитру не берём (тон мультика её меняет)
CURB_REF = {'598b0da02c45': 1, '5a290a0bd875': 2, '2b1ad6fcdc09': 2, '9e7cd22a1990': 2}   # угловые детали: L1/L4 (5a29…), L2/L5 (2b1a…, фон бирюзовый), L7 (9e7c…)


def curb_class(f, t):
    import hashlib
    px = indices(f, t)
    return CURB_REF.get(hashlib.sha1(px.tobytes() + bytes([t['w'] % 256, t['h'] % 256])).hexdigest()[:12], 0)


def asphalt_source(lvl, tex, res):
    """№977: из текстур «асфальт родной» (причина 8) — с наибольшей площадью кусков уровня (чистый асфальт без разметки); None, если таких нет"""
    c = [i for i, r in enumerate(res) if r[1] == 8 and tex[i]['w'] >= 64]
    if not c: return None
    import shr7read as S2
    d = S2.load(lvl); P, T = d['P'], d['TEX']
    area = np.linalg.norm(np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]), axis=1) * .5
    return max(c, key=lambda i: (float(area[T == i].sum()), -i))


def asphalt_set(f, tex, res):
    """№979: текстуры земли с асфальтом (разметка и чистый): тон из группы тонов дорог уровня"""
    out = set()
    if ASPH929_OLD: return out
    tones = road_tones(f, tex, [(r[0], r[1]) for r in res])
    for i, t in enumerate(tex):
        if not t['ground'] or res[i][1] not in (3, 7, 8): continue
        c = cluster(t['pal'], np.bincount(indices(f, t).ravel(), minlength=256))
        if c is None or .299 * c[1][0] + .587 * c[1][1] + .114 * c[1][2] > ASPH_LUM: continue
        if tones and min(float(np.sqrt(((c[1] - tc) ** 2).sum())) for tc in tones) <= ASPH_R_GROUP: out.add(i)
    return out


CORNER987_MARK = 0x87   # №987: у рисунка угловой детали (вид 0) в R,G номер соседней заливки (u16), в B — этот знак; движок берёт итоговый цвет соседа


def corner_partners(lvl, res, cbs):
    """№987: для каждой угловой детали (класс 2) — заливка земли, чаще всего соседствующая с её кусками (общая вершина в кубике 1 м, считаются куски детали): тротуар или бордюр; зелёное и тёмное
    (газон, вода) не считается. -> {номер текстуры: номер соседа}"""
    import shr7read as S2
    from collections import defaultdict
    corners = [i for i, c in enumerate(cbs) if (c & 15) == 2]
    if not corners: return {}
    d = S2.load(lvl); P, T = d['P'], d['TEX']
    def ok(i):
        k, why, col, sh, ed = res[i]
        if k != 1: return False
        r, g, b = (int(v) for v in col); lum = .299 * r + .587 * g + .114 * b
        return (cbs[i] & 15) == 1 or (lum >= 110 and g <= r + 8)
    cand = {i for i in range(len(res)) if ok(i)}
    key = lambda p: (int(np.floor(p[0])), int(np.floor(p[1])), int(np.floor(p[2])))
    g = defaultdict(set)
    for i in np.nonzero(np.isin(T, list(cand)))[0]:
        for v in P[i]: g[key(v)].add(int(T[i]))
    out = {}
    for ct in corners:
        cnt = defaultdict(int)
        for i in np.nonzero(T == ct)[0]:
            seen = set()
            for v in P[i]:
                k0 = key(v)
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        for dz in (-1, 0, 1): seen |= g.get((k0[0] + dx, k0[1] + dy, k0[2] + dz), set())
            for t in seen: cnt[t] += 1
        if cnt: out[ct] = max(cnt.items(), key=lambda kv: (kv[1], -kv[0]))[0]
    return out


XWALK999_MARK = 0x99   # №999: у разметочной текстуры дороги (вид 0, причина 3 или 7, бит асфальта) в R,G номер «донора» — чистой текстуры дороги того же тона; движок тонирует её палитру по статистике донора
XWALK999_C, XWALK999_L = 10.0, 25.0   # донор: цветовой оттенок (цвет минус яркость, по каналам) не дальше C и яркость не дальше L от основного тона разметочной текстуры (L4: бледно-фиолетовый асфальт и перекрёсток на нём — другая дорога, не донор синей)


def xwalk_donors(f, tex, res, cbs):
    """№999: {номер разметочной текстуры дороги: номер чистой дороги того же тона (причина 8, бит асфальта, без разметки)}; ближайший по основному тону, не дальше XWALK999_R"""
    plain = []; marked = []
    for i, t in enumerate(tex):
        if not t['ground'] or not (cbs[i] & 0x10): continue
        c = cluster(t['pal'], np.bincount(indices(f, t).ravel(), minlength=256))
        if c is None: continue
        if res[i][1] == 8: plain.append((i, c[1]))
        elif res[i][1] in (3, 7) and (cbs[i] & 15) not in (1, 2, 3): marked.append((i, c[1]))
    out = {}
    for i, col in marked:
        lm = lambda c: .299 * c[0] + .587 * c[1] + .114 * c[2]
        ok = [(float(np.sqrt((((col - lm(col)) - (pc - lm(pc))) ** 2).sum())), j) for j, pc in plain if abs(lm(col) - lm(pc)) <= XWALK999_L]
        best = min(ok, default=None)
        if best and best[0] <= XWALK999_C: out[i] = best[1]
    return out


# №1005 (Роб 26.10: «на уровне Лизы красивая родная текстура пешеходной зоны, я бы её оставил»): светлая плитка тротуара L3 (текстура 128×128, 29a452…) остаётся родной текстурой, а не заливкой с зерном.
# Только L3: та же картинка на L6 (Барт) по слову Роба не меняется. Ключ — уровень и 12 знаков sha1 от индексов уровня 0 (как у CURB_REF). KEEP1005_OLD=1 — как в TEST71
KEEP1005 = {3: {'29a4529498a4'}}
KEEP1005_REASON = 11


def keep_native_1005(lvl, f, tex, res):
    """№1005: {номер текстуры уровня: ...} — заливка земли, которую оставить родной текстурой на этом уровне"""
    import hashlib, re
    if os.environ.get('KEEP1005_OLD') == '1': return set()
    m = re.fullmatch(r'l(\d)', Path(lvl).resolve().parent.name); L = int(m.group(1)) if m else 1
    keys = KEEP1005.get(L)
    if not keys: return set()
    out = set()
    for i, t in enumerate(tex):
        if not t['ground'] or res[i][0] != 1: continue
        px = indices(f, t)
        if hashlib.sha1(px.tobytes() + bytes([t['w'] % 256, t['h'] % 256])).hexdigest()[:12] in keys: out.add(i)
    return out


def build(lvl, notoon=None):
    """-> (байты flat912.bin, строки (вид, причина) по текстурам)"""
    notoon = notoon or str(Path(lvl).with_name('notoon.txt'))
    f, tex, sig = load_level(lvl); nt = parse_notoon(notoon, len(tex), sig)
    res = [classify(f, t, i in nt) for i, t in enumerate(tex)]
    if not ASPH929_OLD:   # №929: плоский асфальт — родной текстурой: тон земли без рисунка совпал с тоном дороги уровня
        tones = road_tones(f, tex, [(r[0], r[1]) for r in res])
        for i, t in enumerate(tex):
            if not t['ground'] or res[i][0] != 1: continue
            c = cluster(t['pal'], np.bincount(indices(f, t).ravel(), minlength=256))
            if c is not None and tones and min(float(np.sqrt(((c[1] - tc) ** 2).sum())) for tc in tones) <= ASPH_R and .299 * c[1][0] + .587 * c[1][1] + .114 * c[1][2] <= ASPH_LUM:
                res[i] = (0, 8, res[i][2], 0, res[i][4])
    for i in keep_native_1005(lvl, f, tex, res): res[i] = (0, KEEP1005_REASON, res[i][2], res[i][3], res[i][4])   # №1005: после правил асфальта, до списка бордюров и донора
    asph = asphalt_set(f, tex, res)   # №979: родной асфальт уровня — старшие 4 бита последнего байта строки: движок чуть осветляет палитру (flag asphlight)
    rows = []; body = bytearray()
    src = asphalt_source(lvl, tex, res)   # №977: чистый родной асфальт уровня — фрагмент для земли (класс 3)
    cbs = []
    for i in range(len(res)):
        cb = 0 if os.environ.get('CURB974_OLD') == '1' else curb_class(f, tex[i])
        if i == src and cb == 0: cb = 3
        if i in asph: cb |= 0x10
        cbs.append(cb)
    part = {} if os.environ.get('CURB987_OLD') == '1' else corner_partners(lvl, res, cbs)   # №987
    don = {} if os.environ.get('XWALK999_OLD') == '1' else xwalk_donors(f, tex, res, cbs)   # №999
    for i, (k, why, col, sh, ed) in enumerate(res):
        cb = cbs[i]
        if i in part: col = (part[i] & 255, part[i] >> 8, CORNER987_MARK)
        if i in don: col = (don[i] & 255, don[i] >> 8, XWALK999_MARK)
        body += struct.pack('<BBBBBBBB', k, why, col[0], col[1], col[2], sh, ed, cb); rows.append((k, why))
    return b'FLT1' + struct.pack('<II', len(tex), sig) + bytes(body), rows, sig


def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    lvl = a[0]; notoon = a[1] if len(a) > 1 else str(Path(lvl).with_name('notoon.txt')); out = a[2] if len(a) > 2 else str(Path(lvl).with_name('flat912.bin'))
    data, rows, sig = build(lvl, notoon); Path(out).write_bytes(data)
    from collections import Counter
    c = Counter(why for k, why in rows)
    c6 = [i for i, (k, why) in enumerate(rows) if why == 6]
    c7 = [i for i, (k, why) in enumerate(rows) if why == 7]
    c8 = [i for i, (k, why) in enumerate(rows) if why == 8]
    c10 = sum(1 for k, why in rows if why == 10)
    print(f'{lvl}: текстур {len(rows)}, заливка {sum(1 for k, _ in rows if k)}, рисунок {len(rows) - sum(1 for k, _ in rows if k)} (причины {dict(c)}; знак/логотип {len(c6)}; тонкая разметка земли {len(c7)}: {c7}; асфальт родной {len(c8)}: {c8}; стена с рисунком (№929) {c10}), подпись {sig:08x} -> {out}')


if __name__ == '__main__':
    main()
