#!/usr/bin/env python3
"""Стенд картинки «дальние клетки» (просьба дирижёра 09.11, ведут Модели-96): кадр «полные» против «дальние» по набору видов и подъезд к границе кадр за кадром.
Одна команда на уровень, итог PASS/FAIL + лист:

  tools/farcells_gate.py LEVEL --full SET [--far SET2] [--far-flags "farcell"] [--far-extra "l4/farcells.shr"] [--far-filedata "k=v"]
                         [--views FILE] [--approach "x0 z0 x1 z1 h steps"] [--out DIR] [--pct 0.3] [--mean 0.6] [--jump 0.15]

  SET, SET2 — папки, как для tools/smoke.sh (EBOOT.PBP и данные). «Дальние» — любой из способов: другой набор (--far SET2), файлы-флаги рядом с EBOOT (--far-flags),
  дополнительные файлы данных (--far-extra), файлы с содержимым (--far-filedata). Строки сценария на «дальнем» прогоне: --far-scn "flag farcell 1" (повторяемо).
  Прогоны: полные №1, полные №2 (те же настройки, шум), дальние. Маска шума = пиксели, различающиеся между двумя одинаковыми прогонами (трафик, пешеходы, частицы),
  расширенные на 3 px; сравнение полные №1 против дальних вне маски.
  Виды (--views, строки «имя x z h [camheight camdist]»; # — комментарий): по умолчанию точка старта уровня из заголовка level.shr и два сдвига. Присылает Данные-93 (типичные точки).
  Подъезд (--approach, по умолчанию 40 шагов по 2 м вдоль курса старта): машина идёт по шагам goto, кадр на каждом шаге. Условия: на каждом шаге доля плохих пикселей ≤ --pct,
  и скачок доли между соседними шагами ≤ --jump (процентных пунктов) — скачка подмены ближняя/дальняя нет.
  Пороги: плохой пиксель — макс. по каналам |Δ| > 24/255 вне маски; вид проходит при доле плохих ≤ --pct % и средней ошибке вне маски ≤ --mean (из 255).
  До переключения в игре: --make-far farcells.shr [--eye X Z] [--r0 120] (набор «дальние» собирается farcells_apply.py Моделей-96: level.shr с дальними версиями клеток дальше r0 от глаза;
  виды держать около глаза — иначе ближние клетки уже упрощены). После переключения: --far SET_с_farcells.shr --far-scn "flag farcell 1".
  Красный контроль: --self-red подменяет «дальние» кадрами со сдвигом на 6 px (должен упасть); без данных от Моделей зелёный контроль = «дальние» = полные (должен пройти)."""
import warnings, argparse, os, re, struct, subprocess, sys, math, shutil
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

warnings.filterwarnings('ignore', category=RuntimeWarning)
ROOT = Path(__file__).resolve().parents[1]


def spawn_of(setdir, level):
    p = Path(setdir) / ('level.shr' if level == 1 else 'l%d/level.shr' % level)
    b = open(p, 'rb').read(104)
    if b[:4] != b'SHR7': raise SystemExit('нет SHR7: %s' % p)
    x, y, z, h = struct.unpack_from('<4f', b, 36)
    return x, z, h


def read_points(fn, level, only=None):
    """точки обзора Данных-93 (docs/state/viewpoints_93.json): (id, x, z, cam-строка); goto x z 0, wait 5, затем cam"""
    import json
    d = json.load(open(fn))['levels'][str(level)]['points']
    return [(p['id'], p['eye'][0], p['eye'][2], p['cam']) for p in d if not only or p['id'] in only]


def read_views(fn):
    v = []
    for ln in open(fn, encoding='utf-8'):
        ln = ln.split('#')[0].strip()
        if not ln: continue
        f = ln.split()
        v.append((f[0], float(f[1]), float(f[2]), float(f[3]), f[4:]))
    return v


FOOT_ALL = os.environ.get('FARC_FOOT_ALL') == '1'   # 1 — точки через foot + footfsm 1 на всех уровнях (по умолчанию только L4/L6/L7)
LOADWAIT = {1: 6, 2: 50, 3: 50, 4: 70, 5: 60, 6: 60, 7: 60}   # с: ожидание загрузки после golevel (у L4 тяжелее); пустой кадр (нет мира) → повтор с +40 с


def scenario(level, views, appr, extra_scn, lw=None):
    S = ['# farcells_gate: уровень %d' % level, 'scene F_L1', 'flag cfgdefaults 1', 'flag repair', 'wait 6']
    if level != 1:
        S += ['scene F_FARC_GO', 'press circle 0.3', 'wait 0.5', 'press circle 0.3', 'wait 0.5', 'flag golevel %d' % level, 'wait %d' % (lw if lw else LOADWAIT.get(level, 60))]
    S += ['scene F_FARC_V', 'flag camdet 1'] + (['flag player'] if (level in (4, 6, 7) or FOOT_ALL) else []) + ['flag stop'] + extra_scn   # L4/L6 по Моделям-96: без `flag player` после загрузки мир не рисуется (кадры «чёрный фон и HUD»)
    for name, x, z, h, opt in views:
        if isinstance(opt, str):   # точка Данных: goto, пауза на загрузку клеток, свободная камера
            if level in (4, 6, 7) or FOOT_ALL: S += ['flag footfsm 1', 'foot %.2f %.2f 0 0' % (x, z), 'wait 12', opt, 'wait 3', 'shot v_%s' % name]; continue   # L4/L6 по Лестнице-96 и Моделям-96: goto двигает только машину, регион точки не открывается; foot + footfsm 1 ставит Гомера и грузит регион
            S += ['goto %.2f %.2f 0' % (x, z), 'wait 5', opt, 'wait 1.5', 'shot v_%s' % name]; continue
        S += ['goto %.2f %.2f %.3f' % (x, z, h)]
        if len(opt) >= 1: S += ['flag camheight %s' % opt[0]]
        if len(opt) >= 2: S += ['flag camdist %s' % opt[1]]
        S += ['wait 3', 'shot v_%s' % name]
    if appr:
        x0, z0, x1, z1, h, n = appr; n = int(n)
        S += ['scene F_FARC_A']
        for k in range(n + 1):
            t = k / n; S += ['goto %.2f %.2f %.3f' % (x0 + (x1 - x0) * t, z0 + (z1 - z0) * t, h), 'wait 0.8', 'shot a_%02d' % k]
    S += ['end']
    return '\n'.join(S) + '\n'


def run_smoke(setdir, scn, tag, flags, extra, filedata, work):
    env = dict(os.environ, DEVELOPER_DIR='/Library/Developer/CommandLineTools', SHIP_LEVELS='2 3 4 5 6 7', SMOKE_HEADLESS='1', SMOKE_BALLAST='5', SMOKE_MAXT=os.environ.get('SMOKE_MAXT', '900'),
               SMOKE_SCN=str(scn), SMOKE_FILEDATA='meaudio=3 ' + filedata, SMOKE_FLAGS=flags, SMOKE_EXTRA=extra)
    log = Path(work) / ('run_%s.log' % tag)
    with open(log, 'w') as f:
        subprocess.run(['nice', '-n', '20', str(ROOT / 'tools/smoke.sh'), str(setdir)], env=env, stdout=f, stderr=subprocess.STDOUT, cwd=ROOT)
    m = re.findall(r'^отчёт: (.*)/summary\.txt', open(log, encoding='utf-8', errors='replace').read(), re.M)
    if not m: raise SystemExit('нет отчёта прогона %s (см. %s)' % (tag, log))
    d = Path(m[-1]); d = d if d.is_absolute() else ROOT / d
    keep = Path(work) / tag; shutil.rmtree(keep, ignore_errors=True); keep.mkdir(parents=True)
    for p in d.glob('*.png'):
        if p.name != 'contact.png': shutil.copy(p, keep / p.name)
    shutil.copy(d / 'smoke.txt', keep / 'smoke.txt') if (d / 'smoke.txt').exists() else None
    shutil.rmtree(d, ignore_errors=True)
    return keep


def empty_frame(p):
    """кадр без мира (экран загрузки: тёмная виньетка и HUD): среднее модуля градиента по блокам 8x8 вне углов HUD; у кадров с миром ≥ 2,7, у пустых ≤ 0,4 (порог 1,2; калибровка по 40 кадрам)"""
    a = np.asarray(Image.open(p).convert('L'), dtype=np.float32)
    m = np.ones_like(a, bool); m[:90, :90] = False; m[165:, 375:] = False; m[256:, :] = False
    a = np.where(m, a, np.nan)[:256, :480]
    b = np.nanmean(a.reshape(32, 8, 60, 8).transpose(0, 2, 1, 3).reshape(32, 60, 64), axis=2)
    g = np.nanmean(np.concatenate([np.abs(np.diff(b, axis=1)).ravel(), np.abs(np.diff(b, axis=0)).ravel()]))
    return bool(g < 1.2)


def run_checked(a, L, setdir, one_views, extra_scn, tag, flags, extra, filedata, out):
    """прогон точки с проверкой «мир есть»; пустой кадр — повтор с большей паузой загрузки (до 2 раз), иначе FAIL «нет мира» остаётся в результате"""
    for att in range(3):
        lw = LOADWAIT.get(L, 60) + 40 * att
        sc = out / ('scn_%s_%d.txt' % (tag, att)); sc.write_text(scenario(L, one_views, None, extra_scn, lw))
        k = run_smoke(setdir, sc, tag, flags, extra, filedata, out)
        shots = list(k.glob('v_*.png'))
        if shots and not any(empty_frame(f) for f in shots): return k
        print('  кадр без мира (%s), повтор с паузой %d с' % (tag, lw + 40), flush=True)
    return k


def build_far_ingame(a, L, out, name):
    """набор «дальние» для переключения в игре: те же файлы, что FULL, плюс farcells.shr рядом с level.shr (для L1 — в корне); level.shr и боковые файлы не трогаются"""
    fs = out / name; shutil.rmtree(fs, ignore_errors=True); fs.mkdir(parents=True); full = Path(a.full)
    for e in full.iterdir():
        if e.name == 'l%d' % L and e.is_dir(): continue
        os.symlink(e.resolve(), fs / e.name)
    if L == 1: os.symlink(Path(a.ingame).resolve(), fs / 'farcells.shr')
    else:
        d = fs / ('l%d' % L); d.mkdir()
        for e in (full / ('l%d' % L)).iterdir(): os.symlink(e.resolve(), d / e.name)
        os.symlink(Path(a.ingame).resolve(), d / 'farcells.shr')
    return fs


def run_all_checked(a, L, setdir, views, extra_scn, tag, extra, out):
    """все виды одним прогоном; кадр без мира — повтор с большей паузой загрузки (до 2 раз)"""
    for att in range(3):
        lw = LOADWAIT.get(L, 60) + 40 * att
        sc = out / ('scn_%s_%d.txt' % (tag, att)); sc.write_text(scenario(L, views, None, extra_scn, lw))
        k = run_smoke(setdir, sc, tag, '', extra, '', out); shots = list(k.glob('v_*.png'))
        if shots and not any(empty_frame(f) for f in shots): return k
        print('  кадры без мира (%s), повтор с паузой %d с' % (tag, lw + 40), flush=True)
    return k


def load(p): return np.asarray(Image.open(p).convert('RGB'), dtype=np.int16)


def dilate(m, r):
    out = m.copy()
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            out |= np.roll(np.roll(m, dy, 0), dx, 1)
    return out


def moved_match(a1, b, pts, thr=24, R=20):
    """доля пикселей пятна, чей цвет из «дальних» (b) есть в «полных» (a1) рядом (сдвиг ≤ R px): машина трафика, успевшая сместиться между кадрами, находится в обоих кадрах; убранный кусок геометрии — нет (на его месте цвет, которого рядом не было)"""
    H, W = b.shape[:2]; py = [q[0] for q in pts]; px = [q[1] for q in pts]
    y0, y1, x0, x1 = max(min(py) - R, 0), min(max(py) + R + 1, H), max(min(px) - R, 0), min(max(px) + R + 1, W)
    A = a1[y0:y1, x0:x1].astype(np.int16); yy = np.array(py) - y0; xx = np.array(px) - x0; col = b[py, px].astype(np.int16)
    found = np.zeros(len(pts), bool)
    for dy in range(-R, R + 1, 2):
        for dx in range(-R, R + 1, 2):
            ys, xs = np.clip(yy + dy, 0, A.shape[0] - 1), np.clip(xx + dx, 0, A.shape[1] - 1)
            found |= np.abs(A[ys, xs] - col).max(1) <= thr
    return float(found.mean())


def blobs(bad, maxarea, maxw, maxh, fill=0.35, verify=None):
    """компактные пятна плохих пикселей (машина трафика, пешеход): связные компоненты малой площади и габарита — их не считаем, на листе они жёлтые"""
    H, W = bad.shape; seen = np.zeros_like(bad); keep = np.zeros_like(bad); drop = np.zeros_like(bad)
    ys, xs = np.nonzero(bad)
    for y0, x0 in zip(ys, xs):
        if seen[y0, x0]: continue
        st = [(y0, x0)]; seen[y0, x0] = True; pts = []
        while st:
            y, x = st.pop(); pts.append((y, x))
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < H and 0 <= xx < W and bad[yy, xx] and not seen[yy, xx]: seen[yy, xx] = True; st.append((yy, xx))
        py = [q[0] for q in pts]; px = [q[1] for q in pts]; w = max(px) - min(px) + 1; h = max(py) - min(py) + 1
        target = drop if (len(pts) <= maxarea and w <= maxw and h <= maxh and len(pts) >= fill * w * h and (verify is None or verify(pts))) else keep
        for q in pts: target[q] = True
    return keep, drop


def compare(a1, a2, b, thr=24, noise=8, blob=400):
    mask = dilate(np.abs(a1 - a2).max(2) > noise, 3)
    mask[256:, :] = True   # нижняя строка кадра смоука (время, номер кадра) всегда разная
    mask[165:256, 372:] = True   # миникарта HUD справа внизу
    mask[:80, :80] = True   # значок персонажа слева вверху
    d = np.abs(a1 - b).max(2); ok = ~mask
    bad = (d > thr) & ok; drop = np.zeros_like(bad)
    if blob > 0 and bad.any():
        bad, drop = blobs(bad, blob, 48, 32)   # мелкие компактные пятна: как раньше
        if bad.any():   # крупные пятна (машина трафика целиком, до 1500 px): только если их цвет есть в «полных» рядом (машина сместилась между кадрами); пустое место убранного куска так не пройдёт
            bad, drop2 = blobs(bad, 1500, 120, 48, 0.2, lambda pts: moved_match(a1, b, pts) >= 0.85); drop = drop | drop2
    n = int(ok.sum()) or 1
    return 100.0 * bad.sum() / n, float(np.abs(a1 - b).mean(2)[ok & ~drop].mean()) if (ok & ~drop).any() else 0.0, mask | (drop & False), bad, drop


def build_far(a, L, out, eye, name):
    fs = out / name; shutil.rmtree(fs, ignore_errors=True); fs.mkdir(parents=True)
    full = Path(a.full)
    for e in full.iterdir():
        if (L == 1 and e.name == 'level.shr') or (e.name == 'l%d' % L and e.is_dir()): continue   # корневой level.shr — файл L1; smoke требует его наличия и при игре l<N>/ (остаётся ссылкой)
        os.symlink(e.resolve(), fs / e.name)
    if L == 1: lv = full / 'level.shr'; dst = fs / 'level.shr'
    else:
        (fs / ('l%d' % L)).mkdir()
        for e in (full / ('l%d' % L)).iterdir():
            if e.name != 'level.shr': os.symlink(e.resolve(), fs / ('l%d' % L) / e.name)
        lv = full / ('l%d' % L) / 'level.shr'; dst = fs / ('l%d' % L) / 'level.shr'
    print(subprocess.run([sys.executable, '-I', a.apply, str(lv), a.make_far, str(dst), '--eye', str(eye[0]), str(eye[1]), '--r0', str(a.r0)], capture_output=True, text=True, check=True).stdout.strip())
    return fs


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('level', type=int); ap.add_argument('--full', required=True); ap.add_argument('--far')
    ap.add_argument('--far-flags', default=''); ap.add_argument('--far-extra', default=''); ap.add_argument('--far-filedata', default=''); ap.add_argument('--far-scn', action='append', default=[])
    ap.add_argument('--views'); ap.add_argument('--points', help='viewpoints_93.json Данных-93: все точки уровня (--only L4_p01,... — часть); с --make-far набор «дальние» собирается по глазу КАЖДОЙ точки'); ap.add_argument('--only', default=''); ap.add_argument('--approach'); ap.add_argument('--out', default=None)
    ap.add_argument('--blob', type=int, default=400, help='площадь (px) компактных пятен, которые не считаются плохими (трафик); 0 — считать всё'); ap.add_argument('--pct', type=float, default=0.3); ap.add_argument('--mean', type=float, default=0.6); ap.add_argument('--jump', type=float, default=0.15)
    ap.add_argument('--make-far', help='farcells.shr уровня: собрать набор «дальние» (level.shr с дальними версиями клеток дальше --r0 от --eye, до переключения в игре) через tools/farcells_apply.py Моделей-96')
    ap.add_argument('--ingame', help='farcells.shr уровня: «дальние» = ТОТ ЖЕ набор + l<N>/farcells.shr + flag farcell 1 (переключение в игре, EBOOT Лестницы-96 с 21f77ef6); level.shr не трогается. Все точки одним прогоном на «полные» и одним на «дальние»')
    ap.add_argument('--eye', nargs=2, type=float); ap.add_argument('--r0', type=float, default=120.)
    ap.add_argument('--apply', default=str(Path.home() / 'Downloads/shar-psp-mod96/tools/farcells_apply.py'))
    ap.add_argument('--skip', action='append', default=[], help='ИМЯ=причина: точка не считается (анимированный объект в другой фазе между прогонами), попадает в итог строкой SKIP'); ap.add_argument('--self-red', action='store_true'); ap.add_argument('--self-hole', action='store_true', help='красный контроль: в «дальних» закрашен кусок 40×20 (убранная геометрия)'); ap.add_argument('--reuse', action='store_true', help='не гонять эмулятор, взять снимки из --out')
    a = ap.parse_args()
    L = a.level; out = Path(a.out or ROOT / ('build/farcells/L%d' % L)); out.mkdir(parents=True, exist_ok=True)
    sx, sz, sh = spawn_of(a.full, L)
    pts = read_points(a.points, L, a.only.split(',') if a.only else None) if a.points else None
    views = [(i, x, z, 0.0, c) for i, x, z, c in pts] if pts else read_views(a.views) if a.views else [('start', sx, sz, sh, []), ('fwd40', sx + 40 * math.sin(sh), sz + 40 * math.cos(sh), sh, []), ('back40', sx - 40 * math.sin(sh), sz - 40 * math.cos(sh), sh, [])]
    appr = None if a.approach == 'none' or (pts and not a.approach) else [float(x) for x in a.approach.split()] if a.approach else [sx, sz, sx + 80 * math.sin(sh), sz + 80 * math.cos(sh), sh, 40]   # 'none' — без подъезда (набор с фиксированным глазом)
    if a.ingame:   # переключение в игре: два прогона (полные / дальние) по всем видам
        fs = build_far_ingame(a, L, out, 'far_set_ingame'); xf = 'farcells.shr' if L == 1 else 'l%d/farcells.shr' % L
        if not a.reuse:
            print('прогон 1/2: полные (farcell 0)', flush=True); k1 = run_all_checked(a, L, a.full, views, ['flag farcell 0'], 'full1', '', out)
            print('прогон 2/2: дальние (farcell 1, farcells.shr)', flush=True); k2 = run_all_checked(a, L, fs, views, ['flag farcell 1'], 'far', xf, out)
        a.reuse = True   # кадры уже в out/full1 и out/far
    elif a.make_far and not pts: a.far = str(build_far(a, L, out, a.eye or [sx, sz], 'far_set'))
    if not a.reuse:
        sc0 = out / 'scn_full.txt'; sc0.write_text(scenario(L, views, appr, []))
        sc1 = out / 'scn_far.txt'; sc1.write_text(scenario(L, views, appr, a.far_scn))
        if not (a.make_far and pts):
            print('прогон 1/3: полные', flush=True); run_smoke(a.full, sc0, 'full1', '', '', '', out)
            print('прогон 2/3: полные (шум)', flush=True); run_smoke(a.full, sc0, 'full2', '', '', '', out)
        if a.make_far and pts:   # свой набор «дальние» на каждую точку (глаз точки), кадры складываются в far/
            far = out / 'far'; shutil.rmtree(far, ignore_errors=True); far.mkdir()
            f1 = out / 'full1'; shutil.rmtree(f1, ignore_errors=True); f1.mkdir()   # «полные» тоже по точке, тем же сценарием: одинаковый ход времени (трафик, пешеходы) у обоих кадров
            for (i, x, z, c) in pts:
                fsd = build_far(a, L, out, (x, z), 'far_set_%s' % i); one = out / ('scn_far_%s.txt' % i); one.write_text(scenario(L, [(i, x, z, 0.0, c)], None, a.far_scn))
                print('полные: точка %s' % i, flush=True); kf = run_checked(a, L, a.full, [(i, x, z, 0.0, c)], [], 'full_%s' % i, '', '', '', out)
                for f in kf.glob('v_*.png'): shutil.copy(f, f1 / f.name)
                print('дальние: точка %s' % i, flush=True); k = run_checked(a, L, fsd, [(i, x, z, 0.0, c)], a.far_scn, 'far_%s' % i, a.far_flags, a.far_extra, a.far_filedata, out)
                for f in k.glob('v_*.png'): shutil.copy(f, far / f.name)
                shutil.rmtree(fsd, ignore_errors=True)
        else:
            print('прогон 3/3: дальние', flush=True); run_smoke(a.far or a.full, sc1, 'far', a.far_flags, a.far_extra, a.far_filedata, out)
    names = [('v_%s' % v[0]) for v in views] + (['a_%02d' % k for k in range(int(appr[5]) + 1)] if appr else [])
    res = {}; rows = []; lines_empty = []
    for nm in names:
        p1, p2, pf = out / 'full1' / (nm + '.png'), out / 'full2' / (nm + '.png'), out / 'far' / (nm + '.png')
        if not p2.exists(): p2 = p1   # по точке: второго «полного» нет (ход времени один)
        if not (p1.exists() and p2.exists() and pf.exists()): res[nm] = None; continue
        if empty_frame(p1) or empty_frame(pf): res[nm] = None; lines_empty.append(nm); continue   # нет мира — не сравнивать (ложный PASS)
        A1, A2, B = load(p1), load(p2), load(pf)
        if a.self_red: B = np.roll(B, 6, 1)
        if a.self_hole: B = B.copy(); B[120:140, 200:240] = [255, 0, 255]   # «кусок убран»: плашка 40×20 чужого цвета — гейт обязан краснеть
        pct, mean, mask, bad, drop = compare(A1, A2, B, blob=a.blob); res[nm] = (pct, mean); rows.append((nm, A1, B, mask, bad, drop))
    skips = dict(k.split('=', 1) for k in a.skip if '=' in k); fails = []; lines = ['фарселлс L%d: пороги: плохих ≤ %.2f %%, средняя ≤ %.2f/255, скачок ≤ %.2f п.п.' % (L, a.pct, a.mean, a.jump)]
    for nm in names:
        r = res[nm]
        if r is None: lines.append('%-12s %s' % (nm, 'FAIL: НЕТ МИРА (пустой кадр)' if nm in lines_empty else 'НЕТ КАДРА')); fails.append(nm); continue
        ok = r[0] <= a.pct and r[1] <= a.mean
        sk = skips.get(nm[2:] if nm.startswith('v_') else nm)
        if not ok and sk: lines.append('%-12s плохих %.3f %%  средняя %.3f  SKIP (%s)' % (nm, r[0], r[1], sk)); continue
        lines.append('%-12s плохих %.3f %%  средняя %.3f  %s' % (nm, r[0], r[1], 'OK' if ok else 'FAIL'))
        if not ok: fails.append(nm)
    ap_names = [n for n in names if n.startswith('a_')]
    for n0, n1 in zip(ap_names, ap_names[1:]):
        if res[n0] and res[n1]:
            j = abs(res[n1][0] - res[n0][0]); ok = j <= a.jump
            if not ok: lines.append('скачок %s→%s: %.3f п.п. FAIL' % (n0, n1, j)); fails.append('%s→%s' % (n0, n1))
    verdict = 'PASS' if not fails else 'FAIL (%d)' % len(fails)
    lines.append('ИТОГ L%d: %s' % (L, verdict))
    sfx = '_control' if (a.self_red or a.self_hole) else ''   # красные контроли пишут рядом, не затирая итог
    (out / ('summary%s.txt' % sfx)).write_text('\n'.join(lines) + '\n'); print('\n'.join(lines))
    # лист: виды (полные | дальние | плохие красным), подъезд — каждый 4-й кадр и график доли
    vrows = [r for r in rows if r[0].startswith('v_')]
    arows = [r for r in rows if r[0].startswith('a_')][::4]
    sel = vrows + arows
    if sel:
        W, H = 480, 272; sheet = Image.new('RGB', (W * 3, H * len(sel)))
        for i, (nm, A1, B, mask, bad, drop) in enumerate(sel):
            dimg = (A1 * 0.4).astype(np.uint8).copy(); dimg[mask] = [0, 60, 120]; dimg[drop] = [255, 200, 0]; dimg[bad] = [255, 0, 0]
            for j, arr in enumerate((A1.astype(np.uint8), B.astype(np.uint8), dimg)): sheet.paste(Image.fromarray(arr), (j * W, i * H))
            ImageDraw.Draw(sheet).text((4 + 0, i * H + 4), nm, fill=(255, 0, 255))
        sheet.save(out / ('sheet%s.png' % sfx))
    sys.exit(0 if not fails else 1)


if __name__ == '__main__':
    main()
