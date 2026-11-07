#!/usr/bin/env python3
"""Какой это диск: читает SYSTEM.CNF прямо из образа ISO 9660, без распаковки (секунды на 1,6 ГБ).
  python3 app/core/iso_id.py ОБРАЗ.iso  ->  JSON {"ok": true, "exe": "SLES_518.97", "region": "PAL", "volume": "..."}
SHAR: PAL SLES_518.97, NTSC SLUS_206.24 (tools/disc_exe.py). Другой диск или не ISO — ok=false и причина."""
import json, re, struct, sys

GAMES = {'SLES_518.97': 'PAL', 'SLUS_206.24': 'NTSC'}
SECTOR = 2048


def _dir_entries(f, lba, size):
    f.seek(lba * SECTOR); data = f.read(size); i = 0
    while i < len(data):
        n = data[i]
        if n == 0:   # записи не пересекают границу сектора: остаток сектора пуст
            i = (i // SECTOR + 1) * SECTOR; continue
        ext_lba, ext_size = struct.unpack_from('<I', data, i + 2)[0], struct.unpack_from('<I', data, i + 10)[0]
        name_len = data[i + 32]; name = data[i + 33:i + 33 + name_len].decode('ascii', 'replace')
        yield name.split(';')[0].upper(), ext_lba, ext_size
        i += n


def identify(path):
    try:
        with open(path, 'rb') as f:
            f.seek(16 * SECTOR); pvd = f.read(SECTOR)
            if pvd[1:6] != b'CD001': return {'ok': False, 'why': 'not an ISO disc image'}
            volume = pvd[40:72].decode('ascii', 'replace').strip('\x00 ')
            root = pvd[156:190]; lba, size = struct.unpack_from('<I', root, 2)[0], struct.unpack_from('<I', root, 10)[0]
            cnf = next(((l, s) for n, l, s in _dir_entries(f, lba, size) if n == 'SYSTEM.CNF'), None)
            if not cnf: return {'ok': False, 'why': 'no SYSTEM.CNF: not a PS2 game disc', 'volume': volume}
            f.seek(cnf[0] * SECTOR); text = f.read(min(cnf[1], 4096)).decode('ascii', 'replace')
    except OSError as e:
        return {'ok': False, 'why': f'cannot read the file ({e.strerror})'}
    m = re.search(r'BOOT2\s*=\s*cdrom0:\\?([A-Z0-9_.]+)', text)
    exe = m.group(1) if m else ''
    if exe not in GAMES: return {'ok': False, 'why': 'a different PS2 game', 'exe': exe, 'volume': volume}
    return {'ok': True, 'exe': exe, 'region': GAMES[exe], 'volume': volume}


if __name__ == '__main__':
    print(json.dumps(identify(sys.argv[1]), ensure_ascii=False))
