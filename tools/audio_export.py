#!/usr/bin/env python3
"""SHAR -> PSP: disc audio inventory, PSP sound pack export, verification.

Data sources (owner's own PS2 disc, extracted): hit-and-run-web/source/game/
  *.rsd  RSD4VAG: 0x800-byte header (magic, channels @8, bits @12, rate @16), then PS-ADPCM (VAG) blocks,
         16 bytes = 28 samples, stereo interleaved per 16-byte block (L,R,L,R...). Same layout ffmpeg's rsd demuxer uses.
  sound/scripts/*.spt   text scripts: daSoundResourceData (files, trim, looping, pitch range, streaming),
                        carSoundParameters (engine model), globalSettings (bus volumes), positionalSoundSettings.
  sound/music/l?_music.rms, sound/ambience/ambience.rms  binary "radmusic_comp" compositions (regions/events);
                        here only the file references are read (per-level music sets).

Runtime format (see viewer/audio.c): the ORIGINAL VAG blocks are copied bit-for-bit (no re-encode, no loss).
  build/audio/sfx.sab   resident bank: named resources (variants, trim, pitch range, loop), files, car engine params.
  build/audio/music.sap streaming pack: music + ambience tracks (stereo; mono is widened by duplicating blocks),
                        2048-aligned, intro->loop chaining, loop points with stored ADPCM history.

Usage (from the project root, with .venv active):
  python tools/audio_export.py inventory [--out DIR]          catalog of all disc audio (json + markdown)
  python tools/audio_export.py pack [--out build/audio] [--car famil_v] [--full-level-music]
  python tools/audio_export.py verify [--pack build/audio] [--host BIN] [--work DIR]
"""
import argparse, glob, json, os, re, struct, subprocess, sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, 'hit-and-run-web', 'source', 'game')
SND = os.path.join(GAME, 'sound')
SCRIPTS = os.path.join(SND, 'scripts')
OUT_RATE = 44100

# ---------------------------------------------------------------- RSD / VAG
def read_rsd(path):
    with open(path, 'rb') as f:
        b = f.read()
    magic = b[:8]
    ch, bits, rate = struct.unpack_from('<III', b, 8)
    data = b[0x800:]
    if magic != b'RSD4VAG ':
        raise ValueError('%s: unsupported %r' % (path, magic))
    nblk = len(data) // (16 * ch)
    return dict(path=path, ch=ch, bits=bits, rate=rate, data=data[:nblk * 16 * ch], nblk=nblk,
                nsamp=nblk * 28, bytes=len(b))

VAG_F = ((0, 0), (60, 0), (115, -52), (98, -55), (122, -60))

def vag_decode_py(data, ch, blk0=0, nblk=None, hist=None):
    """Reference PS-ADPCM decoder, bit-exact with ffmpeg adpcm_psx (history keeps the unclipped value,
    prediction divided by 64 with truncation toward zero). Slow; used for short spans only."""
    total = len(data) // (16 * ch)
    if nblk is None:
        nblk = total - blk0
    out = [[0] * (nblk * 28) for _ in range(ch)]
    for c in range(ch):
        s1, s2 = (hist[c] if hist else (0, 0))
        k = 0
        for i in range(blk0, blk0 + nblk):
            o = (i * ch + c) * 16
            hd = data[o]; sh = hd & 15; f = hd >> 4; fl = data[o + 1] & 7
            f0, f1 = VAG_F[f if f < 5 else 0]
            for n in range(28):
                by = data[o + 2 + (n >> 1)]
                nib = (by >> 4) if n & 1 else (by & 15)
                if nib >= 8:
                    nib -= 16
                smp = 0
                if fl < 7:
                    pr = s1 * f0 + s2 * f1
                    pr = -((-pr) // 64) if pr < 0 else pr // 64
                    smp = ((nib << 12) >> sh) + pr
                out[c][k] = max(-32768, min(32767, smp)); k += 1
                s2, s1 = s1, smp
    return out

def ffmpeg_decode(path, rate=None):
    """Reference decode via ffmpeg (rsd demuxer + adpcm_psx). Returns numpy int16 [n, ch]."""
    import numpy as np
    cmd = ['ffmpeg', '-v', 'error', '-nostdin', '-i', path]
    if rate:
        cmd += ['-af', 'aresample=%d:resampler=swr:filter_size=128:phase_shift=12:cutoff=0.97' % rate]
    cmd += ['-f', 's16le', '-']
    raw = subprocess.run(cmd, capture_output=True, check=True).stdout
    ch = read_rsd(path)['ch']
    return np.frombuffer(raw, np.int16).reshape(-1, ch)

def hist_at(data, ch, blk):
    """ADPCM history (s1, s2) per channel just before block `blk` (exact, by decoding from the start)."""
    if blk == 0:
        return [(0, 0)] * ch
    pcm = vag_decode_py(data, ch, 0, blk)
    return [(pcm[c][-1], pcm[c][-2]) for c in range(ch)]

def seam_search(pcm, max_head_blocks=4, max_tail=84):
    """Choose loop start S (sample) and loop end E (exclusive) so the jump E-1 -> S looks like ordinary signal.
    Metric: 2nd-order residual at the seam |x[S] - (2x[E-1] - x[E-2])| (summed over channels), reported as its
    percentile rank among the same residuals inside the clip (rank 50 = a perfectly ordinary sample step;
    rank 100 = sharper than anything in the clip = audible click). Head: S >= 28 skips the 28 silent samples of
    VAG block 0; tail: E may drop up to 3 blocks of ADPCM padding."""
    import numpy as np
    x = pcm.astype(np.int64)
    n = len(x)
    d2 = np.sort(np.abs(x[2:] - 2 * x[1:-1] + x[:-2]).sum(axis=1))
    def rank(v):
        return 100.0 * float(np.searchsorted(d2, v, side='left')) / max(len(d2), 1)
    def res(S, E):
        pred = 2 * x[E - 1] - x[E - 2]
        return int(np.abs(x[S] - pred).sum())
    # whole-file restart (what a naive player does): E-1 -> 28 zeros of block 0 -> x[28]
    naive = max(int(np.abs(2 * x[n - 1] - x[n - 2]).sum()), int(np.abs(x[28]).sum()), int(np.abs(x[29] - 2 * x[28]).sum()))
    best = None
    for S in range(28, 28 * (max_head_blocks + 1)):
        for E in range(n - max_tail, n + 1):
            key = (res(S, E), S, -E)
            if best is None or key < best[0]:
                best = (key, S, E)
    r = best[0][0]
    return dict(start=best[1], end=best[2], residual=r, rank=round(rank(r), 1),
                naive_residual=naive, naive_rank=round(rank(naive), 1), trimmed_tail=n - best[2])

# ---------------------------------------------------------------- .spt scripts
def parse_spt(path):
    """-> list of (type, name, [(method, args, option)])"""
    txt = open(path, 'r', encoding='latin-1').read().replace('\r', '')
    objs = []
    for m in re.finditer(r'create\s+(\w+)\s+named\s+(\S+)\s*\{(.*?)\}', txt, re.S):
        calls = []
        for line in m.group(3).split('\n'):
            cm = re.match(r'\s*(\w+)\s*\((.*)\)\s*(?:option\s+(\w+))?', line)
            if not cm:
                continue
            args = re.findall(r'"[^"]*"|[-\w.]+', cm.group(2))
            args = [a[1:-1] if a.startswith('"') else a for a in args]
            calls.append((cm.group(1), args, cm.group(3)))
        objs.append((m.group(1), m.group(2), calls))
    return objs

def load_scripts(lang_skip=('fr', 'ge', 'sp')):
    res, cars, pos, glob_ = {}, {}, {}, {}
    for p in sorted(glob.glob(os.path.join(SCRIPTS, '*.spt'))):
        base = os.path.basename(p)[:-4]
        if any(base.endswith(s) and base[:-2] in ('dialog', 'nis', 'interactive_props') for s in lang_skip):
            continue
        for typ, name, calls in parse_spt(p):
            if typ == 'daSoundResourceData':
                r = dict(name=name, script=base, files=[], trim=1.0, loop=False, stream=False, pitch=(1.0, 1.0))
                for meth, a, opt in calls:
                    if opt not in (None, 'PS2'):
                        continue
                    if meth == 'AddFilename': r['files'].append(a[0].replace('\\', '/'))
                    elif meth == 'SetTrim': r['trim'] = float(a[0])
                    elif meth == 'SetLooping': r['loop'] = a[0] == 'true'
                    elif meth == 'SetStreaming': r['stream'] = a[0] == 'true'
                    elif meth == 'SetPitchRange': r['pitch'] = (float(a[0]), float(a[1]))
                res[name] = r
            elif typ == 'carSoundParameters':
                c = dict(name=name, shift={}, lo={}, hi={}, drop={})
                for meth, a, opt in calls:
                    if opt not in (None, 'PS2'):
                        continue
                    if meth == 'SetShiftPoint': c['shift'][int(a[0])] = float(a[1])
                    elif meth == 'SetGearPitchRange': c['lo'][int(a[0])] = float(a[1]); c['hi'][int(a[0])] = float(a[2])
                    elif meth == 'SetGearShiftPitchDrop': c['drop'][int(a[0])] = float(a[1])
                    else: c[meth[3:]] = a[0] if len(a) == 1 else a
                cars[name] = c
            elif typ == 'positionalSoundSettings':
                d = {meth[3:]: a[0] for meth, a, opt in calls}
                pos[name] = d
            elif typ == 'globalSettings':
                for meth, a, opt in calls:
                    if opt in (None, 'PS2'):
                        glob_[meth[3:]] = a[0]
    return res, cars, pos, glob_

def game_path(rel):
    """Script paths are case-insensitive and relative to the disc root."""
    rel = rel.replace('\\', '/')
    return _ci_index().get(rel.lower())   # canonical spelling (the Mac file system ignores case, the index does not)

_CI = None
def _ci_index():
    global _CI
    if _CI is None:
        _CI = {}
        for p in glob.glob(os.path.join(GAME, '**', '*.rsd'), recursive=True):
            _CI[os.path.relpath(p, GAME).replace('\\', '/').lower()] = p
    return _CI

def rms_files(rms_path, base_dir):
    """File references of a radmusic composition (strings that name an existing .rsd under base_dir)."""
    names = {}
    for p in glob.glob(os.path.join(base_dir, '**', '*.rsd'), recursive=True):
        names[os.path.relpath(p, base_dir)[:-4].replace('\\', '/').lower()] = p
    b = open(rms_path, 'rb').read()
    out = []
    for s in re.findall(rb'[\x20-\x7e]{3,}', b):
        k = s.decode().replace('\\', '/').lower()
        if k in names and names[k] not in out:
            out.append(names[k])
    return out

# ---------------------------------------------------------------- inventory
def cmd_inventory(a):
    res, cars, pos, gl = load_scripts()
    files = sorted(_ci_index().values())
    by_file = defaultdict(list)                  # file -> resources that use it
    for r in res.values():
        for f in r['files']:
            p = game_path(f)
            if p:
                by_file[p].append(r['name'])
    music_level = {}
    for lv in range(1, 8):
        for p in rms_files(os.path.join(SND, 'music', 'l%d_music.rms' % lv), os.path.join(SND, 'music')):
            music_level.setdefault(p, []).append(lv)
    amb_rms = set(rms_files(os.path.join(SND, 'ambience', 'ambience.rms'), os.path.join(SND, 'ambience')))
    engine_files, horn_files = set(), set()
    for c in cars.values():
        for key, dst in (('EngineClipName', engine_files), ('EngineIdleClipName', engine_files),
                         ('HornClipName', horn_files), ('DamagedEngineClipName', engine_files)):
            r = res.get(c.get(key, ''))
            if r:
                for f in r['files']:
                    p = game_path(f)
                    if p: dst.add(p)

    def category(p):
        rel = os.path.relpath(p, GAME).replace('\\', '/')
        parts = rel.split('/')
        if parts[0] == 'sound':
            sub = parts[1]
            if sub == 'music':
                return 'music: in-game (level .rms)' if p in music_level else 'music: frontend/unreferenced'
            if sub == 'carsound':
                if p in engine_files: return 'car: engine loops'
                if p in horn_files: return 'car: horns'
                return 'car: skids/sirens/other'
            if sub == 'soundfx': return 'sfx: ' + parts[2]
            if sub == 'ambience': return 'ambience loops'
            if sub == 'nis': return 'nis (in-game cutscene audio)'
            return 'frontend/ui (sound root)'
        if parts[0] == 'conversations': return 'dialogue: conversations'
        return 'dialogue: character lines'

    cats = defaultdict(lambda: dict(count=0, seconds=0.0, bytes=0, rates=defaultdict(int), ch=defaultdict(int),
                                    looping=0, streamed=0, referenced=0))
    rows = []
    for p in files:
        try:
            r = read_rsd(p)
        except ValueError:
            continue
        c = cats[category(p)]
        sec = r['nsamp'] / r['rate']
        users = by_file.get(p, [])
        c['count'] += 1; c['seconds'] += sec; c['bytes'] += r['bytes']
        c['rates'][r['rate']] += 1; c['ch'][r['ch']] += 1
        if any(res[u]['loop'] for u in users): c['looping'] += 1
        if any(res[u]['stream'] for u in users): c['streamed'] += 1
        if users or p in music_level or p in amb_rms: c['referenced'] += 1
        rows.append(dict(file=os.path.relpath(p, GAME), cat=category(p), rate=r['rate'], ch=r['ch'],
                         seconds=round(sec, 3), bytes=r['bytes'], used_by=users[:8]))
    out = a.out or os.path.join(os.environ.get('TMPDIR', '/tmp'), 'audio_inventory')
    os.makedirs(out, exist_ok=True)
    summary = {k: dict(v, rates=dict(v['rates']), ch=dict(v['ch'])) for k, v in sorted(cats.items())}
    json.dump(dict(summary=summary, files=rows, cars=cars, globals=gl, positional=pos,
                   resources={k: v for k, v in res.items() if not k.startswith(('D_', 'P_', 'C_', 'N_'))}),
              open(os.path.join(out, 'audio_inventory.json'), 'w'), indent=1)
    lines = ['| category | files | duration | bytes | rates Hz (files) | mono/stereo | looping | streamed | referenced |',
             '|---|---|---|---|---|---|---|---|---|']
    T = dict(count=0, seconds=0, bytes=0)
    for k, v in sorted(cats.items()):
        m, s = divmod(int(v['seconds']), 60); h, m = divmod(m, 60)
        lines.append('| %s | %d | %d:%02d:%02d | %.1f MB | %s | %d/%d | %d | %d | %d |' % (
            k, v['count'], h, m, s, v['bytes'] / 1e6, ' '.join('%d(%d)' % kv for kv in sorted(v['rates'].items())),
            v['ch'].get(1, 0), v['ch'].get(2, 0), v['looping'], v['streamed'], v['referenced']))
        for kk in T: T[kk] += v[kk]
    m, s = divmod(int(T['seconds']), 60); h, m = divmod(m, 60)
    lines.append('| **total** | %d | %d:%02d:%02d | %.1f MB | | | | | |' % (T['count'], h, m, s, T['bytes'] / 1e6))
    lines += ['', '| music by level (.rms references; shared tracks counted in every level) | files | duration | MB |',
              '|---|---|---|---|']
    for lv in range(1, 8):
        fs = [p for p, l in music_level.items() if lv in l]
        sec = sum(read_rsd(p)['nsamp'] / 24000.0 for p in fs)
        lines.append('| level %d | %d | %d:%02d | %.1f |' % (lv, len(fs), sec // 60, sec % 60,
                                                           sum(os.path.getsize(p) for p in fs) / 1e6))
    ntypes = defaultdict(int)
    for r in res.values():
        ntypes['multi-variant (random pick)'] += len(r['files']) > 1
        ntypes['pitch range (random pitch)'] += r['pitch'] != (1.0, 1.0)
        ntypes['looping'] += r['loop']
        ntypes['streamed in original'] += r['stream']
    lines += ['', 'resources in .spt: %d named sounds, %d car engine models, %d positional emitters' % (
        len(res), len(cars), len(pos)), 'resource flags: ' + ', '.join('%s %d' % kv for kv in ntypes.items())]
    md = '\n'.join(lines)
    open(os.path.join(out, 'audio_inventory.md'), 'w').write(md + '\n')
    print(md)
    print('\nwritten:', os.path.join(out, 'audio_inventory.json'), os.path.join(out, 'audio_inventory.md'))

# ---------------------------------------------------------------- pack
# Default level-1 selection. Music chain "intro -> main loop" follows the region names in l1_music.rms
# (Level_Intro -> Sunday_Drive_Intro -> Sunday_Drive). Mission 1 = Collection02_* (M1_* regions).
MUSIC_L1 = [
    # name, file under sound/music, loop, next
    ('sunday_drive_intro', 'sunday_drive_intro', False, 'sunday_drive'),
    ('sunday_drive', 'sunday_drive', True, None),
    ('sunday_drive_end_sus1', 'sunday_drive_end_sus1', False, None),
    ('sunday_drive_end_sus2', 'sunday_drive_end_sus2', False, None),
    ('sunday_drive_end_sus3', 'sunday_drive_end_sus3', False, None),
    ('collection02_pickup', 'collection02_pickup', False, 'collection02_main'),
    ('collection02_main', 'collection02_main', True, None),
    ('collection02_end_pos', 'collection02_end_pos_b1_3', False, None),
    ('collection02_end_neg', 'collection02_end_neg_b4', False, None),
    ('simpsons_theme', 'simpsons_theme', True, None),
    ('horn_triumph', 'horn_triumph', False, None),
    ('tuba_001', 'homer/tuba_001', False, None),
    ('tuba_002', 'homer/tuba_002', False, None),
    ('tuba_003', 'homer/tuba_003', False, None),
]
AMBIENCE_L1 = [('amb_suburb_day', 'suburb_day_loop'), ('amb_city_busy', 'city_busy_loop_05')]
SFX_COMMON = [
    'skid', 'dirt_skid', 'gearshift', 'horn', 'fire', 'car_jump',
    'small_car_crash', 'medium_car_crash', 'large_car_crash', 'hit_concrete', 'car_hit_hydrant',
    'car_hit_mailbox', 'car_hit_tree', 'car_hit_streetlamp', 'car_hit_pedestrian', 'car_hit_garbage_can',
    'car_hit_crate', 'hit_shrub', 'car_hit_stopsign', 'car_hit_telepole', 'car_hit_phonestop', 'car_hit_link_fence',
    'coin_collect_01', 'card_collect', 'wrench_collect', 'mission_complete', 'busted', 'stage_complete',
    'accept', 'back', 'scroll', 'pause_on', 'pause_off', 'feet_concrete', 'jump',
]
BUS = dict(sfx=0, car=1, music=2, ambience=3, dialog=4)

def pack_name(s, n):
    b = s.encode()[:n - 1]
    return b + b'\0' * (n - len(b))

def loop_points(r, pcm=None):
    import numpy as np
    if pcm is None:
        pcm = ffmpeg_decode(r['path'])
    pcm = np.asarray(pcm)
    s = seam_search(pcm)
    hist = hist_at(r['data'], r['ch'], s['start'] // 28)
    return s, hist

BUS_OF_SCRIPT = {'carsound': 'car', 'car_tune': 'car'}

def build_pack(out, car_name, sfx_names, music_sel, amb_sel=(), dialog_sel=(), ext=b'', res_bus=None, quiet=False,
               pos_sel=(), extra_cars=(), car_sel=()):
    """Writes out/sfx.sab (resident bank) + out/music.sap (streams) + out/pack_report.json.
    sfx_names: resource names from the .spt scripts (car clips of car_name are added first);
    music_sel: [(name, abs path, loop, next name or None)]; amb_sel: [(name, abs path)] (looping);
    dialog_sel: [(name, abs path)] (one pass, stream bus 2); pos_sel: [(name, abs path)] positional loops (bus 3);
    ext: extension blob (SAB header @80/@84); car_sel: [(owner, track, path, trim)] streaming car overlays (bus 4);
    res_bus: {resource name: 'sfx'|'car'|'ambience'|'dialog'} overrides."""
    import numpy as np
    os.makedirs(out, exist_ok=True)
    res, cars, pos, gl = load_scripts()
    car = cars[car_name]
    car_names = list(dict.fromkeys([car_name, *extra_cars]))
    # Preserve the player's row and resource order; append authored traffic profiles.
    for name in car_names:
        if name not in cars:
            raise ValueError('No authored car sound profile: ' + name)
    need = [car.get(k) for k in ('EngineClipName', 'EngineIdleClipName', 'HornClipName', 'DamagedEngineClipName',
                                 'CarDoorOpenClipName', 'CarDoorCloseClipName')]
    extra_need = [cars[name].get(k) for name in car_names[1:]
                  for k in ('EngineClipName', 'EngineIdleClipName')]
    # правка 413а: свой звук заноса машины (carSoundParameters RoadSkidClipName / DirtSkidClipName: frink_v, hbike_v, ship, witchcar,
    # mono_v, huskA — парящие и остов; у hbike_v/witchcar/huskA это blank.rsd, тишина) — для каждой машины пакета, в том числе игрока
    extra_need += [cars[name].get(k) for name in car_names for k in ('RoadSkidClipName', 'DirtSkidClipName')]
    # правка 413 (хвост, OverlayClipName): SLES_518.97 VehicleSoundPlayer — активация (вход в машину, 0x230e48) последним шагом
    # запускает клип поля +100 параметров машины (0x231254), если имя не пустое; клипы петлёй (SetLooping). В пакет — у машин пакета
    # с не потоковым клипом (rocket, cCell*_overlay, cVan_overlay, pizza_car_overlay). Потоковые (ship, icecream, IStruck, nuctruck,
    # votetruc: SetStreaming true) приходят отдельным car_sel из sound_export в music.sap. Resident таблица — EXT 'COVL'
    def _ovl(name):
        o = cars[name].get('OverlayClipName')
        return o if o and o in res and not res[o]['stream'] else None
    extra_need += [_ovl(name) for name in car_names]
    # правка 413 (гудок): клип гудка каждой машины пакета (HornClipName; по умолчанию SLES 0x231860 — «horn»)
    extra_need += [cars[name].get('HornClipName') or 'horn' for name in car_names]
    names = []
    for n in need + list(sfx_names) + extra_need:
        if n and n in res and n not in names:
            names.append(n)
    missing = [n for n in need + list(sfx_names) + extra_need if n and n not in res]
    # ---- resident bank
    files, fidx, resrows, report = [], {}, [], dict(sfx=[], music=[], missing=missing)
    blob = bytearray()
    for n in names:
        r = res[n]
        for f in r['files']:
            p = game_path(f)
            if not p:
                continue
            if p not in fidx:
                rs = read_rsd(p)
                ls, le, hist, seam = 0, rs['nsamp'], [(0, 0)] * rs['ch'], None
                if r['loop']:
                    # круг 4 (банк в 2112 КБ, правка №1069): зацикленный клип длиннее 8 с (сирена пожарной и скорой, 14,2 с, 195 КБ) режется до окна 4,5 с с поиском
                    # шва внутри окна; хвост за концом петли не хранится. LOOPCAP_OLD=1 — целиком
                    if os.environ.get('LOOPCAP_OLD') != '1' and rs['ch'] == 1 and rs['nsamp'] / rs['rate'] > 8.0:
                        import numpy as np
                        K = int(4.5 * rs['rate']); K -= K % 28
                        seam, hist = loop_points(rs, np.asarray(ffmpeg_decode(rs['path']))[:K])
                        nb = ((seam['end'] + 27) // 28) * 16
                        rs = dict(rs); rs['data'] = rs['data'][:nb]; rs['nsamp'] = nb // 16 * 28
                    else:
                        seam, hist = loop_points(rs)
                    ls, le = seam['start'], seam['end']
                off = len(blob)
                blob += rs['data']
                blob += b'\0' * (-len(blob) % 64)
                fidx[p] = len(files)
                files.append((off, rs, ls, le, hist))
                report['sfx'].append(dict(file=os.path.relpath(p, GAME), res=n, rate=rs['rate'], ch=rs['ch'],
                                          seconds=round(rs['nsamp'] / rs['rate'], 3), bytes=len(rs['data']),
                                          loop=[ls, le] if r['loop'] else None,
                                          seam=seam))
        vf = [fidx[game_path(f)] for f in r['files'] if game_path(f)]
        if not vf:
            report['missing'].append(n + ' (no files on disc)')
        resrows.append((n, vf, r))
    vtab = []
    resbin = bytearray()
    for n, vf, r in resrows:
        b = (res_bus or {}).get(n)
        if b is None:
            b = 'car' if (r['script'] in BUS_OF_SCRIPT or n in (car.get('EngineClipName'), car.get('HornClipName'))
                          or (r['files'] and r['files'][0].lower().startswith('sound/carsound'))) else 'sfx'
        flags = (1 if r['loop'] else 0) | (2 if r['stream'] else 0)
        resbin += pack_name(n, 24) + struct.pack('<HHfffII', len(vtab), len(vf), r['trim'], r['pitch'][0],
                                                   r['pitch'][1], flags, BUS[b])
        vtab += vf
    filebin = bytearray()
    for off, rs, ls, le, hist in files:
        h = [v for pair in hist for v in pair] + [0, 0] * (2 - rs['ch'])
        filebin += struct.pack('<IIIIIHBB4h', off, rs['nblk'], rs['nsamp'], ls, le, rs['rate'], rs['ch'], 0,
                               *[max(-32768, min(32767, v)) for v in h])
    vbin = struct.pack('<%dH' % len(vtab), *vtab) if vtab else b''
    vbin += b'\0' * (-len(vbin) % 16)

    def rid(n):
        return next((i for i, (nn, _, _) in enumerate(resrows) if nn == n), -1)
    def encode_car(name):
        car = cars[name]
        row = pack_name(name, 16) + struct.pack('<8h', rid(car.get('EngineClipName')), rid(car.get('EngineIdleClipName')),
                                                         rid(car.get('HornClipName') or 'horn'), rid(car.get('DamagedEngineClipName')),
                                                         rid(car.get('CarDoorOpenClipName')), rid(car.get('CarDoorCloseClipName')),
                                                         rid(car.get('RoadSkidClipName') or gl.get('SkidRoadClipName', 'skid')),
                                                         rid(car.get('DirtSkidClipName') or gl.get('SkidDirtClipName', 'dirt_skid')))
        fl = lambda k, d: float(car.get(k, d)) if not isinstance(car.get(k, d), list) else float(car.get(k)[0])
        row += struct.pack('<4f', fl('IdleEnginePitch', 0.5), fl('ReversePitchCapKmh', 50),
                              float(car.get('ReversePitchRange', [0.5, 1.3])[0]), float(car.get('ReversePitchRange', [0.5, 1.3])[1]))
        for key in ('shift', 'lo', 'hi', 'drop'):
            row += struct.pack('<5f', *[car[key].get(g, car[key].get(g - 1, 1.0)) for g in range(1, 6)])
        row += struct.pack('<6f', fl('AttackTimeMsecs', 100), fl('DelayTimeMsecs', 40), fl('DecayTimeMsecs', 250),
                              fl('DecayFinishTrim', 0.8), fl('DownshiftDamperSize', 0.05), fl('ClipRPM', 0))
        # сверка g08-sound круг 2 (g12 P10-27): умолчание DownshiftDamperSize 0,05 (было 0,1)
        assert len(row) == 152, len(row)
        return row
    carbin = b''.join(encode_car(name) for name in car_names)
    # 'COVL' (правка 413): i16 номер ресурса наложения (OverlayClipName, не потоковый) на профиль, в порядке профилей; -1 — нет.
    # Дописывается к EXT, собранному tools/sound_export.py (заголовок SXT1 пересобирается со сдвигом смещений)
    covl = struct.pack('<%dh' % len(car_names), *[rid(_ovl(name)) if _ovl(name) else -1 for name in car_names])
    if ext[:4] == b'SXT1' and any(_ovl(name) for name in car_names):
        ns, = struct.unpack_from('<I', ext, 4); body = ext[8 + 16 * ns:]
        secs = [(ext[8 + 16 * i:12 + 16 * i],) + struct.unpack_from('<III', ext, 12 + 16 * i) for i in range(ns)]
        shift = 16; head = b'SXT1' + struct.pack('<I', ns + 1)
        for tag, off, cnt, stride in secs: head += tag + struct.pack('<III', off + shift, cnt, stride)
        body = bytes(body) + b'\0' * (-len(body) % 16)
        head += b'COVL' + struct.pack('<III', 8 + 16 * (ns + 1) + len(body), len(car_names), 2)
        ext = head + body + covl
    # COVS: profile -> first/count in COVF. COVF: i16 track, pad, f32 resource trim.
    # Stream variants are appended after positional tracks, leaving all old IDs stable.
    if car_sel:
        base_track = len(music_sel) + len(amb_sel) + len(dialog_sel) + len(pos_sel)
        variants = bytearray(); profiles = bytearray()
        for name in car_names:
            first = len(variants) // 8
            for i, (owner, _, _, trim) in enumerate(car_sel):
                if owner == name: variants += struct.pack('<hhf', base_track + i, 0, trim)
            profiles += struct.pack('<HH', first, len(variants)//8-first)
        if not ext: ext = b'SXT1' + struct.pack('<I', 0)
        assert ext[:4] == b'SXT1'
        for tag, data, stride in ((b'COVS', profiles, 4), (b'COVF', variants, 8)):
            ns, = struct.unpack_from('<I', ext, 4); body = ext[8+16*ns:]
            head = b'SXT1' + struct.pack('<I', ns+1)
            for i in range(ns):
                q=8+16*i; off,cnt,st=struct.unpack_from('<III',ext,q+4)
                head += ext[q:q+4] + struct.pack('<III',off+16,cnt,st)
            body += bytes(-len(body)%16)
            head += tag + struct.pack('<III',8+16*(ns+1)+len(body),len(data)//stride,stride)
            ext = head+body+data
    g = lambda k, d: float(gl.get(k, d))
    busv = [g('MasterVolume', 1), g('SfxVolume', 1), g('CarVolume', 1), g('MusicVolume', 1), g('AmbienceVolume', 1),
            g('DialogueVolume', 1)]
    hdr_size = 96
    resOff = hdr_size
    vOff = resOff + len(resbin)
    fileOff = vOff + len(vbin)
    carOff = fileOff + len(filebin)
    extOff = (carOff + len(carbin) + 15) & ~15
    dataOff = (extOff + len(ext) + 63) & ~63
    hdr = b'SAB1' + struct.pack('<IIIIIIIIII', 1, len(resrows), len(files), len(car_names), resOff, vOff, fileOff, carOff, dataOff,
                                len(blob))
    hdr += struct.pack('<6f3f', *busv, g('PeeloutMin', 0.1), g('PeeloutMax', 1), g('PeeloutMaxTrim', 0.8))
    hdr += struct.pack('<II', extOff if ext else 0, len(ext))          # @80 extension (tools/sound_export.py)
    hdr += b'\0' * (hdr_size - len(hdr))
    sab = hdr + resbin + vbin + filebin + carbin
    sab += b'\0' * (extOff - len(sab)) + (ext or b'')
    sab += b'\0' * (dataOff - len(sab)) + blob
    open(os.path.join(out, 'sfx.sab'), 'wb').write(sab)

    # ---- streaming pack (music + ambience + dialogue), always stereo
    tracks = [dict(name=n, path=p, loop=lp, next=nx, bus=0) for n, p, lp, nx in music_sel]
    tracks += [dict(name=n, path=p, loop=True, next=None, bus=1) for n, p in amb_sel]
    tracks += [dict(name=n, path=p, loop=False, next=None, bus=2) for n, p in dialog_sel]
    tracks += [dict(name=n, path=p, loop=True, next=None, bus=3) for n, p in pos_sel]
    tracks += [dict(name=n, path=p, loop=True, next=None, bus=4) for _, n, p, _ in car_sel]
    body = bytearray()
    TBL = 64
    base = 64 + TBL * len(tracks)
    base = (base + 2047) & ~2047
    rows = []
    payload_offsets = {}  # same VAG file, distinct event loop/next descriptors
    for t in tracks:
        rs = read_rsd(t['path'])
        d = rs['data']
        if rs['ch'] == 1:  # widen losslessly: duplicate each 16-byte block for L and R
            d = b''.join(d[i:i + 16] * 2 for i in range(0, len(d), 16))
        key = os.path.realpath(t['path'])
        off = payload_offsets.get(key)
        if off is None or os.environ.get('MUSIC_GRAPH_OLD') == '1':
            off = base + len(body)
            body += d
            body += b'\0' * (-len(body) % 2048)
            payload_offsets[key] = off
        ls, le, hist, seam = 0, rs['nsamp'], [(0, 0)] * 2, None
        if t['loop']:
            seam, h = loop_points(rs)
            ls, le = seam['start'], seam['end']
            hist = h if rs['ch'] == 2 else [h[0], h[0]]
        rows.append((t, rs, off, ls, le, hist))
        report['music'].append(dict(name=t['name'], file=os.path.relpath(t['path'], GAME), rate=rs['rate'],
                                    src_ch=rs['ch'], seconds=round(rs['nsamp'] / rs['rate'], 2), bytes=len(d),
                                    loop=[ls, le] if t['loop'] else None, next=t['next'], bus=t['bus'],
                                    seam=seam))
    names_ = [t['name'] for t in tracks]
    assert len(set(names_)) == len(names_) and max(map(len, names_ or [''])) <= 27, 'track names must be unique, <= 27 chars'
    tbl = bytearray()
    for t, rs, off, ls, le, hist in rows:
        nxt = names_.index(t['next']) if t['next'] else -1
        h = [v for pair in hist for v in pair]
        tbl += pack_name(t['name'], 28) + struct.pack('<IIIIIHBBhh4h', off, rs['nblk'], rs['nsamp'], ls, le, rs['rate'],
                                                      2, 1 if t['loop'] else 0, nxt, t['bus'],
                                                      *[max(-32768, min(32767, v)) for v in h])
    assert len(tbl) == TBL * len(tracks)
    sap = b'SAP1' + struct.pack('<III', 1, len(tracks), 64) + b'\0' * 48 + tbl
    sap += b'\0' * (base - len(sap)) + body
    open(os.path.join(out, 'music.sap'), 'wb').write(sap)
    report['sab_bytes'] = len(sab); report['sap_bytes'] = len(sap)
    report['resident_audio_bytes'] = len(blob)
    report['car'] = car_name
    report['cars'] = car_names
    report['resources'] = [n for n, _, _ in resrows]
    json.dump(report, open(os.path.join(out, 'pack_report.json'), 'w'), indent=1)
    if not quiet:
        print('sfx.sab  %7.1f KB  %d resources, %d files (resident)' % (len(sab) / 1024, len(resrows), len(files)))
        print('music.sap %6.1f MB  %d tracks (streamed)' % (len(sap) / 1048576, len(tracks)))
        if report['missing']:
            print('not in the disc scripts (skipped):', ', '.join(report['missing']))
        for r in report['sfx'] + report['music']:
            if r.get('loop'):
                sm = r['seam']
                print('  loop %-34s %7d..%-8d seam rank %5.1f%% (res %5d); whole-file restart: rank %5.1f%% + 1.2 ms gap' % (
                    r.get('name') or r['res'] + ':' + os.path.basename(r['file']), sm['start'], sm['end'], sm['rank'],
                    sm['residual'], sm['naive_rank']))
    return report

def cmd_pack(a):
    mdir = os.path.join(SND, 'music')
    sel = list(MUSIC_L1)
    if a.full_level_music:
        have = {f for _, f, _, _ in sel}
        for p in rms_files(os.path.join(mdir, 'l1_music.rms'), mdir):
            k = os.path.relpath(p, mdir)[:-4].lower()
            if k not in have:
                sel.append((k.replace('/', '_'), k, False, None))
    music = [(n, os.path.join(mdir, f + '.rsd'), lp, nx) for n, f, lp, nx in sel]
    amb = [(n, os.path.join(SND, 'ambience', f + '.rsd')) for n, f in AMBIENCE_L1]
    build_pack(a.out, a.car, SFX_COMMON, music, amb)

# ---------------------------------------------------------------- verify
def cmd_verify(a):
    """Run the PSP runtime code on the Mac (tests/audio/host, AUDIO_HOST build of viewer/audio.c) over the pack
    and compare with ffmpeg: 1) bit-exact decode of every file/track (through the stream ring for tracks),
    2) resampler quality at 44.1 kHz vs soxr, 3) loop seams rendered through the mixer."""
    import numpy as np
    host = a.host or os.path.join(ROOT, 'tests', 'audio', 'host', 'audio_host')
    work = a.work or os.path.join(os.environ.get('TMPDIR', '/tmp'), 'audio_verify')
    os.makedirs(work, exist_ok=True)
    for f in glob.glob(os.path.join(work, '*.raw')) + glob.glob(os.path.join(work, '*.seams')):
        os.remove(f)
    rep = json.load(open(os.path.join(a.pack, 'pack_report.json')))
    subprocess.run([host, 'dump', a.pack, work], check=True)
    ok = True
    results = []

    def cmp(ref, got):
        n = len(ref)
        if len(got) < n:
            return False, -1, 'short by %d' % (n - len(got))
        diff = int(np.abs(ref.astype(np.int64) - got[:n]).max()) if n else 0
        tail = int(np.abs(got[n:]).max()) if len(got) > n else 0
        return diff == 0 and tail == 0, diff, 'tail max %d' % tail
    for i, r in enumerate(rep['sfx']):
        ref = ffmpeg_decode(os.path.join(GAME, r['file']))
        got = np.fromfile(os.path.join(work, 'sfx_%03d.raw' % i), np.int16).reshape(-1, r['ch'])
        good, diff, note = cmp(ref, got)
        results.append(('file %s' % r['file'], len(ref), diff, note, good)); ok &= good
    for t in rep['music']:
        ref = ffmpeg_decode(os.path.join(GAME, t['file']))
        if ref.shape[1] == 1:
            ref = np.repeat(ref, 2, axis=1)
        got = np.fromfile(os.path.join(work, 'mus_%s.raw' % t['name']), np.int16).reshape(-1, 2)
        good, diff, note = cmp(ref, got)
        results.append(('track %s' % t['name'], len(ref), diff, note, good)); ok &= good
    snrs = []
    mixes = [(t['name'], os.path.join(GAME, t['file'])) for t in rep['music'][:3]]
    mixes += [('sfx_%03d' % i, os.path.join(GAME, r['file'])) for i, r in enumerate(rep['sfx'])
              if os.path.exists(os.path.join(work, 'mix_sfx_%03d.raw' % i))]
    for key, path in mixes:
        # the runtime output frame k sits at source position k*step/65536 (step = round(rate/44100*65536));
        # compare with (a) a float model of the same linear interpolation, (b) ideal band-limited interpolation
        rs = read_rsd(path)
        x = ffmpeg_decode(path)[:, 0].astype(np.float64)[:rs['rate'] * 10]
        step = int(rs['rate'] * 65536.0 / OUT_RATE + 0.5)
        got = np.fromfile(os.path.join(work, 'mix_%s.raw' % key), np.int16).reshape(-1, 2)[:, 0].astype(np.float64)
        n = min(len(got), int((len(x) - 40) * 65536 // step))
        k0 = int(40 * 65536 // step) + 1
        pos = np.arange(k0, n, dtype=np.int64) * step
        xi = pos >> 16; fr = (pos & 0xffff) / 65536.0
        lin = x[xi] + (x[xi + 1] - x[xi]) * fr
        ideal = np.zeros(len(pos))
        for j in range(-31, 33):
            d = fr - j
            ideal += x[np.clip(xi + j, 0, len(x) - 1)] * np.sinc(d) * (0.42 + 0.5 * np.cos(np.pi * d / 32) + 0.08 * np.cos(2 * np.pi * d / 32))
        g = got[k0:n]
        model_err = float(np.abs(g - lin).max())
        e = ideal - g
        snr = 10 * np.log10(max((ideal ** 2).sum(), 1) / max((e ** 2).sum(), 1e-9))
        snrs.append((key, round(model_err, 1), round(snr, 1)))
    seams = []
    for f in sorted(glob.glob(os.path.join(work, 'loop_*.raw'))):
        x = np.fromfile(f, np.int16).reshape(-1, 2).astype(np.int64)
        marks = [int(v) for v in open(f[:-4] + '.seams').read().split()]
        d2 = np.abs(x[2:] - 2 * x[1:-1] + x[:-2]).max(axis=1)
        W = 11                                   # same statistic everywhere: max over an 11-frame window
        win = np.lib.stride_tricks.sliding_window_view(d2, W).max(axis=1)
        srt = np.sort(win)
        at = [(int(win[m - 5]), int(max(win[m - 5 - 2 * W], win[m - 5 + 2 * W])))
              for m in marks if m - 5 - 2 * W >= 0 and m - 5 + 2 * W < len(win)]
        worst = max(v for v, _ in at) if at else 0
        local = max(v / max(nb, 1) for v, nb in at) if at else 0   # vs the windows just before/after the seam
        rank = 100.0 * float(np.searchsorted(srt, worst)) / max(len(srt), 1)
        seams.append((os.path.basename(f)[5:-4], len(marks), worst, int(np.median(win)), int(np.percentile(win, 99.9)),
                      round(rank, 2), round(local, 2)))
    print('== 1. bit-exact decode: PSP decoder (host build) vs ffmpeg adpcm_psx; max|diff| must be 0')
    for name, n, diff, note, good in results:
        print('  %-58s %9d samples  max|diff| %d  %s  %s' % (name, n, diff, note, 'OK' if good else 'FAIL'))
    print('== 2. runtime resampler (first 10 s, 44.1 kHz): max |runtime - float model of the same linear')
    print('     interpolation| (implementation check, <=2 LSB) and SNR vs ideal band-limited interpolation')
    for s_ in snrs:
        print('  %-40s model max|diff| %4.1f LSB   SNR vs ideal %5.1f dB' % s_)
    print('== 3. loop seams rendered through the mixer: max |2nd difference| in the 11 frames around each seam,')
    print('     ranked among the same 11-frame statistic at every position of the render, and divided by the larger')
    print('     of the windows 22 frames before/after (a click: rank ~100% AND neighbours ratio >> 2)')
    for s_ in seams:
        print('  %-40s seams %2d  at-seam %6d  median %6d  p99.9 %6d  rank %6.2f%%  vs neighbours x%.2f' % s_)
    bad_seams = [s_ for s_ in seams if s_[5] > 99.5 and s_[6] > 2]
    ok &= all(m[1] <= 2 for m in snrs)
    json.dump(dict(decode=results, snr=snrs, seams=seams, ok=ok and not bad_seams),
              open(os.path.join(work, 'verify.json'), 'w'), indent=1)
    print('RESULT:', 'OK' if ok and not bad_seams else 'PROBLEM', '->', os.path.join(work, 'verify.json'))
    return 0 if ok and not bad_seams else 1

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest='cmd', required=True)
    p = sp.add_parser('inventory'); p.add_argument('--out')
    p = sp.add_parser('pack'); p.add_argument('--out', default=os.path.join(ROOT, 'build', 'audio'))
    p.add_argument('--car', default='famil_v'); p.add_argument('--full-level-music', action='store_true')
    p = sp.add_parser('verify'); p.add_argument('--pack', default=os.path.join(ROOT, 'build', 'audio'))
    p.add_argument('--host'); p.add_argument('--work')
    a = ap.parse_args()
    return {'inventory': cmd_inventory, 'pack': cmd_pack, 'verify': cmd_verify}[a.cmd](a) or 0

if __name__ == '__main__':
    sys.exit(main())
