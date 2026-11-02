#!/usr/bin/env python3
"""№1094: реплика продавца комиксов для заставки (viewer/bootbeep.h, viewer/bootscreen.h) — маленький файл bootvo.bin рядом с EBOOT.
Клип берётся с диска игры (hit-and-run-web/source/game/<путь из tools/bootvo_clip.txt>, формат RSD4VAG, PS-ADPCM моно) и кладётся как есть, без перекодирования:
  'BVO1', u32 частота, u32 число сэмплов (кратно 28), затем блоки PS-ADPCM по 16 байт. Хвост тишины в конце клипа срезается (по блокам, остаётся 30 мс запаса).
Запуск: python tools/bootvo_export.py --out build/bootvo.bin [--clip cbg/имя.rsd] [--ref ref.raw]   (--ref: ещё и эталонная распаковка s16le для проверки декодера движка)
Нет имени клипа в tools/bootvo_clip.txt — пишется пустой файл из 12 байт (движок его не берёт, заставка остаётся с нотами №1011), код 0."""
import argparse, os, struct, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import audio_export as AE   # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--out', required=True); ap.add_argument('--clip', default=None); ap.add_argument('--ref', default=None)
a = ap.parse_args()
clip = a.clip or next((l.strip() for l in (ROOT / 'tools' / 'bootvo_clip.txt').read_text(encoding='utf-8').splitlines() if l.strip() and not l.lstrip().startswith('#')), '')
if not clip:
    Path(a.out).parent.mkdir(parents=True, exist_ok=True); Path(a.out).write_bytes(b'BVO1' + bytes(8))
    print('bootvo: в tools/bootvo_clip.txt нет имени клипа — пустой файл, заставка с нотами'); sys.exit(0)
path = ROOT / 'hit-and-run-web' / 'source' / 'game' / clip
r = AE.read_rsd(str(path))
if r['ch'] != 1: sys.exit('bootvo: нужен моно-клип, у %s каналов %d' % (clip, r['ch']))
data = r['data']; nb = r['nblk']
# срез хвоста: блоки с нулевыми отсчётами (сдвиг/фильтр дают тишину) в конце, оставить 30 мс
pcm = AE.vag_decode_py(data, 1)[0]
last = len(pcm) - 1
while last > 0 and abs(pcm[last]) < 64: last -= 1
keep = min(nb, (last + int(r['rate'] * 0.03)) // 28 + 1)
data = data[:keep * 16]
blob = b'BVO1' + struct.pack('<II', r['rate'], keep * 28) + data
Path(a.out).parent.mkdir(parents=True, exist_ok=True); Path(a.out).write_bytes(blob)
if a.ref:
    ref = AE.vag_decode_py(data, 1)[0]
    Path(a.ref).write_bytes(struct.pack('<%dh' % len(ref), *ref))
print('bootvo: %s -> %s, %d Б, %d Гц, %d сэмплов (%.2f с)' % (clip, a.out, len(blob), r['rate'], keep * 28, keep * 28 / r['rate']))
