#!/usr/bin/env python3
"""1320: вход в помещение в основной части миссии при отказе памяти (flag tffail 5). check.py smoke.txt new|old
new: чистая перезагрузка в миссии, BOOTH1060 msn same=1 (тот же этап), миссия возобновлена (MSN resume1320), игрок в комнате, SIMPLIFY нет, строки XITFAIL нет.
old (flag roomreload1320 0 или rollback.txt ROOMRELOAD1320_OLD): CLEANRELOAD96 skip why=mission, SIMPLIFY, XITFAIL, входа нет (красный контроль: на new-проверке old краснеет)."""
import sys, re
L = open(sys.argv[1], errors='replace').read().split('\n'); mode = sys.argv[2]
tour = len(sys.argv) > 3   # сцена тура F_RR1320_NEW|OLD: загрузка уровня и миссия внутри сцены, окно до следующей SCENE
if tour:
    s = next(i for i, l in enumerate(L) if l.startswith('SCENE ' + sys.argv[3]))
    e = next((i for i in range(s + 1, len(L)) if L[i].startswith('SCENE ')), len(L)); T = L[s:e]; PRE = T
else:
    s = next(i for i, l in enumerate(L) if l.startswith('SCENE KwikEMart_door')); T = L[s:]; PRE = L[:s]
has = lambda p: any(re.search(p, l) for l in T)
ok = True
def chk(n, v):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + n)
chk('миссия идёт, этап основной части (m3.1.1)', any(l.startswith('MSN stage m3.1.1') for l in PRE))
if mode == 'new':
    chk('чистая перезагрузка в миссии (CLEANRELOAD96 ms=)', has(r'^CLEANRELOAD96 ms='))
    chk('прогресс миссии тот же (BOOTH1060 msn same=1)', has(r'^BOOTH1060 msn same=1'))
    chk('миссия возобновлена на этапе (MSN resume1320 phase=1 stage=1)', has(r'^MSN resume1320 .* phase=1 stage=1'))
    chk('игрок в комнате (ROOM ev=enter room=KwikEMart)', has(r'^ROOM ev=enter room=KwikEMart'))
    chk('SIMPLIFY нет', not has(r'^TFSM room_in simplify'))
else:
    chk('CLEANRELOAD96 skip why=mission', has(r'^CLEANRELOAD96 skip why=mission'))
    chk('SIMPLIFY и XITFAIL', has(r'^TFSM room_in simplify') and has(r'^XITFAIL room_in'))
    chk('входа нет', not has(r'^ROOM ev=enter room=KwikEMart'))
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
