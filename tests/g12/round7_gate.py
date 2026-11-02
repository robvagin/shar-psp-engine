#!/usr/bin/env python3
"""Гейт круга 7 окна g12-data: строки, которые в круге 6 стояли «не сверено», проверены по оригиналу; здесь — данные диска и площадки переброса. Каждая проверка с красным контролем."""
import os, re, struct, subprocess, sys, glob, collections, math
from pathlib import Path
R = Path(__file__).resolve().parents[2]; os.chdir(R)
SP = [str(R / 'tools'), str(R / 'hit-and-run-web/tools'), str(R / 'tools/missions')]; sys.path[:0] = SP
fails = []
def check(name, ok, why=''):
    print(('ok     ' if ok else 'ПАДЕНИЕ ') + name + (('  — ' + why) if (why and not ok) else '')); (not ok) and fails.append(name)
def py(code, env=None):
    e = dict(os.environ); e.update(env or {})
    return subprocess.run([sys.executable, '-c', 'import sys; sys.path[:0]=%r\n%s' % (SP, code)], capture_output=True, text=True, env=e, cwd=R).stdout.strip()

# P05-7: признак «следовать за суставом» = ПОСЛЕДНЕЕ слово данных action-локатора == 1; на диске два объекта из 39
import animobj_export as A
A.action_locators([Path(f) for f in sorted(glob.glob(str(R / 'hit-and-run-web/source/game/art/*.p3d')))])
fol = sorted(k for k, v in A.AO_FOLLOW.items() if v == 1)
check('P05-7: следуют за суставом ровно DB_quimby и Splatfrom7Trans (2 из 39)', fol == ['DB_quimby', 'Splatfrom7Trans'] and len(A.AO_FOLLOW) == 39, f'{fol} {len(A.AO_FOLLOW)}')

# Д9 (камера): hypecam на L3 и L6 с Data 2 («только пешком»), z3_ufocam на L7 с Data 1
import zonecam_export as Z
named = {(N, r[:32].split(b'\0')[0].decode()): Z.rec_data(r)[1] for N in range(1, 8) for fn, r, info in Z.world_cams(N)}
check('Д9: hypecam на L3 и L6 (Data 2), z3_ufocam на L7 (Data 1), больше нигде', named.get((3, 'hypecam')) == 2 and named.get((6, 'hypecam')) == 2 and named.get((7, 'z3_ufocam')) == 1 and sum(1 for k in named if k[1] in ('hypecam', 'z3_ufocam')) == 3)
check('  красный контроль: ZONECAM_DATA_OLD=1 толкует Data 1/2 иначе (ufocam пешком)', py("import zonecam_export as Z; print(Z.for_foot(0,1))", {'ZONECAM_DATA_OLD': '1'}) != py("import zonecam_export as Z; print(Z.for_foot(0,1))"))

# P18-13 и P18-14: события локаторов и разделители строки загрузчика зоны
import svdata_export as V
ev = collections.Counter(); kinds = collections.Counter(); sep = collections.Counter()
for N in range(1, 8):
    for fn, _n, name, kind, w, vols, pos in V.locators(N):
        kinds[kind] += 1
        if kind == 0 and w: ev[w[0]] += 1
        if kind == 5:
            s = b''.join(struct.pack('<I', x) for x in w).split(b'\0')[0].decode('latin1')
            for ch in ':;@$&*': sep[ch] += s.count(ch)
check('P18-13: события 49 нет; виды 10, 11, 15 нет; событие 1 есть (камера разрушителя, не используем)', ev.get(49, 0) == 0 and all(kinds.get(k, 0) == 0 for k in (10, 11, 15)) and ev.get(1, 0) > 0)
check('P18-14: в строках загрузчика зон «&» и «*» нет; «:» «;» «@» «$» есть', sep['&'] == 0 and sep['*'] == 0 and all(sep[c] > 0 for c in ':;@$'), str(dict(sep)))
import regions_export as RE
l, u = RE.parse_dyna('l2r2.p3d:l2z2.p3d;l2r1.p3d;l2z1.p3d;')
check('  parse_dyna: «;» загрузка, «:» выгрузка', u == ['l2r2'] and l == ['l2z2', 'l2r1', 'l2z1'], f'{l} {u}')

# Д10: файлов регионов на диске 72 (l7r4a, l7r4b, l7z4 из списка оригинала не поставлялись); регионы уровня = файлы + два «dam»
files = {f[:-4] for f in os.listdir(R / 'hit-and-run-web/source/game/art') if re.match(r'^l[1-7][rz]\d+[a-z]*\.p3d$', f)}
names = set().union(*[set(RE.level_regions(N)) for N in range(1, 8)])
check('Д10: 72 файла регионов, l7r4a/l7r4b/l7z4 нет; наш список = файлы + l3r5_dam, l6r5_dam', len(files) == 72 and not ({'l7r4a', 'l7r4b', 'l7z4'} & files) and names - files == {'l3r5_dam', 'l6r5_dam'})

# P18-12 Teleport: три площадки на L3 и на L6, курс = ось X матрицы локатора (пересчёт независимо по p3dlocs)
from p3dlocs import read_locators
import svdata_export as V2
def heading_from_disk(N):
    out = {}
    for f in sorted((R / 'hit-and-run-web/source/game/art').glob(f'l{N}*.p3d')):
        if len(f.stem) > 2 and f.stem[2] == 'i': continue
        locs, err = read_locators(f, f.name)
        if err: continue
        for L in locs:
            if L.type == 9 and getattr(L, 'mat', None) and L.name in ('R5T01', 'lightport', 'Z5T1'): out[L.name] = math.atan2(L.mat[0], -L.mat[2])
    return out
for N in (3, 6):
    b = Path(f'build/sv12d/l{N}/svdata-l{N}.shr').read_bytes(); na, oa = struct.unpack_from('<2I', b, 36)
    got = {b[oa + i * 120:oa + i * 120 + 16].split(b'\0')[0].decode(): struct.unpack_from('<f', b, oa + i * 120 + 112)[0] for i in range(na) if struct.unpack_from('<I', b, oa + i * 120 + 116)[0] == 7}
    want = heading_from_disk(N)
    check(f'P18-12 L{N}: три площадки, курс из матрицы локатора совпал с независимым пересчётом', set(got) == {'R5T01', 'lightport', 'Z5T1'} and all(abs(got[k] - want[k]) < 1e-5 for k in got), f'{got} {want}')
slot_old = py("import svdata_export as V, struct\nb, _ = V.build(3)\nna, oa = struct.unpack_from('<2I', b, 36)\nprint([struct.unpack_from('<I', b, oa + i * 120 + 112)[0] for i in range(na)])", {'SVDATA_TPHEAD_OLD': '1'})
check('  красный контроль SVDATA_TPHEAD_OLD=1 кладёт в слот номер локатора, а не курс', re.fullmatch(r'\[\d{1,3}(, \d{1,3})*\]', slot_old) is not None, slot_old)
env = dict(os.environ, SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk', PATH=str(Path.home() / 'Downloads/shar-psp/build/xcshim/bin') + ':' + os.environ['PATH'])
for tag, extra, red in (('new', [], 0), ('old', ['-DSVTELEPORT_OLD=1'], 1)):
    rc = subprocess.run(['cc', '-Iviewer', *extra, 'tests/g12/svtp_test.c', 'viewer/svdata.c', '-lm', '-o', f'/tmp/svtp_{tag}'], capture_output=True, text=True, env=env)
    if rc.returncode: check('svtp_test собирается', False, rc.stderr[-200:]); continue
    ok = subprocess.run([f'/tmp/svtp_{tag}', 'build/sv12d/l3/svdata-l3.shr', '3'], capture_output=True, text=True).returncode
    check('P18-12 читатель площадок: ' + ('зелёный (L3: 3 площадки, цель, высота объёма, чужой уровень)' if not red else 'красный контроль SVTELEPORT_OLD=1'), (ok != 0) == bool(red))
print('\nЗЕЛЁНЫЙ' if not fails else f'\nКРАСНЫЙ: {len(fails)}: {fails}'); sys.exit(1 if fails else 0)
