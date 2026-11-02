#!/usr/bin/env python3
"""Validate the owner's PAL instructions proving the Itchy & Scratchy reward contract.
No emulator. Fixed words include registration, branches, arguments,
card-count predicate, persistent bit names and Character Sheet serialization.
"""
from pathlib import Path
import hashlib,json,struct
import disc_exe as DE
ROOT=Path(__file__).resolve().parents[1]
DISC=DE.disc_exe()   # №880: из SYSTEM.CNF
def facts_any():
 # №880 (NTSC SLUS_206.24: те же инструкции по другим адресам): только проверки без адресов — имена битов и таблица View Movies (2,3,8,4,5,6,7)
 b=DISC.read_bytes()
 for name in ('GagCheckCollCards','GagCheckMovie','is_cbg_first','is_ticket'):assert b'\0'+name.encode()+b'\0' in b,f'нет строки {name}'
 assert struct.pack('<7I',2,3,8,4,5,6,7) in b,'нет таблицы View Movies (2,3,8,4,5,6,7)'
 return dict(source=str(DISC.relative_to(ROOT)),sha256=hashlib.sha256(b).hexdigest(),words={},
             eligibility='seven complete level sets; each original count equals7 (49 total)',global_bits={'intro':0,'ticket':1},
             saved_movie_unlock={'zero_based_level':2,'stride':620,'offset':556,'view_movie':8},
             serialization={'object_start':4,'bytes':7140,'covers_ticket_bit_offset':7140},
             dialogue={'success':'third argument/dc','first_failure':'fourth argument/e4','repeat_failure':'fifth argument/e0'},
             movie='CBG success sets global ticket plus saved L3 FMV unlock before viewing; movie check tests saved L3 byte')
def facts():
 if not DE.pal():return facts_any()
 b=DISC.read_bytes();po=struct.unpack_from('<I',b,28)[0];ps,n=struct.unpack_from('<HH',b,42);segs=[]
 for i in range(n):
  typ,off,va,_,size,*_=struct.unpack_from('<8I',b,po+i*ps)
  if typ==1:segs.append((off,va,size))
 def fileoff(va):return next(off+va-base for off,base,size in segs if base<=va<base+size)
 def word(va):return struct.unpack_from('<I',b,fileoff(va))[0]
 expected={
  0x1550dc:0x24a5a5d0,0x1550e0:0x24c67940,0x155110:0x24a5a5e8,0x155114:0x24c67b48,
  0x157964:0xa0430058,0x157b68:0x24030002,0x157b6c:0xa0430058,
  0x156484:0x0c062a02,0x156488:0x24050001,
  0x156494:0x24130001,0x156498:0x0000802d,0x1564a0:0x2a020007,
  0x1564b8:0x0c051486,0x1564bc:0x0200282d,0x1564c0:0x5440fff7,0x1564c4:0x26100001,
  0x1564c8:0x0000982d,0x1564cc:0x1260006d,
  0x145218:0x00052940,0x145220:0x8c820028,0x145224:0x38420007,0x14522c:0x2c420001,
  0x1564dc:0x24060001,0x1564e4:0x0c062a0e,0x1564e8:0x24050001,
  0x1564f8:0x0c0627ce,0x1564fc:0x24050002,0x15651c:0x8c5500dc,
  0x156690:0x0c062a02,0x156694:0x0000282d,0x1566bc:0x8c5500e0,
  0x1566d0:0x0c062a0e,0x1566d4:0x24060001,0x1566f0:0x8c5500e4,
  0x156850:0x0c0627c6,0x156854:0x24050002,0x156858:0x1040000d,
  0x156878:0x24a50059,0x156880:0x0c06722c,0x1568ac:0x8c5500e0,
  0x189f18:0x2402026c,0x189f2c:0x9042022c,0x189f38:0x2402026c,0x189f54:0xa043022c,
  0x18a81c:0x90821be4,0x18a850:0x24831be4,
  0x18a99c:0x24a50448,0x18a9ac:0x26040008,0x18a9b4:0x24a50458,
  0x189c60:0x24061be4,0x1892d8:0x24840004,0x1892f0:0x24a21be0,
  0x189378:0x88a20003,0x189380:0xa8820003,0x189388:0xb8820000,
  0x1893f0:0x24041be4,0x1893f8:0x26250004,
 }
 for pc,w in expected.items():assert word(pc)==w,f'SLES word mismatch {pc:#x}: {word(pc):#x} != {w:#x}'
 for va,name in [(0x45a5d0,'GagCheckCollCards'),(0x45a5e8,'GagCheckMovie'),(0x460448,'is_cbg_first'),(0x460458,'is_ticket')]:
  off=fileoff(va);assert b[off:b.index(0,off)].decode()==name
 # Original View Movies map assigns level3's saved FMV-unlocked byte to fmv8.
 assert tuple(word(0x466c38+4*i) for i in range(7))==(2,3,8,4,5,6,7)
 return dict(source=str(DISC.relative_to(ROOT)),sha256=hashlib.sha256(b).hexdigest(),words={hex(k):hex(v) for k,v in expected.items()},
             eligibility='seven complete level sets; each original count equals7 (49 total)',global_bits={'intro':0,'ticket':1},
             saved_movie_unlock={'zero_based_level':2,'stride':620,'offset':556,'view_movie':8},
             serialization={'object_start':4,'bytes':7140,'covers_ticket_bit_offset':7140},
             dialogue={'success':'third argument/dc','first_failure':'fourth argument/e4','repeat_failure':'fifth argument/e0'},
             movie='CBG success sets global ticket plus saved L3 FMV unlock before viewing; movie check tests saved L3 byte')
if __name__=='__main__':
 import sys
 print(json.dumps(facts_any() if '--any' in sys.argv else facts(),indent=2))
