#!/usr/bin/env python3
"""№1111: пакеты с текстами на других языках (французский F, немецкий G, испанский S) -> build/lang/<буква>/<то же имя, что у английского>.
Берёт правила tools/ship_rules.txt (те же команды, что у английских пакетов) и запускает их с LANG_BIBLE=<буква>. Движок (viewer/lang.c) ищет файл lang/<буква>/имя рядом с EBOOT;
нет файла — английский. На PSP: скопировать build/lang/ в PSP/GAME/SHARVIEW/lang/.
  python3 tools/lang_build.py [--lang F,G,S] [--levels 1,2,3] [--only ui.shr,level01.msn] [--force] [--list]
Какие имена зависят от языка — viewer/menu_rules.h mrLangDepends (тот же список здесь, LANG_FILES)."""
import argparse, os, re, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
ROWPACKS = ('menu.shr', 'pmenu.shr')   # пакеты, где движок ищет строки по английской подписи: пары подписей -> rowkeys.txt (tools/lang_rowkeys.py)
LANG_FILES = {'rowkeys.txt', 'prompts.txt', 'ui.shr', 'msnart.shr', 'load.shr', 'menu.shr', 'pmenu.shr', 'pextra.shr', 'fsave.shr', 'psave.shr', 'scrap.shr', 'scrapgal.shr', 'ending.shr', 'shop.shr', 'phone.shr',
              'phonecars.txt', 'wardrobe.shr', 'mini.shr', 'ticket.shr'}


def depends(name):
    b = name.rsplit('/', 1)[-1]
    return b in LANG_FILES or b.endswith('.msn') or b.startswith('cardqr-')


def rules():
    for line in (ROOT / 'tools/ship_rules.txt').read_text(encoding='utf-8').splitlines():
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        p = [x.strip() for x in line.split('|')]
        if len(p) < 4:
            continue
        cmd = p[3].split('   #')[0].strip()
        yield p[0], p[2].split(), cmd


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--lang', default='F,G,S'); ap.add_argument('--levels', default='1')
    ap.add_argument('--only', default=''); ap.add_argument('--force', action='store_true'); ap.add_argument('--list', action='store_true')
    a = ap.parse_args(); levels = [int(x) for x in a.levels.split(',') if x]; only = set(x for x in a.only.split(',') if x)
    todo = []
    for name, deps, cmd in rules():
        if not depends(name) or '{stage}' in cmd:
            continue
        for n in (levels if re.search(r'\{N+\}', name) else [None]):
            if n is None and re.search(r'\{N+\}', name):
                continue
            if n is not None and not re.search(r'\{N+\}', name):
                continue
            nm = name.replace('{NN}', '%02d' % n).replace('{N}', str(n)) if n else name
            if n == 1 and name.startswith('l{N}/'):
                continue                                     # уровень 1 — корень (его пакеты — правила без {N})
            if only and nm.rsplit('/', 1)[-1] not in only and nm not in only:
                continue
            todo.append((nm, n, deps, cmd))
    todo.append(('prompts.txt', None, [], 'python tools/lang_prompts.py --out {out}'))   # вопросы паузы и меню (savemenu.c)
    bad = 0
    for L in a.lang.split(','):
        for nm, n, deps, cmd in todo:
            out = ROOT / 'build/lang' / L / nm
            c = cmd.replace('{NN}', '%02d' % (n or 0)).replace('{N}', str(n or 0)).replace('{outdir}', str(out.parent)).replace('{out}', str(out))
            if a.list:
                print(L, nm, '|', c); continue
            if not a.force and out.exists() and all(not (ROOT / d).exists() or (ROOT / d).stat().st_mtime <= out.stat().st_mtime for d in deps if '{' not in d):
                print('есть', L, nm); continue
            out.parent.mkdir(parents=True, exist_ok=True)
            env = dict(os.environ, LANG_BIBLE=L, LANG='en_US.UTF-8', LC_ALL='en_US.UTF-8')
            if n: env['LEVEL'] = str(n)
            if nm in ROWPACKS: env['MENU_ROWS_OUT'] = str(ROOT / 'build/lang' / L / ('_rows_' + nm + '.json'))
            r = subprocess.run(c, shell=True, cwd=ROOT, env=env, capture_output=True, text=True)
            ok = r.returncode == 0 and (out.exists() or 'outdir' in cmd)
            print('ok  ' if ok else 'ОШИБКА', L, nm); bad += not ok
            if not ok: print('   ', (r.stderr or r.stdout)[-300:].replace('\n', ' '))
    if not a.list:                                     # таблица «подпись на языке -> английская» для строк, которые движок ищет по английской подписи
        for nm, n, deps, cmd in todo:                  # английские подписи тех же экранов (один раз)
            if nm in ROWPACKS and not (ROOT / 'build/lang/E' / ('_rows_' + nm + '.json')).exists():
                out = ROOT / 'build/lang/E' / nm; out.parent.mkdir(parents=True, exist_ok=True)
                env = dict(os.environ, LANG_BIBLE='E', LANG='en_US.UTF-8', LC_ALL='en_US.UTF-8', MENU_ROWS_OUT=str(out.parent / ('_rows_' + nm + '.json')))
                subprocess.run(cmd.replace('{out}', str(out)), shell=True, cwd=ROOT, env=env, capture_output=True, text=True)
        for L in a.lang.split(','):
            rows = []
            for nm in ROWPACKS:
                e, l = ROOT / 'build/lang/E' / ('_rows_' + nm + '.json'), ROOT / 'build/lang' / L / ('_rows_' + nm + '.json')
                if e.exists() and l.exists(): rows += [str(e), str(l)]
            r = subprocess.run([sys.executable, 'tools/lang_rowkeys.py', '--out', str(ROOT / 'build/lang' / L / 'rowkeys.txt'), '--rows'] + rows, cwd=ROOT,
                               env=dict(os.environ, LANG_BIBLE=L, LANG='en_US.UTF-8', LC_ALL='en_US.UTF-8'), capture_output=True, text=True)
            print('rowkeys', L, r.stdout.strip() or r.stderr[-200:])
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
