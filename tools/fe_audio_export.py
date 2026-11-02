#!/usr/bin/env python3
"""Original FE NIS streams, separate SAP1: no level/music/ticket pack mutation.

13 runtime keys name 12 authored resources: FE_Gag_Homer dispatches its five
segments and is not a thirteenth NIS declaration. RSD PS-ADPCM stays bit exact.
"""
import argparse,hashlib,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import audio_export as A
import credvo_export as CV   # №1104: реплики титров (код Play Credits Dialog) — 31 дорожка KK_<диалог>_<n> после 12 реплик меню
NPC=('Grandpa','Moleman','Frink','Barney','Nick','Snake','Maggie')
HOMER=('ScratchHead','ScratchBum','Yawn','Nightmare','Stretch')
import os
CREDVO_OLD=os.environ.get('CREDVO_OLD')=='1'   # 1 — без реплик титров (как до №1104)
NAMES=tuple('FE_Gag_'+n for n in NPC)+tuple('FE_Gag_Homer_'+n for n in HOMER)
def export(path):
 resources=A.load_scripts()[0];rows=[];table=bytearray();body=bytearray();base=(64+64*(len(NAMES)+(0 if CREDVO_OLD else len(CV.clips())))+2047)//2048*2048   # начало данных: после таблицы, кратно 2048 (12 дорожек — 2048, с титрами — 4096)
 for name in NAMES:
  r=resources[name]
  if len(r['files'])!=1 or not r['stream'] or r['loop'] or r['pitch']!=(1.,1.) or r['trim']!=1.:raise ValueError('unsupported FE NIS parameters: '+name)
  source=Path(A.game_path(r['files'][0]));rs=A.read_rsd(source)
  if rs['ch'] not in (1,2) or rs['rate']>65535:raise ValueError('unsupported RSD: '+str(source))
  data=rs['data'];data=b''.join(data[i:i+16]*2 for i in range(0,len(data),16)) if rs['ch']==1 else data
  off=base+len(body);body+=data;body+=bytes(-len(body)%2048)
  table+=A.pack_name(name,28)+struct.pack('<IIIIIHBBhh4h',off,rs['nblk'],rs['nsamp'],0,rs['nsamp'],rs['rate'],2,0,-1,2,0,0,0,0)
  rows.append(dict(name=name,file=str(source.relative_to(ROOT/'hit-and-run-web/source/game')),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),rate=rs['rate'],source_channels=rs['ch'],frames=rs['nsamp'],bytes=len(data),trim=r['trim'],pitch=r['pitch'],streaming=r['stream'],loop=r['loop']))
 if not CREDVO_OLD:
  for name,source in CV.clips():
   rs=A.read_rsd(source)
   if rs['ch']!=1 or rs['rate']>65535:raise ValueError('unsupported credits RSD: '+str(source))
   data=b''.join(rs['data'][i:i+16]*2 for i in range(0,len(rs['data']),16))
   off=base+len(body);body+=data;body+=bytes(-len(body)%2048)
   table+=A.pack_name(name,28)+struct.pack('<IIIIIHBBhh4h',off,rs['nblk'],rs['nsamp'],0,rs['nsamp'],rs['rate'],2,0,-1,2,0,0,0,0)
   rows.append(dict(name=name,file=str(source.relative_to(ROOT/'hit-and-run-web/source/game')),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),rate=rs['rate'],source_channels=1,frames=rs['nsamp'],bytes=len(data),credits=True))
 header=b'SAP1'+struct.pack('<III',1,len(rows),64)+bytes(48)+table
 packet=header+bytes(base-len(header))+body;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(packet)
 return dict(format='SAP1',bytes=len(packet),sha256=hashlib.sha256(packet).hexdigest(),runtime_keys=13,authored_resources=12,credits_clips=0 if CREDVO_OLD else len(CV.clips()),homer_dispatcher='FE_Gag_Homer has no NIS resource; index0 selects one of five segment resources',implicit_defaults='Maggie/Yawn omit SetTrim; existing audio_export default1, pitch default1 and nonloop. Not a measured PS2 mix gain.',tracks=rows)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,default=ROOT/'build/fe_audio.sap');ap.add_argument('--manifest',type=Path);a=ap.parse_args();r=export(a.out)
 if a.manifest:a.manifest.parent.mkdir(parents=True,exist_ok=True);a.manifest.write_text(json.dumps(r,indent=2)+'\n')
 print('FE_AUDIO SAP1',r['bytes'],'bytes resources=12 runtime_keys=13 SHA256='+r['sha256'])
if __name__=='__main__':main()
