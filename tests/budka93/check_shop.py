#!/usr/bin/env python3
"""Будка-93 / 1233 T09: гардероб при отказе памяти (flag shopfail N). check_shop.py smoke.txt heal|msg|old
heal (shopfail 1): SHOPHEAL1233, гардероб открылся (WARDV open), сообщения нет. msg (shopfail 3): SHOPHEAL1233, SHOPFAIL1233 CHANGING ROOM UNAVAILABLE, не открылся. old (красный, shopheal1233old 1): ни ступеней, ни сообщения, не открылся."""
import sys, re
f, mode = sys.argv[1:3]
L = open(f, errors='replace').read().split('\n')
def cnt(p): return sum(1 for l in L if re.search(p, l))
ok = True
def chk(n, v):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + n)
heal, fail, opened = cnt(r'^SHOPHEAL1233'), cnt(r'^SHOPFAIL1233'), cnt(r'^WARDV open')
fsm = cnt(r'^TFSM wardrobe plan'); steps = cnt(r'^WATCHDOG mem who=wardrobe .* fsm'); simp = cnt(r'^TFSM wardrobe simplify')
print(f'SHOPHEAL={heal} SHOPFAIL={fail} WARDV open={opened} TFSM plan={fsm} FREE_MORE={steps} simplify={simp}')
if mode == 'reload':   # Build 96: все ступени отказали (shopfail 5) — чистая перезагрузка вне миссии, гардероб открылся, SIMPLIFY нет
    cr = next((l for l in L if l.startswith('CLEANRELOAD96 ms=')), ''); m = re.search(r'dxz=([\d.]+)', cr); bs = next((l for l in L if l.startswith('BOOTH1060 msn same=')), '')
    chk('WATCHDOG step=5 reload=1', cnt(r'^WATCHDOG mem who=wardrobe .* step=5 reload=1 ') == 1); chk('игрок там же (dxz<0,5): ' + (m.group(1) if m else '-'), bool(m) and float(m.group(1)) < 0.5)
    chk('прогресс те же (msn same=1)', 'same=1' in bs); chk('гардероб открылся, SIMPLIFY нет', opened == 1 and simp == 0)
    print('время перезагрузки, мс:', (re.search(r'ms=(\d+)', cr) or [0, '-'])[1])
elif fsm and mode in ('heal', 'msg'):
    if mode == 'heal': chk('машина переходов: ступень(и) FREE_MORE, load ok=1, гардероб открылся', steps >= 1 and cnt(r'^TFSM wardrobe load a=1') == 1 and opened == 1 and simp == 0)
    else: chk('машина переходов: все ступени, SIMPLIFY с WATCHDOG, не открылся', steps >= 4 and simp == 1 and cnt(r'^WATCHDOG mem who=wardrobe .* step=5 simplify') == 1 and opened == 0)
elif mode == 'heal': chk('ступени и вторая попытка вылечили', heal == 1 and opened == 1 and fail == 0)
elif mode == 'msg': chk('ступени, потом честное сообщение, не открылся', heal == 1 and fail == 1 and opened == 0)
else: chk('красный: тихая отмена (ни ступеней, ни сообщения, не открылся)', heal == 0 and fail == 0 and opened == 0)
chk('END', any(l == 'END' for l in L)); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
