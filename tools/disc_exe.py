#!/usr/bin/env python3
"""№880 (отчёт сборки с NTSC-диска SLUS_206.24): исполняемый файл диска — по строке BOOT2 из SYSTEM.CNF (PAL SLES_518.97, NTSC SLUS_206.24).
Факты из исполняемого файла своего диска. pal() — европейский ли диск (адреса в briefcards_source/ticket_source сняты с PAL).
elf(path) -> (байты, сегменты, word(va), fileoff(va)); mips_strings(b, seg) — пары «lui rX + addiu rX,rX» с адресом инструкции и строкой по адресу."""
import re, struct
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
DISC_DIR = ROOT / 'hit-and-run-web/source/disc'


def disc_exe():
    cnf = DISC_DIR / 'SYSTEM.CNF'
    if cnf.exists():
        m = re.search(r'BOOT2\s*=\s*cdrom0:\\\\?([A-Z0-9_.]+)', cnf.read_text(errors='ignore'))
        if m and (DISC_DIR / m.group(1)).exists(): return DISC_DIR / m.group(1)
    return DISC_DIR / 'SLES_518.97'


def pal():
    return disc_exe().name == 'SLES_518.97'


def elf(path=None):
    b = Path(path or disc_exe()).read_bytes(); phoff = struct.unpack_from('<I', b, 28)[0]; size, n = struct.unpack_from('<HH', b, 42)
    seg = []
    for i in range(n):
        typ, off, va, _, sz, *_ = struct.unpack_from('<8I', b, phoff + i * size)
        if typ == 1: seg.append((off, va, sz))
    def fileoff(va): return next(off + va - base for off, base, sz in seg if base <= va < base + sz)
    def word(va): return struct.unpack_from('<I', b, fileoff(va))[0]
    return b, seg, word, fileoff


def cstr(b, fileoff, va):
    try: p = fileoff(va); e = b.index(0, p)
    except (StopIteration, ValueError): return None
    s = b[p:e]
    return s.decode() if 0 < len(s) < 256 and all(32 <= c < 127 for c in s) else None


def mips_strings(b, seg, fileoff, regs=(4, 5), back=8):
    """[(va инструкции addiu, регистр, строка)]: addiu rX,rX,lo с lui rX,hi не дальше back инструкций назад"""
    out = []
    for off, base, sz in seg:
        n = sz // 4; W = struct.unpack_from(f'<{n}I', b, off)
        for i in range(n):
            w = W[i]
            if w >> 26 != 9: continue
            rs, rt = (w >> 21) & 31, (w >> 16) & 31
            if rs != rt or rt not in regs: continue
            for k in range(1, back + 1):
                if i - k < 0: break
                u = W[i - k]
                if u >> 26 == 15 and (u >> 16) & 31 == rt:
                    lo = w & 65535; lo = lo - 65536 if lo >= 32768 else lo
                    s = cstr(b, fileoff, ((u & 65535) << 16) + lo)
                    if s: out.append((base + 4 * i, rt, s))
                    break
    return out
