#!/usr/bin/env python3
"""№1162/№1166: набор, собранный ПУБЛИЧНЫМ путём (чистая копия репозитория + свой диск + README), против нашего набора.

  tools/public_path_check.py PUBLIC_SET OUR_SET [--table out.md] [--allow tools/public_path_allow.txt]

PUBLIC_SET и OUR_SET: папки с файлами рядом с EBOOT (у нас build/test86tour/PSP/GAME/SHARTEST15, у публичного пути
build/ship_stage или то, что лежит рядом с EBOOT после tools/public_build.sh). Каждый файл нашего набора получает вердикт
  совпадает | отличается | нет вообще   (побайтно, SHA-256)
и причину из списка допусков (--allow): строка «шаблон  вердикт  причина». Допуск нужен для того, что по закону проекта в
публичный путь попасть не может (обложка и значки Роба, служебные файлы тура, выключатели) или собирается другим, равным по
смыслу, способом. Выход 1, если есть файл нашего набора, который в публичном не совпал и не имеет допуска.
Файлы публичного набора, которых у нас нет, печатаются отдельно («лишние»)."""
import argparse, fnmatch, hashlib, sys
from pathlib import Path


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
    return h.hexdigest()


def walk(d):
    d = Path(d)
    return {str(p.relative_to(d)): p for p in sorted(d.rglob('*')) if p.is_file() and not p.name.startswith('.')}


PBP_NAMES = ['PARAM.SFO', 'ICON0.PNG', 'ICON1.PMF', 'PIC0.PNG', 'PIC1.PNG', 'SND0.AT3', 'DATA.PSP', 'DATA.PSAR']


def pbp_parts(path):
    """Разделы EBOOT.PBP: имя -> байты (заголовок: 0x00 'PBP', версия, 8 смещений u32)."""
    import struct
    b = Path(path).read_bytes()
    if b[:4] != b'\0PBP': raise SystemExit(f'{path}: не PBP')
    offs = list(struct.unpack('<8I', b[8:40])) + [len(b)]
    return {PBP_NAMES[i]: b[offs[i]:offs[i + 1]] for i in range(8)}


def pbp_compare(pub, our):
    """EBOOT.PBP публичного пути и нашего: всё, кроме кода (DATA.PSP), побайтно по разделам. Возвращает (строки, число расхождений)."""
    A, B = pbp_parts(pub), pbp_parts(our)
    rows, bad = [], 0
    for k in PBP_NAMES[:6]:
        a, b = A[k], B[k]
        v = 'совпадает' if a == b else ('нет вообще' if (not a and b) else 'отличается')
        if v != 'совпадает' and k != 'PIC0.PNG': bad += 1
        rows.append(f'| EBOOT:{k} | {v} | {len(b)} | {len(a)} |')
    return rows, bad


def load_allow(path):
    rules = []
    if path and Path(path).exists():
        for ln in Path(path).read_text(encoding='utf-8').splitlines():
            ln = ln.split('#', 1)[0].rstrip()
            if not ln.strip(): continue
            pat, verdict, *why = ln.split(None, 2)
            rules.append((pat, verdict, why[0] if why else ''))
    return rules


def allowed(rules, name, verdict):
    for pat, v, why in rules:
        if fnmatch.fnmatch(name, pat) and v in (verdict, 'любой'): return why or 'допуск'
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('public'); ap.add_argument('ours')
    ap.add_argument('--table'); ap.add_argument('--allow')
    ap.add_argument('--eboot', nargs=2, metavar=('PUB_EBOOT', 'OUR_EBOOT'), help='сверить разделы EBOOT.PBP (значок, живой значок, фон, музыка, название), код не сверяется')
    a = ap.parse_args()
    pub, our = walk(a.public), walk(a.ours)
    rules = load_allow(a.allow)
    rows, bad = [], 0
    for n, p in our.items():
        if n not in pub: v = 'нет вообще'
        elif p.stat().st_size == pub[n].stat().st_size and sha(p) == sha(pub[n]): v = 'совпадает'
        else: v = 'отличается'
        why = '' if v == 'совпадает' else (allowed(rules, n, v) or '')
        if v != 'совпадает' and not why: bad += 1
        rows.append((n, v, why, p.stat().st_size, pub[n].stat().st_size if n in pub else 0))
    if a.eboot:
        prow, pbad = pbp_compare(*a.eboot); bad += pbad
        out_pbp = ['', '| раздел EBOOT | вердикт | наш, Б | публичный, Б |', '|---|---|---|---|'] + prow
    else: out_pbp = []
    extra = [n for n in pub if n not in our and not allowed(rules, n, 'лишний')]
    cnt = {}
    for r in rows: cnt[r[1]] = cnt.get(r[1], 0) + 1
    out = ['| файл | вердикт | причина | наш, Б | публичный, Б |', '|---|---|---|---|---|']
    out += [f'| {n} | {v} | {w} | {s1} | {s2} |' for n, v, w, s1, s2 in rows if v != 'совпадает']
    out += out_pbp
    out += ['', 'лишние в публичном (нет у нас): ' + (', '.join(extra) if extra else 'нет')]
    out += ['', 'итого: ' + ', '.join(f'{k} {v}' for k, v in sorted(cnt.items())) + f'; без допуска и не совпало: {bad}']
    text = '\n'.join(out)
    if a.table: Path(a.table).write_text(text + '\n', encoding='utf-8')
    print(text if len(text) < 6000 else text[:6000] + '\n… (полная таблица: --table)')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
