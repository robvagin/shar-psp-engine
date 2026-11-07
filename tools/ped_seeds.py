#!/usr/bin/env python3
"""№1159: прохожие в езде по улице на трёх seed (набор тура). Вход: три smoke.txt (сборки с -DTRF_SEED_ADD=1,2,3); считаются секунды GATECAT сцены FG_street с peds >= 1.
Условие: сумма по трём прогонам >= 3 (не «хотя бы в одном»: нуль в езде при включённом сне съехавших машин оказался настоящей ошибкой, а не случайностью).
    tools/ped_seeds.py A/smoke.txt B/smoke.txt C/smoke.txt [--scene FG_street] [--min 3]
    tools/ped_seeds.py --selftest     (красный контроль: три журнала без прохожих -> FAIL; с прохожими -> PASS)"""
import argparse, os, re, sys, tempfile


def seconds(path, scene):
    n = tot = 0
    for ln in open(path, errors='replace'):
        if ln.startswith('END'): break
        if not ln.startswith('GATECAT'): continue
        m = re.search(r'sc=(\S+)', ln); p = re.search(r' peds=(\d+)', ln)
        if not m or m.group(1) != scene or not p: continue
        tot += 1; n += int(p.group(1)) >= 1
    return n, tot


def verdict(paths, scene='FG_street', need=3):
    per = [seconds(p, scene) for p in paths]
    s = sum(a for a, _ in per); t = sum(b for _, b in per)
    return t > 0 and s >= need, f"секунд {scene} с прохожим по прогонам: {' / '.join(f'{a} из {b}' for a, b in per)}, сумма {s} (нужно >= {need})"


def selftest():
    def log(d, peds_at):
        p = os.path.join(d, f'smoke{len(os.listdir(d))}.txt')
        with open(p, 'w') as f:
            for i in range(12):
                f.write(f'GATECAT t={i}.0 sc=FG_street fr=30 geo city=1 kt car=1 untex car=0 cars=3 park=1 peds={1 if i in peds_at else 0} skin={384 if i in peds_at else 0} skinbad=0\n')
            f.write('END t=12\n')
        return p
    with tempfile.TemporaryDirectory() as d:
        red = [log(d, set()) for _ in range(3)]; ok_red, msg_red = verdict(red)
        one = [log(d, {2, 3}), log(d, set()), log(d, set())]; ok_one, msg_one = verdict(one)
        low = [log(d, {2}), log(d, set()), log(d, {5, 6})]; ok_low, msg_low = verdict(low)
    print('красный (три журнала без прохожих):', 'упал, как надо' if not ok_red else 'НЕ ПОЙМАН', '|', msg_red)
    print('два прохожих в одном прогоне (сумма 2):', 'упал, как надо' if not ok_one else 'НЕ ПОЙМАН', '|', msg_one)
    print('три в сумме по двум прогонам:', 'прошёл, как надо' if ok_low else 'ПОЧЕМУ-ТО УПАЛ', '|', msg_low)
    return 0 if (not ok_red and not ok_one and ok_low) else 1


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('logs', nargs='*'); ap.add_argument('--scene', default='FG_street'); ap.add_argument('--min', type=int, default=3); ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest: return selftest()
    if len(a.logs) != 3: print('нужны три smoke.txt'); return 2
    ok, msg = verdict(a.logs, a.scene, a.min); print(('PASS ' if ok else 'FAIL ') + msg); return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
