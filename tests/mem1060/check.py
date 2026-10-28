#!/usr/bin/env python3
"""№1060 проверка журнала одного прогона: check.py smoke.txt РЕЖИМ L.  РЕЖИМ: new — будка открылась и после неё всё на месте; red — выключатель (flag mem1060 0), обязан дойти до «unavailable»;
normal — куча не тесная, четвёртая ступень не включалась (цена ноль). Выход 0 — режим выполнен"""
import re, sys
import os
f, mode, L = sys.argv[1], sys.argv[2], int(sys.argv[3])
T = open(f, errors='replace').read().splitlines()
def has(p): return any(re.search(p, l) for l in T)
def first(p):
    for l in T:
        m = re.search(p, l)
        if m: return m
    return None
ok = True
def need(c, msg):
    global ok
    print(('  ok   ' if c else '  FAIL ') + msg); ok &= bool(c)
if mode != 'red': need(has(r'^END'), 'прогон дошёл до END')   # красный: после «unavailable» на выжатой куче (1,5 МБ) старый код может встать — доказательство уже в журнале
opened = has(r'^PHONE view open'); fail = has(r'^PHONE open-fail')
if mode == 'red':
    need(has(r'^BOOTH1051 result ok=0'), 'три ступени №1051 не дали места')
    need(fail and not opened, '«TELEPHONE BOOTH UNAVAILABLE» (PHONE open-fail), будка не открылась')
    need(not has(r'^BOOTH1060'), 'четвёртой ступени нет (выключатель)')
elif mode == 'normal':
    need(opened and not fail, 'будка открылась')
    need(not has(r'^BOOTH1060') and not has(r'^MEM1060 (all|pre)'), 'ни сцены, ни перехода, ни выгрузки всех клеток: цена ноль')
else:
    need(opened and not fail, 'будка открылась, «unavailable» нет')
    need(has(r'^BOOTH1060 scene(-\w+)? '), 'сцена: газета, клетки и пакет про запас в кучу')
    need(has(r'^BOOTH1060 rebuild '), 'пересборка кучи (выгрузить и загрузить уровень обратно)')
    need(has(r'^BOOTH1060 result ok=1'), 'итог ступени: ok=1')
    # переход виден всегда: либо газета, либо шнур без кучи (кадры считает движок), и кадр перехода — окном: чёрный фон, на нём шнур с искрой (не сплошной чёрный)
    mt = first(r'^TRANS1060 scene frames=(\d+) bar=(\d+)')
    if has(r'^BOOTH1060 scene(-fe)? '): need(True, 'переход: газета или LoadingFE открылись в сцене')
    else:
        need(mt and int(mt.group(1)) >= 30 and int(mt.group(2)) >= 500, f'переход без кучи: кадров {mt.group(1) if mt else 0} (не меньше 30), шкала дошла до {mt.group(2) if mt else 0} из 1000')
        shot = os.path.join(os.path.dirname(f), 'trans1060.png')
        try:
            from PIL import Image
            im = Image.open(shot).convert('RGB'); px = im.getdata(); n = len(px); lit = sum(1 for p_ in px if max(p_) > 90)
            need(0.001 <= lit / n <= 0.08, f'кадр перехода окном: светлых точек {100.0 * lit / n:.2f} % (шнур и искра на чёрном; сплошной чёрный — провал)')
        except Exception as e: need(False, f'кадр перехода {shot}: {e}')
    mm = first(r'^BOOTH1060 msn same=(\d)')
    need(mm and mm.group(1) == '1', 'миссия: прогресс уровня, этапа и пройденных миссий после пересборки тот же (строка BOOTH1060 msn)')
    need(has(r'^BOOTH1060 back(-fe)? '), 'возврат освобождённого под газетой после будки')
    m1 = first(r'^MEM1060 stat tag=1 cells=(\d+) free=([\d.]+)'); m2 = first(r'^MEM1060 stat tag=2 cells=(\d+) free=([\d.]+)')
    if m1 and m2:
        c1, f1, c2, f2 = int(m1.group(1)), float(m1.group(2)), int(m2.group(1)), float(m2.group(2))
        need(c2 >= 0.8 * c1, f'клетки на месте: до будки {c1}, после {c2} (не меньше 80 %)')
        need(f2 >= f1 - 2.0, f'утечки нет: свободно до {f1:.2f} МБ, после {f2:.2f} МБ (не меньше чем до минус 2,0)')
    else: need(False, 'строки MEM1060 stat tag=1/2')
    need(has(r'^PHONECAR cDuff ok=1') or has(r'^PHONE car cDuff'), 'выбранная машина приехала (cDuff из пакета уровня)')
    need(not has(r'open-fail'), 'после будки отказов нет')
# №1060б: переход — LoadingFE (сверка с кадром выхода в меню), шнур только запасной. FE1060=1 (accept60) — проверка; REF1060 — эталон ref1060.png (flag mem1060fedemo)
if mode == 'new' and os.environ.get('FE1060') == '1':
    fe = first(r'^TRANS1060 (?:rebuild-fe|scene-fe) frames=(\d+)')
    need(fe and int(fe.group(1)) >= 20, f'№1060б переход LoadingFE: кадров {fe.group(1) if fe else 0} (не меньше 20), строка TRANS1060 rebuild-fe/scene-fe')
    cord = first(r'^TRANS1060 scene frames=(\d+)')
    need((not cord) or int(cord.group(1)) <= 60, f'шнур — только запасной: кадров шнура {cord.group(1) if cord else 0} (не больше 60)')
    need(has(r'^BOOTH1060 back-fe'), 'возврат клеток тоже под LoadingFE (back-fe)')
    shot = os.path.join(os.path.dirname(f), 'trans1060_fe.png'); ref = os.environ.get('REF1060', '')
    try:
        from PIL import Image, ImageChops
        a_ = Image.open(shot).convert('L').resize((96, 54)); b_ = Image.open(ref).convert('L').resize((96, 54))
        pa = list(a_.getdata()); pb = list(b_.getdata()); n = len(pa); ma = sum(pa) / n; mb = sum(pb) / n
        cov = sum((x - ma) * (y - mb) for x, y in zip(pa, pb)); va = sum((x - ma) ** 2 for x in pa) ** .5; vb = sum((y - mb) ** 2 for y in pb) ** .5
        r = cov / (va * vb) if va * vb else 0.; lit = sum(1 for x in pa if x > 40) / n
        need(r >= 0.85 and lit >= 0.03, f'кадр перехода (trans1060_fe) = LoadingFE: корреляция с эталоном выхода в меню {r:.2f} (не меньше 0,85), светлых {100*lit:.1f} % (не меньше 3 %; шнур даёт 0,5 %)')
    except Exception as e: need(False, f'сравнение с эталоном LoadingFE: {e} ({shot} / {ref})')
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
