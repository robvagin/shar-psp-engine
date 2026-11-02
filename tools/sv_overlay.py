#!/usr/bin/env python3
"""Доставка правок окна g12-data ПОВЕРХ готового level.shr (круг 4, 02.11.2026).

Зачем: уровни TEST81 собраны не тем рецептом, что tools/build_level.sh сейчас (дальняя версия без нарезки SHR_FAR_PLAIN=1,
правило забора на крышах №705 тогда не было, правило №488 выключено), поэтому свежий level.shr подменять нельзя — потеряем то, что
в TEST81 сверх обычной сборки. Здесь уровень TEST81 остаётся как есть, а мои изменения вписываются в него по разделам:

  * тип поверхности земли (SRF1)      -> файл рядом `surf-l<N>.shr` (формат SRFS, viewer/surface.c): COL1 не меняется, значит хеш проб
                                          машин миссий (mcpr.shr, mcProbeHash) и tools/lmap_pack остаются верными
  * PRP2 (монеты колы 5, частицы, строка 0 таблицы материалов, флаг/сустав «следовать» у ANM1) и CAR1 (флаги машины, масштаб
    персонажей, блеск) -> записываются в копию level.shr на месте: разделы тех же размеров, меняются только те байты, которыми
    «новый» экспорт отличается от «старого» (все мои выключатели _OLD=1), и только если в TEST81 на этих местах лежит то же, что
    у «старого» (иначе — отказ по разделу и строка в отчёте)

Вход: два экспорта ЛЮБОГО порядка секций, сделанные одним рецептом, где отличаются только мои выключатели:
    OLD  SHR_SURF_OLD=1 SHR_CARCMD_OLD=1 ATC0_OLD=1 PRP2_COINS25_OLD=1 PRP2_PARTICLES_OLD=1 AO_FOLLOW_OLD=1 python tools/psp_export.py N --out OLD.shr
    NEW  (те же переменные рецепта, мои выключатели не заданы)              python tools/psp_export.py N --out NEW.shr
    tools/sv_overlay.py --level N --base build/test81/PSP/GAME/SHARTEST15/lN/level.shr --old OLD.shr --new NEW.shr --out build/sv12b/lN
Рецепт TEST81 для сверки (reissue_level.sh): SHR_PROP_OWNED_OLD=1 SHR_FENCE_ROOF_OLD=1 + флаги качества build_level.sh.
Выход в --out: level.shr (копия основы с правками PRP2/CAR1; `cp -c` — клон APFS), surf-l<N>.shr, overlay-l<N>.txt (отчёт).
Выключатель SV_OVERLAY_OLD=1 — ничего не пишет."""
import argparse, os, shutil, struct, subprocess, sys
from pathlib import Path

HDR = '<26I'   # 104 Б: [19] куски, [20] число клеток, [21] таблица клеток, [22] PVS1, [23] COL1, [24] PRP2, [25] CAR1


def sections(b):
    """-> {имя: (начало, конец)} для COL1, PRP2, CAR1: конец = ближайшее большее смещение любой секции или блоба клетки (порядок секций любой)"""
    h = struct.unpack_from(HDR, b, 0)
    marks = {h[19], h[21], h[22], h[23], h[24], h[25], len(b)}
    for i in range(h[20]):
        marks.add(struct.unpack_from('<I', b, h[21] + i * 48 + 36)[0])
    srt = sorted(marks)
    nxt = lambda o: srt[srt.index(o) + 1]
    return {'COL1': (h[23], nxt(h[23])), 'PRP2': (h[24], nxt(h[24])), 'CAR1': (h[25], nxt(h[25]))}


def fingerprint(col):
    """тот же отпечаток, что svSurfaceFingerprint в viewer/surface.c: FNV-1a по 44 Б заголовка COL1 и по 16 Б от 256 строк треугольников"""
    ntri = struct.unpack_from('<I', col, 24)[0]
    f = 2166136261
    def feed(chunk):
        nonlocal f
        for x in chunk: f = ((f ^ x) * 16777619) & 0xffffffff
    feed(col[:44])
    for k in range(256 if ntri else 0):
        o = 48 + (k * ntri // 256) * 48
        feed(col[o:o + 16])
    return f or 1


def srf1(col):
    """-> (ntri, байты по треугольнику) из блока SRF1 секции COL1 свежего экспорта"""
    s = col.find(b'SRF1')
    if s < 0: raise SystemExit('в NEW нет блока SRF1 (SHR_SURF_OLD=1 при экспорте?)')
    ver, ntri, _ = struct.unpack_from('<3I', col, s + 4)
    return ntri, col[s + 16:s + 16 + ntri]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--level', type=int, required=True); ap.add_argument('--base', required=True)
    ap.add_argument('--old', required=True); ap.add_argument('--new', required=True); ap.add_argument('--out', required=True)
    a = ap.parse_args()
    if os.environ.get('SV_OVERLAY_OLD') == '1': print('SV_OVERLAY_OLD=1: ничего не пишу'); return
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    base, old, new = (Path(p).read_bytes() for p in (a.base, a.old, a.new))
    sb, so, sn = sections(base), sections(old), sections(new)
    rep = [f'уровень {a.level}: основа {a.base} ({len(base)} Б)']

    # 1) поверхность -> файл рядом
    colB = base[slice(*sb['COL1'])]; colN = new[slice(*sn['COL1'])]
    nB, nN = struct.unpack_from('<I', colB, 24)[0], struct.unpack_from("<I", colN, 24)[0]
    ntri, bytes_ = srf1(colN)
    tri_same = nB == nN == ntri and colB[48:48 + nB * 48] == colN[48:48 + nN * 48]
    rep.append(f'COL1: треугольников основа {nB} / новый {nN}; таблица треугольников {"ПОБАЙТНО ОДИНАКОВА" if tri_same else "РАЗНАЯ"}')
    if not tri_same:
        rep.append('ОТКАЗ по поверхности: порядок треугольников разный — файл рядом был бы неверным'); surf = None
    else:
        bad = [v for v in set(bytes_) if (v & 0x7f) > 7]
        if bad: raise SystemExit(f'значения поверхности вне 0..7|0x80: {bad}')
        nib = bytearray((ntri + 1) // 2)
        for i, v in enumerate(bytes_):
            n = (v & 7) | (8 if v & 0x80 else 0)
            nib[i >> 1] |= n << ((i & 1) * 4)
        surf = b'SRFS' + struct.pack('<3I', 1, ntri, fingerprint(colB)) + bytes(nib)
        (out / f'surf-l{a.level}.shr').write_bytes(surf)
        rep.append(f'surf-l{a.level}.shr: {len(surf)} Б (по полубайту на треугольник; в COL1 тот же блок SRF1 занял бы {16 + (ntri + 15) // 16 * 16} Б); '
                   f'отпечаток COL1 {fingerprint(colB):08x}; COL1 основы НЕ тронут')

    # 2) PRP2 и CAR1 -> на месте в копии основы
    dst = out / 'level.shr'
    subprocess.run(['cp', '-c', a.base, str(dst)], check=True)
    patched = {}
    with open(dst, 'r+b') as f:
        for name in ('PRP2', 'CAR1'):
            B, O, N = (x[slice(*s[name])] for x, s in ((base, sb), (old, so), (new, sn)))
            if len(O) != len(N):
                rep.append(f'{name}: размер OLD {len(O)} / NEW {len(N)} различается — на месте нельзя (нужен перевыпуск уровня)'); continue
            if len(B) != len(O):
                rep.append(f'{name}: размер в основе {len(B)}, у OLD {len(O)} — основа собрана иначе, на месте нельзя'); continue
            diff = [i for i in range(len(O)) if O[i] != N[i]]
            # хвост CAR1: метка REGF и за ней абсолютное смещение секции регионов в файле — зависит от порядка секций, а не от моих правок
            skip = set()
            for sec in (O, N):
                k = sec.rfind(b'REGF')
                if name == 'CAR1' and k > len(sec) - 64: skip |= set(range(k + 4, k + 8))
            nskip = len([i for i in diff if i in skip]); diff = [i for i in diff if i not in skip]
            if nskip: rep.append(f'  {name}: {nskip} байт смещения секции регионов за меткой REGF не трогаю (зависит от порядка секций)')
            conflict = [i for i in diff if B[i] != O[i]]   # на местах правки в основе должно лежать то же, что у OLD
            same_all = (B == O)
            rep.append(f'{name}: {len(N)} Б; основа {"== OLD целиком" if same_all else "отличается от OLD в %d байтах" % sum(1 for i in range(len(O)) if B[i] != O[i])}; '
                       f'правка: {len(diff)} байт, конфликтов с основой {len(conflict)}')
            if conflict:
                rep.append(f'  ОТКАЗ по {name}: на {len(conflict)} местах в основе лежит не то, что у OLD (первые {conflict[:6]})'); continue
            base_off = sb[name][0]
            # сгруппировать подряд идущие байты в куски
            i = 0
            while i < len(diff):
                j = i
                while j + 1 < len(diff) and diff[j + 1] == diff[j] + 1: j += 1
                f.seek(base_off + diff[i]); f.write(N[diff[i]:diff[j] + 1]); i = j + 1
            patched[name] = len(diff)
    (out / f'overlay-l{a.level}.txt').write_text('\n'.join(rep) + '\n', encoding='utf-8')
    print('\n'.join(rep))


if __name__ == '__main__':
    main()
