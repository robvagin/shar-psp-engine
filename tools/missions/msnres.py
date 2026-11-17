#!/usr/bin/env python3
"""Список ресурсов миссий уровня N в порядке чтения -> msnres-l<N>.shr (формат MRS1) и отчёт (запекание З9, 13.11.2026, окно «Запекание»).

  python tools/missions/msnres.py --level 1 --data DIR [--out DIR/msnres-l1.shr] [--report out.txt]
  python tools/missions/msnres.py --all --data DIR

DIR — набор игры (EBOOT-папка): уровень 1 лежит в корне, уровни 2–7 в l<N>/ (msnart.shr, msncars-l<N>.shr), персонажи в chars/.

Что считает. Компилятор миссий (compile.py) уже разобрал скрипты: фазы (вступление mXsd, основная mX), команды загрузки фазы
(LoadP3DFile, LoadDisposableCar), этапы, на этапах значки (SetHUDIcon), машины (AddStageVehicle), персонажи (AddNPC), картинка карточки
(SetPresentationBitmap). Здесь они выстраиваются в порядок чтения (под карточкой, как у оригинала: загрузка миссии целиком до старта),
у каждой записи берутся ТОЧНЫЕ размеры из файлов набора. Движок читает это как таблицу и не ищет по скрипту сам; ничего не вычисляет в игре.

Формат MRS1 (little-endian). Заголовок 32 Б: 'MRS1', u32 версия 1, u32 уровень, u32 миссий, u32 записей всего, u32 смещение записей,
u32 хеш набора (FNV-1a по размерам и именам использованных файлов: не сошёлся — движок не доверяет списку), u32 pad.
Миссия 40 Б: id[8] ('m1', 'bm1'), u32 первая запись, u32 число записей, u32 art_bytes, u32 car_bytes, u32 chr_bytes, u32 peak_bytes
(art + самая большая сумма машин одного этапа + chr), u32 pad[2].
Запись 96 Б: u8 kind (1 art p3d из msnart.shr, 2 модель машины из msncars, 3 персонаж chars/<имя>.chr, 4 картинка карточки (art)),
u8 phase (0 вступление sd, 1 основная), u8 stage (0xFF — команда загрузки фазы), u8 flags (1 — в наборе нет: размер 0; 2 — читает сам движок, не скрипт), u32 bytes, key[88] (путь/имя).
Миссия 'L' (всегда первая) — уровневая: LoadP3DFile из level.mfk и leveli.mfk и ресурсы, которые движок берёт сам (машина у локатора rocke на L1, полиция).
"""
import argparse, json, os, re, struct, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
import compile as mc   # noqa: E402

K_ART, K_CAR, K_CHR, K_PIC = 1, 2, 3, 4
STAGE_LOAD = 0xFF

def norm(p):
    return str(p).replace('\\', '/').lower()

def read_msnart(path):
    """таблица msnart.shr: {путь: размер записи}; нет файла — пусто"""
    if not path.exists():
        return {}
    f = path.read_bytes()
    magic, n, off, _ = struct.unpack_from('<4sIII', f, 0)
    if magic != b'MSA2':
        return {}
    t = {}
    for i in range(n):
        e = f[off + i * 96: off + (i + 1) * 96]
        t[e[:80].split(b'\0')[0].decode()] = struct.unpack_from('<II', e, 80)[1]
    return t

def read_msncars(path):
    """каталог msncars-l<N>.shr: {имя (нижний регистр): размер блока}"""
    if not path.exists():
        return {}
    f = path.read_bytes()
    h = struct.unpack_from('<4sIIIIIII', f, 0)
    if h[0] != b'MCR1':
        return {}
    t = {}
    for i in range(h[3]):
        e = f[h[4] + i * 176: h[4] + (i + 1) * 176]
        t[e[:16].split(b'\0')[0].decode().lower()] = struct.unpack_from('<I', e, 144 + 4)[0]
    return t

def fnv(h, b):
    for c in b:
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return h

def read_briefcards():
    """viewer/mission/briefcards_data.h: NPC-даватель бонусной миссии -> картинка карточки (правило движка msn_game.c, правка 423)"""
    t = (ROOT / 'viewer/mission/briefcards_data.h').read_text()
    return {n.lower(): p.lower() for n, p in re.findall(r'\{"(\w+)",\s*"([^"]+)"\}', t)}

def build(level, data):
    lvdir = data if level == 1 else data / f'l{level}'
    art = read_msnart(lvdir / 'msnart.shr')
    cars = read_msncars(lvdir / f'msncars-l{level}.shr')
    stem_icon = {}   # имя значка -> путь записи msnart (папка msnicons)
    for p in art:
        if '/msnicons/' in p:
            stem_icon.setdefault(Path(p).stem.lower(), p)
    cards = read_briefcards()
    c = mc.Compiler(level); c.compile()
    with tempfile.TemporaryDirectory() as td:
        jp = Path(td) / 'l.json'; mc.dump_json(c.pack, jp); d = json.loads(jp.read_text())
    missions = []; entries = []; used = []
    # уровневая запись 'L': команды уровня (level.mfk, leveli.mfk) + то, что движок читает сам (машина-«ParkedCar» уровня из worldcars.h, погоня CreateChaseManager, терра для стрелок)
    lvl = []; bonus_npc = {}
    for fn in ('level.mfk', 'leveli.mfk'):
        pth = mc.GAME / f'scripts/missions/level{level:02d}' / fn
        if not pth.exists():
            continue
        calls, _ = mc.mfk.parse_file(pth, fn)
        for cl in calls:
            a = cl.args; op = cl.op
            if op == 'LoadP3DFile' and a:
                p = norm(a[0])
                if p in art: lvl.append((K_ART, p, art[p], 0))
            elif op == 'LoadDisposableCar' and len(a) > 1:
                n = str(a[1]).lower()
                if n in cars: lvl.append((K_CAR, n, cars[n], 0))
            elif op == 'CreateChaseManager' and a:
                n = str(a[0]).lower()
                if n in cars: lvl.append((K_CAR, n, cars[n], 2))
            elif op in ('AddAmbientCharacter', 'AddNPCCharacterBonusMission', 'AddPurchaseCarReward') and a:
                n = str(a[0]).lower(); f = data / 'chars' / (n + '.chr')
                if f.exists(): lvl.append((K_CHR, n, f.stat().st_size, 0 if op == 'AddNPCCharacterBonusMission' else 2))
            if op == 'AddNPCCharacterBonusMission' and len(a) > 3:
                bonus_npc[str(a[3]).lower()] = str(a[0]).lower()   # миссия -> NPC-даватель (картинка карточки бонусной миссии)
    tp = f'art/l{level}_terra.p3d'   # движок просит сам (msn_view.c, стрелки на дороге)
    if tp in art: lvl.append((K_ART, tp, art[tp], 2))
    wc = (ROOT / 'viewer/worldcars.h').read_text()
    for lv_, n in re.findall(r'\{(\d),\s*"(\w+)"', wc):   # WORLD_CAR_LIST: по одной машине у локатора на уровне
        n = n.lower()
        if int(lv_) == level and n in cars: lvl.append((K_CAR, n, cars[n], 2))
    seen_l = set(); lvl2 = []
    for e in lvl:
        if (e[0], e[1]) not in seen_l: seen_l.add((e[0], e[1])); lvl2.append(e)
    lvl = lvl2
    first = len(entries)
    for kind, key, size, fl in lvl:
        entries.append((kind, 0, STAGE_LOAD, fl, size, key)); used.append((kind, key, size))
    ab = sum(e[4] for e in entries if e[0] == K_ART); cb = sum(e[4] for e in entries if e[0] == K_CAR)
    missions.append(('L', first, len(entries) - first, ab, cb, 0, ab + cb))
    for m in d['missions']:
        first = len(entries); seen = set(); stage_cars = []
        def add(kind, phase, stage, key, size, miss):
            k = (kind, key.lower())
            if k in seen:
                return
            seen.add(k); entries.append((kind, phase, stage, 1 if miss else 0, size, key)); used.append((kind, key.lower(), size))
        for pi, ph in enumerate(m['phases']):
            for cmd in ph['setup']:
                if cmd['op'] == 'StreetRacePropsLoad' and cmd['args']:   # реквизит уличной гонки: art/<файл>.p3d
                    p = 'art/' + norm(cmd['args'][0]).rstrip(';:')
                    if p in art: add(K_ART, pi, STAGE_LOAD, p, art[p], 0)
            for cmd in ph['load']:
                a = cmd['args']
                if cmd['op'] == 'LoadP3DFile' and a:
                    p = norm(a[0])
                    if p in art:
                        add(K_ART, pi, STAGE_LOAD, p, art[p], 0)
                elif cmd['op'] == 'LoadDisposableCar' and len(a) > 1:
                    n = str(a[1]).lower()
                    add(K_CAR, pi, STAGE_LOAD, n, cars.get(n, 0), n not in cars)
            for si, st in enumerate(ph['stages']):
                sc = []
                for cmd in st['cmds']:
                    a = cmd['args']; op = cmd['op']
                    if op == 'SetHUDIcon' and a and str(a[0]):
                        p = stem_icon.get(str(a[0]).lower())
                        if p:
                            add(K_ART, pi, si, p, art[p], 0)
                    elif op == 'AddStageVehicle' and a:
                        n = str(a[0]).lower(); add(K_CAR, pi, si, n, cars.get(n, 0), n not in cars); sc.append(cars.get(n, 0))
                        dr = str(a[4]).lower() if len(a) > 4 else ''   # водитель машины (5-й аргумент) — персонаж chars/<имя>.chr
                        f = data / 'chars' / (dr + '.chr')
                        if dr and dr not in ('null', 'none') and f.exists(): add(K_CHR, pi, si, dr, f.stat().st_size, 0)
                    elif op == 'AddNPC' and a:
                        n = str(a[0]).lower(); f = data / 'chars' / (n + '.chr')
                        add(K_CHR, pi, si, n, f.stat().st_size if f.exists() else 0, not f.exists())
                    elif op == 'SetPresentationBitmap' and a:
                        p = norm(a[0]); add(K_PIC, pi, si, p, art.get(p, 0), p not in art)
                stage_cars.append(sum(sc))
        if (K_PIC, 'x') not in seen and not any(e[0] == K_PIC for e in entries[first:]):   # бонусная миссия без картинки в скрипте: карточка по даватель-NPC (msn_game.c, 423)
            npc = bonus_npc.get(m['id'].lower(), '')
            pic = cards.get(npc[2:] if npc.startswith('b_') else npc)
            if pic and pic in art: add(K_PIC, 0, 0, pic, art[pic], 0)
        es = entries[first:]
        ab = sum(e[4] for e in es if e[0] in (K_ART, K_PIC)); cb = sum(e[4] for e in es if e[0] == K_CAR); hb = sum(e[4] for e in es if e[0] == K_CHR)
        missions.append((m['id'], first, len(es), ab, cb, hb, ab + max(stage_cars or [0]) + hb))
    h = 2166136261
    for kind, key, size in sorted(used):
        h = fnv(h, f'{kind}:{key}:{size}'.encode())
    return missions, entries, h

def pack(level, missions, entries, h):
    off = 32 + 40 * len(missions)
    out = struct.pack('<4sIIIIIII', b'MRS1', 1, level, len(missions), len(entries), off, h, 0)
    for mid, first, n, ab, cb, hb, pk in missions:
        out += struct.pack('<8sIIIIII8x', mid.encode()[:7], first, n, ab, cb, hb, pk)
    for kind, phase, stage, miss, size, key in entries:
        out += struct.pack('<BBBBI88s', kind, phase, stage, miss, size, key.encode()[:87])
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--level', type=int, action='append'); ap.add_argument('--all', action='store_true')
    ap.add_argument('--data', required=True); ap.add_argument('--out', default=None); ap.add_argument('--report', default=None)
    a = ap.parse_args()
    data = Path(a.data); levels = list(range(1, 8)) if a.all else (a.level or [1])
    rep = []
    for lv in levels:
        missions, entries, h = build(lv, data)
        blob = pack(lv, missions, entries, h)
        out = Path(a.out) if a.out and len(levels) == 1 else (data if lv == 1 else data / f'l{lv}') / f'msnres-l{lv}.shr'
        out.write_bytes(blob)
        rep.append(f'L{lv}: {len(missions)} миссий, {len(entries)} записей, {len(blob)} Б, хеш {h:08x} -> {out}')
        for mid, first, n, ab, cb, hb, pk in missions:
            miss = sum(1 for e in entries[first:first + n] if e[3] == 1)
            rep.append(f'  {mid:5s} записей {n:3d}  art {ab/1024:7.0f} КБ  машины {cb/1024:7.0f} КБ  персонажи {hb/1024:6.0f} КБ  пик {pk/1024:7.0f} КБ' + (f'  (нет в наборе: {miss})' if miss else ''))
    txt = '\n'.join(rep)
    print(txt)
    if a.report:
        Path(a.report).write_text(txt + '\n')

if __name__ == '__main__':
    main()
