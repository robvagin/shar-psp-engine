#!/usr/bin/env python3
"""Будка-93 / 1239: ролик при нехватке памяти (flag moviefail N). check_movie.py smoke.txt heal|skip|old
heal (N=1): MOVIE1239 open-fail, retry под прелоадером ok=1, ролик сыгран (MSNFMV played=1 frames>0, err=0). skip (N=3): open-fail, retry ok=0, MOVIE1239 skipped (надпись), игра идёт дальше (END).
old (красный, movie1239old 1): ни прелоадера, ни строки skipped, MSNFMV frames=0 err=-3 молча."""
import sys, re
f, mode = sys.argv[1:3]
L = open(f, errors='replace').read().split('\n')
def cnt(p): return sum(1 for l in L if re.search(p, l))
ok = True
def chk(n, v):
    global ok; ok &= bool(v); print(('PASS ' if v else 'FAIL ') + n)
fail, retry_ok, retry_no, skipped = cnt(r'^MOVIE1239 open-fail'), cnt(r'^MOVIE1239 retry .* ok=1'), cnt(r'^MOVIE1239 retry .* ok=0'), cnt(r'^MOVIE1239 skipped')
played = cnt(r'^MSNFMV name=fmv2 have=1 played=1 frames=[1-9]'); silent = cnt(r'^MSNFMV name=fmv2 .* frames=0 .* err=-3')
fsm = cnt(r'^TFSM movie plan'); steps = cnt(r'^WATCHDOG mem who=movie .* fsm'); simp = cnt(r'^TFSM movie simplify')
print(f'TFSM plan={fsm} FREE_MORE={steps} simplify={simp} open-fail={fail} retry ok={retry_ok} fail={retry_no} skipped={skipped} played={played} silent={silent}')
if fsm and mode in ('heal', 'skip'):
    if mode == 'heal': chk('машина переходов: ступени, load ok=1, ролик сыгран', steps >= 1 and cnt(r'^TFSM movie load a=1') == 1 and played == 1 and simp == 0)
    else: chk('машина переходов: все ступени, SIMPLIFY с WATCHDOG, ролик не сыгран', steps >= 4 and simp == 1 and cnt(r'^WATCHDOG mem who=movie .* step=5 simplify') == 1 and played == 0)
elif mode == 'heal': chk('отказ, прелоадер, вторая попытка сыграла ролик', fail == 1 and retry_ok == 1 and played == 1 and skipped == 0)
elif mode == 'skip': chk('отказ дважды: честная надпись, не тишина (MOVIE1239 skipped)', fail == 1 and retry_no == 1 and skipped == 1)
else: chk('красный: молчаливый пропуск (ни прелоадера, ни надписи)', fail == 0 and skipped == 0 and silent == 1)
chk('END', any(l == 'END' for l in L)); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
