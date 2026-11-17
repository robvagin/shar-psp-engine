#!/usr/bin/env python3
"""Сверка тура 107 на PSP с предыдущим туром по уровням (метод docs/status.md: числа только из smoke.txt с карты, первые 2 с сцены отброшены).
  python3 -I tools/tour107_compare.py ТУР_107 ТУР_106 [--levels 1,3,6] [--out файл.md]    (первый — новый, второй — база)
  python3 -I tools/tour107_compare.py --check-set ПАПКА_НАБОРА                              (какие файлы-выключатели _OLD лежат рядом с EBOOT: должны отсутствовать)
Что считается по каждому уровню (уровень сцены = последний «LEVEL ev=load n=N» перед сценой; сцены меню, пауз, роликов, комнат в езду не входят: dd=120 PAUSE_DD отбрасывается):
  к/с медиана (RFAST fps), пропуски кадров (STAT missed / frames), work_ms p50 / p95 (RFAST), dd: круг (RFAST dd) и ось шара (DAL1272 ax, только тур 107+),
  дыры: STAT missvis (кадров с видимой незагруженной клеткой), DAL1272 f150 / fdd (кадров с дырой в круге 150 м / внутри текущей дальности; есть только в сборках с Даль-1272),
  куча мин (STAT heapmin), CELLREGFIX, MCRNONE и WATCHDOG mcr, WATCHDOG msnart pic-give-up (карточка без картинки), LOST (строки со словом LOST).
Тур-обёртки: для L1/L3/L6 (по умолчанию) и «всё» (все уровни). Только чтение."""
import re, sys, argparse, statistics as st
from collections import defaultdict
from pathlib import Path

KV = re.compile(r'(\w+)=([^\s]+)')
OLD_FLAGS = ['dal1272old', 'dalbeltoff', 'streamhalfold', 'sr1272old', 'ddreg1244old', 'cellthrashold', 'cellthrashceilold', 'streamtight99old', 'dalrampold', 'dal877old']


def fnum(x):
    try: return float(str(x).split('/')[0])
    except Exception: return None


def pct(v, p):
    v = sorted(v)
    return v[int(p * (len(v) - 1))] if v else None


def parse(d):
    f = Path(d) / 'smoke.txt'
    if not f.exists(): sys.exit('нет smoke.txt: ' + str(f))
    lvl = 0; cur = None; sc_t0 = {}; sc_lvl = {}
    R = defaultdict(lambda: defaultdict(list))
    cnt = defaultdict(lambda: defaultdict(int))
    for ln in f.read_text(errors='replace').splitlines():
        if ln.startswith('LEVEL ev=load'):
            m = re.search(r'\bn=(\d+)', ln); lvl = int(m.group(1)) if m else lvl
            if cur is not None: sc_lvl[cur] = lvl   # 107б (Fable §1.7): строка сцены идёт раньше строки загрузки уровня этой же сцены (golevel в её начале), сцена принадлежит новому уровню
            continue
        if ln.startswith('SCENE '):
            p = ln.split(); cur = p[1]; sc_t0[cur] = float(p[2]); sc_lvl[cur] = lvl
            continue
        tag = ln.split(' ', 1)[0]
        L = sc_lvl.get(cur, lvl)
        if tag == 'STAT' and ' frames ' in ln:
            p = ln.split(); k = dict(zip(p[2::2], p[3::2]))
            n = int(k.get('frames', 0))
            if n:
                R[L]['frames'].append(n); R[L]['missed'].append(int(k.get('missed', 0))); R[L]['missvis'].append(int(k.get('missvis', 0)))
                R[L]['heapmin'].append(float(k.get('heapmin', 99)))
        elif tag == 'RFAST':
            k = dict(KV.findall(ln)); sc = k.get('sc'); t = fnum(k.get('t'))
            if not sc or sc not in sc_t0 or t is None or t < sc_t0[sc] + 2.0: continue
            dd = fnum(k.get('dd'))
            if dd is None or abs(dd - 120.0) < 0.5: continue   # PAUSE_DD: пауза, меню, ролик
            sl = sc_lvl.get(sc, lvl)
            R[sl]['fps'].append(fnum(k.get('fps'))); R[sl]['work'].append(fnum(k.get('work_ms'))); R[sl]['dd'].append(dd)
        elif tag == 'DAL1272':
            k = dict(KV.findall(ln))
            for a, b in (('f150', 'f150'), ('fdd', 'fdd'), ('frames', 'dalfr')):
                R[L][b].append(int(float(k.get(a, 0))))
            if 'ax' in k: R[L]['ax'].append(float(k['ax']))
        elif tag == 'CELLREGFIX': cnt[L]['cellregfix'] += 1
        elif tag == 'MCRNONE': cnt[L]['mcrnone'] += 1
        elif ln.startswith('WATCHDOG mcr'): cnt[L]['wdmcr'] += 1
        elif 'pic-give-up' in ln and ln.startswith('WATCHDOG'): cnt[L]['picgiveup'] += 1
        if re.search(r'\bLOST\b', ln): cnt[L]['lost'] += 1
    return R, cnt


def agg(R, cnt, lv):
    ks = [x for x in R if (lv is None or x in lv)]
    def cat(key): return [x for k in ks for x in R[k][key] if x is not None]
    def cs(key): return sum(cnt[k][key] for k in set(list(cnt) + ks) if lv is None or k in lv)
    fr = sum(cat('frames')); ms = sum(cat('missed'))
    dalfr = sum(cat('dalfr'))
    return dict(
        fps=st.median(cat('fps')) if cat('fps') else None,
        miss=100.0 * ms / fr if fr else None, frames=fr,
        w50=pct(cat('work'), .5), w95=pct(cat('work'), .95),
        dd50=pct(cat('dd'), .5), dd10=pct(cat('dd'), .1), dd90=pct(cat('dd'), .9),
        ax50=pct(cat('ax'), .5), ax90=pct(cat('ax'), .9),
        missvis=sum(cat('missvis')), f150=sum(cat('f150')) if dalfr else None, fdd=sum(cat('fdd')) if dalfr else None,
        heap=min(cat('heapmin')) if cat('heapmin') else None,
        regfix=cs('cellregfix'), mcrnone=cs('mcrnone'), wdmcr=cs('wdmcr'), pic=cs('picgiveup'), lost=cs('lost'))


def f(x, p=1):
    return '—' if x is None else (f'{x:.{p}f}')


ROWS = [('к/с, медиана', 'fps', 1, 'up'), ('кадров > периода, %', 'miss', 1, 'down'), ('work_ms p50', 'w50', 2, 'down'), ('work_ms p95 (в туре +диагностика смоука, см. оговорку)', 'w95', 2, 'down'),
        ('dd круг p10 / p50 / p90, м', None, 0, None), ('dd ось (DAL1272 ax) p50 / p90, м', None, 0, None),
        ('кадров с видимой дырой (STAT missvis)', 'missvis', 0, 'down'), ('кадров с дырой в круге 150 м', 'f150', 0, 'down'), ('кадров с дырой внутри дальности', 'fdd', 0, 'down'),
        ('куча мин, МБ', 'heap', 2, 'up'), ('CELLREGFIX', 'regfix', 0, 'down'), ('MCRNONE', 'mcrnone', 0, 'down'), ('WATCHDOG mcr', 'wdmcr', 0, 'down'),
        ('WATCHDOG msnart pic-give-up', 'pic', 0, 'down'), ('LOST', 'lost', 0, 'down')]


def table(A, B, an, bn):
    out = [f'| показатель | {an} | {bn} | {an} − {bn} |', '|---|---|---|---|']
    for lab, key, p, good in ROWS:
        if key is None and lab.startswith('dd круг'):
            a = f"{f(A['dd10'],0)} / {f(A['dd50'],0)} / {f(A['dd90'],0)}"; b = f"{f(B['dd10'],0)} / {f(B['dd50'],0)} / {f(B['dd90'],0)}"; d = ''
        elif key is None:
            a = f"{f(A['ax50'],0)} / {f(A['ax90'],0)}"; b = f"{f(B['ax50'],0)} / {f(B['ax90'],0)}"; d = ''
        else:
            va, vb = A[key], B[key]
            a, b = f(va, p), f(vb, p)
            d = '' if va is None or vb is None else f'{va - vb:+.{p}f}'
        out.append(f'| {lab} | {a} | {b} | {d} |')
    return '\n'.join(out)


def check_set(d):
    d = Path(d); rows = []
    for n in OLD_FLAGS:
        if (d / n).exists(): rows.append(n)
    print('Набор:', d)
    print('Файлы-выключатели прежнего поведения рядом с EBOOT:', ', '.join(rows) if rows else 'нет (игра = значения по умолчанию)')
    exp = ['dalbeltoff', 'streamhalfold', 'dal1272old']
    bad = [n for n in exp if (d / n).exists()]
    eb = d / 'EBOOT.PBP'; blob = eb.read_bytes() if eb.exists() else b''
    inside = {n: (n.encode() in blob) for n in exp}   # имена файлов откатов лежат в EBOOT открытым текстом: нет строки = кода Даль-1272 в сборке нет
    if not blob: print('EBOOT.PBP в наборе не найден')
    elif not all(inside.values()): print('В EBOOT НЕТ кода Даль-1272 (нет строк ' + ', '.join(n for n, v in inside.items() if not v) + '): шар, streamhalf, потолок 200 и круг 170 в этой сборке не существуют')
    else: print('В EBOOT код Даль-1272 есть (строки откатов найдены)')
    if all(inside.values()) or not blob: print('streamhalf / потолок 200 / круг:', 'ВЫКЛЮЧЕНЫ файлом: ' + ', '.join(bad) if bad else 'включены (файлов отката нет)')
    bad = bad or ([n for n, v in inside.items() if not v] if blob else [])
    print('Шар пояса и дальняя плоскость по оси (с 21:35 13.11 ВКЛЮЧЕНЫ по умолчанию, слово Роба; откат — файлы dalbeltoff / beltfarold):', ', '.join(n for n in ('dalbeltoff', 'beltfarold') if (d / n).exists()) or 'включены (файлов отката нет)')
    if (d / 'sharme_old.prx').exists():   # набор 108 (план упаковки Fable §5.4)
        forb = [n for n in ('dalbelton', 'beltfaron', 'dalbeltoff', 'beltfarold', 'streamchunkon', 'lmapverify', 'lmapbreak', 'p1pagesold') if (d / n).exists()]
        lm = [n for n in ('lmap.shr', 'l2/lmap.shr', 'l3/lmap.shr', 'l4/lmap.shr', 'l5/lmap.shr', 'l6/lmap.shr', 'l7/lmap.shr') if (d / n).exists()]
        tour = (d / 'scenario.txt').exists(); so = (d / 'sharmeold').exists()
        print('Набор 108: модули звука sharme.prx %d Б, sharme_old.prx %d Б; lmap.shr v2 на %d уровнях из 7; запрещённые файлы: %s; sharmeold: %s; П1: страницы включены (p1pagesold быть не должно)' % ((d / 'sharme.prx').stat().st_size if (d / 'sharme.prx').exists() else 0, (d / 'sharme_old.prx').stat().st_size, len(lm), ', '.join(forb) or 'нет', ('тур: нет (новый модуль)' if not so else 'ТУР С sharmeold (новый модуль не проверится!)') if tour else ('игра: есть (прежний модуль до тура)' if so else 'ИГРА БЕЗ sharmeold (новый модуль на железе не запускался!)')))
        # слияние 109→108 (слово Роба 14.11): msnres на 7 уровнях, сброс и автосброс машины ВКЛЮЧЕНЫ файлами, texbake на 7 уровнях, rollback.txt в игре = выключенные новые пути загрузки, в туре файла нет
        mr = [n for n in ('msnres-l1.shr',) + tuple('l%d/msnres-l%d.shr' % (i, i) for i in range(2, 8)) if (d / n).exists()]
        tb = [n for n in ('texbake.shr',) + tuple('l%d/texbake.shr' % i for i in range(2, 8)) if (d / n).exists()]
        sr = [n for n in ('selreset1285on', 'autoreset1285on') if (d / n).exists()]
        sky = [n for n in tb if b'SKY1' in (d / n).read_bytes()]   # Запекание З8: признак решётки (fl[12..13]) и хвост SKY1 в texbake.shr; без хвоста выигрыш загрузки не работает
        rbt = (d / 'rollback.txt').read_text().split() if (d / 'rollback.txt').exists() else []
        rb_exp = None   # 1320: в игре ROOMRELOAD1320_OLD обязателен до проверки туром на PSP; 16.11: COLLCHAIN1364_OLD допускается в игре и в туре (решение дирижёра по туру 0f39d2ba)
        print('Слияние 109: msnres-l<N>.shr %d из 7; texbake.shr %d из 7; сброс машины/автосброс: %s; rollback.txt: %s' % (len(mr), len(tb), ', '.join(sr) or 'НЕТ', ', '.join(rbt) or ('нет' if tour else 'НЕТ (ожидается ROOMRELOAD1320_OLD)')))
        print('texbake.shr с хвостом SKY1: %d из 7' % len(sky))
        if len(sky) != 7: bad = bad or ['texbake без SKY1']
        allowed = {'COLLCHAIN1364_OLD'} if tour else {'ROOMRELOAD1320_OLD', 'COLLCHAIN1364_OLD'}; rb_ok = set(rbt) <= allowed and (tour or 'ROOMRELOAD1320_OLD' in rbt)
        if len(mr) != 7 or len(tb) != 7 or len(sr) != 2 or not rb_ok: bad = bad or ['слияние 109']
        if forb or len(lm) != 7 or not (d / 'sharme.prx').exists() or (tour and so) or (not tour and not so): bad = bad or ['набор 108']
    # Fable 16.11 (оценка п.6г): файлы-выключатели обоих наборов и их расхождение печатаются всегда (prereadold и pfetchold лежат в обоих с набора TEST26, tools/test26load_set.sh; в test108_set.sh их нет)
    def zero_files(q): return sorted(f.name for f in q.iterdir() if f.is_file() and f.stat().st_size == 0)
    me = zero_files(d); print('Пустые файлы-выключатели рядом с EBOOT (%d): %s' % (len(me), ', '.join(me) or 'нет'))
    rb_ = (d / 'rollback.txt'); print('rollback.txt:', ', '.join(rb_.read_text().split()) if rb_.exists() else 'нет')
    ps = str(d); other = None
    if '/test108tour/' in ps: other = Path(ps.replace('/test108tour/', '/test108/'))
    elif '/test108/' in ps: other = Path(ps.replace('/test108/', '/test108tour/'))
    if other is not None and other.is_dir():
        ot = zero_files(other); orb = (other / 'rollback.txt')
        game, tour = (other, d) if '/test108tour/' in ps else (d, other)
        gz, tz = set(zero_files(game)), set(zero_files(tour))
        print('Расхождение игра ↔ тур по файлам-выключателям: только в игре: %s; только в туре: %s' % (', '.join(sorted(gz - tz)) or 'нет', ', '.join(sorted(tz - gz)) or 'нет'))
        gr = (game / 'rollback.txt').read_text().split() if (game / 'rollback.txt').exists() else []; tr = (tour / 'rollback.txt').read_text().split() if (tour / 'rollback.txt').exists() else []
        print('Расхождение rollback.txt: игра [%s], тур [%s]' % (', '.join(gr) or '-', ', '.join(tr) or '-'))
    print('Файлы tour-only (замеряют, не игра):', ', '.join(n for n in ('smokelog', 'scenario.txt', 'cellthrashold', 'slowio', 'flypath') if (d / n).exists()) or 'нет')
    sys.exit(1 if bad else 0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('new', nargs='?'); ap.add_argument('base', nargs='?')
    ap.add_argument('--levels', default='1,3,6'); ap.add_argument('--out'); ap.add_argument('--check-set')
    a = ap.parse_args()
    if a.check_set: check_set(a.check_set)
    if not a.new or not a.base: ap.error('нужны два тура')
    RN, CN = parse(a.new); RB, CB = parse(a.base)
    an, bn = Path(a.new).name.split('_')[-1], Path(a.base).name.split('_')[-1]
    out = [f'# Сверка тура {an} с {bn}', '', 'Первые 2 с сцены отброшены; dd=120 (пауза, меню, ролик) не входит; уровень сцены = последний «LEVEL ev=load». Пропуски: STAT missed / frames. «—» нет данных (строки DAL1272 есть только в сборках с Даль-1272).', '',
             '**Оговорка к work_ms и пикам кадра:** тур идёт со смоуком (smokelog), а он сам даёт всплески в stream(): диагностика CITYMEM (до 640 мкс на Маке) и STREAMWHY (до 825 мкс на Маке, на PSP ×5,8) раз в 5 с; в релизной сборке без смоука их нет. Пики work_ms и p95 тура поэтому чуть выше, чем у игрока; сравнивать туры между собой можно (диагностика в обоих), с «ощущением кадра» игрока нельзя без поправки.', '']
    for lv in [int(x) for x in a.levels.split(',')]:
        out += [f'## L{lv}', '', table(agg(RN, CN, {lv}), agg(RB, CB, {lv}), an, bn), '']
    out += ['## Все уровни', '', table(agg(RN, CN, None), agg(RB, CB, None), an, bn), '']
    txt = '\n'.join(out)
    if a.out: Path(a.out).write_text(txt)
    print(txt)


if __name__ == '__main__':
    main()
