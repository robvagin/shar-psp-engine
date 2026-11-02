#!/usr/bin/env python3
"""Facts extracted from Robert's SLES_518.97 (his PAL disc).
Original 0x19e378..0x19e770 compares b_<giver>, then loads its briefing P3D.
We validate the actual pointer-building instructions; string adjacency alone is not evidence.
"""
import struct
from pathlib import Path
import disc_exe as DE
ROOT=Path(__file__).resolve().parents[1]
DISC=DE.disc_exe()   # №880: исполняемый файл из SYSTEM.CNF (PAL SLES_518.97, NTSC SLUS_206.24)
def scan_mappings():
 # №880 (NTSC: функция брифинга сдвинута на −0x414, адреса PAL не подходят): по коду — addiu a1 "b_<имя>", следом не дальше 96 Б addiu a0 "…/mis…";
 # на PAL даёт те же 15 пар, что адреса ниже (проверка: python tools/briefcards_source.py --check-scan)
 b,seg,word,fileoff=DE.elf(DISC);ms=DE.mips_strings(b,seg,fileoff)
 names=[(va,s) for va,r,s in ms if r==5 and s.startswith('b_') and '%' not in s]
 paths=[(va,s) for va,r,s in ms if r==4 and s.lower().startswith('art/frontend/dynaload/images/mis')]
 out={}
 for nv,nm in names:
  pp=[s for va,s in paths if nv<va<=nv+96]
  if pp:out[nm[2:]]=pp[0].lower()
 return out
def builtin_mappings():
 if not DE.pal():return scan_mappings()
 b=DISC.read_bytes();phoff=struct.unpack_from('<I',b,28)[0];size,n=struct.unpack_from('<HH',b,42)
 seg=[]
 for i in range(n):
  typ,off,va,_,sz,*_=struct.unpack_from('<8I',b,phoff+i*size)
  if typ==1:seg.append((off,va,sz))
 def fileoff(va):return next(off+va-base for off,base,sz in seg if base<=va<base+sz)
 def word(va):return struct.unpack_from('<I',b,fileoff(va))[0]
 def ptr(va,reg):
  w=word(va);assert w>>16==((9<<10)|(reg<<5)|reg),hex(va)
  hi=0x46
  lo=w&65535;lo=lo-65536 if lo>=32768 else lo
  p=fileoff((hi<<16)+lo);return b[p:b.index(0,p)].decode()
 # These are factual instruction addresses on the owned PAL executable.
 low_name=[0x19e39c]+[0x19e3d4+64*i for i in range(13)]+[0x19e728]
 low_path=[0x19e3c8]+[0x19e408+64*i for i in range(12)]+[0x19e704,0x19e75c]
 out={}
 for np,pp in zip(low_name,low_path):
  # Each return/load path follows the comparison of the NPC constructed at np.
  assert word(np-4)==0x0c0c8212 or word(np+4)==0x0c0c8212
  assert word(pp-4)==0x10000000|((0x19e708-(pp-4+4))//4&0xffff) or pp in (0x19e704,0x19e75c)
  name=ptr(np,5);path=ptr(pp,4)
  assert name.startswith('b_') and path.startswith('art/frontend/dynaload/images/mis')
  out[name[2:]]=path.lower()
 return out

def builtin_paths():
 # PS/HW are also loaded by original failure/tutorial flows; all six misXX assets exist on the disc.
 return sorted(set(builtin_mappings().values())|{str(p.relative_to(ROOT/'hit-and-run-web/source/game')).lower() for p in (ROOT/'hit-and-run-web/source/game/art/frontend/dynaload/images').glob('misxx_*.p3d')})
if __name__=='__main__':
 import sys
 if '--check-scan' in sys.argv:
  a,c=builtin_mappings(),scan_mappings();print('OK' if a==c else 'FAIL',len(a),len(c));sys.exit(0 if a==c else 1)
 for npc,path in builtin_mappings().items():print(npc,path)
