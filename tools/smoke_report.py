#!/usr/bin/env python3
"""Разбор смоук-прогона (tools/smoke.sh): smoke.txt движка + снимки окна -> summary.txt и contact.png.
python tools/smoke_report.py OUTDIR SCENARIO.txt [--negative]
Код выхода 0 = всё прошло, 1 = провал (первая строка вывода — причина одной строкой).

Проверки сцены (движок считает после успокоения — viewer/smoke.h; STAT-строка на сцену):
  упала/зависла (нет END, сцена последняя начатая) · средний FPS < цели −2% (long 30, view60 60)
  · work max больше целевого периода (33.3 / 16.7 мс) · пропуски целевого периода > 1% кадров (одиночные — дрожь эмулятора)
  · кадры с видимой, но не загруженной клеткой > 0 · ошибки чтения > 0 · отказы памяти > 0
  · успокоение не дождалось догрузки за 10 с · свободной кучи < 0.5 МБ
--negative: сцены neg_* — отрицательные контроли; прогон проходит, только если КАЖДАЯ neg_* сцена провалилась
(т.е. проверки ловят подброшенную ошибку), а остальные сцены — прошли."""
import os, sys
from pathlib import Path

TARGET = {'long': 30.0, 'view60': 60.0}
PERIOD = {1: 16.7, 2: 33.4}
TOL = 0.98
# 04.10: режим 60 FPS / 115 м (view60) служебный — игроку недоступен (в режиме игрока всегда 30 FPS / 250 м, viewer/main.c
# longView). После правки швов 570ac3a (ближняя версия кусков на любой дальности, +28 % треугольников) он даёт 58–59 FPS
# (было 59,9). Слово Роба (26.09: «ровные 30 FPS допустимы»; 03.10: «лучше прыгающие полигоны, как на PS1, чем швы»):
# FPS этого режима — предупреждение в отчёте, не провал. Остальные проверки view60 (падение, кадр, пропуски, память, GE) — в силе
SOFT_FPS = {'view60'}
HEAP_MIN = 0.5


BONUS_SCENES = set()   # сцены бонуса: кадры мерит tests/bonus/perf_gate.py по BONUSPERF, строк кадров в smoke.txt нет


def parse(out):
    BONUS_SCENES.clear()
    log = (out / 'smoke.txt').read_text(errors='replace').splitlines() if (out / 'smoke.txt').exists() else []   # журнал может обрезать строку посреди буквы (OBJCNT, TEST86)
    ended = any(l.startswith('END') for l in log)
    rows, stats, started = {}, {}, []
    for l in log:
        p = l.split()
        if not p or p[0].startswith('#'):
            continue
        if p[0] == 'SCENE':
            started.append(p[1]); continue
        if p[0] == 'STAT':
            d = {'name': p[1], 'timeout': p[-1] == 'TIMEOUT'}
            kv = p[2:-1] if d['timeout'] else p[2:]
            for k, v in zip(kv[::2], kv[1::2]):
                d[k] = float(v)
            stats[p[1]] = d; continue
        if p[0] == 'BONUSPERF':
            BONUS_SCENES.add(p[1][3:] if p[1].startswith('sc=') else p[1]); continue
        if p[0] in ('SHOT', 'END', 'FAULT', 'ZONE'):   # ZONE gate N res … — строка ворот (4-е поле не число)
            continue
        try:
            float(p[0])  # frame rows start with a numeric timestamp; service events do not
            sc, fps, mode = p[1], float(p[2]), p[9]
        except (ValueError, IndexError):
            continue
        rows.setdefault(sc, []).append(dict(fps=fps, mode=mode, work=float(p[3]), wmax=float(p[4]), tris=int(p[6]),
                                            draws=int(p[7]), ram=float(p[8]), settling=p[-1] == 'settling'))
    return ended, rows, stats, started


def check(sc, ended, rows, stats, started):
    """-> (строка таблицы, [причины провала], короткая подпись для листа)"""
    if sc not in started:
        return f"{sc:12} не дошёл", [f"{sc}: не дошёл (упал раньше)"], 'не дошёл'
    crashed = not ended and started[-1] == sc
    r = [x for x in rows.get(sc, []) if not x['settling']]
    r = r[1:] if len(r) > 1 else r
    st = stats.get(sc)
    old = not stats and bool(r)   # движок без счётчиков приёмки (до viewer/smoke.h со STAT): только FPS и work max
    if old and not crashed:
        st = dict(p95=float('nan'), p99=float('nan'), max=max(x['wmax'] for x in r), missed=0, missvis=0, readerr=0,
                  allocfail=0, heapmin=float('nan'), timeout=False, per=2 if r[-1]['mode'] == 'long' else 1)
    if sc in BONUS_SCENES and ended and not r and not os.environ.get('SMOKE_REPORT_OLD'):
        return f"{sc:12} замер BONUSPERF (tests/bonus/perf_gate.py)", [], 'замер BONUSPERF'   # игра дошла до END; цифры не здесь
    if crashed or not st or not r:
        return f"{sc:12} упал/завис", [f"{sc}: упал / завис (нет END)"], 'упал'
    mode = r[-1]['mode']; tgt = TARGET.get(mode, 30.0); per = int(st.get('per', 2))
    favg = sum(x['fps'] for x in r) / len(r); fmin = min(x['fps'] for x in r)
    tmax = max(x['tris'] for x in r); dmax = max(x['draws'] for x in r)
    f = []; soft = mode in SOFT_FPS and favg < tgt * TOL
    if favg < tgt * TOL and not soft: f.append(f"FPS {favg:.1f} < {tgt:.0f}")
    if st['max'] > PERIOD.get(per, 33.4): f.append(f"кадр {st['max']:.1f} мс > {PERIOD.get(per):.1f}")
    # пропуски периода: в эмуляторе бывают одиночные от планировщика Мака при коротком work — допуск 1% кадров;
    # длинный кадр ловит отдельная проверка work max; на PSP (bench.txt ACCEPT) допуска нет
    if st['missed'] > 0.01 * st.get('frames', 0): f.append(f"пропуски периода {st['missed']:.0f} из {st.get('frames', 0):.0f}")
    if st['missvis'] > 0: f.append(f"видимых недогруженных клеток: {st['missvis']:.0f} кадров (до {st['missvismax']:.0f})")
    if st['readerr'] > 0: f.append(f"ошибки чтения {st['readerr']:.0f}")
    if st['allocfail'] > 0: f.append(f"отказы памяти {st['allocfail']:.0f}")
    # правка 103 (30.09): список команд GE был почти полон — куски пропущены (viewer/main.c geOver); отказы памяти любых модулей
    # (viewer/memwatch.c). Старые движки без этих полей — не проверяются
    if st.get('geskip', 0) > 0: f.append(f"список GE полон: пропущено {st['geskip']:.0f} кусков (пик {st.get('gepeak', 0):.0f} КБ)")
    if st.get('memfail', 0) > 0: f.append(f"отказы выделения памяти {st['memfail']:.0f}")
    if st.get('gebad', 0) > 0: f.append(f"пустой/битый адрес для GE: {st['gebad']:.0f} (на PSP это зависание)")
    if st['timeout']: f.append("клетки не догрузились за 10 с")
    if old: f = [x for x in f if not x.startswith('кадр')]   # work max старого движка включает подгрузку после goto
    if st['heapmin'] < HEAP_MIN: f.append(f"куча {st['heapmin']:.2f} МБ")
    line = (f"{sc:12} {mode:6} {fmin:5.1f} {favg:5.1f} {tgt:3.0f} {st['p95']:5.1f} {st['p99']:5.1f} {st['max']:5.1f} "
            f"{st['missed']:4.0f} {st['missvis']:5.0f} {st['readerr']:3.0f} {st['allocfail']:3.0f} {st['heapmin']:5.1f} "
            f"{tmax / 1000:5.1f}k {dmax:4d} {st.get('gepeak', float('nan')):5.0f}  {'ПРОВАЛ' if f else 'ok, предупреждение: FPS служебного режима ниже цели' if soft else 'ok'}")
    return line, [f"{sc}: " + ', '.join(f)] if f else [], f"{mode} {favg:.0f}/{tgt:.0f} FPS  max {st['max']:.1f} ms" + ('  FAIL' if f else '')


def main(out, scn, negative=False):
    out = Path(out)
    text = Path(scn).read_text().splitlines()
    planned = [l.split()[1] for l in text if l.startswith('scene ')]
    shots = [l.split()[1] for l in text if l.startswith('shot ')]
    ended, rows, stats, started = parse(out)
    head = (f"{'сцена':12} {'режим':6} {'FPSmin':>5} {'avg':>5} {'цель':>3} {'p95':>5} {'p99':>5} {'max':>5} "
            f"{'проп':>4} {'клет':>5} {'чт':>3} {'пам':>3} {'куча':>5} {'tris':>6} {'drw':>4} {'GE':>5}")
    lines, fails, label, caught = [head], [], {}, []
    for sc in planned:
        line, f, label[sc] = check(sc, ended, rows, stats, started)
        lines.append(line)
        if negative and sc.startswith('neg_'):
            # засчитываем только СВОЙ признак подброшенной ошибки (ревью Codex §14): посторонний провал не считается
            st = stats.get(sc) or {}
            want = {'neg_longframe': lambda t: t.get('max', 0) >= 50.0,
                    'neg_delaycell': lambda t: t.get('missvis', 0) > 0,
                    'neg_badread': lambda t: t.get('readerr', 0) > 0,
                    'neg_gelist': lambda t: t.get('geskip', 0) > 0,
                    'neg_gebad': lambda t: t.get('gebad', 0) > 0}.get(sc)
            if want and st and want(st):
                caught.append(sc)
            else:
                fails.append(f"{sc}: подброшенная ошибка НЕ поймана своим признаком")
        else:
            fails += f
    missing = [s for s in shots if not (out / f'{s}.png').exists()]
    if missing and not fails:
        fails.append('нет снимков: ' + ', '.join(missing))
    if negative:
        nneg = sum(1 for s in planned if s.startswith('neg_'))
        verdict = (f'OK: отрицательные контроли пойманы {len(caught)}/{nneg}' if not fails
                   else 'ПРОВАЛ: ' + '; '.join(fails))
    else:
        verdict = 'OK' if not fails else 'ПРОВАЛ: ' + '; '.join(fails)
    if not stats and rows:
        verdict += '  (движок без счётчиков приёмки: проверены только падение, FPS и снимки)'
    legend = ('p95/p99/max — work, мс; проп — пропуски целевого периода; клет — кадры с видимой недогруженной клеткой; '
              'чт/пам — ошибки чтения/памяти; куча — минимум свободной, МБ (оценка); GE — пик списка команд GE, КБ из 256. '
              'Считается после успокоения сцены.')
    (out / 'summary.txt').write_text(verdict + '\n\n' + '\n'.join(lines) + '\n\n' + legend + '\n')
    contact(out, shots, label)
    print(verdict)
    print('\n'.join(lines))
    return 0 if not fails else 1


def contact(out, shots, label):
    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        return
    W, H, LAB, COLS = 480, 272, 22, 3
    n = len(shots); rws = max(1, (n + COLS - 1) // COLS)
    sheet = Image.new('RGB', (COLS * W, rws * (H + LAB)), (24, 24, 24))
    d = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf', 14)
    except OSError:
        font = ImageFont.load_default()
    for i, s in enumerate(shots):
        p = out / f'{s}.png'
        x, y = (i % COLS) * W, (i // COLS) * (H + LAB)
        if p.exists():
            im = Image.open(p).convert('RGB')
            w, h = im.size; ch = min(h, round(w * 272 / 480))   # окно: заголовок сверху, кадр 480x272 снизу
            sheet.paste(im.crop((0, h - ch, w, h)).resize((W, H)), (x, y + LAB))
        else:
            d.text((x + 8, y + LAB + 8), 'нет снимка', fill=(255, 90, 90), font=font)
        d.text((x + 6, y + 3), f"{s}   {label.get(s, '')}", fill=(230, 230, 230), font=font)
    sheet.save(out / 'contact.png')


if __name__ == '__main__':
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    sys.exit(main(a[0], a[1], '--negative' in sys.argv))
