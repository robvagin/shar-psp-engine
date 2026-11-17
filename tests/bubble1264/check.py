#!/usr/bin/env python3
"""Гейт 1264/1267/1268 (Бабл, 107): читает журналы tests/bubble1264/gate.sh в build/bub1264. Выход 0 — всё как задумано.
 новое (OLD=0) обязано пройти, старое (маска 1/2/4) обязано упасть — иначе гейт ничего не доказывает."""
import re, sys, pathlib
D = pathlib.Path(__file__).resolve().parents[2] / 'build/bub1264'
def rd(n): return (D / f'bub1264_{n}.txt').read_text(errors='replace').splitlines()
def after_dialogue_msg(L):      # текст бабла (MSNICON msg) после этапа-разговора m3.0.3
    i = next(k for k, l in enumerate(L) if l.startswith('MSN stage m3.0.3'))
    for l in L[i:]:
        m = re.match(r'MSNICON .* msg="([^"]*)"', l)
        if m: return m.group(1)
    return None
def stage3_rise(L):             # кадр выхода бабла этапа destroy минус кадр выхода без задержки значка
    i = next(k for k, l in enumerate(L) if l.startswith('MSN stage m3.1.3'))
    for l in L[i:]:
        m = re.match(r'BUBBLE1247 rise fr=(\d+)', l)
        if m: return int(m.group(1))
def stale_after_cleanup(L):     # сколько раз после второго старта (этап 3) снова поставлен текст TALK TO MARGE
    i = [k for k, l in enumerate(L) if l.startswith('MSN mainat m3 stage=3')][0]
    return sum(1 for l in L[i:] if l.startswith('ICON set') and 'TALK_TO_MARGE' in l)
ok = True
def chk(name, cond, val):
    global ok; ok &= bool(cond); print(('OK  ' if cond else 'FAIL'), name, val)
# А: этап-разговор бабл не гасит
chk('A new: бабл остался после этапа-разговора', after_dialogue_msg(rd('0_L1_m3sp0s2')) == 'TALK TO MARGE', after_dialogue_msg(rd('0_L1_m3sp0s2')))
chk('A old(1): красный, бабл погашен', after_dialogue_msg(rd('1_L1_m3sp0s2')) == '', after_dialogue_msg(rd('1_L1_m3sp0s2')))
# Б: значок опаздывает на 2,5 с — бабл не ждёт больше ~0,5 с (15 кадров); база — без задержки
b0, bn, bo = stage3_rise(rd('0_L1_m3s2')), stage3_rise(rd('0_L1_m3ss2avd')), stage3_rise(rd('2_L1_m3ss2avd'))
chk('B new: выход ≤ базы+20 кадров при задержке значка', bn - b0 <= 20, f'база {b0} новое {bn} (+{bn-b0})')
chk('B old(2): красный, ждёт значок ≥ +40 кадров', bo - b0 >= 40, f'старое {bo} (+{bo-b0})')
# В: ждущий текст снимается в cleanup
chk('C new: прежний текст не воскресает', stale_after_cleanup(rd('0_L1_m3sp0s2d')) == 0, stale_after_cleanup(rd('0_L1_m3sp0s2d')))
chk('C old(4): красный, воскресает', stale_after_cleanup(rd('4_L1_m3sp0s2d')) >= 1, stale_after_cleanup(rd('4_L1_m3sp0s2d')))
print('ГЕЙТ', 'ЗЕЛЁНЫЙ' if ok else 'КРАСНЫЙ'); sys.exit(0 if ok else 1)
