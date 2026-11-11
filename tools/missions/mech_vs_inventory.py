#!/usr/bin/env python3
"""Сверка итога Механика (missioner --mech, строки MH/MS/MM) с описью docs/state/mechanic_inventory.json
(и эталоном механики docs/state/inspector_ref/mechanics.json). Данные-93.

  mech_vs_inventory.py ЛОГ... [--out DIR] [--nb]
ЛОГ: файлы вида L1_v0.txt (уровень в имени) или любой журнал с MS/MM; --level N, если в имени уровня нет.
Для каждой стадии сравнивается с описью: механика, прошла ли, персонажи (есть в списке и с моделью), машины,
диалог (есть событие), ролик, предметы (собрано не меньше, чем в описи), гонка (точки, соперники);
для каждой миссии: пройдена, следующая открыта (story +1 у сюжетных; у бонусных не меняется), событие mission_done.
Выход: mech_diff.tsv (одна строка на расхождение: level mission stage вид ожидали получили), mech_diff.md, и с --nb
строки блокнота Опоры (NB<TAB>4<TAB>...). Вердикт (игра/бот) здесь не ставится автоматом: колонка verdict заполняется
известными классами K1-K12 (docs/handoff/МЕХАНИК_37_СТАДИЙ_РАЗБОР_93.md), остальное «проверить»."""
import argparse, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
INV = ROOT / 'docs/state/mechanic_inventory.json'
K37 = ROOT / 'docs/state/mechanic_37_classes.json'
ALIAS = {'timer': 'timer', 'pickupitem': 'pickupitem'}  # имена механик описи = имена msn_objective_name; расхождение само по себе находка


def parse_log(path, level):
    st, ms = {}, {}
    for line in Path(path).read_text(errors='replace').splitlines():
        f = line.split('\t')
        if f[0] == 'MS' and len(f) > 8:
            kv = {x.split('=', 1)[0]: x.split('=', 1)[1] for x in f[8:] if '=' in x}
            ph, sg = f[3].split('.')
            ms[(level, f[2], int(ph), int(sg))] = {'mech': f[4], 'res': f[5], 'why': f[6], 't': f[7], **kv}
        elif f[0] == 'MM' and len(f) > 4:
            kv = {x.split('=', 1)[0]: x.split('=', 1)[1] for x in f[4:] if '=' in x}
            st[(level, f[2])] = {'res': f[3], **kv}
    return ms, st


def load_k37():
    try:
        j = json.loads(K37.read_text())
    except Exception:
        return {}
    out = {}
    for r in j.get('rows', []):
        out[(r.get('level'), r.get('mission'), r.get('stage'))] = '%s | %s' % (r.get('cls', ''), r.get('verdict', ''))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('logs', nargs='+'); ap.add_argument('--out', default='build/mech_vs_inv'); ap.add_argument('--nb', action='store_true')
    ap.add_argument('--level', type=int)
    a = ap.parse_args()
    inv = json.loads(INV.read_text())
    stages, missions = {}, {}
    for L in inv['levels']:
        for m in L['missions']:
            missions[(L['level'], m['id'])] = m
            for s in m['stages']:
                stages[(L['level'], m['id'], s['phase_idx'], s['stage_idx'])] = (m, s)
    ms, mm = {}, {}
    for p in a.logs:
        mt = re.search(r'L(\d)', Path(p).name); lv = a.level or (int(mt.group(1)) if mt else 0)
        x, y = parse_log(p, lv); ms.update(x); mm.update(y)
    k37 = load_k37()
    diffs = []
    def D(lv, mi, st, kind, exp, got, note=''):
        diffs.append({'level': lv, 'mission': mi, 'stage': st, 'kind': kind, 'expected': exp, 'got': got,
                      'known_class': k37.get((lv, mi, st), ''), 'note': note})
    seen = set()
    for key, r in sorted(ms.items()):
        lv, mi, ph, sg = key; s_id = '%d.%d' % (ph, sg); seen.add(key)
        ent = stages.get(key)
        if not ent:
            D(lv, mi, s_id, 'нет в описи', '-', r['mech']); continue
        m, s = ent; e = s['expect']
        if r['mech'] != s['mech']:
            D(lv, mi, s_id, 'механика', s['mech'], r['mech'])
        if r['res'] != 'прошла':
            D(lv, mi, s_id, 'не прошла', 'прошла', r['res'] + (': ' + r['why'] if r['why'] else ''))
        veh0 = [x for x in r.get('veh', '').split(',') if x]
        npcs = [x for x in r.get('npc', '').split(',') if x]; nom = [x for x in r.get('nomodel', '').split(',') if x]
        nolist = not npcs and not veh0   # списки персонажей/машин не сняты: стадия закрылась раньше снимка (0 с) - проверка недоступна, не расхождение
        for n in ([] if nolist else e.get('npcs', [])):
            if n not in npcs: D(lv, mi, s_id, 'персонаж не заведён', n, ','.join(npcs)[:60])
            elif n in nom: D(lv, mi, s_id, 'у персонажа нет модели', n, 'nomodel')
        veh = veh0
        for v in ([] if nolist else e.get('vehicles', [])):
            if not any(x.startswith(v) for x in veh): D(lv, mi, s_id, 'машина не заведена', v, ','.join(veh)[:60])
            elif any(x == v + '(zaglushka)' for x in veh): D(lv, mi, s_id, 'у машины заглушка', v, 'zaglushka')
        dl = int(r.get('dlg', 0) or 0)
        if 'dialogue' in e and dl < 1: D(lv, mi, s_id, 'диалог не прозвучал', '%s %d реп.' % (e['dialogue']['key'], e['dialogue']['lines']), 'dlg=0')
        if 'fmv' in e and int(r.get('fmv', 0) or 0) < 1: D(lv, mi, s_id, 'ролик не показан', e['fmv'], 'fmv=0')
        its = s.get('items')
        if its and its.get('count') and s['mech'] in ('delivery', 'dump'):
            got = int(r.get('collect', 0) or 0)
            if got < its['count']: D(lv, mi, s_id, 'предметов меньше', its['count'], got, 'предел 40 с? (класс K3)')
        if s['mech'] == 'race' and 'checkpoints' in e:
            lap, nit, opp = (r.get('race', '0:0:0').split(':') + ['0', '0', '0'])[:3]
            if int(opp) < e.get('opponents', 0): D(lv, mi, s_id, 'соперников меньше', e.get('opponents', 0), opp, 'opp в MS = число включённых машин')
            if int(nit) != e['checkpoints'] and not (int(lap) and int(nit) * int(lap) == e['checkpoints']): D(lv, mi, s_id, 'контрольных точек', e['checkpoints'], '%s (кругов %s)' % (nit, lap))
    for key in stages:
        if key not in ms and key[0] in {k[0] for k in ms} and key[2:] != (0, 0):   # (0,0): старт миссии, строки MS у него нет по устройству Механика
            D(key[0], key[1], '%d.%d' % key[2:], 'стадия не пройдена ботом (нет строки MS)', '-', '-')
    for (lv, mi), r in sorted(mm.items()):
        m = missions.get((lv, mi))
        if not m: continue
        if r.get('completed') != '1': D(lv, mi, '-', 'миссия не завершена', 'completed=1', r.get('completed'))
        if int(r.get('mission_done_ev', 0) or 0) < 1: D(lv, mi, '-', 'нет события mission_done', '>=1', r.get('mission_done_ev'))
        story = r.get('story', '')
        if '>' in story:
            b0, b1 = [int(x) for x in story.split('>')]
            bonus = any(f in m.get('flags', []) for f in ('bonus', 'streetrace', 'wager', 'transition'))
            last = (not bonus) and m is max((x for (l2, _), x in missions.items() if l2 == lv and not any(f in x.get('flags', []) for f in ('bonus', 'streetrace', 'wager', 'transition'))), key=lambda x: x['mi'])
            if not bonus and not last and b1 != b0 + 1: D(lv, mi, '-', 'следующая не открылась', '%d>%d' % (b0, b0 + 1), story)
            if last and b1 not in (0, b0 + 1): D(lv, mi, '-', 'конец уровня: сюжет не закрыт', '%d>0 или %d' % (b0, b0 + 1), story)
            if bonus and b1 != b0: D(lv, mi, '-', 'бонусная/переходная сдвинула сюжет', '%d>%d' % (b0, b0), story)
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    cols = ['level', 'mission', 'stage', 'kind', 'expected', 'got', 'known_class', 'note']
    (out / 'mech_diff.tsv').write_text('\n'.join('\t'.join(str(d[c]) for c in cols) for d in diffs) + '\n')
    from collections import Counter
    c = Counter(d['kind'] for d in diffs)
    md = ['# Механик против описи (Данные-93)', '', 'Стадий в логах: %d, миссий: %d, расхождений: %d.' % (len(ms), len(mm), len(diffs)), '',
          '| вид | сколько |', '|---|---|'] + ['| %s | %d |' % kv for kv in c.most_common()] + ['', '| уровень | миссия | стадия | вид | ожидали | получили | класс | заметка |', '|---|---|---|---|---|---|---|---|']
    md += ['| ' + ' | '.join(str(d[k]) for k in cols) + ' |' for d in diffs]
    (out / 'mech_diff.md').write_text('\n'.join(md) + '\n')
    if a.nb:
        lines = []
        for k, n in c.most_common():
            ex = [d for d in diffs if d['kind'] == k][:3]
            ev = '; '.join('L%s %s %s: ждали %s, получили %s' % (d['level'], d['mission'], d['stage'], d['expected'], d['got']) for d in ex)
            lines.append('NB\t4\tМеханик против описи: %s\t-\tdiff\t%d шт.; %s' % (k, n, ev[:260]))
        (out / 'nb_lines.tsv').write_text('\n'.join(lines) + '\n')
    print('stages=%d missions=%d diffs=%d' % (len(ms), len(mm), len(diffs)))
    for k, n in c.most_common(): print('%5d  %s' % (n, k))


if __name__ == '__main__':
    main()
