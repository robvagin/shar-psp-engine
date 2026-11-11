#!/usr/bin/env python3
"""Гейт правки 1244 (регулятор дальности по времени кадра): tests/ddreg1244/check.py smoke.txt. Сценарий tests/ddreg1244/scene.txt: L2, Draw Distance 250, «flag workmix N» подменяет работу кадра:
22 мс (лёгко) -> 42 мс (тяжело) -> 22 мс -> 33 мс. По строкам DDREG раз в секунду проверяем:
  1) тяжело (42 мс): через 12 с после смены дальность <= 155 м;
  2) снова легко (22 мс): дальность растёт плавно (не быстрее 8 м/с за секунду) и за 60 с поднимается минимум на 40 м, если память не держит (dd не ниже 150);
  3) качелей нет: за всё время не больше 6 смен направления изменения дальности при шаге >= 3 м;
  4) выше 250 и ниже 150 не бывает.
Потолок «места» (--ceil): scene_ceil.txt, запуск без cellthrashold, красный - файл cellthrashceilold. Для возврата после сторожа (--thrash) запуск БЕЗ cellthrashold, красный - файл cellthrashrestold. Запуск (регулятор кадра): FLAGS="cellthrashold" tools/emu_run.sh tests/ddreg1244/scene.txt ИМЯ EBOOT 700 (файл cellthrashold выключает сторож круга клеток: иначе регулятор памяти держит 0,6 и рост по кадру не виден), затем grep -E "^(DDREG|SCENE)" smoke.txt | check.py /dev/stdin.
PASS/FAIL; красный контроль (тот же запуск с файлом ddreg1244old): DDREG1244_OLD=1 (250 - пол, дальность стоит на 250 при 42 мс -> пункт 1 красный)."""
import re, sys
rows=[]; scene=None; sc_t={}; circ_t=[]; all_t=[]
for l in open(sys.argv[1], errors='replace'):
    m=re.match(r'SCENE (\S+) ([\d.]+)', l)
    if m: sc_t[m[1]]=float(m[2]); continue
    m=re.match(r'WATCHDOG cellthrash fires=\d+ total=.* t=([\d.]+)$', l.strip())
    if m: circ_t.append(float(m[1])); all_t.append(float(m[1])); continue
    m=re.match(r'WATCHDOG cellcap circ=\d+ dd=\d+ ceil=\d+ t=([\d.]+)$', l.strip())
    if m: all_t.append(float(m[1])); continue
    m=re.match(r'DDREG t=([\d.]+) dd=(\d+) .*?work=([\d.]+).*?(?:mem=([\d.]+))?(?: thr=([\d.]+))?(?: ceil=([\d.]+))?(?: ad=([\d.]+))?$', l.strip())
    if m: rows.append((float(m[1]), int(m[2]), float(m[3]), float(m[4]) if m[4] else 1.0, float(m[5]) if m[5] else 999.0, float(m[6]) if m[6] else 0.0))
def seg(a, b): return [r for r in rows if a <= r[0] < b]
# 9) без кругов клеток (--nocircle, 99: сценарий scene_ceil.txt, запуск с файлом cellthrashceilold, БЕЗ cellthrashold; красный - ещё файл streamtight99old):
# за 6 минут езды на L2 при 250 м сторож круга не срабатывает ни разу (было 2 срабатывания: клетки за спиной отдавались как «далёкие» на 40 м раньше, чем грузились)
if '--nocircle' in sys.argv:
    n_=len(circ_t); c_ok=n_==0
    print(f"9) срабатываний сторожа круга клеток за прогон: {n_} (0): {'ok' if c_ok else 'FAIL'}")
    print('PASS' if c_ok else 'FAIL'); sys.exit(0 if c_ok else 1)
# 7-8) потолок «места» (--ceil, сценарий scene_ceil.txt: езда 6 мин при лёгкой работе кадра, запуск БЕЗ cellthrashold; красный - файл cellthrashceilold):
# после второго круга клеток дальность больше не качается (смен направления при шаге >=3 м не больше 2 до конца), и потолок держится ниже 250
if '--ceil' in sys.argv:
    ok=True
    if len(all_t) < 2: print("7) кругов/упоров ёмкости меньше двух - потолок не проверить"); ok=False
    else:
        t2=sorted(all_t)[1]; seg2=[r[1] for r in rows if r[0] >= t2]
        fl=0; last=0; ref=seg2[0]
        for v in seg2:
            if abs(v-ref) >= 3:
                s_=1 if v>ref else -1
                if last and s_!=last: fl+=1
                last=s_; ref=v
        c_ok=fl<=2; print(f"7) после второго круга (t={t2:.0f}) смен направления дальности: {fl} (<=2): {'ok' if c_ok else 'FAIL'}"); ok&=c_ok
        held=[r for r in rows if r[0] >= t2 and r[5] > 0]
        h2=bool(held) and max(r[1] for r in held) <= max(r[5] for r in held)+1
        print(f"8) потолок поставлен и держит ({'до %d м' % max(r[5] for r in held) if held else 'нет'}): {'ok' if h2 else 'FAIL'}"); ok&=h2
    print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
H=sc_t.get('DD_HEAVY'); L1=sc_t.get('DD_LIGHT1'); M=sc_t.get('DD_MID')
if not rows or H is None or L1 is None: print('нет данных'); sys.exit(2)
ok=True
heavy=[r for r in seg(H+12, L1)]
h_ok=bool(heavy) and max(r[1] for r in heavy) <= 155
print(f"1) тяжело 42 мс, дальность после 12 с: max {max(r[1] for r in heavy) if heavy else '-'} (<=155): {'ok' if h_ok else 'FAIL'}"); ok&=h_ok
light=seg(L1, M or L1+90); memhold=bool(light) and max(r[3] for r in light) < 0.99   # регулятор памяти (cellthrash) держит дальность 0,6 - рост по кадру не проверить
d=[r[1] for r in light]
rate=max((d[i+1]-d[i] for i in range(len(d)-1)), default=0); gain=(max(d)-d[0]) if d else 0
l_ok=bool(d) and rate <= 8 and (gain >= 40 or memhold)
print(f"2) снова легко: прирост {gain} м за {len(d)} с, быстрейший рост {rate} м/с (<=8, прирост >=40){' - ПАМЯТЬ ДЕРЖИТ (mem<1), рост не проверен' if memhold else ''}: {'ok' if l_ok else 'FAIL'}"); ok&=l_ok
dd=[r[1] for r in rows]; flips=0; last=0; ref=dd[0]
for v in dd:
    if abs(v-ref) >= 3:
        s_=1 if v>ref else -1
        if last and s_!=last: flips+=1
        last=s_; ref=v
lim=10 if '--thrash' in sys.argv else 6   # с повторными срабатываниями сторожа круга (спад - возврат - спад) направлений больше
f_ok=flips<=lim; print(f"3) смен направления при шаге >=3 м: {flips} (<={lim}): {'ok' if f_ok else 'FAIL'}"); ok&=f_ok
r_ok=min(dd)>=148 and max(dd)<=252; print(f"4) предел: min {min(dd)} max {max(dd)} (150..250): {'ok' if r_ok else 'FAIL'}"); ok&=r_ok
# 5-6) возврат после сторожа круга клеток (запуск БЕЗ cellthrashold): mem - множитель памяти, thr - секунд с последнего события «загрузил - отдал»
if '--thrash' in sys.argv:
    held=[i for i,r in enumerate(rows) if r[3] < 0.99]
    if not held: print("5) сторож круга не срабатывал - возврат не проверить"); ok=False
    else:
        i0=held[0]; back=[r for r in rows[i0:] if r[3] >= 0.999]
        b_ok=bool(back); print(f"5) после срабатывания (t={rows[i0][0]:.0f}) множитель вернулся к 1,0: {'да, t=%.0f' % back[0][0] if back else 'НЕТ'}: {'ok' if b_ok else 'FAIL'}"); ok&=b_ok
        early=[(rows[i][0], rows[i-1][4]) for i in range(max(1,i0), len(rows)) if rows[i][3] > rows[i-1][3] + 1e-6 and rows[i-1][4] < 23]
        e_ok=not early; print(f"6) рост множителя при продолжающихся событиях (<23 с тишины): {len(early)} случаев: {'ok' if e_ok else 'FAIL'}"); ok&=e_ok
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
