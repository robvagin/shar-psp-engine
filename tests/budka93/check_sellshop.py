#!/usr/bin/env python3
"""Build 95 П2: класс SHOP (лавка продавца). check_sellshop.py smoke.txt heal|msg|old
heal (shopfail 1): TFSM shop load ok=1 после ступени(ей), SHOPV open. msg (shopfail 5): все ступени, SIMPLIFY + WATCHDOG step=5 simplify, лавка не открылась. old (красный): ни TFSM, ни надписи, SHOPV nopack."""
import sys, re
f, mode = sys.argv[1:3]
L = open(f, errors='replace').read().split('\n')
def cnt(p): return sum(1 for l in L if re.search(p, l))
steps, opened, nopack, simp = cnt(r'^WATCHDOG mem who=shop .* fsm'), cnt(r'^SHOPV open'), cnt(r'^SHOPV nopack'), cnt(r'^TFSM shop simplify')
first = next((l for l in L if l.startswith('TFSM shop load')), '')   # первая попытка (Гил повторяет △, следующие открытия уже без отказа)
fi = next((i for i, l in enumerate(L) if l.startswith('TFSM shop load')), len(L)); steps1 = sum(1 for l in L[:fi] if re.search(r'^WATCHDOG mem who=shop .* step=[1-4] .* fsm', l))
print(f'первая попытка: ступеней={steps1} load: {first[:24]}; FREE_MORE={steps} SHOPV open={opened} nopack={nopack} simplify={simp}')
ok = True
def chk(n, v):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + n)
if mode == 'heal': chk('ступени, load ok=1, open', steps1 >= 1 and first.startswith('TFSM shop load a=1'))
elif mode == 'msg': chk('все ступени, SIMPLIFY с WATCHDOG, не открылась', steps1 == 4 and first.startswith('TFSM shop load a=0') and simp >= 1 and cnt(r'^WATCHDOG mem who=shop .* step=5 simplify') >= 1)
else: chk('красный: прежний путь, машины нет (ни TFSM, ни WATCHDOG fsm)', cnt(r'^TFSM shop') == 0 and steps == 0)
chk('END', any(l == 'END' for l in L)); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
