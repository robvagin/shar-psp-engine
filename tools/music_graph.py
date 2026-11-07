"""Mission/race events compiled from the owner's RMS action graph.

SAB MEVT chooses the first alternative; SAP next links preserve the sequence
and the repeat boundary. A repeat(1,1) is one pass, not an infinite loop.
Descriptors are scoped to a region: sharing a file cannot change its next/loop.
The existing stream player and its fixed ring budget are unchanged.
"""
import re
import rms

KINDS = {'start': 0, 'drama': 1, 'win': 2, 'lose': 3,
         'get_out_of_car': 4, '10sec_to_go': 5}
import os
# STREETMUS_OLD=1 — прежний экспорт без музыки уличных гонок (красный контроль tests/levels/mevt_check.py); по умолчанию гонки StreetRace01..03 включены
EVENT = re.compile(r'^(M\d|Bonus' + ('' if os.environ.get('STREETMUS_OLD') == '1' else '|StreetRace0[123]') + ')_(' + '|'.join(KINDS) + ')$', re.I)


def canonical(name):
    return name.replace('_getoutofcar', '_get_out_of_car').replace('_Win', '_win')


def region_plan(comp, ri):
    """Ordered steps and the index of the infinite repeat, if any.

Only forms actually used by these disc mission regions are accepted. Do not
silently flatten a new/unsupported tree into a different tune.
"""
    def visit(ref):
        kind, _, e = comp.el(ref)
        if kind == 'event_stream': return [[comp.owner(e[1])[1]]], None
        if kind == 'logic_and':
            steps, loop = [], None
            for child in comp.store[e[1]]:
                part, repeat = visit(child)
                if loop is not None: raise ValueError('unreachable RMS tail')
                if repeat is not None: loop = len(steps) + repeat
                steps += part
            return steps, loop
        if kind == 'logic_or':
            choices = []
            for child in comp.store[e[1]]:
                part, repeat = visit(child)
                if len(part) != 1 or repeat is not None:
                    raise ValueError('RMS alternative sequence requires graph branching')
                choices += part[0]
            return [choices], None
        if kind == 'logic_repeat':
            part, repeat = visit(e[3])
            if e[1] == e[2] == 0xffffffff:
                if repeat is not None: return part, repeat  # inner loop never returns
                return part, 0
            if e[1] != e[2] or e[1] > 16:
                raise ValueError('random/large repeat in mission RMS')
            if repeat is not None and e[1] != 1:
                raise ValueError('repeated infinite RMS child')
            return part * e[1], repeat
        raise ValueError('unsupported mission RMS node: ' + kind)

    region = comp.pools['regions']['els'][ri]
    layers = comp.store[region[1]]
    if len(layers) != 1: raise ValueError('mission RMS needs simultaneous layers')
    layer = comp.el(layers[0])[2]
    return visit(comp.el(layer[3])[2][0])


def mission_music(level, music_dir):
    comp = rms.read(music_dir / f'l{level}_music.rms')
    tracks, events, report, regions = [], [], {}, {}

    def region_tracks(ri):
        if ri in regions: return regions[ri]
        steps, repeat = region_plan(comp, ri)
        if not steps: return []
        if len(steps[0]) > 3 or any(len(s) != 1 for s in steps[1:]):
            raise ValueError('mission RMS alternatives exceed stream/MEVT capacity')
        names = [[f'mg{ri}_{i}_{j}' for j in range(len(ss))] for i, ss in enumerate(steps)]
        for i, ss in enumerate(steps):
            nxt = names[i + 1][0] if i + 1 < len(steps) else names[repeat][0] if repeat is not None else None
            for j, si in enumerate(ss):
                _, fname = comp.stream_file(si)
                path = music_dir / (fname.replace('\\', '/') + '.rsd')
                if not path.is_file(): raise ValueError('RMS file absent: ' + str(path))
                name = names[i][j]
                tracks.append((name, str(path), nxt == name, None if nxt == name else nxt))
        regions[ri] = names[0]
        return names[0]

    for raw in comp.pools['events']['els']:
        name = canonical(comp.store[raw[0]])
        match = EVENT.fullmatch(name)
        if not match: continue
        kind = KINDS[match.group(2).lower()]
        arrays = comp.store[raw[1]]
        values, state_name = [None], None
        if comp.owner(raw[2]):
            refs = comp.store[comp.el(raw[2])[2][0]]
            if len(refs) != 1: raise ValueError('unsupported RMS multi-state matrix')
            state = comp.el(refs[0])[2]
            state_name = comp.store[state[0]]
            values = [comp.store[v] for v in comp.store[state[1]]]
        if len(values) != len(arrays): raise ValueError('RMS event action count mismatch')
        for index, value in enumerate(values):
            selected = []
            targets = []
            for action in comp.store[arrays[index]]:
                typ, _, data = comp.el(action)
                if typ in ('play_region', 'push_region'):
                    target = comp.owner(data[1])
                    if target: targets.append(target[1])
            if len(targets) > 1: raise ValueError('simultaneous RMS mission regions')
            if targets: selected = region_tracks(targets[0])
            key = name + ('@' + value if value else '')
            if len(key) > 27: raise ValueError('MEVT name too long: ' + key)
            events.append((key, selected, kind))
            if value and index == 0: events.append((name, selected, kind))
            report[key] = dict(state=state_name, value=value,
                               region=comp.region_name(targets[0]) if targets else None,
                               tracks=selected)
    return tracks, events, {'graph': report}
