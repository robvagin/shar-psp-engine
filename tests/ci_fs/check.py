#!/usr/bin/env python3
"""Проверка tools/ci_fs.py (поиск файла без учёта регистра для Linux/WSL). Запуск: python3 tests/ci_fs/check.py
1) resolve на подставном каталоге: настоящее имя находится по любому регистру; точное имя главнее; два имени, отличающихся только регистром, — None
2) красный контроль: поиск «только точное имя» на тех же данных ничего не находит (проверка способна упасть)
3) на файловой системе с учётом регистра (Linux, WSL, том macOS Case-sensitive): настоящий open/os.stat/os.listdir до install падают, после находят;
   запись нового файла не перенаправляется на старое имя. На macOS по умолчанию шаг пропускается (пишет «пропуск»), шаги 1 и 2 идут всегда."""
import os, sys, tempfile, shutil
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
import ci_fs

FAKE = {'/': ['game', 'Mixed'], '/game': ['art', 'sound'], '/game/art': ['frontend', 'chars'], '/game/art/frontend': ['tvscreen.p3d', 'Two', 'two'],
        '/game/sound': ['music'], '/game/sound/music': ['sunday_drive_intro.rsd'], '/Mixed': ['Keep.TXT']}
def ls(d):
    d = os.path.normpath(d)
    if d in FAKE: return list(FAKE[d])
    raise FileNotFoundError(d)
def exact_only(path):
    cur = '/'
    for part in [x for x in path.split('/') if x]:
        if part not in FAKE.get(cur.rstrip('/') or '/', []): return None
        cur = (cur.rstrip('/') + '/' + part)
    return cur
ok = True
def check(name, cond, info=''):
    global ok
    print(('OK   ' if cond else 'FAIL ') + name + (' — ' + info if info else '')); ok &= bool(cond)

r = ci_fs.resolve('/game/art/frontend/TVscreen.p3d', listdir=ls)
check('resolve находит имя в другом регистре', r == '/game/art/frontend/tvscreen.p3d', r)
r = ci_fs.resolve('/GAME/Sound/MUSIC/Sunday_Drive_Intro.rsd', listdir=ls)
check('resolve по всем частям пути', r == '/game/sound/music/sunday_drive_intro.rsd', r)
check('точное имя главнее', ci_fs.resolve('/game/art/frontend/two', listdir=ls) == '/game/art/frontend/two')
check('два имени, отличающихся регистром, — None', ci_fs.resolve('/game/art/frontend/TWO', listdir=ls) is None)
check('нет файла — None', ci_fs.resolve('/game/art/frontend/none.p3d', listdir=ls) is None)
check('«.» и «..» не трогаются', ci_fs.resolve('/game/art/../Sound', listdir=ls) == '/game/art/../sound')
check('красный контроль: только точное имя ничего не находит', exact_only('/game/art/frontend/TVscreen.p3d') is None and exact_only('/game/art/frontend/tvscreen.p3d') is not None)

td = tempfile.mkdtemp(prefix='cifs_')
try:
    os.makedirs(os.path.join(td, 'game', 'art')); fp = os.path.join(td, 'game', 'art', 'tvscreen.p3d'); open(fp, 'wb').write(b'x')
    if ci_fs.case_sensitive_fs(td + '/game'):
        want = os.path.join(td, 'Game', 'Art', 'TVscreen.P3D')
        before = os.path.exists(want)
        try: open(want, 'rb'); op0 = True
        except FileNotFoundError: op0 = False
        ci_fs.install(force=True)
        try:
            after = os.path.exists(want); data = open(want, 'rb').read(); names = os.listdir(os.path.join(td, 'GAME'))
            st = os.stat(want).st_size
            w = os.path.join(td, 'game', 'art', 'NEW.bin'); open(w, 'wb').write(b'y')
            check('до install точного имени нет (open и exists падают)', not before and not op0)
            check('после install exists/open/stat/listdir находят', after and data == b'x' and st == 1 and 'art' in names)
            check('запись нового файла не перенаправлена', os.path.exists(w) and 'NEW.bin' in os.listdir(os.path.dirname(w)))
        finally:
            ci_fs.uninstall()
    else:
        print('пропуск: файловая система без учёта регистра (шаг 3 идёт на Linux, WSL или томе Case-sensitive)')
finally:
    shutil.rmtree(td, ignore_errors=True)
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
