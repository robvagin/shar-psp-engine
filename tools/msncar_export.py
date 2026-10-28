#!/usr/bin/env python3
"""Машины миссий уровня N (29.09) -> msncars-l<N>.shr (формат MCR1: читают viewer/msn_view.c — модели по требованию, и
viewer/msncar.c / tools/bots/world.c — кузов и колёса из каталога).

  python tools/msncar_export.py --level 1 [--out build/levels/l1/msncars-l1.shr]

Какие машины: все модели AddStageVehicle и LoadDisposableCar(..., "AI") миссий уровня из расписания (компилятор миссий
tools/missions/compile.py — тот же, что собирает level0N.msn: находит переставленные файлы диска). Модель — art/cars/<имя>.p3d,
как у трафика (tools/traffic_export.car_model: кузов в позе покоя со светотенью из нормалей, колёса w0..w3 — отдельные куски в
осях колеса, LOD <= 400 треуг. дальше 30 м). Каждая модель — отдельный блок со своими текстурами: движок читает с карты
памяти только машины текущей миссии (как LoadDisposableCar оригинала) и отдаёт их в кучу по её окончании.

Формат MCR1 (little-endian): заголовок 32 Б ('MCR1', u32 версия 2, u32 уровень, u32 машин, u32 смещение каталога,
u32 размер файла, u32 смещение таблицы машин, u32 её запись = 128); таблица машин (версия 2, 29.09) — по записи CAR_INFO_FMT на
машину в порядке каталога: блок машины CAR1 v2 (tools/psp_export.car_info: имя .con, кузов, сиденье, дверь, камера — то же правило,
что у машины уровня). Нужен машине, которую сценарий даёт игроку (SetForcedCar / InitLevelPlayerVehicle(…, "OTHER"): L1 m6 —
marge_v): движок читает его как машину уровня (viewer/car.h carParamsFromInfo). Версия 1 — без таблицы (читатели каталога те же); каталог — по 176 Б на машину: имя[16], TrModel[128] (viewer/traffic.h; firstPiece/firstLod —
номера кусков внутри блока), u32 смещение блока, размер блока, кусков, смещение вершин (от блока), байт вершин, смещение
текстур (от блока), байт текстур, текстур. Блок: куски TrPiece по 80 Б (tex — номер текстуры блока), вершины, текстуры
(как в level.shr: заголовок 16 Б, CLUT 1024 Б, уровни). Выравнивание 64."""
import argparse, json, os, re, struct, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/missions'))
import traffic_export as te                 # noqa: E402  car_model (модель машины как у трафика), GAME, Converter
import psp_export as pe                     # noqa: E402  car_info / car_info_bytes: блок машины CAR1 v2
from missions import compile as mc         # noqa: E402

DIR = 176
INFO = 128   # запись таблицы машин: CAR_INFO_FMT (124 Б) + выравнивание 16 (psp_export.car_info_bytes)


def al(b, a):
    b += b'\0' * (-len(b) % a)
    return b


def mission_cars(level):
    """-> [имя модели] в порядке первого появления (AddStageVehicle, LoadDisposableCar со слотом AI)"""
    c = mc.Compiler(level); c.compile()
    with tempfile.TemporaryDirectory() as d:
        p = Path(d) / 'm.json'; mc.dump_json(c.pack, p); data = json.loads(p.read_text())
    names = []
    def v(a): return a['v'] if isinstance(a, dict) else a
    for m in data['missions']:
        for ph in m['phases']:
            cmds = ph['load'] + ph['setup'] + [k for st in ph['stages'] for k in st['cmds']]
            for k in cmds:
                if k['op'] == 'AddStageVehicle': n = str(v(k['args'][0]))
                elif k['op'] == 'LoadDisposableCar' and len(k['args']) > 2 and str(v(k['args'][2])).upper() == 'AI': n = str(v(k['args'][1]))
                else: continue
                if n.lower() not in [x.lower() for x in names]: names.append(n)
    return names


def world_cars():
    """-> [(уровень, модель)] машин мира из viewer/worldcars.h (правка 268)"""
    t = (ROOT / 'viewer/worldcars.h').read_text(errors='replace')
    return [(int(l), n) for l, n in re.findall(r'\{\s*(\d+)\s*,\s*"([^"]+)"', t)]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--level', type=int, default=1)
    ap.add_argument('--out')
    # правка 420 (бонус-игра): --names a,b,c — пакет ровно из этих машин (tools/bonus/export.py: машины выбора MiniMenu.pag
    # и машины соперников), свет — уровня --level; машины миссий, награды и машины мира уровня не добавляются. Без ключа — как было
    ap.add_argument('--names')
    a = ap.parse_args()
    out = Path(a.out) if a.out else ROOT / f'build/levels/l{a.level}/msncars-l{a.level}.shr'
    if a.names: return pack(a.level, [n for n in a.names.split(',') if n], out)
    names = mission_cars(a.level)
    # правка Роба 64/72: машина погони Hit & Run — CreateChaseManager("cPolice", "Pursuit\\L1cop.con", 1) скриптов уровня
    for f in sorted((te.GAME / 'scripts/missions' / f'level{a.level:02d}').glob('*.mfk')):
        for mm in re.finditer(r'CreateChaseManager\(\s*"([^"]+)"', f.read_text(errors='replace')):
            if mm.group(1) not in names: names.append(mm.group(1))
    # правка Роба 94 (30.09): телефонная будка — машины-награды уровня (rewards.mfk BindReward car: defaultcar, bonusmission,
    # streetrace, forsale) можно вызвать к будке, модель и ходовые берутся из этого же пакета
    rtxt = re.sub(r'//[^\n]*', '', (te.GAME / 'scripts/missions/rewards.mfk').read_text(errors='replace'))
    for mm in re.finditer(r'BindReward\s*\(([^)]*)\)', rtxt):
        ra = [x.strip().strip('"') for x in mm.group(1).split(',')]
        if len(ra) >= 5 and ra[2] == 'car' and ra[4].isdigit() and int(ra[4]) == a.level and ra[3] != 'defaultcar' and ra[0] not in names:
            names.append(ra[0])
    # №535 (11.10): в будке можно вызвать купленную машину ЛЮБОГО уровня (сохранение с дальними уровнями, Coins.owned), поэтому модели наград всех семи уровней
    # лежат в пакете каждого уровня (на карте памяти; в ОЗУ — каталог и по требованию). SHR_PHONE_LEVELS_OLD=1 — прежнее: только уровни до текущего.
    # правка Роба 450 (10.10): будка показывает машины-награды ВСЕХ уровней (viewer/phone_view.c, вариант Б) — модели и ходовые чужих уровней
    # должны быть и в этом пакете. Только уровни до текущего включительно: машина более позднего уровня ещё не могла быть получена. Свой defaultcar уровня не дублируется (это машина игрока уровня). SHR_PHONE_ALL_OLD=1 — как было (вариант А)
    extra = []   # дописывается в самый конец списка (ниже): предохранитель размера отбрасывает именно их
    # №1056 (Роб 27.10: на L3 выбрал в будке машину Барта с L6, приехала машина Лизы): defaultcar более позднего уровня (bart_v L6, homer_v L7 …)
    # галерея показывает и открывает по сохранению (phone_view.c ownedOther: уровень defaultcar <= достигнутого), а модели в пакете текущего
    # уровня не было (условие выше «defaultcar только до текущего уровня»); без модели движок молча вызывает свою машину (PHONECAR nomodel).
    # Теперь defaultcar всех семи уровней — в пакет каждого уровня (кроме своего). SHR_PHONE_DEFCARS_OLD=1 — прежнее
    defall = os.environ.get('SHR_PHONE_DEFCARS_OLD') != '1'
    if os.environ.get('SHR_PHONE_ALL_OLD') != '1':
        own_def = {ra[0].lower() for ra in (
            [x.strip().strip('"') for x in mm.group(1).split(',')] for mm in re.finditer(r'BindReward\s*\(([^)]*)\)', rtxt))
            if len(ra) >= 5 and ra[2] == 'car' and ra[4].isdigit() and int(ra[4]) == a.level and ra[3] == 'defaultcar'}
        for mm in re.finditer(r'BindReward\s*\(([^)]*)\)', rtxt):
            ra = [x.strip().strip('"') for x in mm.group(1).split(',')]
            if len(ra) >= 5 and ra[2] == 'car' and ra[4].isdigit() and 1 <= int(ra[4]) <= 7 and (ra[3] != 'defaultcar' or defall or int(ra[4]) <= a.level) \
                    and (os.environ.get('SHR_PHONE_LEVELS_OLD') != '1' or int(ra[4]) <= a.level) and ra[0].lower() not in own_def \
                    and ra[0].lower() not in [x.lower() for x in names + extra]:
                extra.append(ra[0])
    # правка 100: машины персонажей в будке (AddVehicleSelectInfo level.mfk) — когда открыты, вызываются так же
    ltxt = re.sub(r'//[^\n]*', '', (te.GAME / 'scripts/missions' / f'level{a.level:02d}' / 'level.mfk').read_text(errors='replace'))
    for mm in re.finditer(r'AddVehicleSelectInfo\s*\(([^)]*)\)', ltxt):
        va = [x.strip().strip('"') for x in mm.group(1).split(',')]
        if len(va) >= 2 and va[1] not in names: names.append(va[1])
    # правка Роба 268 (04.10: «у ракетной машины — машина, которую не видно: въехал в пустоту и разбил»): машины мира, которые
    # ставит движок (viewer/worldcars.h, правка 53: ракетная Гомера у локатора rocke l1r3), — тоже в пакет: без модели машина
    # стояла твёрдой и невидимой. Список — из того же файла, что читает движок (вторая копия правды не нужна)
    for lv, nm in world_cars():
        if lv == a.level and nm not in names: names.append(nm)
    extra = [x for x in extra if x.lower() not in [y.lower() for y in names]]; names += extra; n_extra = len(extra)
    pack(a.level, names, out)
    # предохранитель кучи PSP (минимум свободной 13 МБ, tools/build2-series): пакет больше SHR_MSNCARS_MAX_MB (по умолчанию 7) — лишние чужие машины
    # отбрасываются с конца (по 4), пока пакет не влезет; в отчёт идёт предупреждение
    cap = float(os.environ.get('SHR_MSNCARS_MAX_MB', '0')) * 1048576   # правка 474: 0 — без предела (пакет лежит на карте памяти, в ОЗУ читается каталог и модели по требованию)
    while cap > 0 and n_extra and out.stat().st_size > cap:
        drop = min(4, n_extra); names = names[:len(names) - drop]; n_extra -= drop
        print(f'ПРЕДУПРЕЖДЕНИЕ: пакет {out.name} больше {cap / 1048576:g} МБ — чужих машин будки осталось {n_extra}')
        pack(a.level, names, out)


def pack(level, names, out):
    """машины names -> пакет MCR1 out (свет уровня level)"""
    class A: pass
    a = A(); a.level = level
    conv = te.Converter(); entries = []; blobs = bytearray(); skipped = []; infos = bytearray()
    light = te.level_light.level_light(a.level)   # 03.10, сведение света уровней: машины миссий запекаются светом уровня (у L1 — множители 1)
    for n in names:
        if not (te.GAME / 'art' / 'cars' / f'{n.lower()}.p3d').exists():
            skipped.append(n); continue
        cache, texblob, vblob, pieces = {}, [], bytearray(), []
        m = te.car_model(conv, n, cache, texblob, vblob, pieces, tmp=f'mcr_car_{a.level}', light=light)   # свет уровня, как у трафика
        used = sorted({p['tex'] for p in pieces if p['tex'] >= 0}); tmap = {t: i for i, t in enumerate(used)}
        recs = bytearray()
        for p in pieces:
            r = bytearray(p['rec']); t, = struct.unpack_from('<i', r, 24); struct.pack_into('<i', r, 24, tmap.get(t, -1)); recs += r
        trec = bytearray()
        for t in used: trec += texblob[t]; trec = al(trec, 16)
        blob = bytearray(recs); blob = al(blob, 64); vo = len(blob); blob += vblob; blob = al(blob, 64); to = len(blob); blob += trec
        blob = al(blob, 64)
        g = m['geo']
        import numpy as np
        model = struct.pack('<16s4H10f12ff3I', n.encode()[:15], m['first'], m['n'], m['lodFirst'], m['nLod'],
                            m['lift'], g['wheelRad'], g['halfW'], g['halfL'], g['bodyOfs'], g['wheelX'], g['wheelF'], g['wheelR'],
                            g['bodyLo'], g['bodyHi'], *np.concatenate(m['wheel']).tolist(), 0., m['tris'], m['lodTris'], 0)
        entries.append((n, model, len(blobs), len(blob), len(pieces), vo, len(vblob), to, len(trec), len(used)))
        blobs += blob
        info = pe.car_info_bytes(pe.car_info(n.lower(), te.GAME / 'art' / 'cars' / f'{n.lower()}.p3d'))
        assert len(info) == INFO, (n, len(info)); infos += info
    head = 32; dir_off = head; info_off = head + DIR * len(entries); body_off = len(al(bytearray(info_off + len(infos)), 64))
    d = bytearray()
    for n, model, bo, bs, npc, vo, vb, to, tb, nt in entries:
        d += struct.pack('<16s', n.encode()[:15]) + model + struct.pack('<8I', body_off + bo, bs, npc, vo, vb, to, tb, nt)
    total = body_off + len(blobs)
    data = struct.pack('<4s7I', b'MCR1', 2, a.level, len(entries), dir_off, total, info_off, INFO) + d + infos
    data = bytes(al(bytearray(data), 64)) + bytes(blobs)
    assert len(data) == total
    out.parent.mkdir(parents=True, exist_ok=True); out.write_bytes(data)
    print(f'{out}: {len(entries)} машин, {total / 1024:.0f} КБ' + (f'; нет модели на диске: {", ".join(skipped)}' if skipped else ''))
    for n, model, bo, bs, *_ in entries: print(f'  {n}: {bs / 1024:.0f} КБ')


if __name__ == '__main__':
    main()
