#!/usr/bin/env python3
"""Будка-93 / 1226: проверка входа в комнату по smoke.txt. check_door.py smoke.txt new|old ROOM
new: ирис room_in закрыт ДО чтения комнаты (строка DOORIRIS1226 pre-close раньше XITION room_in и ROOM ev=enter, кадры close между ними), комната полная (tex/pieces как в базе), WATCHDOG room 0 отказов.
old (красный контроль): XITION room_in раньше первого кадра IRIS close — комната читалась при стоящем кадре."""
import sys, re
f, mode, room = sys.argv[1], sys.argv[2], sys.argv[3]
L = open(f, errors='replace').read().split('\n')
def idx(pat, start=0):
    for i in range(start, len(L)):
        if re.search(pat, L[i]): return i
    return -1
s = idx(r'^SCENE ' + room + '_door'); e = idx(r'^ROOM ev=enter room=' + room, s)
# pre: прежний путь пишет DOORIRIS1226, машина переходов TFSM close (оба до чтения)
xi = idx(r'^XITION room_in', s); pre = idx(r'^(DOORIRIS1226 pre-close|TFSM room_in close)', s); c0 = idx(r'^IRIS why=room_in phase=close', s)
ok = True
def chk(name, v):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + name)
chk('вход состоялся (ROOM ev=enter)', e > 0)
m = re.search(r'tex=(\d+) pieces=(\d+)', L[e]) if e > 0 else None
chk('комната полная (Kwik: tex=38 pieces=217) ' + str(m.groups() if m else '-'), bool(m) and (room != 'KwikEMart' or m.groups() == ('38', '217')))
if mode == 'new':
    tp = idx(r'^TPLAN room_in', s); chk('план до закрытия экрана (TPLAN room_in раньше DOORIRIS1226 pre-close)', 0 < tp < pre)
    chk('ирис закрыт до чтения (DOORIRIS1226 pre-close < XITION room_in)', 0 < pre < xi)
    chk('кадры ириса close до XITION room_in', 0 < c0 < xi)
else:
    chk('ирис закрыт ПОСЛЕ чтения (XITION room_in < первый кадр close), красный контроль', 0 < xi < c0 and pre < 0)
chk('нет WATCHDOG room fail', idx(r'WATCHDOG room .*fail', s) < 0)
chk('нет ROOM ev=fail', idx(r'^ROOM ev=fail', s) < 0)
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
