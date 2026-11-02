#!/usr/bin/env python3
"""Данные уровня из сверки с исходником (окно g12-data, 02.11.2026): то, чего не было в экспорте ни одним файлом.
Один файл на уровень, `svdata-l<N>.shr`, формат SVD1 (little endian):

  заголовок 64 Б: 'SVD1', u32 версия (1), u32 уровень, u32 nLgt, u32 offLgt, u32 nOcc, u32 offOcc, u32 nBill, u32 offBill, u32 nAct, u32 offAct, нули
  VolRec 120 Б (таблицы LGT и OCC): char loc[16] имя локатора; char vol[16] имя объёма; f32 aux[4]; u32 kind (0 сфера, 1 ящик);
      f32 c[3] центр (оси движка, z зеркален); f32 h[3] полуоси; f32 ax[9] оси объёма (три строки); u32 loc_i номер локатора в файле;
      u32 role (@116)
  BillRec 80 Б: char group[16]; char shader[16]; char mode[4]; f32 t[3] (оси движка); u32 colorARGB; f32 w, h, dist; u8 ztest, zwrite;
      u16 pad; u32 scope @68 (0 билборд мира из файла района, 1 блик солнца из мировой сферы); u32 pad2; u32 pad3

LGT — строки сверки P12-08: объёмы «смена света» (локатор вида 0, событие 65). aux = множитель цвета солнца R, G, B и A (слово
  данных ARGB, каждый канал / 255); пока игрок внутри объёма, цвет солнца умножается на него; вход и выход плавно за 1,0 с; число
  объёмов считается (вошёл в два, вышел из одного: свет остаётся). Движок — окно света/рендера, не этот файл.
OCC — P18-07, P12-13: локаторы окклюзии (вид 6). role 0 — первая зона (игрок в ней = правило действует), 1 — «скрывающий» объём
  (имя не на V: статичные объекты внутри скрыты), 2 — «показывающий» (имя на V: объекты внутри принудительно видны). Первое слово
  данных = число скрывающих объёмов минус один (в aux[0]). Скрывающих только у occ04 уровней 1 и 4.
BILL — P12-09 (scope 1: блик sunshine мировой сферы l1_terra и l3_terra, рисуется поверх всего без записи глубины) и Д1 (scope 0:
  чанки 0x17002 районов и дорожных полос; L1 12 чанков / 177 квадов, L4 16 / 200).

ACT — P18-12, P05-18: action-локаторы (вид 9), которые экспорт анимобъектов не берёт: Teleport (по три на L3 и L6: R5T01, lightport, Z5T1) и
  PlayOnce (один: объект library, локатор bs в l4r4b). VolRec: loc — имя локатора, vol — имя объекта, aux[0..2] — позиция локатора (оси
  движка), aux[3] — код действия (6 PlayOnce, 7 Teleport), объём — триггер локатора (оси объёма = ориентация локатора, курс переброса).
  Поведение (△ закрывает ирис и ставит игрока в точку локатора с его курсом; PlayOnce — клип один раз и триггер навсегда снят) — движок.

JDIR — круг 2, g01-cars P01-07 (СВЕРКА_С_ИСХОДНИКОМ): курс зоны прыжка (Event 48). Зона срабатывает, только если курс машины идёт в сторону курса
  локатора зоны (скалярное произведение горизонтальных направлений > 0), а в JMP1 (ColZone) курса нет — только оси объёма. Запись 48 Б:
  char name[16] (имя зоны, как в ColZone); f32 c[3] центр объёма (оси движка, чтобы различать одноимённые); f32 d[3] единичный курс —
  третья строка матрицы локатора (чанк 0x300000C), z отзеркален как всё в движке; u32 flags (бит 0 — матрица была, курс валиден;
  без неё проверка курса не нужна), pad. Таблица в заголовке: u32 nJdir @44, u32 offJdir @48 (версия остаётся 1: прежние читатели
  эти поля не смотрят). Читатель — car.c jumpZoneDrive (окно g01-cars) и событие «выход из зоны» (g10a).

DYN — круг 2, g11b-collect P12-10: набор регионов (зон города), который миссия грузит командой SetDynaLoadData (старт, рестарт, выбор миссии).
  Запись 32 Б: char script[16] (имя файла сценария без .mfk, например `level01` или `l1m1`; на диске их 20–22 на уровень), u32 seq (порядковый номер
  команды в файле, с 0), u32 mask (бит i — i-й регион уровня в порядке tools/regions_export.level_regions: сначала l<N>r*, потом l<N>z*, по
  имени), u32 nload (сколько регионов в команде), u32 0. Команды без регионов-районов (чистые интерьеры «…@») не пишутся. В конце таблицы
  записи с seq = 0xFFFFFFFF: имена регионов (script = имя региона, mask = его номер i) — чтобы читатель сверил порядок со своей таблицей REG1.
  Таблица в заголовке: u32 nDyn @52, u32 offDyn @56. Читатель: старт уровня и рестарт (main_levels.h, окно g10b) вместо ближайшего AddTeleportDest.

Выключатель SVDATA_OLD=1 — файл не пишется (прежнее поведение). Отчёт в stdout.
    python tools/svdata_export.py [--level 1] [--out build/sv12/svdata-l1.shr]"""
import argparse, math, os, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
from p3d import read, walk, string    # noqa: E402

GAME = (ROOT / 'hit-and-run-web/source/game').resolve()
VOLREC, BILLREC, HEAD = 120, 80, 64
EV_LIGHT = 65   # событие локатора «смена света»


def mirror(v): return (v[0], v[1], -v[2])


def locators(level):
    """-> [(файл, номер, имя, вид, слова данных, [объёмы (имя, kind, c, h, ax)], позиция)] — Locator2 0x3000005 всех файлов уровня (без комнат)"""
    out = []
    for f in sorted(GAME.glob(f'art/l{level}*.p3d')):
        if len(f.stem) > 2 and f.stem[2] == 'i': continue
        try: root = read(f)
        except Exception: continue
        n = 0
        for c in walk(root):
            if c.id != 0x3000005: continue
            name, p = string(c.data); kind, ln = struct.unpack_from('<II', c.data, p)
            words = struct.unpack_from(f'<{ln}I', c.data, p + 8)
            vols = []
            for t in c.children:
                if t.id != 0x3000006: continue
                vn, q = string(t.data); rect, = struct.unpack_from('<I', t.data, q)
                half = struct.unpack_from('<3f', t.data, q + 4); m = struct.unpack_from('<16f', t.data, q + 16)
                axes, sc = [], []
                for r in range(3):
                    ax = mirror(m[r * 4: r * 4 + 3]); ln_ = math.sqrt(sum(a * a for a in ax)) or 1.0
                    axes.append(tuple(a / ln_ for a in ax)); sc.append(half[r] * ln_)
                vols.append((vn, 1 if rect else 0, mirror(m[12:15]), tuple(sc), axes))
            pos = mirror(struct.unpack_from('<3f', c.data, p + 8 + ln * 4))
            out.append((f.stem, n, name, kind, words, vols, pos)); n += 1
    return out


def volrec(loc, vol, aux, kind, c, h, ax, loc_i, role):
    flat = [x for row in ax for x in row]
    return struct.pack('<16s16s4fI3f3f9f2I', loc.encode()[:15], vol.encode()[:15], *aux, kind, *c, *h, *flat, loc_i, role)


def light_volumes(locs):
    """P12-08: событие 65, слово данных 1 — цвет ARGB"""
    recs = []
    for li, (fn, _n, name, kind, w, vols, _pos) in enumerate(locs):
        if kind != 0 or len(w) < 2 or w[0] != EV_LIGHT: continue
        a, r, g, b = (w[1] >> 24) & 255, (w[1] >> 16) & 255, (w[1] >> 8) & 255, w[1] & 255
        for vn, k, c, h, ax in vols:
            recs.append(volrec(name, vn, (r / 255., g / 255., b / 255., a / 255.), k, c, h, ax, li, 0))
    return recs


def occlusion(locs):
    """P18-07: вид 6; первое слово = число скрывающих объёмов минус один. Первый объём — сама зона; следующие, чьё имя не на V,
    прячут, а на V — показывают"""
    recs = []
    for li, (fn, _n, name, kind, w, vols, _pos) in enumerate(locs):
        if kind != 6: continue
        nh = w[0] if w else 0
        for i, (vn, k, c, h, ax) in enumerate(vols):
            role = 0 if i == 0 else (2 if vn[:1] in ('V', 'v') else 1)
            recs.append(volrec(name, vn, (float(nh), 0., 0., 0.), k, c, h, ax, li, role))
    return recs


ACT_CODES = {'PlayOnce': 6, 'Teleport': 7}


def actions(locs):
    """P18-12 / P05-18: Teleport и PlayOnce (в ANM1 их нет); слова данных: три строки (объект, сустав, действие), затем 3 и признак «следовать»"""
    recs = []
    for li, (fn, _n, name, kind, w, vols, pos) in enumerate(locs):
        if kind != 9: continue
        strs = [x.decode('latin1') for x in struct.pack(f'<{len(w)}I', *w).split(b'\0') if x]
        if len(strs) < 3 or strs[2] not in ACT_CODES: continue
        for vn, k, c, h, ax in vols[:1]:
            recs.append(volrec(name, strs[0], (pos[0], pos[1], pos[2], float(ACT_CODES[strs[2]])), k, c, h, ax, li, ACT_CODES[strs[2]]))
    return recs


def jump_dirs(level):
    """P01-07: курс зон прыжка машин. Те же локаторы и тот же отбор, что у tools/jumps_export.collect (Event 48, один локатор в двух файлах
    считается раз); третья строка LocatorMatrix = направление"""
    sys.path.insert(0, str(ROOT / 'tools/missions'))
    import jumps_export as J
    from p3dlocs import read_locators
    recs, seen = [], set()
    for f in J.script_files(level) + [p for p in J.scene_files(level) if p.exists()]:
        locs, err = read_locators(f, str(f.relative_to(J.GAME)))
        if err: continue
        for L in locs:
            if L.type != 0 or L.data != J.EV_JUMPZONE or not L.triggers: continue
            key = (L.name, round(L.pos[0], 2), round(L.pos[2], 2))
            if key in seen: continue
            seen.add(key)
            _rect, c, _h, _axes = L.triggers[0]
            m = getattr(L, 'mat', None)
            d, fl = (0.0, 0.0, 0.0), 0
            if m:
                v = (m[8], m[9], -m[10]); n = math.sqrt(sum(x * x for x in v))
                if n > 1e-6: d, fl = tuple(x / n for x in v), 1
            recs.append(struct.pack('<16s3f3fII', L.name.encode()[:15], *c, *d, fl, 0))
    return recs


def dyna_sets(level):
    """P12-10: SetDynaLoadData каждой команды сценариев миссий уровня -> маска регионов (разбор строки — tools/regions_export.parse_dyna)"""
    import re, regions_export as R
    regs = R.level_regions(level); idx = {n: i for i, n in enumerate(regs)}
    recs = []
    sd = GAME / f'scripts/missions/level{level:02d}'
    for f in sorted(sd.glob('*.mfk')) if sd.exists() else []:
        if f.stem.startswith(('e3', 'demo', 'level-d')) or f.stem.endswith('-d'): continue    # демо и сборки E3 уровень не грузит
        txt = re.sub(r'//[^\n]*', '', f.read_text(errors='replace'))
        for seq, m in enumerate(re.finditer(r'SetDynaLoadData\(\s*"([^"]*)"', txt)):
            ld, _ul = R.parse_dyna(m.group(1)); ld = [x for x in ld if x in idx]
            if not ld: continue
            mask = 0
            for x in ld: mask |= 1 << idx[x]
            recs.append(struct.pack('<16s4I', f.stem.encode()[:15], seq, mask, len(set(ld)), 0))
    for i, n in enumerate(regs): recs.append(struct.pack('<16s4I', n.encode()[:15], 0xFFFFFFFF, i, 0, 0))
    return recs


def billboards(level):
    """Д1 + P12-09: группы билбордов 0x17002 (разбор групп — как tools/car_dump.billboards) в районах/полосах (scope 0) и в composite
    блика sunshine мировой сферы (scope 1)"""
    def f32(b, n, o): return struct.unpack_from(f'<{n}f', b, o)
    recs = []
    for f in sorted(GAME.glob(f'art/l{level}*.p3d')):
        if len(f.stem) > 2 and f.stem[2] == 'i': continue
        try: root = read(f)
        except Exception: continue
        flare_ids = {id(k) for fl in walk(root) if fl.id == 0x3f0000d for k in fl.children}   # группы внутри блика 0x3F0000D
        # только самостоятельные группы: верхний уровень файла (билборды мира) и группы внутри блика; билборды внутри предметов, машин
        # и анимированных объектов (суставы) уже в их собственных данных
        tops = [c for c in root.children if c.id == 0x17002] + [k for fl in walk(root) if fl.id == 0x3f0000d for k in fl.children if k.id == 0x17002]
        for g in tops:
            try:
                gname, p = string(g.data, 4); sh, p = string(g.data, p)
                zt, zw, fog, nq = struct.unpack_from('<4I', g.data, p)
                for q in (c for c in g.children if c.id == 0x17001):
                    qn, p2 = string(q.data, 4); mode = q.data[p2:p2 + 4]; p2 += 4
                    t = f32(q.data, 3, p2); col, = struct.unpack_from('<I', q.data, p2 + 12)
                    w, h, dist = f32(q.data, 3, p2 + 16 + 32)
                    scope = 1 if id(g) in flare_ids else 0
                    recs.append(struct.pack('<16s16s4s3fI3f2BH3I', gname.encode()[:15], sh.encode()[:15], mode, *mirror(t), col, w, h, dist,
                                            zt & 255, zw & 255, 0, scope, 0, 0))
            except (struct.error, ValueError):
                continue
    return recs


def build(level):
    locs = locators(level)
    L, O, B, A = light_volumes(locs), occlusion(locs), billboards(level), actions(locs)
    J = jump_dirs(level) if os.environ.get('SVDATA_JDIR_OLD') != '1' else []   # круг 2 P01-07; SVDATA_JDIR_OLD=1 — без таблицы курсов
    D = dyna_sets(level) if os.environ.get('SVDATA_DYN_OLD') != '1' else []   # круг 2 P12-10; SVDATA_DYN_OLD=1 — без таблицы наборов
    body = bytearray(); offs = []
    for tab, rec in ((L, VOLREC), (O, VOLREC), (B, BILLREC), (A, VOLREC), (J, 48), (D, 32)):
        body += b'\0' * (-(HEAD + len(body)) % 16); offs.append(HEAD + len(body)); body += b''.join(tab)
    head = bytearray(HEAD); head[:4] = b'SVD1'
    struct.pack_into('<14I', head, 4, 1, level, len(L), offs[0], len(O), offs[1], len(B), offs[2], len(A), offs[3], len(J), offs[4], len(D), offs[5])
    nflare = sum(1 for r in B if struct.unpack_from('<I', r, 68)[0] == 1)
    return bytes(head) + bytes(body), dict(light=len(L), occ=len(O), bill=len(B), flare=nflare, act=len(A), jdir=len(J), dyna=len(D),
                                           light_names=sorted({r[:16].split(b'\0')[0].decode() for r in L}),
                                           occ_hide=sum(1 for r in O if struct.unpack_from('<I', r, 116)[0] == 1))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--level', type=int, default=1); ap.add_argument('--out', default=None)
    a = ap.parse_args()
    if os.environ.get('SVDATA_OLD') == '1':
        print('SVDATA_OLD=1: файл не пишется'); return
    out = Path(a.out or ROOT / f'build/svdata-l{a.level}.shr')
    data, st = build(a.level)
    out.parent.mkdir(parents=True, exist_ok=True); out.write_bytes(data)
    print(f'-> {out}: {len(data)} Б; объёмов смены света {st["light"]} {st["light_names"]}; объёмов окклюзии {st["occ"]} '
          f'(скрывающих {st["occ_hide"]}); квадов билбордов {st["bill"]} (из них блик солнца {st["flare"]}); Teleport/PlayOnce {st["act"]}; курсов зон прыжка {st["jdir"]}; записей наборов регионов {st["dyna"]}')


if __name__ == '__main__':
    main()
