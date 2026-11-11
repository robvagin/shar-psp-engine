#!/usr/bin/env python3
"""Сканер «сырых» выделений памяти движка (аудит Fable §5/§7, шаг 13): memalign( / malloc( / calloc( / realloc( с размером >= 16 КБ или неизвестным (выражение с переменными)
вне зонных аллокаторов (zone*.h, zoneslot*, cellpool.h, memwatch.c — обёртки). Ключ места = файл : охватывающая функция : вызов : размер (без номера строки,
чтобы правки рядом базовую линию не ломали). Функции lva* (участок уровня, main_stream.h) тоже считаются аллокатором.
  rawalloc_scan.py [КОРЕНЬ] -> строки "ключ" в stdout (отсортированы); код 0"""
import re, sys
from pathlib import Path

ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
V = ROOT / 'viewer'
SKIP_FILE = re.compile(r'(^|/)(zone[^/]*\.h|zoneslot[^/]*|cellpool\.h|memwatch\.c)$')
SKIP_DIR = re.compile(r'/(build[^/]*|me/vendor|kexc)/')
ALLOC_FN = re.compile(r'^(lva|zone)', re.I)
CALL = re.compile(r'\b(memalign|malloc|calloc|realloc)\s*\(')
FUNC = re.compile(r'^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\([^;]*\)\s*\{?\s*$')
LIMIT = 16 * 1024


def strip(line, in_c):
    out = []; i = 0
    while i < len(line):
        if in_c:
            j = line.find('*/', i)
            if j < 0: return ''.join(out), True
            i = j + 2; in_c = False; continue
        if line.startswith('/*', i): in_c = True; i += 2; continue
        if line.startswith('//', i): break
        out.append(line[i]); i += 1
    return ''.join(out), in_c


def split_args(s):
    args, depth, cur = [], 0, ''
    for ch in s:
        if ch in '([': depth += 1
        elif ch in ')]':
            if depth == 0: break
            depth -= 1
        if ch == ',' and depth == 0: args.append(cur.strip()); cur = ''
        else: cur += ch
    args.append(cur.strip())
    return args


def literal(expr):
    e = re.sub(r'\b(\d+)[uUlL]+\b', r'\1', expr.strip())
    e = re.sub(r'\(\s*(unsigned|int|size_t|long)\s*\)', '', e)
    if re.fullmatch(r'[\d\s\*\+\-<>\(\)xXa-fA-F]+', e) and re.search(r'\d', e):
        try: return int(eval(e, {'__builtins__': {}}))
        except Exception: return None
    return None


def size_of(kind, args):
    if kind == 'memalign': ex = [args[1]] if len(args) > 1 else []
    elif kind == 'malloc': ex = [args[0]]
    elif kind == 'calloc': ex = args[:2]
    else: ex = [args[1]] if len(args) > 1 else []
    vals = [literal(e) for e in ex]
    if ex and all(v is not None for v in vals):
        t = 1
        for v in vals: t *= v
        return t, str(t)
    return None, '*'.join(re.sub(r'\s+', '', e) for e in ex) or '?'


def scan():
    keys = set()
    for p in sorted(list(V.rglob('*.c')) + list(V.rglob('*.h'))):
        rel = str(p.relative_to(ROOT))
        if SKIP_FILE.search(rel) or SKIP_DIR.search('/' + rel): continue
        fn = ''; in_c = False
        for line in p.read_text(errors='replace').split('\n'):
            m = FUNC.match(line)
            if m and not line.startswith((' ', '\t', '#')): fn = m.group(1)
            code, in_c = strip(line, in_c)
            for cm in CALL.finditer(code):
                if ALLOC_FN.match(fn): continue
                kind = cm.group(1); args = split_args(code[cm.end():])
                if not args or args == ['']: continue
                n, sz = size_of(kind, args)
                if n is not None and n < LIMIT: continue
                keys.add(f'{rel}:{fn or "-"}:{kind}:{sz}')
    return sorted(keys)


if __name__ == '__main__':
    print('\n'.join(scan()))
