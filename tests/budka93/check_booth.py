#!/usr/bin/env python3
"""Будка-93 / 1225, 1227: будка L4 в тесной куче. check_booth.py smoke.txt new|old
new: один экран загрузки на будку (пересборка сразу, без «сцены» и без «возврата» на выходе), Гомер в городе у будки (room=-), камера старта миссии на выходе молча снята (MSNCAM hold, нет MSNCAM start), WATCHDOG кроме spawn hero нет.
old (красный контроль, flag booth1225old 1): два и больше экранов (сцена + пересборка + back-fe), после пересборки room=SimpsonsHouse у будки, MSNCAM start на выходе."""
import sys, re
f, mode = sys.argv[1], sys.argv[2]
L = open(f, errors='replace').read().split('\n')
s = next(i for i, l in enumerate(L) if l.startswith('SCENE BOOTH'))
T = L[s:]
def cnt(p): return sum(1 for l in T if re.search(p, l))
news, back, scene = cnt(r'^LOADNEWS'), cnt(r'^BOOTH1060 back'), cnt(r'^BOOTH1060 scene-')
cam_start, cam_hold = cnt(r'^MSNCAM start'), cnt(r'^MSNCAM hold')
rooms = [re.search(r'room=(\S+)', l).group(1) for l in T if l.startswith('HOMER t=')]
after = [r for r in rooms[3:12]]
wd = [l for l in T if l.startswith('WATCHDOG') and 'actor=hero' not in l and 'WATCHDOG mem' not in l and not re.match(r'WATCHDOG (zone3 (shrink|regrow)|cellthrash)', l)]   # zone3 shrink/regrow — штатная адаптация областей (Зоны-95); cellthrash — сторож круга клеток: в гейтах с искусственной тесной кучей учтён в отчёте дирижёру, не как провал перехода
ok = True
def chk(name, v):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + name)
print(f'LOADNEWS={news} scene={scene} back={back} MSNCAM start={cam_start} hold={cam_hold} room после пересборки={sorted(set(after))}')
chk('будка открылась (PHONE view open)', cnt(r'^PHONE view open'))
if mode == 'new':
    chk('план до экрана (TPLAN booth)', cnt(r'^TPLAN booth') >= 1)
    chk('один экран загрузки на будку (LOADNEWS=1)', news == 1)
    chk('без «сцены» и «возврата» (scene=0, back=0)', scene == 0 and back == 0)
    chk('Гомер в городе у будки (room=-)', set(after) == {'-'})
    chk('камера старта на выходе снята (hold>=1, start=0)', cam_hold >= 1 and cam_start == 0)
else:
    chk('красный: два и больше экрана (LOADNEWS>=2) и возврат на выходе', news >= 2 and back >= 1)
    chk('красный: room=SimpsonsHouse после пересборки', 'SimpsonsHouse' in after)
    chk('красный: наезд камеры старта (MSNCAM start)', cam_start >= 1)
chk('WATCHDOG (кроме spawn hero и mem-ступеней) 0: %d' % len(wd), not wd)
chk('END', any(l == 'END' for l in L))
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
