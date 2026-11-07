"""№1178: боковая таблица вывесок уровня sign1178.bin (формат — viewer/sign1178.h).
  python3 -I tools/far1177/signs.py LEVEL.shr OUT.bin [--list tools/far1177/signs_L5.json] [--mul 0.5]
  python3 -I tools/far1177/signs.py --curate LEVEL.shr IDS.txt OUT.json   (один раз: номера текстур -> подписи рисунка)
Список вывесок хранится по ПОДПИСИ рисунка (sha1 уровня 0, RGBA, размер), не по номеру: номера меняются при пересборке уровня.
Подпись не нашлась — ошибка (устаревший список не должен молча сдвигать чужие текстуры)."""
import sys, json, struct, hashlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import tex_gate as T

def sigs(b):
    recs, *_ = T.walk(b); out = {}
    for i, r in enumerate(recs):
        a = T.level0(b, r); out.setdefault(hashlib.sha1(a.tobytes() + struct.pack('<HH', r[1], r[2])).hexdigest()[:16], []).append(i)
    return recs, out

def curate(level, ids_file, out):
    b = Path(level).read_bytes(); recs, sg = sigs(b); inv = {i: s for s, v in sg.items() for i in v}
    ids = [int(x) for x in Path(ids_file).read_text().replace(',', ' ').split() if x.strip().isdigit()]
    lst = [{'sig': inv[i], 'w': recs[i][1], 'h': recs[i][2], 'id_at_curation': i} for i in sorted(set(ids))]
    Path(out).write_text(json.dumps({'level': Path(level).parent.name, 'mul': 0.5, 'signs': lst}, ensure_ascii=False, indent=1)); print('записано', len(lst), '->', out)

def build(level, out, lst, mul=None):
    b = Path(level).read_bytes(); recs, sg = sigs(b); d = json.loads(Path(lst).read_text()); k = int(round((mul if mul else d['mul']) * 4096))
    ent = []; miss = []
    for s in d['signs']:
        v = sg.get(s['sig'])
        if not v: miss.append(s); continue
        ent += [(i, k) for i in v]   # одинаковый рисунок под двумя номерами — оба
    if miss: raise SystemExit('нет текстуры с подписью: %s' % [m['id_at_curation'] for m in miss])
    ent = sorted(set(ent)); blob = b'SG78' + struct.pack('<III', 1, len(recs), len(ent)) + b''.join(struct.pack('<HH', t, m) for t, m in ent)
    Path(out).write_bytes(blob); print('sign1178: записей %d, множитель %.3f, файл %d Б -> %s' % (len(ent), k / 4096, len(blob), out))

if __name__ == '__main__':
    a = sys.argv[1:]
    if a and a[0] == '--curate': curate(a[1], a[2], a[3]); raise SystemExit
    mul = float(a[a.index('--mul') + 1]) if '--mul' in a else None
    lst = a[a.index('--list') + 1] if '--list' in a else str(Path(__file__).parent / ('signs_%s.json' % ('L' + Path(a[0]).parent.name[1:] if Path(a[0]).parent.name.startswith('l') and Path(a[0]).parent.name[1:].isdigit() else 'L1')))
    build(a[0], a[1], lst, mul)
