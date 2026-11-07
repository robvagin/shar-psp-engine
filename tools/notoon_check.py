#!/usr/bin/env python3
"""№788: сверка списков notoon.txt с level.shr набора — та же подпись, что проверяет движок (main_levels.h): «ntex N sig X»,
FNV-1a по номерам текстур всех кусков (Rec.tex @24). Не совпало — движок список не применит (картинка как без списка).
  python3 tools/notoon_check.py НАБОР [--inputs ФАЙЛ]      (L1 — level.shr и notoon.txt рядом с EBOOT, L2–L7 — в l<N>/)
  python3 tools/notoon_check.py НАБОР --write-inputs ФАЙЛ  записать хеши принятых входов (после осознанной смены экспорта; ФАЙЛ в git — смена видна в диффе)
04.11 (gates): подпись «ntex+FNV по номерам текстур» ловит только расхождение пары level.shr ↔ notoon.txt; пара «старый + старый» проходила, а байт вне номеров текстур (тело списка,
вершины, высоты) не менял подпись вовсе. Теперь рядом ХЕШ ВХОДОВ: sha256 каждого level.shr и notoon.txt L1–L7 против записанных в ФАЙЛ (по умолчанию tools/notoon_inputs.sha256).
Нет файла записи — код 2 «НЕ СВИДЕТЕЛЬ» (не PASS и не FAIL); хеш не совпал — код 1 «вход не тот, что принят»; подпись не сошлась — код 1 (как раньше)."""
import hashlib
import struct, sys
from pathlib import Path

def shr_sig(shr):
    h = open(shr, 'rb').read(104); ntex, npc = struct.unpack_from('<II', h, 4); toff = struct.unpack_from('<I', h, 76)[0]
    f = open(shr, 'rb'); f.seek(toff); tab = f.read(80 * npc); x = 2166136261
    for i in range(npc): x = ((x ^ struct.unpack_from('<I', tab, 80 * i + 24)[0]) * 16777619) & 0xffffffff
    return ntex, x

def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
    return h.hexdigest()

def level_dir(S, L): return S if L == 1 else S / ('l%d' % L)

def inputs_table(S):
    t = {}
    for L in range(1, 8):
        d = level_dir(S, L)
        for fn in ('level.shr', 'notoon.txt'):
            if (d / fn).exists(): t['L%d/%s' % (L, fn)] = sha(d / fn)
    return t

def main():
    S = Path(sys.argv[1]); bad = 0
    opt = lambda n: sys.argv[sys.argv.index(n) + 1] if n in sys.argv else None
    rec = Path(opt('--inputs') or opt('--write-inputs') or Path(__file__).resolve().parent / 'notoon_inputs.sha256')
    if opt('--write-inputs'):
        t = inputs_table(S); rec.write_text(''.join('%s  %s\n' % (v, k) for k, v in t.items())); print('записано %d хешей входов в %s' % (len(t), rec)); return
    for L in range(1, 8):
        d = S if L == 1 else S / ('l%d' % L)
        nt = d / 'notoon.txt'
        if not nt.exists(): print('L%d: нет notoon.txt' % L); continue
        head = nt.read_text().split('\n', 1)[0].split()
        ntex, sig = shr_sig(d / 'level.shr')
        ok = len(head) == 4 and head[0] == 'ntex' and int(head[1]) == ntex and head[2] == 'sig' and int(head[3], 16) == sig
        bad += not ok
        print('L%d: %s (level.shr ntex %d sig %08x; список %s)' % (L, 'совпадает' if ok else 'НЕ СОВПАДАЕТ — список не применится', ntex, sig, ' '.join(head)))
    # хеш входов (04.11): пара, согласная сама с собой, всё равно может быть чужой/старой
    if not rec.exists():
        print('ХЕШ ВХОДОВ: НЕ СВИДЕТЕЛЬ — нет %s (подпись сходится, но свежесть level.shr/notoon.txt не доказана)' % rec); sys.exit(1 if bad else 2)
    want = {l.split('  ', 1)[1]: l.split('  ', 1)[0] for l in rec.read_text().splitlines() if '  ' in l}
    have = inputs_table(S); diff = [k for k in sorted(set(want) | set(have)) if want.get(k) != have.get(k)]
    for k in diff: print('ХЕШ ВХОДОВ: %s — %s (принят %s, в наборе %s)' % (k, 'нет в наборе' if k not in have else 'нет в записи' if k not in want else 'ВХОД ДРУГОЙ', (want.get(k) or '-')[:10], (have.get(k) or '-')[:10]))
    print('ХЕШ ВХОДОВ: %s (%d файлов, запись %s)' % ('совпал' if not diff else 'НЕ СОВПАЛ', len(have), rec.name))
    sys.exit(1 if bad or diff else 0)

if __name__ == '__main__': main()
