#!/usr/bin/env python3
"""Будка-93 / Build 96 (правка Роба 1250): машина игрока из будки не бывает невидимой. check_boothcar.py smoke.txt ok|fail|hide|hideold
ok: PHONE car (модель пришла), WATCHDOG car 0. fail (carfail96 5): ступени WATCHDOG mem who=car 1..3, PHONECAR nomem, своя машина (нет PHONE car lisa_v). hide (carhide96 1): WATCHDOG car invisible >=2 и WATCHDOG car revert. hideold (carvis96 0, красный): CARMISS96 есть, revert нет."""
import sys, re
f, mode = sys.argv[1:3]
L = open(f, errors='replace').read().split('\n')
c = lambda p: sum(1 for l in L if re.search(p, l))
ok = True
def chk(n, v, i=''):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + n + (' ' + str(i) if i != '' else ''))
if mode == 'ok': chk('машина вызвана (PHONE car)', c(r'^PHONE car ') == 1); chk('WATCHDOG car 0', c(r'^WATCHDOG car ') == 0 and c(r'who=car .* carvis') == 0)
if mode == 'fail':
    chk('ступени 1..3 лестницы (who=car carvis)', c(r'who=car .* step=[123] .* carvis') == 3); chk('PHONECAR nomem', c(r'^PHONECAR nomem') == 1); chk('невидимой нет: машина уровня вместо (PHONE car нет)', c(r'^PHONE car ') == 0)
if mode == 'hide': chk('WATCHDOG car invisible >=2', c(r'^WATCHDOG car invisible') >= 2, c(r'^WATCHDOG car invisible')); chk('WATCHDOG car revert (своя машина уровня)', c(r'^WATCHDOG car revert') == 1)
if mode == 'hideold': chk('красный: CARMISS96 есть, revert нет', c(r'^CARMISS96') >= 1 and c(r'^WATCHDOG car revert') == 0, c(r'^CARMISS96'))
if mode == 'hard':   # решение Роба: машина выбранная, без надписи и подмены
    chk('ступени 1..3 + 4 (world-free) в журнале', c(r'who=car .* step=[123] .* carvis') >= 3 and c(r'who=car .* step=4 world-free') == 1)
    chk('игрок в ВЫБРАННОЙ машине (PHONE car, не подмена)', c(r'^PHONE car ') == 1); chk('подмены нет (PHONECAR nomem 0, fallback 0)', c(r'^PHONECAR nomem') == 0 and c(r'^WATCHDOG car fallback') == 0)
if mode == 'hardimp': chk('невозможный случай: WATCHDOG car fallback, PHONECAR nomem, без PHONE car', c(r'^WATCHDOG car fallback') == 1 and c(r'^PHONECAR nomem') == 1 and c(r'^PHONE car ') == 0)
if mode == 'hardold': chk('красный: прежнее — PHONECAR nomem, PHONE car нет', c(r'^PHONECAR nomem') == 1 and c(r'^PHONE car ') == 0 and c(r'world-free') == 0)
chk('END', any(l == 'END' for l in L)); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
