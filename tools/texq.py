"""Качество текстур для SHR7: точные палитры, k-means вместо FASTOCTREE, по желанию CLUT4 / крупнее 128 px.

Главное: load_tex(url, cache, blob, blend=False) — замена psp_export.load_tex (blend: материал полупрозрачный, см. ниже), тот же
формат записи (16 байт заголовка <HHH10x> + палитра 256 x RGBA + уровни CLUT8 по 16 байт), те же размеры
(предел 128 px, мипы пока ширина >= 16). Меняется только палитра и выбор индексов:
  * если в картинке уровня <= 256 цветов (так почти у всех: диск PS2 хранит текстуры 4/8 бит с палитрой) —
    палитра = ровно эти цвета, ошибка 0;
  * иначе (картинку уменьшили до 128 px, или 32-битная) — k-means по цветам всех уровней (старт — FASTOCTREE,
    потом итерации Ллойда), индексы — ближайший цвет палитры; мипы берут ту же палитру.
Переключение в экспортёре (одна строка):  from texq import load_tex   (после определения своей load_tex)

Опции (по умолчанию выключены — формат и память ровно как сейчас):
  CAP       общий предел стороны (128 как сейчас; рекомендуется 256 — это ~8-15 вывесок/щитов с текстом на уровень)
  CAPS      {url: предел} поштучно (или файл $TEXQ_CAPS, json {url: предел})
  CLUT4     True: текстуры с <= 16 цветами на уровне 0 пишутся 4 битами (байт 6 заголовка = 4 = GU_PSM_T4, палитра всё так
            же 1024 байта, в ней 16 цветов; строка уровня lw/2 байт, не меньше 16 Б; мипы те же, что у T8, до 1 px), если
            ошибка мипов в 16 цветах не больше T4_MIP_ERR (правка 129). Движок: viewer/main.c texRowBytes.
  PRESWIZZLE True: уровни уже в swizzle-блоках 16 байт x 8 строк (байт 7 заголовка = 1, высота уровня добита
            до кратной 8 повтором последней строки — как swizzleLevel в main.c). Движку не нужна вторая копия.
Анализ (docs/textures.md):
  python tools/texq.py --level 1         -> метрики ошибок, таблица памяти, сравнительные картинки в scratch
"""
import sys, json, struct, argparse, os
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
TEXDIR = ROOT / 'hit-and-run-web/public/assets'          # как OUT в convert.py (url = 'textures/<hash>.png')
CAP = 128
CAPS = {}                                                  # url -> предел стороны (по умолчанию CAP)
CAPS_FILE = Path(os.environ['TEXQ_CAPS']) if os.environ.get('TEXQ_CAPS') else None   # json {url: предел}, по желанию
CLUT4 = False
T4_MIP_ERR = float(os.environ.get('SHR_T4_ERR', '4'))   # правка 129, «золотая середина»: T4, только если мипы в 16 цветах
                                                       # ошибаются в среднем не больше чем на 4/255 (иначе T8, как было)
PRESWIZZLE = False
SMALL_MIPS = False   # Ф2: мипы и ниже 16 px (до 1 px, не больше 8 уровней); строка уровня в файле минимум 16 Б
                     # (TBW GE >= 16 Б), высота блока 8 строк; байт 8 заголовка = 1. Мелкие уровни — BOX (среднее по
                     # площади), у «вырезок» (альфа 0/255, альфа-тест GREATER 0x40) покрытие держится как у уровня 0
KMEANS_ITERS = 16
GU_PSM_T4, GU_PSM_T8 = 4, 5
_caps_loaded = False


# ---------------------------------------------------------------------------------------------------------------
# палитра и индексы
# Правка 366 (05.10): у «вырезок» с альфа-тестом (полосы неба l1_BACKMOUNTAINS, l1_valleybackground: ATST=1, порог 0x40) почти
# прозрачный тексел мипа (альфа 2–15) по евклиду RGBA подбирался к цвету палитры с альфой 87–152 и ПРОХОДИЛ тест — над горами
# штрихи и строки пикселей (мип 1 верх: 62 % прошедших вместо 0 %). Теперь ближайший цвет ищется среди записей палитры того же
# класса «проходит / не проходит» (альфа > ALPHA_REF, как движок sceGuAlphaFunc(GU_GREATER, 0x40)); нет записей нужного класса —
# как прежде, среди всех. SHR_ALPHA_NEAREST_OLD=1 — прежний подбор (красный контроль, tests/texq/alpha366.py)
ALPHA_NEAREST_OLD = bool(os.environ.get('SHR_ALPHA_NEAREST_OLD'))
ALPHA_REF = 64   # движок: sceGuAlphaFunc(GU_GREATER, 0x40)


def _nearest(px, pal):
    """px (N,4) uint8, pal (K,4) uint8 -> индексы ближайших цветов (RGBA, евклид), кусками по 64k;
    правка 366: только среди цветов того же класса альфа-теста (если в палитре есть оба класса)"""
    P = pal.astype(np.float32); pn = (P ** 2).sum(1); out = np.empty(len(px), np.uint8 if len(pal) <= 256 else np.int32)
    pp = pal[:, 3] > ALPHA_REF
    split = not ALPHA_NEAREST_OLD and pp.any() and (~pp).any()
    for i in range(0, len(px), 65536):
        X = px[i:i + 65536].astype(np.float32)
        D = pn[None] - 2 * X @ P.T
        if split:
            xp = px[i:i + 65536, 3] > ALPHA_REF
            D = D + np.where(xp[:, None] != pp[None], np.float32(1e12), np.float32(0))
        out[i:i + 65536] = D.argmin(1)
    return out


def kmeans_palette(px, k, init=None, iters=KMEANS_ITERS):
    """Палитра k цветов для пикселей px (N,4 uint8). <= k различных цветов -> ровно они (ошибка 0).
    Иначе k-means (Ллойд) по уникальным цветам с весом = число пикселей; пустые кластеры пересеваются
    в самые плохо представленные цвета. -> (k,4) uint8 (лишние записи нулевые), число занятых записей"""
    u, cnt = np.unique(px.view('<u4').reshape(-1), return_counts=True)
    U8 = u.view(np.uint8).reshape(-1, 4)
    if len(U8) <= k:
        pal = np.zeros((k, 4), np.uint8); pal[:len(U8)] = U8; return pal, len(U8)
    U = U8.astype(np.float32); w = cnt.astype(np.float32)
    if init is None or len(init) < k:
        C = U[np.argsort(-w)[:k]].copy()
    else: C = init[:k].astype(np.float32).copy()
    un = (U ** 2).sum(1)
    for _ in range(iters):
        d = un[:, None] - 2 * U @ C.T + (C ** 2).sum(1)[None]
        a = d.argmin(1); e = d[np.arange(len(U)), a] * w
        s = np.zeros((k, 4), np.float64); np.add.at(s, a, U * w[:, None]); n = np.bincount(a, weights=w, minlength=k)
        ok = n > 0; C[ok] = (s[ok] / n[ok, None]).astype(np.float32)
        if (~ok).any():
            worst = np.argsort(-e)[:int((~ok).sum())]; C[~ok] = U[worst]
    pal = np.clip(np.round(C), 0, 255).astype(np.uint8)
    return pal, k


def octree_palette(img, k=256):
    """Палитра так, как её делает psp_export.load_tex сегодня (FASTOCTREE) -> (256,4) uint8"""
    q = img.quantize(k, method=Image.Quantize.FASTOCTREE)
    pal = np.zeros((256, 4), np.uint8)
    p = np.array(q.getpalette(rawmode='RGBA'), np.uint8).reshape(-1, 4)[:256]; pal[:len(p)] = p
    return pal, q


# ---------------------------------------------------------------------------------------------------------------
# размеры и уровни — ровно как psp_export.load_tex
def target_size(w, h, cap):
    s = min(1.0, cap / max(w, h))
    return max(16, int(w * s)), max(8, int(h * s))


# План оптимизации А5 (10.10): уровни мипов 1..N — окно Кайзера (a=4, beta=4) вместо Ланцоша-3: на понижении вдвое проход у границы нового найквиста (0,20 цикл/px источника)
# 0,90 против 0,82, утечка в алиас (0,35) 0,005 против 0,014, перелёт ступени 6,1 % против 5,6 % (расчёт tests/mipfilter/curve.py; на 81 текстуре диска резкость уровней 1-2 +10 %, перелёт +21 % пикселей вне диапазона источника на уровне 1, средняя разница 1,2-1,5 из 255).
# Альфа берётся предумноженной, как у Pillow. SHR_MIPKAISER_OLD=1 — прежний Ланцош-3 (PIL). Уровень 0 и хвост BOX (SMALL_MIPS) не меняются
MIP_KAISER = os.environ.get('SHR_MIPKAISER_OLD') != '1'
KAISER_A, KAISER_BETA = 4.0, 4.0


def _kaiser_w(src, dst, a=KAISER_A, beta=KAISER_BETA):
    """матрица весов (dst x src): окно Кайзера, центр отсчёта как у Pillow ((i + 0.5) * scale), края нормируются"""
    scale = src / dst; fs = max(scale, 1.0); sup = a * fs
    W = np.zeros((dst, src), np.float64)
    for i in range(dst):
        c = (i + 0.5) * scale; lo = max(int(c - sup + 0.5), 0); hi = min(int(c + sup + 0.5), src)
        x = (np.arange(lo, hi) + 0.5 - c) / fs
        t = np.clip(1 - (x / a) ** 2, 0, 1)
        k = np.sinc(x) * np.i0(beta * np.sqrt(t)) / np.i0(beta); k[np.abs(x) >= a] = 0
        sm = k.sum(); W[i, lo:hi] = k / sm if sm else 0
    return W


def kaiser_resize(img, w, h):
    """RGBA/RGB -> уменьшение окном Кайзера из исходника (предумноженная альфа); возвращает PIL-картинку того же режима"""
    mode = img.mode
    im = img if mode in ('RGB', 'RGBA', 'L', 'LA') else img.convert('RGBA'); mode = im.mode
    a = np.asarray(im, np.float64); sh = a.shape
    if a.ndim == 2: a = a[..., None]
    if mode in ('RGBA', 'LA'):
        al = a[..., -1:] / 255.0; a = np.concatenate([a[..., :-1] * al, a[..., -1:]], -1)
    Wy = _kaiser_w(sh[0], h); Wx = _kaiser_w(sh[1], w)
    o = np.einsum('ij,jkc->ikc', Wy, a); o = np.einsum('kj,ijc->ikc', Wx, o)
    if mode in ('RGBA', 'LA'):
        al = o[..., -1:] / 255.0; col = o[..., :-1]; col = np.where(al > 1e-6, col / np.maximum(al, 1e-6), 0.0); o = np.concatenate([col, o[..., -1:]], -1)
    o = np.clip(np.round(o), 0, 255).astype(np.uint8)
    if len(sh) == 2: o = o[..., 0]
    return Image.fromarray(o, mode)


def _fully_opaque(img):
    """текстура без прозрачности (альфа везде >= 250; как условие fade_mips)"""
    if img.mode == 'P': return 'transparency' not in img.info
    if img.mode in ('RGBA', 'LA'): return int(np.asarray(img.getchannel('A')).min()) >= 250
    return True


def mip_images(img, w, h, min_w=16, coverage=True):
    """[уровень 0, 1, ...] RGBA, как в load_tex: LANCZOS из исходника, пока ширина >= min_w, не больше 8.
    coverage=False — без масштаба альфы «покрытия» (keep_coverage): для текстур, которые рисуются полупрозрачными"""
    base = img if img.size == (w, h) else img.resize((w, h), Image.LANCZOS)
    kz = MIP_KAISER and _fully_opaque(img)   # 11.10 (TEST11: стекло и очки розовые/чёрные): окно Кайзера только у непрозрачных текстур; у стекла, листвы и вывесок с альфой звон давал
    out = [base]; lw, lh = w // 2, max(1, h // 2)   # цвета 0/255 в мипах (деление предумноженного цвета на малую альфу) — у них прежний Ланцош Pillow
    while lw >= min_w and len(out) < 8:
        out.append(kaiser_resize(img, lw, lh) if kz else img.resize((lw, lh), Image.LANCZOS)); lw, lh = lw // 2, max(1, lh // 2)
    if SMALL_MIPS and min_w == 16:   # Ф2: дальше до 1 px — среднее по площади (BOX): у LANCZOS на 8..1 px звон и «грязь»
        while lw >= 1 and len(out) < 8:
            out.append(img.resize((lw, lh), Image.BOX)); lw, lh = lw // 2, max(1, lh // 2)
        if coverage: keep_coverage(out)
    if _FADE_NOW: fade_mips(out)
    return out


# Правка Роба 500 / 416 (слово Роба: «дороги вдалеке бьются пикселями», ресёрч docs/handoff/ОПТИМИЗАЦИЯ_PSP_РЕСЁРЧ_2026-10-10.md §4, приём 1): мерцание
# дали даёт контраст тонких линий (разметка, швы плит, асфальт со стыками), который глубокие уровни мипов всё ещё несут: на экране 480x272 такой уровень
# сэмплируется реже периода линии и «бьётся». Уровни с k >= 1 текстур с заметным контрастом смешиваются со средним цветом самого
# уровня (альфа-взвешенно): k=1 на 15 %, k=2 на 40 %, k=3 на 75 %, k>=4 на 100 % (масштаб MIP_FADE_K; слово Роба 10.10 «давай сильно»). Уровень 0, альфа, любые текстуры с прозрачностью (частицы, стекло, вырезки)
# и текстуры с малым контрастом (ровные поверхности, небо) не меняются. Цена 0 мс и 0 КБ. SHR_MIPFADE_OLD=1 — прежние мипы (красный контроль)
import os as _os
MIP_FADE = _os.environ.get('SHR_MIPFADE_OLD') != '1'
MIP_FADE_K = float(_os.environ.get('SHR_MIPFADE_K', '1.0'))
MIP_FADE_STD = float(_os.environ.get('SHR_MIPFADE_STD', '5'))   # порог контраста уровня 0 (стандартное отклонение яркости, 0..255)
FADE_URLS = None   # None — любая подходящая текстура (инструменты); множество url — только они (psp_export: земля, дороги, тротуары, трава по имени)
_FADE_NOW = True   # выставляет load_tex на время сборки одной текстуры


def fade_mips(levels):
    """levels — список RGBA-картинок; меняет уровни k >= 2 на месте. -> число изменённых уровней"""
    if not MIP_FADE or len(levels) < 2: return 0
    a0 = np.asarray(levels[0], np.float32)
    w0 = a0[..., 3] / 255.0
    if w0.sum() < 1 or a0[..., 3].min() < 250: return 0   # только непрозрачное: частицы, стекло, вырезки (альфа) и вывески-билборды не трогаем
    lum0 = (a0[..., 0] * .299 + a0[..., 1] * .587 + a0[..., 2] * .114)
    mu0 = float((lum0 * w0).sum() / w0.sum())
    if float(np.sqrt(((lum0 - mu0) ** 2 * w0).sum() / w0.sum())) < MIP_FADE_STD: return 0   # ровная поверхность: рябить нечему
    n = 0
    for k in range(1, len(levels)):
        t = min(1.0, MIP_FADE_K * (0.15 if k == 1 else 0.4 if k == 2 else 0.75 if k == 3 else 1.0))
        px = np.asarray(levels[k], np.float32).copy(); w = px[..., 3:4] / 255.0
        if w.sum() < 1e-3: continue
        avg = (px[..., :3] * w).sum((0, 1)) / w.sum()
        px[..., :3] = px[..., :3] * (1 - t) + avg * t
        levels[k] = Image.fromarray(np.clip(np.round(px), 0, 255).astype(np.uint8), 'RGBA'); n += 1
    return n


# №511 (швы неба, слово Роба: «будто PNG плохо наслоен на небо, край слоя виден»): слои неба (холмы, облака) — PNG с альфой, у прозрачных и
# полупрозрачных пикселей в файле «чужой» цвет (облака: белый, холмы: серо-зелёный 114,148,130). Билинейный фильтр GE и мипы смешивают цвет с соседом
# и на границе альфа-теста даёт светлую/тёмную кайму. Правило: цвет НЕВИДИМЫХ пикселей (альфа ниже порога: 1 для смешиваемого слоя-облака, 65 = ALPHA_REF + 1
# для слоя с альфа-тестом) берётся у ближайших видимых (расползание по 8 соседям, по горизонтали с переходом через край: u повторяется); видимые пиксели и
# вся альфа не меняются. Только для url из BLEED_URLS = {url: порог} (psp_export кладёт туда текстуры объектов неба). SKY_SEAM_OLD=1 / SHR_SKY_SEAM_OLD=1 — как раньше (красный контроль)
import os as _os2
SKY_SEAM_OLD = _os2.environ.get('SKY_SEAM_OLD') == '1' or _os2.environ.get('SHR_SKY_SEAM_OLD') == '1'
BLEED_URLS = {}
SKY_KEY = '#sky'   # ключ кэша копии текстуры слоя неба с bleed (url + SKY_KEY)


def _shift2d(x, dy, dx):
    """сдвиг на (dy, dx): по горизонтали с переходом через край, по вертикали край не переходит (пустое место = 0)"""
    x = np.roll(x, dx, 1)
    if dy > 0: x = np.roll(x, dy, 0); x[:dy] = 0
    elif dy < 0: x = np.roll(x, dy, 0); x[dy:] = 0
    return x


def bleed_rgb(img, thr=1):
    a = np.asarray(img, np.uint8).copy(); al = a[..., 3]
    if int(al.max()) < thr: return img
    known = al >= thr
    if known.all() or not known.any(): return img
    rgb = a[..., :3].astype(np.float32); H, W = al.shape
    for _ in range(max(H, W)):
        unk = ~known
        if not unk.any(): break
        kf = known.astype(np.float32); kr = rgb * kf[..., None]
        sr = np.zeros_like(rgb); sk = np.zeros_like(kf)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dy == 0 and dx == 0: continue
                sr += _shift2d(kr, dy, dx); sk += _shift2d(kf, dy, dx)
        new = unk & (sk > 0)
        if not new.any(): break
        rgb[new] = sr[new] / sk[new][:, None]; known = known | new
    a[..., :3] = np.clip(np.round(rgb), 0, 255).astype(np.uint8)
    return Image.fromarray(a, 'RGBA')


def is_cutout(a0):
    """«Вырезка»: альфа уровня 0 почти только 0/255 (листва, заборы, решётки, лестницы) — её мипы правит keep_coverage"""
    a0 = np.asarray(a0)
    mid = ((a0 > 8) & (a0 < 247)).mean()
    return not (mid > .02 or a0.min() >= 128 or a0.max() < 128)


def keep_coverage(levels):
    """«Вырезка» (листва, заборы, решётки: альфа почти только 0/255) — уменьшенные уровни теряют покрытие при
    альфа-тесте (дерево вдали «тает»). Масштабируем альфу уровня так, чтобы доля пикселей > ALPHA_REF была как у
    уровня 0 (Castaño, alpha-to-coverage mipmaps). Полупрозрачное (стекло) не трогаем.
    Только для текстур, которые рисуются одним альфа-тестом: у полупрозрачного материала (флаг 2) альфа — это
    непрозрачность, масштаб 0.3–0.5 делает мип прозрачнее, а при альфе <= ALPHA_REF предмет пропадает совсем
    (лестница пожарной машины вдали и под углом). Такие текстуры строятся с coverage=False (load_tex blend=True)."""
    a0 = np.asarray(levels[0], np.uint8)[..., 3]
    if not is_cutout(a0): return
    c0 = (a0 > ALPHA_REF).mean()
    for i in range(1, len(levels)):
        px = np.asarray(levels[i], np.uint8).copy(); a = px[..., 3].astype(np.float32)
        lo, hi = .25, 8.
        for _ in range(24):
            s = (lo * hi) ** .5
            if (np.clip(a * s, 0, 255) > ALPHA_REF).mean() < c0: lo = s
            else: hi = s
        px[..., 3] = np.clip(np.round(a * hi), 0, 255).astype(np.uint8)
        levels[i] = Image.fromarray(px, 'RGBA')


def swizzle(idx2d, row_bytes):
    """Уровень (h, row_bytes) байт -> swizzle-блоки 16 байт x 8 строк (высота добивается повтором последней строки)"""
    h = idx2d.shape[0]; hp = (h + 7) & ~7
    a = np.concatenate([idx2d, np.repeat(idx2d[-1:], hp - h, 0)]) if hp > h else idx2d
    rb = row_bytes // 16
    return a.reshape(hp // 8, 8, rb, 16).transpose(0, 2, 1, 3).reshape(-1)


def encode(levels_rgba, pal, fmt=GU_PSM_T8, preswizzle=False, idx0=None):
    """RGBA-уровни + палитра -> байты записи текстуры SHR7 (формат load_tex; байт 6 = формат T4, байт 7 = swizzle).
    idx0 — готовые индексы уровня 0 (так делает сегодняшний путь: индексы FASTOCTREE, а не ближайший цвет)"""
    w, h = levels_rgba[0].size
    t4 = fmt == GU_PSM_T4   # строка T4 — полбайта на тексел; строка уже 16 Б добивается до 16 Б (байт 8 = 1): T8 < 16 px, T4 < 32 px
    small = any((im.size[0] + 1) // 2 < 16 if t4 else im.size[0] < 16 for im in levels_rgba)
    assert preswizzle or not small, 'мипы < 16 px только с PRESWIZZLE (движок читает строку max(w,16) в swizzle)'
    head = struct.pack('<HHHBBB7x', w, h, len(levels_rgba), GU_PSM_T4 if fmt == GU_PSM_T4 else 0, 1 if preswizzle else 0, 1 if small else 0)
    out = bytearray(head + pal.astype(np.uint8).tobytes() + b'\0' * (1024 - pal.size))
    for li, im in enumerate(levels_rgba):
        lw, lh = im.size
        if li == 0 and idx0 is not None: idx = np.asarray(idx0, np.uint8).reshape(lh, lw)
        else: idx = _nearest(np.asarray(im, np.uint8).reshape(-1, 4), pal).astype(np.uint8).reshape(lh, lw)
        if fmt == GU_PSM_T4:
            if idx.shape[1] & 1: idx = np.concatenate([idx, idx[:, -1:]], 1)            # мип 1 px: пара из одного тексела
            idx = (idx[:, 0::2] & 15) | ((idx[:, 1::2] & 15) << 4)                      # младший полубайт — левый пиксель
        if idx.shape[1] < 16: idx = np.concatenate([idx, np.repeat(idx[:, -1:], 16 - idx.shape[1], 1)], 1)   # TBW >= 16 Б
        b = swizzle(idx, idx.shape[1]).tobytes() if preswizzle else idx.tobytes()
        out += b; out += b'\0' * (-len(out) % 16)
    return bytes(out)


def _load_caps():
    global _caps_loaded
    if _caps_loaded: return
    _caps_loaded = True
    try:
        if CAPS_FILE and CAPS_FILE.exists():
            for u, c in json.loads(CAPS_FILE.read_text()).items(): CAPS.setdefault(u, int(c))
    except Exception as e:
        print('texq: caps file ignored:', e)


def build(img, cap=CAP, method='kmeans', clut4=False, preswizzle=False, coverage=True):
    """RGBA исходник -> (байты записи, уровни RGBA-эталоны, палитра, формат). method: 'kmeans' | 'octree'
    coverage=False — мипы без масштаба альфы покрытия (текстура рисуется полупрозрачной)"""
    w, h = target_size(*img.size, cap)
    if method == 'octree':   # ровно psp_export.load_tex: мипы из уже уменьшенной картинки, уровень 0 — индексы FASTOCTREE
        base = img if img.size == (w, h) else img.resize((w, h), Image.LANCZOS)
        pal, q = octree_palette(base); levels = [base]; lw, lh = w // 2, max(1, h // 2)
        while lw >= 16 and len(levels) < 8:
            levels.append(base.resize((lw, lh), Image.LANCZOS)); lw, lh = lw // 2, max(1, lh // 2)
        return encode(levels, pal, GU_PSM_T8, preswizzle, np.asarray(q, np.uint8)), levels, pal, GU_PSM_T8
    levels = mip_images(img, w, h, coverage=coverage)
    px0 = np.asarray(levels[0], np.uint8).reshape(-1, 4)
    fmt = GU_PSM_T8
    if clut4 and w >= 32 and len(np.unique(px0.view('<u4'))) <= 16:
        pal16 = palette16(levels)
        if t4_mip_error(levels, pal16) <= T4_MIP_ERR:   # правка 129: та же цепочка мипов (до 1 px), уровень 0 без потерь
            return encode(levels, pal16, GU_PSM_T4, preswizzle), levels, pal16, GU_PSM_T4
    k = 256
    if len(np.unique(px0.view('<u4'))) <= k:   # уровень 0 без потерь; свободные записи палитры — цветам мипов
        pal, used = kmeans_palette(px0, k)
        if used < k and len(levels) > 1:
            rest = np.concatenate([np.asarray(l, np.uint8).reshape(-1, 4) for l in levels[1:]])
            extra, ne = kmeans_palette(rest, k - used)
            pal[used:used + min(ne, k - used)] = extra[:min(ne, k - used)]
    else:                                        # уменьшенная или 32-битная: k-means по пикселям всех уровней
        allpx = np.concatenate([np.asarray(l, np.uint8).reshape(-1, 4) for l in levels])
        init = octree_palette(levels[0])[0] if k == 256 else None
        pal, _ = kmeans_palette(allpx, k, init)
    return encode(levels, pal, fmt, preswizzle), levels, pal, fmt


def palette16(levels):
    """T4: ровно цвета уровня 0 (их <= 16), свободные записи — k-means по цветам мипов (как у T8)"""
    px0 = np.asarray(levels[0], np.uint8).reshape(-1, 4)
    pal, used = kmeans_palette(px0, 16)
    if used < 16 and len(levels) > 1:
        rest = np.concatenate([np.asarray(l, np.uint8).reshape(-1, 4) for l in levels[1:]])
        extra, ne = kmeans_palette(rest, 16 - used)
        pal[used:used + min(ne, 16 - used)] = extra[:min(ne, 16 - used)]
    return pal


def t4_mip_error(levels, pal):
    """Средняя ошибка RGB (0..255) видимых пикселей трёх первых мипов в 16 цветах palette16 + потеря покрытия альфа-теста
    (доля пикселей > ALPHA_REF) — что больше. Мипов нет -> 0"""
    errs = []
    for l in levels[1:4]:
        a = np.asarray(l, np.uint8).reshape(-1, 4); q = pal[_nearest(a, pal)]
        vis = a[:, 3] >= 128
        rgb = np.abs(a[:, :3].astype(np.int16) - q[:, :3].astype(np.int16))
        cov = abs(float((a[:, 3] > ALPHA_REF).mean()) - float((q[:, 3] > ALPHA_REF).mean())) * 255
        errs.append(max(float(rgb[vis].mean()) if vis.any() else 0.0, cov))
    return float(np.mean(errs)) if errs else 0.0


BLEND_KEY = '#blend'   # ключ кэша копии «вырезки» для полупрозрачного материала (url + BLEND_KEY)


FLIP_V = set()   # правка Роба 226: url картинок, которые на диске лежат PNG (строки сверху вниз) — перевернуть (psp_export.PNG_SRC)


def load_tex(url, cache, blob, blend=False, sky=False, flat=False):
    """Замена psp_export.load_tex: тот же формат; палитра точная / k-means (см. шапку файла).
    blend=True — текстуру рисует полупрозрачный материал (флаг 2 куска, билборд, блик): альфа мипов = непрозрачность,
    масштаб покрытия альфа-теста (keep_coverage) ей нельзя. Если это «вырезка» (только у неё keep_coverage что-то
    меняет), она строится отдельной копией без масштаба (ключ url + BLEND_KEY); тот же рисунок у материала с одним
    альфа-тестом остаётся с масштабом покрытия. Не вырезка — одна общая запись, как раньше."""
    if not url: return -1
    skyb = sky and url in BLEED_URLS and not SKY_SEAM_OLD   # №511: bleed только у копии для объекта неба (свой ключ), общая текстура города не меняется
    key = url + (BLEND_KEY if blend else '') + (SKY_KEY if skyb else '') + ('#toon' if flat else '')
    if key in cache: return cache[key]
    _load_caps()
    img = Image.open(TEXDIR / url).convert('RGBA')
    if url in FLIP_V: img = img.transpose(Image.FLIP_TOP_BOTTOM)   # правка 226
    if flat:   # №679 мультик: однотонная заливка средним цветом по альфе; у вырезок форма альфы остаётся, у сплошных текстур размер 16x8
        a_ = np.asarray(img, np.float64).reshape(-1, 4); wt_ = a_[:, 3:4] / 255.0 + 1e-6
        avg_ = tuple(int(x) for x in ((a_[:, :3] * wt_).sum(0) / wt_.sum()).round())
        al_ = np.asarray(img.getchannel('A'))
        if al_.min() >= 250: img = Image.new('RGBA', (16, 8), avg_ + (255,))
        else:
            arr_ = np.zeros((img.size[1], img.size[0], 4), np.uint8); arr_[..., :3] = avg_; arr_[..., 3] = al_
            img = Image.fromarray(arr_, 'RGBA')
        data, _, _, _ = build(img, CAPS.get(url, CAP), 'kmeans', CLUT4, PRESWIZZLE)
        blob.append(data); cache[key] = len(blob) - 1
        return cache[key]
    if skyb: img = bleed_rgb(img, BLEED_URLS[url])   # №511: кайма слоя неба
    global _FADE_NOW; _FADE_NOW = FADE_URLS is None or url in FADE_URLS
    if skyb:   # отдельная копия слоя неба (ни общей записи с городом, ни копии «вырезки»)
        data, _, _, _ = build(img, CAPS.get(url, CAP), 'kmeans', CLUT4, PRESWIZZLE, coverage=not blend)
        blob.append(data); cache[key] = len(blob) - 1
        return cache[key]
    if blend:
        data, levels, _, fmt = build(img, CAPS.get(url, CAP), 'kmeans', CLUT4, PRESWIZZLE, coverage=False)
        # условие mip_images/keep_coverage: мипы до 1 px (T4 тоже, правка 129) и вырезка; иначе копия совпала бы байт в байт
        if not (SMALL_MIPS and is_cutout(np.asarray(levels[0], np.uint8)[..., 3])):
            if url not in cache: blob.append(data); cache[url] = len(blob) - 1
            cache[key] = cache[url]
            return cache[key]
        blob.append(data); cache[key] = len(blob) - 1
        return cache[key]
    data, _, _, _ = build(img, CAPS.get(url, CAP), 'kmeans', CLUT4, PRESWIZZLE)
    blob.append(data); cache[url] = len(blob) - 1
    return cache[url]


def load_img(key, img, cache, blob):
    """№1061: готовая картинка (вырезка колеса машины) -> запись текстуры под своим ключом; те же палитра, мипы и формат, что у load_tex"""
    if key in cache: return cache[key]
    _load_caps()
    global _FADE_NOW; _FADE_NOW = FADE_URLS is None   # как у load_tex для машинной текстуры: вдали диск сливается в ровный тон, а не в полосу (приёмка Оптимизации: диагональ на диске sedanB, мип 3); среднее вырезки — резина
    data, _, _, _ = build(img, CAP, 'kmeans', CLUT4, PRESWIZZLE)
    blob.append(data); cache[key] = len(blob) - 1
    return cache[key]


# ---------------------------------------------------------------------------------------------------------------
# анализ: ошибки, память, картинки
def decode(data):
    """байты записи -> список RGBA-уровней (numpy), как их увидит GE (без swizzle)"""
    w, h, n = struct.unpack_from('<HHH', data, 0); fmt, swz, small = data[6], data[7], data[8]
    pal = np.frombuffer(data, np.uint8, 1024, 16).reshape(256, 4); p = 16 + 1024; out = []; lw, lh = w, h
    for _ in range(n):
        rl = (lw + 1) // 2 if fmt == GU_PSM_T4 else lw; rb = max(rl, 16) if small else rl; hp = (lh + 7) & ~7 if swz else lh
        raw = np.frombuffer(data, np.uint8, rb * hp, p); p += rb * hp; p += -p % 16
        if swz: raw = raw.reshape(hp // 8, rb // 16, 8, 16).transpose(0, 2, 1, 3).reshape(hp, rb)[:lh]
        raw = raw.reshape(lh, rb)[:, :rl]
        if fmt == GU_PSM_T4: raw = np.stack([raw & 15, raw >> 4], -1).reshape(lh, 2 * rl)[:, :lw]
        out.append(pal[raw]); lw, lh = lw // 2, max(1, lh // 2)
    return out


def errors(levels_ref, levels_got):
    """-> dict: RGB средняя/макс ошибка на уровне 0 (по видимым пикселям, alpha >= 128), макс ошибка альфы,
    средняя по мипам, «размытая» средняя ошибка уровня 0 (бокс 3x3, как видит глаз при фильтрации)"""
    r0 = np.asarray(levels_ref[0], np.int16); g0 = np.asarray(levels_got[0], np.int16)
    vis = r0[..., 3] >= 128; d = np.abs(r0 - g0)
    rgb = d[..., :3][vis] if vis.any() else d[..., :3].reshape(-1, 3)
    def blur(a):
        a = a.astype(np.float32); p = np.pad(a, ((1, 1), (1, 1), (0, 0)), mode='wrap')
        return sum(p[i:i + a.shape[0], j:j + a.shape[1]] for i in range(3) for j in range(3)) / 9
    bl = np.abs(blur(r0[..., :3]) - blur(g0[..., :3]))[vis] if vis.any() else np.zeros((1, 3))
    mm = [np.abs(np.asarray(a, np.int16) - np.asarray(b, np.int16))[..., :3].mean() for a, b in zip(levels_ref[1:], levels_got[1:])]
    return dict(mean=float(rgb.mean()), max=int(rgb.max()), amax=int(d[..., 3].max()), p99=float(np.percentile(rgb.max(1), 99)),
                blur=float(bl.mean()), mips=float(np.mean(mm)) if mm else 0.0)


def ordered_dither_build(img, cap=CAP, amp=6.0):
    """Опыт: упорядоченный дизеринг (Байер 4x4) поверх k-means-палитры — только для уменьшенных картинок."""
    data, levels, pal, fmt = build(img, cap, 'kmeans')
    B = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.float32) / 16 - .5
    out = []
    for im in levels:
        a = np.asarray(im, np.float32); h, w = a.shape[:2]
        t = np.tile(B, (h // 4 + 1, w // 4 + 1))[:h, :w, None] * amp
        a2 = np.clip(a + np.concatenate([np.repeat(t, 3, 2), np.zeros_like(t)], 2), 0, 255).astype(np.uint8)
        out.append(Image.fromarray(a2, 'RGBA'))
    return encode(out, pal, fmt), levels


def tex_bytes(w, h, levels, fmt=GU_PSM_T8, swz=False):
    n = 16 + 1024; lw, lh = w, h
    for _ in range(levels):
        rb = lw // 2 if fmt == GU_PSM_T4 else lw; hp = (lh + 7) & ~7 if swz else lh
        n += rb * hp; n += -n % 16; lw, lh = lw // 2, max(1, lh // 2)
    return n


def n_levels(w, min_w=16):
    n, lw = 0, w
    while lw >= min_w and n < 8: n += 1; lw //= 2
    return n


def swizzle_copy_bytes(w, h, levels, fmt=GU_PSM_T8):
    """вторая копия, которую сегодня делает swizzleAll() в main.c при загрузке (высота до кратной 8)"""
    n, lw, lh = 0, w, h
    for _ in range(levels):
        n += (lw // 2 if fmt == GU_PSM_T4 else lw) * ((lh + 7) & ~7); lw, lh = lw // 2, max(1, lh // 2)
    return n


def screen_px_per_m(dist, fovy=62.0, scr_h=272):
    return scr_h / 2 / np.tan(np.radians(fovy / 2)) / dist


def analyze(level_json, outdir, want_px_per_m=None, cap_hi=256):
    L = json.loads(Path(level_json).read_text())
    use, src = L.get('texuse', {}), L.get('texsrc', {})
    urls = sorted(set(use) | set(src))
    outdir = Path(outdir); (outdir / 'crops').mkdir(parents=True, exist_ok=True)
    want = want_px_per_m or screen_px_per_m(10.0)            # ~22.6 px экрана на метр в 10 м от камеры
    rows = []; mem = dict(cur=0, cur_swz=0, t4=0, cap_all=0, cap_sel=0, cap_sel_t4=0)
    for i, u in enumerate(urls):
        p = TEXDIR / u
        if not p.exists(): continue
        img = Image.open(p).convert('RGBA'); W0, H0 = img.size
        # сегодняшний путь (FASTOCTREE) и k-means/точная палитра на тех же размерах
        d_oct, ref, _, _ = build(img, CAP, 'octree'); d_km, _, _, _ = build(img, CAP, 'kmeans')
        e_oct = errors(ref, decode(d_oct)); e_km = errors(ref, decode(d_km))
        w, h = ref[0].size; nlev = len(ref)
        ncol0 = len(np.unique(np.asarray(ref[0], np.uint8).reshape(-1, 4).view('<u4')))
        ncol_src = len(np.unique(np.asarray(img, np.uint8).reshape(-1, 4).view('<u4')))
        t4ok = w >= 32 and ncol0 <= 16
        e_dith = None
        if (W0, H0) != (w, h) and ncol0 > 256:
            dd, _ = ordered_dither_build(img, CAP); e_dith = errors(ref, decode(dd))
        # плотность: сколько текселей на метр даёт текстура сейчас и при исходном размере
        uu = use.get(u, {}); dens = uu.get('uv_per_m_p50', 0.0)
        tpm_cur = dens * np.sqrt(w * h); tpm_src = dens * np.sqrt(W0 * H0)
        wh_hi = target_size(W0, H0, cap_hi); tpm_hi = dens * np.sqrt(wh_hi[0] * wh_hi[1])
        big = (W0, H0) != (w, h) and tpm_cur < want and uu.get('area', 0) > 0
        b_cur = tex_bytes(w, h, nlev); b_t4 = tex_bytes(w, h, n_levels(w, 32), GU_PSM_T4) if t4ok else b_cur
        b_hi = tex_bytes(*wh_hi, n_levels(wh_hi[0]))
        mem['cur'] += b_cur; mem['cur_swz'] += swizzle_copy_bytes(w, h, nlev); mem['t4'] += b_t4
        mem['cap_all'] += b_hi; mem['cap_sel'] += b_hi if big else b_cur; mem['cap_sel_t4'] += b_hi if big else b_t4
        rows.append(dict(url=u, src=f'{W0}x{H0}', size=f'{w}x{h}', colors_src=ncol_src, colors=ncol0, t4=t4ok,
                         oct=e_oct, km=e_km, dith=e_dith, area=uu.get('area', 0.0), car=uu.get('car', False),
                         tpm_cur=float(tpm_cur), tpm_src=float(tpm_src), tpm_hi=float(tpm_hi), big=bool(big), bytes=b_cur, bytes_hi=b_hi))
        if i % 50 == 0: print(f'  {i}/{len(urls)}', flush=True)
    (outdir / 'texq_rows.json').write_text(json.dumps(rows, indent=1))
    (outdir / 'texq_mem.json').write_text(json.dumps(mem, indent=1))
    return rows, mem


def report(rows, mem):
    R = rows; n = len(R)
    le = [r for r in R if r['colors'] <= 256]; gt = [r for r in R if r['colors'] > 256]
    def s(rs, key, f):
        if not rs: return '—'
        a = np.array([r[key][f] for r in rs]); return f'{a.mean():.2f} / {np.percentile(a, 95):.1f} / {a.max():.0f}'
    print(f'textures: {n}; <=256 colours at export size: {len(le)}; >256 (downscaled or 32-bit): {len(gt)}; CLUT4-able: {sum(r["t4"] for r in R)}')
    print('| set | path | RGB mean err (avg / p95 / max over textures) | RGB max err (avg / p95 / max) | alpha max err (max) | mips mean err (avg) |')
    for name, rs in (('<=256 colours', le), ('>256 colours', gt), ('all', R)):
        for k, lab in (('oct', 'FASTOCTREE (today)'), ('km', 'exact / k-means')):
            if not rs: continue
            print(f"| {name} | {lab} | {s(rs, k, 'mean')} | {s(rs, k, 'max')} | {max(r[k]['amax'] for r in rs)} | {np.mean([r[k]['mips'] for r in rs]):.2f} |")
    d = [r for r in R if r['dith']]
    if d:
        print(f"dither ({len(d)} textures): mean {np.mean([r['dith']['mean'] for r in d]):.2f} vs km {np.mean([r['km']['mean'] for r in d]):.2f}; "
              f"blurred {np.mean([r['dith']['blur'] for r in d]):.2f} vs km {np.mean([r['km']['blur'] for r in d]):.2f}")
    MB = 2**20
    print(f"memory: today {mem['cur'] / MB:.2f} MB (+ swizzle copy {mem['cur_swz'] / MB:.2f}); CLUT4 {mem['t4'] / MB:.2f}; "
          f"cap256 all {mem['cap_all'] / MB:.2f}; cap256 selective {mem['cap_sel'] / MB:.2f} ({sum(r['big'] for r in R)} textures); "
          f"selective + CLUT4 {mem['cap_sel_t4'] / MB:.2f}")


def level_memory(level_json):
    """Память текстур уровня по вариантам, МБ — по исходным размерам/числу цветов из прогона level_sweep (texsrc).
    today: как сейчас (T8, предел 128) + вторая swizzle-копия в main.c; t4: CLUT4 для <= 16 цветов (ширина >= 32);
    cap256: предел 256; pre: уровни уже в swizzle (копии нет). Градиенты неба, ушедшие в цвет вершин, не вычитаются."""
    src = json.loads(Path(level_json).read_text()).get('texsrc', {}); m = dict(n=len(src), today=0, swz=0, t4=0, cap256=0, cap256_t4=0, over128=0)
    for u, t in src.items():
        if not t['w']: continue
        for cap, key in ((128, None), (256, 'cap256')):
            w, h = target_size(t['w'], t['h'], cap); scaled = (w, h) != (t['w'], t['h'])
            t4 = (not scaled) and t['colors'] <= 16 and w >= 32
            b8 = tex_bytes(w, h, n_levels(w)); b4 = tex_bytes(w, h, n_levels(w, 32), GU_PSM_T4) if t4 else b8
            if key is None:
                m['today'] += b8; m['swz'] += swizzle_copy_bytes(w, h, n_levels(w)); m['t4'] += b4; m['over128'] += scaled
            else:
                m['cap256'] += b8; m['cap256_t4'] += b4
    return {k: (v / 2**20 if k not in ('n', 'over128') else v) for k, v in m.items()}


def crop_sheet(url, out, cap_hi=256, zoom_to=256, box=None):
    """Картинка сравнения: исходник | сегодня (FASTOCTREE) | k-means/точная | крупнее (cap_hi) | ошибка x8 (сегодня)."""
    img = Image.open(TEXDIR / url).convert('RGBA')
    d_oct, ref, _, _ = build(img, CAP, 'octree'); d_km, _, _, _ = build(img, CAP, 'kmeans'); d_hi, ref_hi, _, _ = build(img, cap_hi, 'kmeans')
    tiles = [('source %dx%d' % img.size, np.asarray(img)), ('today FASTOCTREE', decode(d_oct)[0]), ('exact/k-means', decode(d_km)[0]),
             ('cap %d' % cap_hi, decode(d_hi)[0])]
    err = np.abs(np.asarray(ref[0], np.int16) - decode(d_oct)[0].astype(np.int16))[..., :3].max(-1)
    tiles.append(('|err| x8 today', np.dstack([np.clip(err * 8, 0, 255).astype(np.uint8)] * 3 + [np.full(err.shape, 255, np.uint8)])))
    panels = []
    for lab, a in tiles:
        im = Image.fromarray(np.ascontiguousarray(a), 'RGBA')
        if box:   # box — доля исходника (x0, y0, x1, y1), чтобы смотреть мелочь
            W, H = im.size; im = im.crop((int(box[0] * W), int(box[1] * H), int(box[2] * W), int(box[3] * H)))
        s = zoom_to / max(im.size); im = im.resize((max(1, int(im.size[0] * s)), max(1, int(im.size[1] * s))), Image.NEAREST)
        bg = Image.new('RGBA', im.size, (128, 128, 128, 255)); bg.alpha_composite(im); panels.append((lab, bg.convert('RGB')))
    from PIL import ImageDraw
    W = sum(p.size[0] for _, p in panels) + 8 * (len(panels) - 1); H = max(p.size[1] for _, p in panels) + 16
    sheet = Image.new('RGB', (W, H), (20, 20, 20)); dr = ImageDraw.Draw(sheet); x = 0
    for lab, p in panels:
        sheet.paste(p, (x, 16)); dr.text((x + 2, 2), lab, fill=(230, 230, 230)); x += p.size[0] + 8
    sheet.save(out)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--level', type=int, action='append', help='level(s) to analyze (needs levelN.json from level_sweep)')
    ap.add_argument('--sweep', default=str(Path('/private/tmp/claude-501/-Users-USER-Downloads-shar-psp/'
                                                 '3f2ce002-62fd-4bf8-9553-98f95067c719/scratchpad/streams/levels')))
    ap.add_argument('--report-only', action='store_true')
    ap.add_argument('--memory', action='store_true', help='per-level texture memory table (all levels found in --sweep)')
    ap.add_argument('--crop', nargs='*', help='urls for comparison sheets')
    a = ap.parse_args()
    out = Path(a.sweep) / 'texq'
    if a.crop:
        (out / 'crops').mkdir(parents=True, exist_ok=True)
        for u in a.crop: crop_sheet(u, out / 'crops' / (Path(u).stem + '.png')); print('sheet', out / 'crops' / (Path(u).stem + '.png'))
        sys.exit()
    if a.memory:
        print('| level | textures | >128 px | today T8 | + swizzle copy (main.c) | CLUT4 | cap 256 | cap 256 + CLUT4 |')
        print('|---|---|---|---|---|---|---|---|')
        for lv in range(1, 8):
            f = Path(a.sweep) / f'level{lv}.json'
            if not f.exists(): continue
            m = level_memory(f)
            print(f"| L{lv} | {m['n']} | {m['over128']} | {m['today']:.2f} | {m['swz']:.2f} | {m['t4']:.2f} | {m['cap256']:.2f} | {m['cap256_t4']:.2f} |")
    for lv in a.level or []:
        o = out / f'L{lv}'; print(f'== level {lv}')
        if a.report_only:
            rows = json.loads((o / 'texq_rows.json').read_text()); mem = json.loads((o / 'texq_mem.json').read_text())
        else:
            rows, mem = analyze(Path(a.sweep) / f'level{lv}.json', o)
        report(rows, mem)
