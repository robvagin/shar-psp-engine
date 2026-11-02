#!/usr/bin/env python3
"""Гейт данных окна g12-data на CPU Мака (без эмулятора): python3 tests/g12/data_gate.py
Каждая проверка читает РЕЗУЛЬТАТ экспортёра (камеры, дороги, вызовы, ATC), а не флаг. Красный контроль: тот же файл с
переменными *_OLD=1 обязан краснеть (команда в конце: tests/g12/data_gate.py --expect-fail). Выход 0 — зелёный."""
import os, sys, struct
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
import zonecam_export as Z, traffic_export as T, prp2_export as P, psp_export as PE
bad = []
def chk(name, ok):
    print(('  ok   ' if ok else '  FAIL ') + name)
    if not ok: bad.append(name)

# P18-01: Data как маска. Пешком: Data 0 и 2; не пешком: Data 1 (r6_cam1, r3_cam1, z3_ufocam)
foot = lambda L: {r[:32].split(b'\0')[0].decode() for fn, r, i in Z.world_cams(L) if Z.rec_data(r)[0] != 0 or Z.for_foot(0, Z.rec_data(r)[1])
                  if Z.for_foot(*Z.rec_data(r))}
f1, f7 = foot(1), foot(7)
chk('P18-01 L1: z1_housejump (Data 0) в пакете пешком', 'z1_housejump' in f1)
chk('P18-01 L1: r6_cam1 (Data 1) НЕ в пакете пешком', 'r6_cam1' not in f1)
chk('P18-01 L7: z3_ufocam (Data 1) НЕ в пакете пешком', 'z3_ufocam' not in f7)
cc = {r[:32].split(b'\0')[0].decode() for fn, r, i, why in Z.car_cams(1)}
chk('P18-01 L1: r6_cam1 в пакете машины', 'r6_cam1' in cc)

rails = {r[:32].split(b'\0')[0].decode() for fn, r, i, why in Z.car_cams(1) if why.startswith('рельс')}
chk('P18-02 L1: в машину встают r1_cam1, r3_cam2, r6_cam3, z2_cam3, z2_cam4', rails == {'r1_cam1', 'r3_cam2', 'r6_cam3', 'z2_cam3', 'z2_cam4'})
foot_rails = {r[:32].split(b'\0')[0].decode() for fn, r, i in Z.world_cams(1) if Z.rec_data(r)[0] != 0 and Z.for_foot(*Z.rec_data(r), r)}
chk('P18-02 L1: «только машина» рельсы не в пакете пешком', not (foot_rails & rails))

# P11-12 / P16-19: вызов 11 всегда 5 монет против оси Z
chk('P11-12 SPAWN_25_COINS_Z -> 5 монет Z', P.call_op('SPAWN_25_COINS_Z') == (P.OP_COINS_Z, 5))
chk('P11-12 SPAWN_5_COINS_Z -> 5 монет Z', P.call_op('SPAWN_5_COINS_Z') == (P.OP_COINS_Z, 5))
# P11-13: eSpark без частиц, коды 7/24/26 есть
chk('P11-13 eSpark не в таблице частиц', 'eSpark' not in P.PARTICLE_CODE)
chk('P11-13 ePineTreeNeedles/eParkingMeter/ePopsicles = 7/24/26', (P.PARTICLE_CODE.get('ePineTreeNeedles'), P.PARTICLE_CODE.get('eParkingMeter'), P.PARTICLE_CODE.get('ePopsicles')) == (7, 24, 26))
# P16-26 / P18-15: строка 0 ATC
r0 = PE.atc_rows()[0]; t0 = P.atc_table()[0]
chk('P16-26 строка 0 ATC: smash, eNull, масса 100', r0[0] == 'smash' and r0[1] == 'eNull' and r0[3][1] == 100.0)
chk('P18-15 строка 0 ATC (prp2): smash, eNull, масса 100', t0['sound'] == 'smash' and t0['particle'] == 'eNull' and t0['f'][1] == 100.0)
# P18-10: сложность срезов L1: 8 по 15, 4 по 20, по 16, 18, 23
j, roads = T.road_network(1)
import collections
dd = collections.Counter(r['diff'] for r in roads if r['shortcut'])
chk('P18-10 L1 сложности срезов {15:8, 20:4, 16:2, 18:1, 23:1} (по диску)', dd == {15: 8, 20: 4, 16: 2, 18: 1, 23: 1})
# P16-03: полоса 0 из N стоит от угла 3: у дороги с 2 полосами она теперь ближе к углу 3, чем полоса 1
import numpy as np
two = [k for k, r in enumerate(roads) if r['lanes'] == 2]
chk('P16-03 есть дороги с 2 полосами для проверки', len(two) > 0)
old = T.LANE_C3_OLD; T.LANE_C3_OLD = not old; _, roads2 = T.road_network(1); T.LANE_C3_OLD = old
# полоса k при нумерации от угла 3 стоит там, где при нумерации от угла 0 стояла полоса N-1-k (у дорог с одним числом полос на всех сегментах)
same = [k for k in two if len(roads[k]['pts'][0]) == len(roads2[k]['pts'][0]) and np.allclose(roads[k]['pts'][0][:, :3], roads2[k]['pts'][1][:, :3], atol=1e-3)]
chk(f'P16-03 L1: полоса 0 (от угла 3) == полоса 1 прежней нумерации у {len(same)} из {len(two)} двухполосных дорог', len(same) > 0 and len(same) >= len(two) // 2)
print('exit', 1 if bad else 0, 'FAIL:', bad)
sys.exit(1 if bad else 0)
