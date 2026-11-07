#!/usr/bin/env python3
"""Музыка игры в меню PSP (XMB), SND0.AT3 — правка Роба 130 (01.10): «мелодия как из телефонной будки, не напряжённая,
чтобы интро шли по нарастающей».

Источник — данные диска: экран телефонной будки (viewer/phone_view.c, событие Store: Store_region -> Muzak, разбор
tools/sound_export.py) играет sound/music/muzak.rsd (PS-ADPCM, 24 кГц, стерео, 68,6 с). У оригинала PS2 музыки XMB нет,
поэтому отрывок и его обработка — правило порта:
  отрывок DUR с из muzak.rsd; начало START — самое ровное место дорожки (наименьший разброс громкости по 0,5 с, при
  равенстве — тише), чтобы фон XMB был спокойным и интро игры после него шли по нарастающей; вход FADE_IN с, затухание
  FADE_OUT с; 24 -> 44,1 кГц (кодировщик берёт только 44,1 кГц 16 бит), пик -1 дБFS.
Решено терминалом за Роба (он велел не спрашивать): DUR 22 с, затухание 3 с, ATRAC3 132 кбит/с (LP2, частый формат
SND0 у самоделок: XMB его играет; образец Sony Tony Hawk — ATRAC3plus, его можно включить --codec atrac3plus).
Вкусовые ручки — ключи --start/--dur/--codec; превью отрывков: --preview DIR (WAV для Роба).

Кодировщик — открытый atracdenc (LGPL, github.com/dcherednik/atracdenc), собирается в build/tools/atracdenc (не в git);
закрытый Sony at3tool не берём. Без него снимок WAV готов, а SND0.AT3 — нет (выход 2).
Пишет build/assets/snd0.at3 и build/assets/snd0.json (параметры).
  tools/snd0.py [--start S] [--dur 22] [--codec atrac3|atrac3plus] [--out build/assets/snd0.at3] [--preview DIR]
  tools/snd0.py --verify FILE.at3 [--src]     гейт: RIFF WAVE, fmt ATRAC3 (0x0270) или AT3+ (0xFFFE + GUID), 44,1 кГц,
      стерео, длина 10..30 с, ≤ 500 КБ, есть петля smpl, ffmpeg разжимает; с --src — сходство с отрывком muzak (корреляция ≥ 0,8)
  tools/snd0.py --loop FILE.at3               дописать петлю smpl в готовый файл (правка 188)
Правка Роба 188 (01.10 22:2x, «музыки в меню PSP так и нет», снова 130): и AT3+, и ATRAC3 LP2 XMB молчал. Сверка с SND0.AT3 пяти
игр с карты Роба (Tomb Raider Anniversary, GTA LCS, LocoRoco, Cube, NFS Undercover; build/xmb_ref, только заголовки): у всех,
и ATRAC3, и AT3+, после fact идёт чанк smpl на 60 байт — одна петля от задержки кодера (второе число fact) до total + задержка − 1,
период 22676, нота 60, samplerData 24. У atracdenc smpl нет — дописываем тем же правилом (add_loop)"""
import argparse, json, os, struct, subprocess, sys, tempfile
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
FF = '/opt/homebrew/bin/ffmpeg'
SRC = ROOT / 'hit-and-run-web/source/game/sound/music/muzak.rsd'
# Правка Роба 262 (04.10: «музыку туда поставить самую брендовую — ту, что звучит в превью-видео»): дорожка демо-ролика диска
# (movies/demo.rmv, из него нарезан ICON1) по огибающей громкости ближе всего к sound/music/simpsons_theme.rsd (корреляция 0,74;
# следующая motown_intro 0,64) — тема Симпсонов, она же музыка главного меню (FE_region). Отрывок — с начала темы (узнаваемое
# вступление), длиной с превью. --track muzak — прежняя мелодия будки (правка 130)
# Правка Роба 307 (04.10, выбор из папки МУЗЫКА_МЕНЮ_НА_ВЫБОР, дорожка 12): «вот это надо добавить в меню на PSP при наведении…
# секунд 25 с затуханием» — sunday_drive (свободная езда уровней 1 и 4) с начала, 25 с, затухание 3 с. --track simpsons_theme — правка 262
TRACKS = {'simpsons_theme': ROOT / 'hit-and-run-web/source/game/sound/music/simpsons_theme.rsd', 'muzak': SRC,
          'sunday_drive': ROOT / 'hit-and-run-web/source/game/sound/music/sunday_drive.rsd'}
ENC = ROOT / 'build/tools/atracdenc/build/src/atracdenc'
# Правка Роба 344 (05.10: «просил 25 с, сейчас 9–10; у Тони Хока 15–16 с, мелодия недоиграна»): 16 с с начала sunday_drive — ровно фраза
# 8 тактов (120 уд/мин, такт 2 с), затухание 2 с на последнем такте. 16 с ATRAC3 LP2 = 265 КБ — как SND0 Need for Speed Undercover
# с карты Роба (build/xmb_ref/44Needf: ATRAC3 132 кбит/с, 16,0 с, 265 872 Б, XMB играет); 25 с (414 КБ) XMB не играл
DUR, FADE_IN, FADE_OUT, PEAK_DB = 16.0, 0.4, 2.0, -1.0
CODEC, BITRATE = 'atrac3', 132   # 01.10 14:0x: AT3+ PSP в XMB не сыграл (Роб: «нету музыки»); atracdenc -e atrac3 без --bitrate даёт стандартный ATRAC3 LP2 132 кбит/с (кадр 384) — классический SND0 самоделок
MIN_SEC, MAX_SEC, MAX_BYTES = 10.0, 30.0, 500 * 1024
SND0_OLD_FACT = bool(__import__('os').environ.get('SND0_OLD_FACT'))   # красный правки 196: fact как пишет atracdenc
AT3P_GUID = bytes.fromhex('bfaa23e958cb7144a119fffa01e4ce62')   # KSDATAFORMAT ATRAC3plus (как у SND0.AT3 Tony Hawk)
SR = 44100


def decode(path, sr=SR, start=None, dur=None):
    """Файл (rsd/wav/at3) -> float32 [n, 2] через ffmpeg."""
    cmd = [FF, '-v', 'error']
    if start is not None: cmd += ['-ss', f'{start:.3f}']
    if dur is not None: cmd += ['-t', f'{dur:.3f}']
    cmd += ['-i', str(path), '-ac', '2', '-ar', str(sr), '-f', 'f32le', '-']
    r = subprocess.run(cmd, capture_output=True)
    if r.returncode: raise SystemExit('snd0: ffmpeg: ' + r.stderr.decode(errors='replace')[-200:])
    return np.frombuffer(r.stdout, np.float32).reshape(-1, 2)


def pick_start(x, dur, sr=SR, hop=0.5):
    """Самое ровное место: окно dur с с наименьшим разбросом громкости (дБ по 0,5 с), при равенстве — тише."""
    h = int(sr * hop); n = len(x) // h
    db = 20 * np.log10(np.sqrt((x[:n * h].reshape(n, h, 2) ** 2).mean((1, 2))) + 1e-6)
    w = int(round(dur / hop)); best = None
    for i in range(0, n - w + 1):
        seg = db[i:i + w]; score = float(seg.std()) + 0.02 * float(seg.mean())
        if best is None or score < best[0]: best = (score, i * hop, float(seg.std()), float(seg.mean()))
    return best[1], best[2], best[3]


def excerpt(start, dur):
    x = decode(SRC, start=start, dur=dur).copy()
    t = np.arange(len(x)) / SR
    env = np.clip(t / FADE_IN, 0, 1) * np.clip((dur - t) / FADE_OUT, 0, 1)
    env = np.sin(env * np.pi / 2) ** 2                     # мягкие вход и затухание (без щелчка)
    x *= env[:, None]
    x *= 10 ** (PEAK_DB / 20) / max(float(np.abs(x).max()), 1e-6)
    return x


def write_wav(path, x):
    pcm = (np.clip(x, -1, 1) * 32767).astype('<i2').tobytes()
    hdr = b'RIFF' + struct.pack('<I', 36 + len(pcm)) + b'WAVEfmt ' + struct.pack('<IHHIIHH', 16, 1, 2, SR, SR * 4, 4, 16)
    Path(path).write_bytes(hdr + b'data' + struct.pack('<I', len(pcm)) + pcm)


def chunks(b):
    """RIFF WAVE -> {id: (смещение данных, длина)}."""
    out, p = {}, 12
    while p + 8 <= len(b):
        cid, ln = b[p:p + 4], struct.unpack_from('<I', b, p + 4)[0]
        out.setdefault(cid.decode('latin1'), (p + 8, ln)); p += 8 + ln + (ln & 1)
    return out


def verify_bytes(b):
    """Гейт формата SND0 без файлов: (список причин FAIL, строка сведений)."""
    if b[:4] != b'RIFF' or b[8:12] != b'WAVE': return ['не RIFF WAVE'], ''
    ch = chunks(b)
    if 'fmt ' not in ch or 'data' not in ch: return ['нет fmt или data'], ''
    o, ln = ch['fmt ']
    tag, nch, sr, brate, align = struct.unpack_from('<HHIIH', b, o)
    fails = []
    if tag == 0x0270: kind, spf = 'ATRAC3', 1024
    elif tag == 0xFFFE and ln >= 40 and b[o + 24:o + 40] == AT3P_GUID: kind, spf = 'ATRAC3plus', 2048
    else: return [f'кодек 0x{tag:04x} — не ATRAC3/ATRAC3plus (PCM XMB не играет)'], ''
    if sr != SR: fails.append(f'частота {sr} (нужно {SR})')
    if nch != 2: fails.append(f'каналов {nch} (нужно 2)')
    if not align: return fails + ['block align 0'], ''
    sec = ch['data'][1] // align * spf / sr
    if not MIN_SEC <= sec <= MAX_SEC: fails.append(f'длина {sec:.1f} с (нужно {MIN_SEC:g}..{MAX_SEC:g})')
    if len(b) > MAX_BYTES: fails.append(f'{len(b)} Б > {MAX_BYTES} Б')
    if 'smpl' not in ch: fails.append('нет петли smpl (у SND0 всех игр Роба она есть; без неё XMB молчит — правка 188)')
    # правка 196 (03.10): у шести эталонов (пять игр с карты Роба и Tony Hawk) fact = (total, задержка) и total + задержка +
    # EXTRA[кодек] = сэмплов в data ровно (у NFS — меньше): XMB-декодер пропускает задержку и EXTRA. У atracdenc total = длине
    # входа, и звук «кончался» за данными на 1093 сэмпла — XMB молчал (и ATRAC3, и AT3+). Петля — [задержка, total + задержка − 1]
    frames = ch['data'][1] // align; have = frames * spf
    if 'fact' in ch and ch['fact'][1] >= 8:
        total, delay = struct.unpack_from('<II', b, ch['fact'][0])
        if total + delay + EXTRA[kind] > have:
            fails.append(f'fact: звука {total} + задержка {delay} + {EXTRA[kind]} = {total + delay + EXTRA[kind]} > сэмплов в данных {have} '
                         f'(у SND0 игр Sony — не больше; правка 196)')
        if 'smpl' in ch and ch['smpl'][1] >= 60:
            ls, le = struct.unpack_from('<II', b, ch['smpl'][0] + 44)
            if ls < delay or le > total + delay - 1 or le <= ls:
                fails.append(f'петля {ls}..{le} вне звука [{delay}, {total + delay - 1}] (у Sony — внутри; правка 196)')
    else: fails.append('нет fact (total и задержка)')
    return fails, f'{kind} {sr} Гц {nch} кан, {brate * 8 / 1000:.0f} кбит/с, {sec:.1f} с'


def verify(path, src=False, meta=None):
    b = Path(path).read_bytes(); fails, info = verify_bytes(b)
    if not fails:
        try: y = decode(path)
        except SystemExit as e: fails.append(f'ffmpeg не разжимает: {e}'); y = None
        if y is not None and src:
            meta = meta or json.loads(Path(path).with_suffix('.json').read_text())
            global SRC
            global FADE_OUT
            SRC = next((p for p in TRACKS.values() if p.name == meta.get('src')), SRC)   # правка 262: дорожка из .json
            FADE_OUT = meta.get('fade_out', FADE_OUT)   # правка 307: затухание отрывка из .json
            ref = excerpt(meta['start'], meta['dur'])
            h = SR // 20; n = min(len(y), len(ref)) // h
            ey = np.sqrt((y[:n * h].reshape(n, h, 2) ** 2).mean((1, 2))); er = np.sqrt((ref[:n * h].reshape(n, h, 2) ** 2).mean((1, 2)))
            best = max(float(np.corrcoef(ey[max(0, k):n + min(0, k)], er[max(0, -k):n - max(0, k)])[0, 1]) for k in range(-40, 41))
            info += f', огибающая против {SRC.stem} {meta["start"]:g}+{meta["dur"]:g} с: корреляция {best:.2f}'
            if best < 0.8: fails.append(f'не отрывок {SRC.stem} (корреляция огибающей {best:.2f} < 0,8)')
    print(('FAIL: ' + fails[0]) if fails else f'OK: SND0 {path}: {info}')
    [print('  - ' + f) for f in fails[1:]]
    return fails


DELAY_MIN = 2048   # задержка не меньше: у эталонов Sony 1837–3560, у pop-fe (github.com/sahlberg/pop-fe, riff.py: SND0 из atracdenc для XMB PSP) — 0x800
EXTRA = {'ATRAC3': 69, 'ATRAC3plus': 368}   # сэмплы, которые декодер PSP пропускает сверх задержки fact (у эталонов Sony — разница ровно эта)


def fix_fact(path):
    """правка 196: fact total = сэмплов в data − задержка − EXTRA (правило всех эталонов Sony), петля smpl — по нему же"""
    b = bytearray(Path(path).read_bytes()); ch = chunks(bytes(b))
    o = ch['fmt '][0]; tag, align = struct.unpack_from('<H', b, o)[0], struct.unpack_from('<H', b, o + 12)[0]
    kind, spf = ('ATRAC3', 1024) if tag == 0x0270 else ('ATRAC3plus', 2048)
    have = ch['data'][1] // align * spf
    fo, fl = ch['fact']; total, delay = struct.unpack_from('<II', b, fo)
    delay = max(delay, DELAY_MIN)
    new = have - delay - EXTRA[kind]
    struct.pack_into('<II', b, fo, new, delay)
    if 'smpl' in ch: struct.pack_into('<II', b, ch['smpl'][0] + 44, delay, new + delay - 1)
    Path(path).write_bytes(bytes(b)); return new != total


def add_loop(path):
    """правка 188: чанк smpl как у SND0.AT3 игр Sony — одна петля [задержка, total + задержка − 1] сразу после fact"""
    b = bytearray(Path(path).read_bytes()); ch = chunks(bytes(b))
    if 'smpl' in ch: return False
    if 'fact' not in ch or 'data' not in ch: raise SystemExit(f'snd0: {path}: нет fact или data — петлю не из чего считать')
    o, ln = ch['fact']; total, delay = struct.unpack_from('<II', b, o)
    body = struct.pack('<15I', 0, 0, 22676, 60, 0, 0, 0, 1, 24, 0, 0, delay, total + delay - 1, 0, 0)
    at = o + ln + (ln & 1)
    b[at:at] = b'smpl' + struct.pack('<I', len(body)) + body
    struct.pack_into('<I', b, 4, len(b) - 8)
    Path(path).write_bytes(bytes(b)); return True


def main():
    global SRC, FADE_OUT
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=str(ROOT / 'build/assets/snd0.at3'))
    ap.add_argument('--start', type=float, help='начало отрывка, с (по умолчанию — самое ровное место)')
    ap.add_argument('--dur', type=float, default=None, help='длина отрывка, с (№1162: заданная явно берётся как есть, даже 16; по умолчанию — 16, у sunday_drive 25, у simpsons_theme 20)')
    ap.add_argument('--codec', default=CODEC, choices=['atrac3', 'atrac3plus'])
    ap.add_argument('--preview', help='папка: WAV отрывка и ещё двух мест дорожки — послушать Робу')
    ap.add_argument('--verify'); ap.add_argument('--src', action='store_true'); ap.add_argument('--loop')
    ap.add_argument('--track', default='sunday_drive', choices=sorted(TRACKS), help='дорожка диска: sunday_drive (правка 307), simpsons_theme (262), muzak (130)')
    ap.add_argument('--fade-out', type=float, help='затухание в конце, с (по умолчанию %g; правка 307 — 3)' % FADE_OUT)
    a = ap.parse_args()
    dur_given = a.dur is not None and os.environ.get('SND0_DUR1162_OLD') != '1'   # №1162: «--dur 16» раньше совпадало с умолчанием и молча становилось 25 (принятый SND0 — 16 с)
    if a.dur is None: a.dur = DUR
    if not a.verify and not a.loop:
        SRC = TRACKS[a.track]
        if a.track == 'simpsons_theme' and a.start is None: a.start = 0.0   # вступление темы
        if a.track == 'simpsons_theme' and a.dur == DUR and not dur_given: a.dur = 20.0      # длиной с превью ICON1 (20 с)
        if a.track == 'sunday_drive' and a.start is None: a.start = 0.0     # правка 307: с начала дорожки, как Роб слушал
        if a.track == 'sunday_drive' and a.dur == DUR and not dur_given: a.dur = 25.0         # «секунд 25»
        if a.track == 'sunday_drive' and a.fade_out is None: a.fade_out = 3.0   # «с затуханием»
    if a.fade_out is not None: FADE_OUT = a.fade_out
    if a.verify: sys.exit(1 if verify(a.verify, a.src) else 0)
    if a.loop:
        if not SND0_OLD_FACT: fix_fact(a.loop)
        print(f'snd0: петля smpl {"дописана" if add_loop(a.loop) else "уже есть"}: {a.loop}'); sys.exit(1 if verify(a.loop) else 0)
    full = decode(SRC)
    st, sd, mean = pick_start(full, a.dur)
    if a.start is not None: st = a.start
    print(f'snd0: {SRC.name} {len(full) / SR:.1f} с; отрывок {st:g}+{a.dur:g} с (ровное место: разброс {sd:.1f} дБ, средняя {mean:.1f} дБFS)')
    x = excerpt(st, a.dur)
    if a.preview:
        d = Path(a.preview); d.mkdir(parents=True, exist_ok=True)
        write_wav(d / f'snd0_выбран_{st:g}с.wav', x)
        for alt in sorted({0.0, round(len(full) / SR - a.dur - 1, 1)} - {st}):
            write_wav(d / f'snd0_другое_{alt:g}с.wav', excerpt(alt, a.dur))
        print(f'snd0: превью в {d}')
    out = Path(a.out); out.parent.mkdir(parents=True, exist_ok=True)
    meta = dict(src=SRC.name, start=st, dur=a.dur, fade_in=FADE_IN, fade_out=FADE_OUT, peak_db=PEAK_DB, codec=a.codec, bitrate=BITRATE)
    if not ENC.exists():
        print(f'snd0: кодировщика ATRAC3 нет ({ENC.relative_to(ROOT)}): собрать открытый atracdenc '
              f'(git clone https://github.com/dcherednik/atracdenc build/tools/atracdenc; cmake -S build/tools/atracdenc '
              f'-B build/tools/atracdenc/build -DCMAKE_BUILD_TYPE=Release -DATRACDENC_PCM_IO_BACKEND=libsndfile; '
              f'cmake --build build/tools/atracdenc/build --target atracdenc). SND0.AT3 не собран.')
        sys.exit(2)
    with tempfile.TemporaryDirectory(prefix='shar-snd0-') as tmp:
        wav = Path(tmp) / 'snd0.wav'; write_wav(wav, x)
        cmd = [str(ENC), '-e', a.codec, '-i', str(wav), '-o', str(out), '--container', 'riff']
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode: raise SystemExit('snd0: atracdenc: ' + (r.stderr or r.stdout)[-300:])
    if not SND0_OLD_FACT: fix_fact(out)   # правка 196: fact по правилу Sony (звук не длиннее данных)
    add_loop(out)   # правка 188: петля smpl, как у SND0 игр Sony
    out.with_suffix('.json').write_text(json.dumps(meta, ensure_ascii=False, indent=1))
    print(f'snd0: {out} {out.stat().st_size} Б')
    sys.exit(1 if verify(out, True, meta) else 0)


if __name__ == '__main__':
    main()
