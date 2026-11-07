"""№1185: лист-карта секторов трасс бонус-игры. Вход: папка с run<N>/out/sector_b<N>.bmp (кадр вида сверху), run<N>/run/smoke.txt (строка BONUS ev=topcamera), boundaries.txt (sector_host -d).
Рисует поверх кадра (в 3 раза крупнее) границы секторов (чёрточка поперёк пути), номера каждого пятого сектора, начало (01) и сводную таблицу; пишет sector_map_bN.png, лист all.png."""
import math, re, sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

out = Path(sys.argv[1]); K = 3; W, H = 480, 272; ASPECT = 16 / 9
bnd = {}
for l in (out / 'boundaries.txt').read_text().splitlines():
    p = l.split()
    if p and p[0] == 'B': bnd.setdefault(int(p[1]), []).append((int(p[2]), float(p[3]), float(p[4]), float(p[5]), float(p[6]), float(p[7])))
def font(sz):
    for f in ('/System/Library/Fonts/Menlo.ttc', '/System/Library/Fonts/Supplemental/Arial Bold.ttf', '/Library/Fonts/Arial.ttf'):
        try: return ImageFont.truetype(f, sz)
        except Exception: pass
    return ImageFont.load_default()
def cam(t):
    f = out / f'run{t}/run/smoke.txt'
    if not f.exists(): return None
    sm = f.read_bytes().decode(errors='replace')
    m = re.search(r'BONUS ev=topcamera track=%d eye=([-\d.,]+) at=([-\d.,]+) up=([-\d.,]+) fovy=([-\d.]+)' % t, sm)
    if not m: return None
    v = lambda s: [float(x) for x in s.split(',')]
    return v(m.group(1)), v(m.group(2)), v(m.group(3)), float(m.group(4))
def norm(a): l = math.sqrt(sum(x * x for x in a)) or 1; return [x / l for x in a]
def cross(a, b): return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
def dot(a, b): return sum(x * y for x, y in zip(a, b))
def project(c, P):
    eye, at, up, fovy = c; f = norm([at[i] - eye[i] for i in range(3)]); r = norm(cross(f, up)); u = cross(r, f); d = [P[i] - eye[i] for i in range(3)]
    zc = dot(d, f); th = math.tan(math.radians(fovy) / 2)
    return ((dot(d, r) / (zc * th * ASPECT) * .5 + .5) * W * K, (.5 - dot(d, u) / (zc * th) * .5) * H * K)
def samples(t):
    f = out / f'run{t}/run/smoke.txt'
    if not f.exists(): return []
    r = []; go = None
    for l in f.read_bytes().decode(errors='replace').splitlines():
        if l.startswith('BONUS ev=countdigit digit=0') and go is None: go = float(re.search(r' t=([\d.]+)', l).group(1))   # «GO!»: до него машины стоят на решётке старта
        if l.startswith('BONUSAI ') and ('track=%d ' % t) in l and go is not None and float(re.search(r' t=([\d.]+)', l).group(1)) >= go + 3.0:   # соперники: едут по линии секторов
            for q in l.split()[3:]: x, z = q.split(','); r.append((float(x), float(z)))
    return r
def seg_dist(p, a, b):
    ex, ey = b[0] - a[0], b[1] - a[1]; l2 = ex * ex + ey * ey; u = max(0, min(1, ((p[0] - a[0]) * ex + (p[1] - a[1]) * ey) / l2)) if l2 else 0
    return math.hypot(p[0] - a[0] - ex * u, p[1] - a[1] - ey * u)
sheets = []
stats = []
codes = []
for t in range(1, 8):
    shot = out / f'run{t}/out/sector_b{t}.bmp'; c = cam(t)
    if not shot.exists() or not c or t not in bnd: print('нет данных для трассы', t); continue
    im = Image.open(shot).convert('RGB').crop((0, 0, W, H)).resize((W * K, H * K), Image.NEAREST); d = ImageDraw.Draw(im, 'RGBA'); fn = font(15); fs = font(11)
    B = bnd[t]; n = len(B) - 1
    pts = [project(c, (b[1], b[5], b[2])) for b in B]
    for i in range(len(pts) - 1): d.line([pts[i], pts[i + 1]], fill=(255, 255, 255, 110), width=2)
    for (k, x, z, dx, dz, gy), (sx, sy) in zip(B, pts):
        ex, ey = project(c, (x - dz * 2.2, gy, z + dx * 2.2)); fx, fy = project(c, (x + dz * 2.2, gy, z - dx * 2.2))
        big = (k - 1) % 5 == 0
        d.line([(ex, ey), (fx, fy)], fill=(255, 40, 40, 255) if k == 1 else (255, 230, 0, 235 if big else 150), width=3 if big else 1)
        if big and k <= n:
            lab = f'{k:02d}'; tx, ty = (ex + fx) / 2 + (fx - ex) * .0, (ey + fy) / 2
            ox, oy = (ex - fx), (ey - fy); l = math.hypot(ox, oy) or 1; tx, ty = ex + ox / l * 14, ey + oy / l * 14
            d.rectangle([tx - 13, ty - 9, tx + 13, ty + 9], fill=(0, 0, 0, 170)); d.text((tx, ty), lab, fill=(255, 255, 255, 255), font=fs, anchor='mm')
    # проверка проекции: позиции машины игрока из журнала кадра (раз в секунду, округлены до метра) — точки; расстояние до линии пути в пикселях кадра 480×272
    dd = []
    for (x, z) in samples(t):
        sx, sy = project(c, (x, gy, z)); dist = min(seg_dist((sx, sy), pts[i], pts[i + 1]) for i in range(len(pts) - 1)) / K; dd.append(dist)
        d.ellipse([sx - 5, sy - 5, sx + 5, sy + 5], outline=(0, 255, 255, 255), width=2)
    if dd: dd.sort(); stats.append((t, len(dd), dd[len(dd) // 2], dd[-1])); print(f'трасса {t}: точек машины {len(dd)}, расстояние до линии пути (px кадра) медиана {dd[len(dd)//2]:.1f}, максимум {dd[-1]:.1f}')
    # код на экране игры в момент снимка code_bN: позиция машины игрока из строки SHOT (мир, x z) — точка на карте; сектор по линии считается здесь же и пишется в таблицу, на снимке code_bN.bmp в левом нижнем углу виден код — они обязаны совпасть
    smk = (out / f'run{t}/run/smoke.txt').read_bytes().decode(errors='replace').splitlines()
    sh = next((l.split() for l in smk if l.startswith('SHOT code_b%d ' % t)), None)
    if sh:
        px, pz = float(sh[4]), float(sh[6]); best = (1e9, 0)
        for (k, x1, z1, dx1, dz1, y1), (k2, x2, z2, dx2, dz2, y2) in zip(B, B[1:]):
            ex, ez = x2 - x1, z2 - z1; l2 = ex * ex + ez * ez; u = max(0, min(1, ((px - x1) * ex + (pz - z1) * ez) / l2)) if l2 else 0
            dd0 = math.hypot(px - x1 - ex * u, pz - z1 - ez * u)
            if dd0 < best[0]: best = (dd0, k)
        sx, sy = project(c, (px, B[best[1] - 1][5], pz)); d.ellipse([sx - 9, sy - 9, sx + 9, sy + 9], outline=(255, 0, 255, 255), width=3)
        d.text((sx + 14, sy - 6), f'машина на снимке кода: B{t}-{best[1]:02d}', fill=(255, 0, 255, 255), font=fn); codes.append((t, best[1], best[0]))
    d.rectangle([0, 0, 330, 30], fill=(0, 0, 0, 190)); d.text((8, 15), f'B{t}  секторов {n} по 5 м   код B{t}-NN', fill=(255, 255, 255, 255), font=fn, anchor='lm')
    p = out / f'sector_map_b{t}.png'; im.save(p); sheets.append(im.resize((W * 2, H * 2), Image.LANCZOS)); print('ok', p.name, n, 'секторов')
if codes: (out / 'таблица_кода.txt').write_text(''.join(f'трасса {t}: на снимке code_b{t}.png сектор по линии B{t}-{k:02d} (машина в {dd:.1f} м от линии)\n' for t, k, dd in codes)); print(''.join(f'трасса {t}: B{t}-{k:02d}\n' for t, k, dd in codes))
if sheets:
    cols = 2; rows = (len(sheets) + 1) // 2; A = Image.new('RGB', (W * 2 * cols, H * 2 * rows), (20, 20, 20))
    for i, s in enumerate(sheets): A.paste(s, ((i % cols) * W * 2, (i // cols) * H * 2))
    A.save(out / 'all.png'); print('ok all.png')
