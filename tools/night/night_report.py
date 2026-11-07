#!/usr/bin/env python3
"""Ночные боты (№876): утренний отчёт по NIGHT_DIR/runs -> NIGHT_DIR/report.html + листы PNG + сводка текстом.
  python3 tools/night/night_report.py NIGHT_DIR
Исследователь: на точку (сцена P###) — выброс GE (DIAGS drop_real − drop_sky + listdrop, из короткого круга explore_diag_L{N}; прежде citydrop+listdrop+drop_car), камера в теле (CAMCLIPSTAT eye_inside),
недогруженные клетки в кадре (BB miss), медленные кадры (SLOWFRAME); подозрительные — в начало, с 4 фото. Гэги — лист 2 кадров на гэг.
Долгая игра — куча после каждой загрузки уровня (LEVEL ev=load heap/mx) по кругам: тренд вниз = утечка/дробление.
Миссии и догонялки (дорожка laneD, missions_L{N}): по каждой миссии — этапы, что прошёл, где встал (этап не менялся все циклы = недоходимая цель/зависший этап), провал, конец уровня;
по погоне CH_i — цена кадра (STAT p95/max, медленные кадры); зависание — нет строки END / STALL в чёрном ящике."""
import html, re, sys

from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import watchdog_col
from PIL import Image, ImageDraw

N = Path(sys.argv[1]); R = N / 'runs'; out = []; text = []


def scenes(p):
    cur = None; S = {}
    for ln in open(p, errors='ignore'):
        if ln.startswith('SCENE '): cur = ln.split()[1]; S.setdefault(cur, {'drop': 0, 'eye': 0, 'miss': 0, 'slow': 0, 'pos': ''}); continue
        if cur is None: continue
        s = S[cur]
        if ln.startswith('DIAGS'):
            k = dict(re.findall(r'(\w+)=(\d+)', ln))
            # №922: drop_real — выброшенные GE и НЕ заплатанные (настоящая потеря), drop_ink — из них обводка машин; небо (drop_sky) не в счёт — его закрывает подстилка земли (замер L3: ≤0,22 % кадра)
            # (прежний citydrop считал и заплатанные кузова машин, и сборку листов клеток). Старая сборка без drop_real — прежняя формула
            if 'drop_real' in k: s['drop'] += max(0, int(k['drop_real']) - int(k.get('drop_sky', 0))) + int(k.get('listdrop', 0)); s['ink'] = s.get('ink', 0) + int(k.get('drop_ink', 0))
            else: s['drop'] += int(k.get('drop_city', 0)) + int(k.get('listdrop', 0)) + int(k.get('drop_car', 0))
        elif ln.startswith('SLOWFRAME'): s['slow'] += 1 if s.get('bb', 0) >= 2 else 0   # первые 2 с после телепорта — подгрузка, не в счёт
        elif ln.startswith('BB '):
            f = ln.split(); s['bb'] = s.get('bb', 0) + 1; s['pos'] = ' '.join(f[3:5]) + f' ({f[5]}, {f[7]})' if len(f) > 8 else s['pos']
            if 'miss' in f: s['miss'] = int(f[f.index('miss') + 1])   # последняя секунда точки: подгрузка после телепорта закончилась
    return S


def sheet(files, path, cols=4, w=240, h=136, labels=None):
    ims = [f for f in files if f.exists()]
    if not ims: return None
    rows = (len(ims) + cols - 1) // cols; im = Image.new('RGB', (cols * w, rows * (h + 14)), (20, 20, 20)); d = ImageDraw.Draw(im)
    for i, f in enumerate(ims):
        x, y = (i % cols) * w, (i // cols) * (h + 14)
        im.paste(Image.open(f).convert('RGB').resize((w, h)), (x, y + 14)); d.text((x + 3, y + 1), (labels[i] if labels else f.stem), fill=(230, 230, 230))
    im.save(path); return path.name


out.append('<h2>Исследователь</h2>')
for L in range(1, 8):
    d = R / f'explore_L{L}'
    if not (d / 'smoke.txt').exists(): out.append(f'<p>L{L}: нет журнала</p>'); continue
    S = {k: v for k, v in scenes(d / 'smoke.txt').items() if k.startswith('P')}
    dd = R / f'explore_diag_L{L}'   # №922: выброс GE — из короткого круга с diaglog 1 (в основном круге diaglog 0: кадры и недогруз без цены журнала)
    if (dd / 'smoke.txt').exists():
        for k, v in scenes(dd / 'smoke.txt').items():
            if k in S: S[k]['drop'] = v['drop']; S[k]['ink'] = v.get('ink', 0)
    bad = sorted(S.items(), key=lambda kv: -(kv[1]['drop'] + 10 * kv[1]['miss'] + kv[1]['slow']))
    flagged = [(k, v) for k, v in bad if v['drop'] or v['miss'] or v['slow'] > 2][:24]
    text.append(f'L{L}: точек {len(S)}, подозрительных {len(flagged)} (выброс GE в {sum(1 for v in S.values() if v["drop"])}, недогруженные клетки в {sum(1 for v in S.values() if v["miss"])}, медленные кадры в {sum(1 for v in S.values() if v["slow"] > 2)})')
    out.append(f'<h3>L{L}</h3><p>{html.escape(text[-1])}</p>')
    for k, v in flagged:
        i = int(k[1:]); nm = sheet([d / f'e{i:03d}_{j}.png' for j in range(4)], N / f'ex_L{L}_{k}.png')
        if nm: out.append(f'<p>{k} {html.escape(v["pos"])} · выброс {v["drop"]} (обводка {v.get("ink", 0)}) · недогруз {v["miss"]} · медленных {v["slow"]}</p><img src="{nm}">')
    allf = sorted(d.glob('e*_0.png'))
    nm = sheet(allf, N / f'ex_L{L}_all.png', cols=10, w=160, h=90)
    if nm: out.append(f'<p>все точки (первое фото каждой):</p><img src="{nm}">')
out.append('<h2>Очень дальние виды (бот-репортёр)</h2>')
import numpy as np, csv, shutil
SC = Path.home() / 'Downloads/shar-psp-render-w/build/scout'
for L in range(1, 8):
    f = SC / f'l{L}.csv'
    if not f.exists(): continue
    rows = [r for r in csv.DictReader(open(f)) if r.get('px250') not in (None, '') and r.get('pieces_d250') not in (None, '')]; tot = 128 * 72
    far = np.array([(float(r['px250']) / tot) for r in rows]); far2 = np.array([(float(r['px200']) + float(r['px250'])) / tot for r in rows])
    pid = np.array([int(r['point']) for r in rows]); xs = np.array([float(r['x']) for r in rows]); zs = np.array([float(r['z']) for r in rows])
    need = far > .01; pts = np.unique(pid[need]); allp = np.unique(pid)
    zones = set((int(x // 40), int(z // 40)) for x, z in zip(xs[need], zs[need]))
    text.append(f'L{L}: мест (точек 10 м), где в каком-то направлении нужна даль 250 м: {len(pts)} из {len(allp)} ({100 * len(pts) / max(len(allp), 1):.0f}%), клеток 40 м: {len(zones)}; дальше 140 м заметно (> 1% экрана) в {100 * (far2 > .01).mean():.0f}% ракурсов')
    out.append(f'<p>{html.escape(text[-1])}</p>')
    for nm_ in (f'l{L}_dal.png', f'l{L}_load.png'):
        if (SC / nm_).exists(): shutil.copy(SC / nm_, N / nm_); out.append(f'<img src="{nm_}" style="margin-right:8px">')
out.append('<h2>Гэги</h2>')
for L in range(1, 8):
    d = R / f'gags_L{L}'
    if not (d / 'smoke.txt').exists(): continue
    names = {int(m.group(1)): (m.group(2), int(m.group(3))) for m in re.finditer(r'GAG idx=(\d+) name=(\S+) room=\S+ inst=\S+ nskin=(\d+)', (d / 'smoke.txt').read_text(errors='ignore'))}
    files, labels = [], []
    for k, (nm_, ns) in sorted(names.items()):
        if ns <= 0: continue
        for s_ in 'ab': files.append(d / f'g{k:02d}_{s_}.png'); labels.append(f'{nm_} {s_}')
    nm = sheet(files, N / f'gags_L{L}.png', cols=6, w=200, h=113, labels=labels)
    text.append(f'L{L}: гэгов с персонажами {sum(1 for v in names.values() if v[1] > 0)}')
    if nm: out.append(f'<h3>L{L}</h3><img src="{nm}">')
out.append('<h2>Долгая игра</h2>')
sk = R / 'soak' / 'smoke.txt'
if sk.exists():
    t = sk.read_text(errors='ignore'); loads = re.findall(r'LEVEL ev=load n=(\d+) .*? heap=(\d+) mx0=\d+ mx=(\d+)', t)
    end = '^END$' in t or '\nEND\n' in t
    rows = ''.join(f'<tr><td>{i + 1}</td><td>L{n}</td><td>{int(h) / 1048576:.2f}</td><td>{int(m) / 1048576:.2f}</td></tr>' for i, (n, h, m) in enumerate(loads))
    text.append(f'долгая игра: загрузок {len(loads)}, доиграла до конца: {"да" if end else "НЕТ"}; куча после загрузки, МБ: ' + ' '.join(f'{int(h) / 1048576:.1f}' for _, h, _ in loads))
    out.append(f'<p>{html.escape(text[-1])}</p><table border=1 cellpadding=3><tr><th>№</th><th>уровень</th><th>куча, МБ</th><th>крупнейший кусок, МБ</th></tr>{rows}</table>')

out.append('<h2>Миссии и догонялки</h2>')
FIND = []   # находки в TSV формата autoexplore: level x y z type weight seed frame description
for L in range(1, 8):
    d = R / f'missions_L{L}'; f = d / 'smoke.txt'
    if not f.exists(): out.append(f'<p>L{L}: нет журнала</p>'); continue
    T = f.read_text(errors='ignore').splitlines(); bb = (d / 'blackbox.txt'); stall = len(re.findall(r'STALL', bb.read_text(errors='ignore'))) if bb.exists() else 0
    end = any(l == 'END' for l in T); seg = {}; tag = None; cyc = 0; stat = {}; slow = {}
    for ln in T:
        ms_ = re.match(r'STAT (CH_(\d)) ', ln)
        if ms_: stat['CH' + ms_.group(2)] = ln   # STAT сцены пишется при смене сцены — берём по имени, не по текущей
        m = re.match(r'SCENE (MS_(\w+?)_(start|c(\d+))|CH_(\d))\b', ln)
        if m:
            if m.group(2): tag = m.group(2); seg.setdefault(tag, {'mis': None, 'stages': [], 'done': 0, 'last': -1, 'n': 0, 'fail': '', 'go_none': 0, 'complete': 0, 'lvdone': 0, 'goto': ''}); cyc = int(m.group(4)) if m.group(4) else -1
            else: tag = 'CH' + m.group(5)
            if m.group(2) and m.group(4): seg[tag]['n'] = cyc + 1
            continue
        if tag is None: continue
        if tag.startswith('CH'):
            if ln.startswith('SLOWFRAME'): slow[tag] = slow.get(tag, 0) + 1
            continue
        g = seg[tag]
        m2 = re.match(r'MSN (select|mainat) (\S+)', ln)
        if m2 and g['mis'] is None: g['mis'] = m2.group(2)
        m2 = re.match(r'MSN stage (\S+)\.(\d+)\.(\d+) obj=(\S+) text="([^"]*)"', ln)
        if m2 and m2.group(1) == g['mis'] and not (g['stages'] and ln.endswith(' t=0.0')): g['stages'].append((m2.group(2) + '.' + m2.group(3), m2.group(4), m2.group(5))); g['last'] = cyc
        if ln.startswith('MSN done') and g['stages']: g['done'] += 1; g['last'] = cyc
        if ln.startswith('MSN complete'): g['complete'] += 1
        if ln.startswith('MSN failed'): g['fail'] = ln[11:60]
        if ln.startswith('MSNGO none') or ln.startswith('MSNGO room'): g['go_none'] += 1
        if ln.startswith('MSNGO obj='): g['goto'] = ln[6:60]
        if ln.startswith('LEVELDONE'): g['lvdone'] += 1
    rows = []; stuckn = 0
    for t, g in seg.items():
        mg_ = re.search(r'to (-?[\d.]+) (-?[\d.]+) (-?[\d.]+)', g.get('goto', ''))   # цель последнего msngo — место находки в TSV
        if g['mis'] is None: rows.append(f'<tr><td>{t}</td><td colspan=6>миссия не стартовала (select не дошёл)</td></tr>'); stuckn += 1; continue
        last = g['stages'][-1] if g['stages'] else ('', '', '')
        waited = g['n'] - 1 - max(g['last'], 0)   # циклов после последнего движения этапа
        st = 'пройдена' if g['complete'] or g['lvdone'] else ('ПРОВАЛ ' + g['fail'] if g['fail'] else ('СТОИТ %d циклов' % waited if waited >= 4 else 'идёт'))
        if st.startswith(('СТОИТ', 'ПРОВАЛ')):
            stuckn += 1; px, py, pz = (float(v) for v in mg_.groups()) if mg_ else (0., 0., 0.)
            FIND.append('\t'.join([str(L), '%.1f' % px, '%.1f' % py, '%.1f' % pz, 'mission_stage_stuck' if st.startswith('СТОИТ') else 'mission_failed', '7' if st.startswith('СТОИТ') else '5', str(L), '0', f'{t} {g["mis"]}: этап {last[0]} {last[1]} "{last[2]}", {st}']))
        rows.append(f'<tr><td>{t}</td><td>{g["mis"]}</td><td>{len(set(x[0] for x in g["stages"]))}</td><td>{g["done"]}</td><td>{html.escape(last[0] + " " + last[1] + " " + last[2])}</td><td>{html.escape(g["goto"])}</td><td>{st}</td></tr>')
    ch = ''.join('<tr><td>%s</td><td>%s</td><td>слабых кадров %d</td></tr>' % (k, html.escape(re.sub(r'^STAT \S+ ', '', v)[:110]), slow.get(k, 0)) for k, v in sorted(stat.items()))
    text.append(f'L{L} миссии: проходов {len(seg)}, встали/провал {stuckn}, STALL {stall}, до END {"да" if end else "НЕТ (зависание или время)"}; погоня: ' + '; '.join('%s %s' % (k, re.sub(r'^STAT \S+ frames \d+ ', '', v)[:40]) for k, v in sorted(stat.items())))
    out.append(f'<h3>L{L}</h3><p>{html.escape(text[-1])}</p><table border=1 cellpadding=3><tr><th>проход</th><th>миссия</th><th>этапов</th><th>засчитано</th><th>последний этап</th><th>последний MSNGO</th><th>итог</th></tr>{"".join(rows)}</table>'
               f'<table border=1 cellpadding=3><tr><th>погоня</th><th>кадр</th><th></th></tr>{ch}</table>')
    nm = sheet(sorted(d.glob('ms_*_c*.png'))[::3][:48], N / f'ms_L{L}.png', cols=8, w=150, h=85)
    if nm: out.append(f'<img src="{nm}">')
_wc, _wr = watchdog_col.scan(*sorted(R.glob('*/smoke.txt')), *sorted(R.glob('*/stdout.txt')))   # графа «сторож» (CLAUDE.md п.12): любое срабатывание = дыра
text.append(watchdog_col.summary(_wc)); text.extend(watchdog_col.lines(_wr))
out.append('<h2>Сторож</h2><p>' + html.escape(watchdog_col.summary(_wc)) + '</p><pre>' + html.escape('\n'.join(watchdog_col.lines(_wr, 30))) + '</pre>')
if FIND: (N / 'findings_missions.tsv').write_text('\n'.join(FIND) + '\n')
(N / 'report.html').write_text('<html><meta charset="utf-8"><body style="background:#111;color:#ddd;font-family:sans-serif">'
                               '<h1>Ночные боты</h1><pre>' + html.escape('\n'.join(text)) + '</pre>' + '\n'.join(out) + '</body></html>')
print('\n'.join(text)); print('отчёт:', N / 'report.html')
