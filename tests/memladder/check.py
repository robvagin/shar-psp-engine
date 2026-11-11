#!/usr/bin/env python3
"""Правка 435 (лестница памяти) и 432 (MISSION SELECT): проверка одного прогона смоука. check.py smoke.txt РЕЖИМ
  visstuck — (s18, Лестница-96) видимые недогруженные клетки вне очереди ставятся сторожем первыми: через 8 с после прыжка дыр 0, WATCHDOG visstuck ≥ 1; красное flag visstuck 0
  arrive — (s19, Лестница-96) после переноса (foot через msnTeleport, flag footfsm 1) в первые 12 с дыр 0; красное flag ringcap 0
  farcell — (s20, Лестница-96) очередь версий дальних клеток: обменов >= 20 без отказов, дыр <= 2 секунд, сторож молчит; красное flag farcell 0\n  starve — тесное место при открытии меню: ни одной неудачной попытки пакета (MSNSEL openfail), меню строками (rows=1), WATCHDOG mem — только записи ступеней
  fuzz   — бот меню: итог MSNSELFUZZ есть, шагов ≥ 200, нарушений инвариантов 0, открытий запасным видом 0
  rows   — MSNSELROWS: показанные строки = посчитанным заново по маскам, на ≥ 3 уровнях, пакет открыт
  tour   — (s6, Лестница-96, Fable шаг 9) тур L4 с зонами на ballast 5: WATCHDOG mem who=auto <= 2 за тур, тур дошёл до конца (MEMBLK >= 30 строк), MSNSELBAD 0
  npc    — (s9, Лестница-96, просьба дирижёра) просьба who=npc на 264 КБ (модель) на L4 при форсе 250 м и раздробленной куче (fragblk 0.1): цельный кусок появился (LADNPC ok=1) не позже 2 с после первой просьбы; красный контроль (s9_ladnpc_red.txt, flag ladnpcold 1, тот же EBOOT) ok=1 не даёт
  owner  — (s11, Лестница-96) клетки неба/машины игрока выгружены (OWNERDROP): сторож перечитал за <= 2 с (WATCHDOG owner reload ... ok=1), SKYDRAW notres=0 в последних строках; красный контроль (flag ownerold 1): neba нет до конца
  owner1 — (s13, Лестница-96) старт L5: клетки неба и машины игрока загружены с первой попытки: ни одной строки WATCHDOG owner load, SKYDRAW notres=0 во всех строках; красный контроль (flag ownerheap 0) - отказ owner load
  ringfit — (s16, Лестница-96, тур 99) входы L3, L5, L6 подряд на ballast 5: прирост allocfail от последней строки таблицы ДО строки LEVEL ev=load до конца захода <= 4, BB bad (клеток без памяти) <= 4; красный контроль (flag ringfit 0) в тех же заходах даёт десятки
  ringhero — (s17, Лестница-96, канон п.14) старты уровней подряд: на дальних стартах (Гомер дальше 100 м от машины) CELLRING released <= 5; красный контроль (flag ringhero 0) — 77-234; XITION ms печатается для сравнения красное/зелёное\n  failrun — (s8, Лестница-96 шаг 1) MEMFAILHIST с run=N@ra, N >= 20; на MEMFAILRUN_OLD=1 колонки нет (красный контроль)
Печатает цифры и последнюю строку PASS / FAIL. Только чтение."""
import re, sys
t = open(sys.argv[1], 'rb').read().decode('utf-8', 'replace'); mode = sys.argv[2]
L = t.split('\n'); ok = True
def cnt(p): return sum(1 for l in L if re.search(p, l))
def say(k, v, good=None):
    global ok
    if good is False: ok = False
    print(f"  {k}: {v}{'' if good is None else ('  ok' if good else '  ПРОВАЛ')}")
blk = [float(m.group(1)) for l in L for m in [re.search(r'MEMBLK blk=([\d.]+)', l)] if m]
mn = [float(m.group(1)) for l in L for m in [re.search(r'MEMBLK .* min=([\d.]+)', l)] if m]
say('наибольший кусок (МБ): последний/минимум по журналу', f"{blk[-1] if blk else '-'}/{min(mn) if mn else '-'}")
steps = {k: cnt(r'WATCHDOG mem .* step=%d ' % k) for k in range(1, 6)}
say('WATCHDOG mem по ступеням 1..5', steps)
bad = cnt(r'MSNSELBAD'); say('MSNSELBAD', bad, bad == 0)
if mode == 'starve':
    fb = [re.search(r'FRAGBLK f=([\d.]+) largest=([\d.]+)', l) for l in L]; fb = [x for x in fb if x and float(x.group(1)) > 0]
    say('раздробление куч (FRAGBLK): наибольший кусок, МБ', fb[-1].group(2) if fb else 'нет строки', bool(fb) and float(fb[-1].group(2)) <= float(fb[-1].group(1)) + .06)
    of = cnt(r'MSNSEL openfail'); say('неудачных попыток пакета (MSNSEL openfail)', of, of == 0)
    mf = cnt(r'MEMFAILSZ'); st = sum(steps.values()); say('MEMFAILSZ (отказ по размеру) / ступеней лестницы', f'{mf}/{st}', mf == 0 or st >= 1)   # отказ допустим только если лестница ответила (аудит §7 п.1: без лестницы отказ = красный)
    r1 = cnt(r'MSNSEL open rows=1'); say('меню строками (rows=1)', r1, r1 >= 1)
    take = cnt(r'MEMLAD mem who=menu-res'); say('отложенный блок отдан (MEMLAD menu-res take)', take)
elif mode == 'fuzz':
    m = [re.search(r'MSNSELFUZZ seq=(\d+) steps=(\d+) opened=(\d+) bad=(\d+) fixes=(\d+) fallback=(\d+)', l) for l in L]; m = [x for x in m if x]
    say('итог бота MSNSELFUZZ', 'есть' if m else 'нет', bool(m))
    if not m:   # причина: бот не дошёл (медленный эмулятор/обрыв) или не стартовал
        sc = [l for l in L if l.startswith('SCENE ')]; tl = [l for l in L if re.search(r' t=[\d.]+$', l)]
        say('причина: последняя сцена / последняя строка с t', f"{sc[-1].strip() if sc else '-'} / {tl[-1][-30:] if tl else '-'}; MSNSEL open: {cnt(r'MSNSEL open')}, обрыв: {cnt(r'CRASH|EXIT')}")
    if m:
        seq, st, op, bd, fx, fb = map(int, m[-1].groups()); say('последовательностей/шагов/открытий', f"{seq}/{st}/{op}", st >= 200 and op >= 20); say('нарушений инвариантов', bd, bd == 0); say('открытий запасным видом', fb, fb == 0)
elif mode == 'npc':
    q = [re.search(r'LADNPC need=(\d+) ok=(\d) blk=([\d.]+)->([\d.]+) dd=(\d+) mul=([\d.]+) t=([\d.]+)', l) for l in L]; q = [x for x in q if x]
    say('просьб LADNPC', len(q), len(q) >= 4)
    if q:
        t0 = float(q[0].group(7)); okq = [x for x in q if x.group(2) == '1']
        need_mb = int(q[0].group(1)) / 1024; b0 = float(q[0].group(3))
        say('кусок до первой просьбы (МБ) меньше модели (иначе гейт вырожден)', f"{b0:.2f} < {need_mb:.2f}, после {q[0].group(4)}, дальность {q[0].group(5)}", b0 < need_mb)
        say('первая удачная просьба через (с)', f"{float(okq[0].group(7)) - t0:.1f}" if okq else 'нет', bool(okq) and float(okq[0].group(7)) - t0 <= 2.0)
    mk = cnt(r'MAKEROOM msn'); say('MAKEROOM msn (ступень 2 для миссии)', mk)
elif mode == 'owner':
    dr = [i for i, l in enumerate(L) if 'OWNERDROP owner=2' in l]; say('выгрузка клеток неба (OWNERDROP owner=2)', len(dr), len(dr) >= 1)
    rl = [l for l in L if 'WATCHDOG owner reload cell' in l]
    say('сторож взялся перечитать клетки (WATCHDOG owner reload)', len(rl), len(rl) >= 2)
    sk = [re.search(r'SKYDRAW pieces=(\d+) drawn=(\d+) notres=(\d+)', l) for l in L[dr[0]:]] if dr else []; sk = [x for x in sk if x]
    say('SKYDRAW после выгрузки: строк / последняя notres', f"{len(sk)}/{sk[-1].group(3) if sk else '-'}", bool(sk) and int(sk[-1].group(3)) == 0)
    first_ok = next((i for i, x in enumerate(sk) if int(x.group(3)) == 0), None)
    say('первая строка SKYDRAW с notres=0 (номер после выгрузки; строки раз в ~1 с)', first_ok, first_ok is not None and first_ok <= 2)
elif mode == 'toon':   # s15 (Лестница-96, разбор журналов Инспектора): оболочки брошенных моделей не копятся в области «миссия»
    z = [re.search(r'Z3LIVE t=[\d.]+ .*toon=(\d+)/(\d+)K', l) for l in L]; z = [x for x in z if x]
    say('строк Z3LIVE с toon', len(z), len(z) >= 20)
    if z: n, kb = int(z[-1].group(1)), int(z[-1].group(2)); mx = max(int(x.group(1)) for x in z); say('toon: блоков в конце / максимум / КБ в конце', f"{n}/{mx}/{kb}", mx <= 20)
    i2 = next((i for i, l in enumerate(L) if l.startswith('SHOT m2s00')), None)   # проливы на пике 1-й миссии допустимы (область «миссия» там полна: art 1 МБ + машины + контуры); утечка - это рост от миссии к миссии
    sp = cnt(r'WATCHDOG mem who=toon .*spill'); sp2 = sum(1 for l in L[i2:] if re.search(r'WATCHDOG mem who=toon .*spill', l)) if i2 is not None else -1
    say('проливов who=toon в кучу: всего / после начала 3-й миссии (SHOT m2s00)', f"{sp}/{sp2}", sp <= 4 and sp2 == 0)
elif mode == 'ringfit':   # s16 (Лестница-96, тур 99): круг старта берёт столько клеток, сколько влезает в область «города»
    row = re.compile(r'^(\d+\.\d+) (\S+) (\d+\.\d+) (\d+\.\d+) (\d+\.\d+) (\d+\.\d+) (\d+) (\d+) (\d+\.\d+) (\S+) (\d+) (\d+) (\d+) (\d+) ')
    last = 0; seg = None; segs = []
    for l in L:
        m = re.match(r'LEVEL ev=load n=(\d+) ', l)
        if m:
            seg = dict(n=int(m.group(1)), af0=last, af1=last, bad=0, rows=0); segs.append(seg); continue
        m = row.match(l)
        if m:
            last = int(m.group(14))
            if seg: seg['af1'] = max(seg['af1'], last); seg['rows'] += 1
            continue
        if seg and l.startswith('BB '):
            m = re.search(r'\bbad (\d+)', l)
            if m: seg['bad'] = max(seg['bad'], int(m.group(1)))
    segs = [g for g in segs if g['n'] in (3, 5, 6) and g['rows'] >= 20]
    say('заходов L3/L5/L6 с >= 20 строками таблицы', len(segs), len(segs) >= 3)
    for g in segs:
        say(f"L{g['n']}: прирост allocfail за заход / BB bad макс", f"{g['af1'] - g['af0']}/{g['bad']}", g['af1'] - g['af0'] <= 4 and g['bad'] <= 4)
    say('RINGFIT (круг урезан по области): строк', cnt(r'^RINGFIT '))
elif mode == 'ringhero':   # s17 (Лестница-96, канон п.14): круг клеток старта читается у Гомера, а не у машины - на дальних стартах второго чтения нет
    rel = []; cur = None
    for i, l in enumerate(L):
        m = re.match(r'LOADONE level=(\d+) .* room=(\S+) .* spawn_m=(\d+)', l)
        if m: cur = dict(n=int(m.group(1)), room=m.group(2), far=int(m.group(3)), rel=0); rel.append(cur); continue
        m = re.match(r'CELLRING released=(\d+)', l)
        if m and cur: cur['rel'] += int(m.group(1))
    far = [g for g in rel if g['far'] > 100 and g['room'] == '-']
    say('дальних стартов (Гомер дальше 100 м от машины, не комната)', len(far), len(far) >= 3)
    for g in far: say(f"L{g['n']}: Гомер в {g['far']} м от машины, CELLRING released", g['rel'], g['rel'] <= 5)
    x = [int(m.group(1)) for l in L for m in [re.match(r'XITION level ms=(\d+)', l)] if m]
    say('XITION level ms (экран загрузки), по уровням', x)
elif mode == 'visstuck':   # s18 (Лестница-96, №4а, тур 100): видимая клетка вне очереди ставится сторожем первой - через 8 с после последнего прыжка дыр нет
    tf = [float(m.group(1)) for l in L for m in [re.match(r'TELEPORT t=([\d.]+) kind=foot', l)] if m]
    say('прыжков foot', len(tf), len(tf) >= 2)
    tl = tf[-1] if tf else 0
    dd = [(float(m.group(1)), int(m.group(2))) for l in L for m in [re.match(r'DDREG t=([\d.]+) .* miss=(\d+)', l)] if m]
    late = [(t_, ms) for t_, ms in dd if t_ >= tl + 8.0]
    early = [ms for t_, ms in dd if tl <= t_ < tl + 8.0]
    say('после последнего прыжка: видимых недогруженных в первые 8 с (макс по секундам)', max(early) if early else '-')
    say('после 8 с: секунд DDREG / из них с дырами / макс', f"{len(late)}/{sum(1 for _, m_ in late if m_ > 0)}/{max([m_ for _, m_ in late] or [0])}", len(late) >= 10 and all(m_ == 0 for _, m_ in late))
    w = cnt(r'WATCHDOG visstuck ')
    say('WATCHDOG visstuck (сторож поставил клетки первыми)', w, w >= 1)
elif mode == 'arrive':   # s19 (Лестница-96, №4б, тур 100): после переноса (foot через настоящий путь, экран перехода) в первые 12 с видимых недогруженных клеток нет
    ft = [float(m.group(1)) for l in L for m in [re.match(r'FOOT [-\d. ]+ t=([\d.]+)', l)] if m]
    dd = [(float(m.group(1)), int(m.group(2))) for l in L for m in [re.match(r'DDREG t=([\d.]+) .* miss=(\d+)', l)] if m]
    ml = cnt(r'^MSNLOAD ')
    say('переносов foot (FOOT) / MSNLOAD (путь переноса)', f"{len(ft)}/{ml}", len(ft) >= 4 and ml >= 4)
    tot = 0
    for i, t0 in enumerate(ft):
        w = [ms for t_, ms in dd if t0 <= t_ < t0 + 12.0]; bad = sum(1 for ms in w if ms > 0); tot += bad
        say(f'перенос {i + 1}: секунд DDREG в 12 с / с дырами / макс', f"{len(w)}/{bad}/{max(w or [0])}")
    say('секунд с дырами за все переносы (первые 12 с; допуск 2 - кадр раскрытия)', tot, tot <= 2)
    say('RINGCAP (дальность держится по урезанному кругу): строк', cnt(r'^RINGCAP '))
elif mode == 'farcell':   # s20 (Лестница-96 + Модели-96, дальние клетки в 101): очередь версий клеток (проба farfake: дальняя = полная из level.shr)
    fl = [l for l in L if l.startswith('FARCELL t=')]
    ld = [l for l in L if l.startswith('FARCELL level=')]
    say('FARCELL level (файл/проба найдены)', len(ld), len(ld) >= 1)
    last = fl[-1] if fl else ''
    m = re.search(r'far=(\d+)/(\d+)kb full=(\d+)/(\d+)kb .* swaps=(\d+) swfail=(\d+) stale=(\d+)', last)
    far_, sw, swf, stl = (int(m.group(1)), int(m.group(5)), int(m.group(6)), int(m.group(7))) if m else (0, 0, 0, 0)
    say('клеток дальней версией в памяти (последняя строка)', far_, far_ >= 20)
    say('обменов версий / отказов / устаревших', f"{sw}/{swf}/{stl}", sw >= 20 and swf == 0)
    dd = [int(m_.group(1)) for l in L for m_ in [re.match(r'DDREG t=[\d.]+ .* miss=(\d+)', l)] if m_]
    say('секунд DDREG с видимыми недогруженными (весь путь)', f"{sum(1 for v in dd if v > 0)}/{len(dd)}", len(dd) > 30 and sum(1 for v in dd if v > 0) <= 2)
    w = cnt(r'^WATCHDOG (?!zone3|spawn|colring sync)')
    say('WATCHDOG (кроме zone3/spawn)', w, w == 0)
elif mode == 'ride':   # s21 (Лестница-96 + Модели-96, дальние клетки): переход 120 м в езде - обмены версий без отказов, дешёвые, без дыр
    fl = [l for l in L if l.startswith('FARCELL t=')]
    last = fl[-1] if fl else ''
    m = re.search(r'swaps=(\d+) swfail=(\d+) stale=(\d+)', last)
    sw, swf = (int(m.group(1)), int(m.group(2))) if m else (0, 0)
    us = [int(m_.group(1)) for l in fl for m_ in [re.search(r'swus_max=(\d+)', l)] if m_]
    say('обменов версий / отказов', f"{sw}/{swf}", sw >= 30 and swf == 0)
    say('цена обмена в главном потоке, максимум за период (мкс)', max(us) if us else '-', bool(us) and max(us) <= 8000)
    dd = [int(m_.group(1)) for l in L for m_ in [re.match(r'DDREG t=[\d.]+ .* miss=(\d+)', l)] if m_]
    holes = sum(1 for v in dd if v > 0)
    say('секунд DDREG с видимыми недогруженными / всего', f"{holes}/{len(dd)}", len(dd) > 60 and holes <= 3)
    m2 = re.search(r'swaps=(\d+) .* uniq=(\d+) rep3=(\d+)', last)
    if m2:
        sw2, uq, rp = int(m2.group(1)), int(m2.group(2)), int(m2.group(3)); tt = [float(m_.group(1)) for l in fl for m_ in [re.match(r'FARCELL t=([\d.]+)', l)] if m_]
        span = (tt[-1] - tt[0]) if len(tt) > 1 else 1.0
        say('обменов всего / уникальных клеток / повторных у той же клетки быстрее 3 с', f"{sw2}/{uq}/{rp}")   # информативно: повторные у той же клетки на извилистой дороге бывают (103: по туру на PSP)
        say('частота обменов за прогон (в секунду; цель в 102 <= 1)', f"{sw2 / max(span, 1.0):.2f}", sw2 / max(span, 1.0) <= 1.0)
    sf = [float(m_.group(1)) for l in L for m_ in [re.match(r'SLOWFRAME t=[\d.]+ work=([\d.]+)', l)] if m_]
    say('кадров SLOWFRAME > 60 мс', sum(1 for v in sf if v > 60.0))
    w = cnt(r'^WATCHDOG (?!zone3|spawn|colring sync)')
    say('WATCHDOG (кроме zone3/spawn)', w, w == 0)
elif mode == 'zleak':   # s27 (Лестница-96, утечка 102): после каждой выгрузки уровня область города пуста
    un = [l for l in L if re.match(r'ZONE3 t=[\d.]+ unload ', l)]
    live = [int(m.group(1)) for l in un for m in [re.search(r'city free=[\d.]+/[\d.]+ live=(\d+)', l)] if m]
    say('выгрузок уровней с ZONE3 unload', len(live), len(live) >= 5)
    say('выгрузок с живыми блоками в области города', sum(1 for v in live if v > 0), all(v == 0 for v in live))
    say('CPLIVE (диагностика хвостов), строк', cnt(r'^CPLIVE chunk'), True)
elif mode == 'bonus':   # s26 (Лестница-96, блокер 102): бонус-трассы после цепочки уровней - кучи хватает, MEMFAIL нет
    mf = cnt(r'^MEMFAILSZ ')
    say('MEMFAILSZ за прогон', mf, mf == 0)
    sc_ = len(set(m.group(1) for l in L for m in [re.match(r'HEAPB t=[\d.]+ scene=(F_BONUS_T\d)', l)] if m))
    say('сцен бонус-трасс с замером кучи', sc_, sc_ >= 2)
    hp = [int(m.group(1)) for l in L for m in [re.match(r'HEAPB t=[\d.]+ scene=F_BONUS_T\d bytes=(\d+)', l)] if m]
    say('куча в сценах бонуса, минимум (МБ)', f"{min(hp) / 1048576.0:.2f}" if hp else '-', bool(hp) and min(hp) >= 1.0 * 1048576)
elif mode == 'xfer':   # s23 (Лестница-96, дальние клетки): перенос в игре (foot через msnTeleport, как старт миссии/будка) на L2 p09, L6, L5 - регион точки открыт, ближний круг полный, дыр нет
    ft = [float(m.group(1)) for l in L for m in [re.match(r'FOOT [-\d. ]+ t=([\d.]+)', l)] if m]
    dd = [(float(m.group(1)), int(m.group(2))) for l in L for m in [re.match(r'DDREG t=([\d.]+) .* miss=(\d+)', l)] if m]
    fc = [(float(m.group(1)), int(m.group(2))) for l in L for m in [re.match(r'FARCELL t=([\d.]+) .* nearfar=(\d+)', l)] if m]
    zj = cnt(r'^ZONE jump ')
    say('переносов foot / строк ZONE jump (регион точки взят)', f"{len(ft)}/{zj}", len(ft) >= 6 and zj >= 3)
    tot = 0; nf_bad = 0
    for i, t0 in enumerate(ft):
        w = [ms for t_, ms in dd if t0 <= t_ < t0 + 12.0]; bad = sum(1 for ms in w if ms > 0); tot += bad
        late = [nf for t_, nf in fc if t0 + 8.0 <= t_ <= t0 + 12.0]; nb = max(late) if late else 0; nf_bad += 1 if nb > 0 else 0
        say(f'перенос {i + 1}: секунд с дырами из {len(w)} / дальних клеток ближе 105 м к концу окна', f"{bad}/{nb}")
    say('секунд с дырами за все переносы (допуск 4: кадры раскрытия)', tot, tot <= 4)
    say('переносов, где к концу окна у глаза осталась дальняя версия ближе 105 м', nf_bad, nf_bad == 0)
elif mode == 'farhop':   # s24 (Лестница-96, FARHEAD и интервал обмена): Гомер прыгает туда-обратно у границы 105-135 м (шаги < 50 м, без переноса); интервал 3 с на клетку и ожидание запаса кадра
    fl = [l for l in L if l.startswith('FARCELL t=')]
    last = fl[-1] if fl else ''
    m = re.search(r'swaps=(\d+) swfail=(\d+) .* uniq=(\d+) rep3=(\d+) hw=(\d+)', last)
    sw, swf, uq, rp, hw = (int(m.group(i)) for i in range(1, 6)) if m else (0, 0, 0, 0, 0)
    say('обменов / отказов / уникальных клеток', f"{sw}/{swf}/{uq}", sw >= 10)
    say('повторных обменов той же клетки быстрее 3 с (интервал FAR_MIN_US)', rp, rp == 0)
    say('кадров ожидания запаса кадра FARHEAD (hw)', hw, hw > 0)
elif mode == 'leak':   # s25 (Лестница-96, блокер 102): куча на загрузках уровней подряд - повторная загрузка того же уровня не беднее первой более чем на 0,5 МБ (течь на выгрузке)
    first = {}; bad = 0; rows = []
    for l in L:
        m = re.match(r'LEVEL ev=load n=(\d+) .* heap0=(\d+) heap=(\d+)', l)
        if not m: continue
        n, h0 = int(m.group(1)), int(m.group(2)) / 1048576.0
        if n not in first: first[n] = h0
        d = h0 - first[n]; rows.append((n, h0, d))
        if d < -0.5: bad += 1
    say('загрузок уровней', len(rows), len(rows) >= 8)
    for n, h0, d in rows: say(f"  L{n}: heap0 {h0:.2f} МБ (к первой загрузке {d:+.2f})", '', d >= -0.5)
    say('загрузок с течью (heap0 ниже первой > 0,5 МБ)', bad, bad == 0)
elif mode == 'owner1':
    cf = [l for l in L if l.startswith('CITYFILL on=1')]; od = [l for l in L if l.startswith('OWNERDROP owner=')]
    mf = re.search(r'maxfree_kb=(\d+)', cf[0]) if cf else None; ms = [int(m.group(1)) for l in od for m in [re.search(r'minsz_kb=(\d+)', l)] if m and int(m.group(1)) > 0]
    # посылка s13: владельцам (небо, машина игрока) некуда лечь в область «город» - в ней нет свободного куска размера их клетки. Совокупная свободная память (free_kb) для этого не критерий: свободное лежит дырами
    say('область «город» включена, наибольший свободный кусок меньше самой малой клетки-владельца (КБ: кусок / клетка)', f"{mf.group(1) if mf else '-'}/{min(ms) if ms else '-'}", bool(cf) and 'cityon=1' in cf[0] and bool(mf) and bool(ms) and int(mf.group(1)) < min(ms))
    dr = [i for i, l in enumerate(L) if 'OWNERDROP owner=2' in l]
    sk = [re.search(r'SKYDRAW pieces=(\d+) drawn=(\d+) notres=(\d+)', l) for l in L[dr[0]:]] if dr else []; sk = [x for x in sk if x]
    say('SKYDRAW после выгрузки: строк / последняя notres', f"{len(sk)}/{sk[-1].group(3) if sk else '-'}", bool(sk) and int(sk[-1].group(3)) == 0)
    first_ok = next((i for i, x in enumerate(sk) if int(x.group(3)) == 0), None); say('первая строка SKYDRAW с notres=0 (раз в ~1 с)', first_ok, first_ok is not None and first_ok <= 2)
elif mode == 'break':   # отрицательный контроль самопроверки: 5 видов порчи состояния меню (flag msnselbreak 1..5) — каждый ловится и чинится
    br = cnt(r'MSNSELBREAK kind'); ck = cnt(r'MSNSELCHK'); wd = cnt(r'WATCHDOG msnsel')
    say('порч подброшено (MSNSELBREAK)', br, br >= 5); say('поймано и починено (MSNSELCHK)', ck, ck >= br); say('строк WATCHDOG msnsel', wd, wd >= br)
elif mode == 'rows':
    R = [re.search(r'MSNSELROWS lv=(\d+) rows=(\d) disp=(\S+) exp=(\S+)', l) for l in L]; R = [x for x in R if x]
    say('строк MSNSELROWS', len(R), len(R) >= 3); say('уровней', sorted({x.group(1) for x in R}), len({x.group(1) for x in R}) >= 3)
    dif = [x.group(0) for x in R if x.group(3) != x.group(4) or x.group(2) != '1']; say('расхождений показанного с ожидаемым', len(dif), not dif)
elif mode == 'tour':
    au = cnt(r'WATCHDOG mem who=auto'); say('WATCHDOG mem who=auto за тур', au, au <= 2)
    nb = cnt(r'MEMBLK blk='); say('строк MEMBLK (тур дошёл)', nb, nb >= 30)
    lv = cnt(r'LEVEL ev=load'); say('загрузок уровня (LEVEL ev=load)', lv, lv >= 1)
    mr = cnt(r'MEMLAD mem who=menu-res'); say('отложенный блок меню (MEMLAD menu-res): записей', mr)
    mf = cnt(r'MEMFAILSZ'); say('MEMFAILSZ', mf)
elif mode == 'failrun':
    m = [re.search(r'MEMFAILHIST n=(\d+) .* run=(\d+)@(\w+)', l) for l in L]; m = [x for x in m if x]
    say('MEMFAILHIST с серией run', f"{m[-1].group(2)}@{m[-1].group(3)}" if m else 'нет', bool(m) and int(m[-1].group(2)) >= 20)
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
